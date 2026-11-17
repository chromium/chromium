// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// A small in-memory DOM, just large enough to run the viewer script in
// templates/map.html under Node. codebase_map_test.py prepends this file to
// the template's own script blocks, so the tests exercise the code that ships
// rather than a copy of it.
//
// It implements only what the viewer uses: elements with attributes, classes,
// `dataset`, text content and child lists; bubbling events; and a selector
// engine for tag, #id, .class, [attr="value"], :scope and :not(), joined by
// descendant and child (`>`) combinators. Anything else throws, so a viewer
// change that needs more fails the tests loudly instead of passing by
// accident. There is no layout or CSS: `page.visibleRows()` applies the two
// stylesheet rules that hide tree rows itself.

'use strict';

const VOID_TAGS = new Set(['br', 'input', 'link', 'meta']);

class FakeText {
  constructor(data) {
    this.nodeType = 3;
    this.data = String(data);
    this.parentNode = null;
  }

  get textContent() {
    return this.data;
  }
}

class FakeClassList {
  constructor(element) {
    this.element = element;
  }

  names() {
    return (this.element.getAttribute('class') || '')
        .split(/\s+/)
        .filter(Boolean);
  }

  contains(name) {
    return this.names().includes(name);
  }

  add(name) {
    if (!this.contains(name)) {
      this.element.setAttribute('class', [...this.names(), name].join(' '));
    }
  }

  remove(name) {
    this.element.setAttribute(
        'class', this.names().filter(n => n !== name).join(' '));
  }

  toggle(name, force) {
    const on = force === undefined ? !this.contains(name) : Boolean(force);
    if (on) {
      this.add(name);
    } else {
      this.remove(name);
    }
    return on;
  }
}

class FakeElement {
  constructor(tagName, ownerDocument) {
    this.nodeType = 1;
    this.tagName = tagName.toLowerCase();
    this.ownerDocument = ownerDocument;
    this.parentNode = null;
    this.childNodes = [];
    this.attributes = new Map();
    this.listeners = new Map();
    this.classList = new FakeClassList(this);
    this.style = {};
    this.value = '';
    this.checked = false;
    this.indeterminate = false;
    const attributes = this.attributes;
    this.dataset = new Proxy({}, {
      get: (_, key) =>
          (typeof key === 'string' ? attributes.get('data-' + key) : undefined),
      set: (_, key, value) => {
        attributes.set('data-' + key, String(value));
        return true;
      },
    });
  }

  get id() {
    return this.getAttribute('id') || '';
  }

  get className() {
    return this.getAttribute('class') || '';
  }

  set className(value) {
    this.setAttribute('class', value);
  }

  getAttribute(name) {
    return this.attributes.has(name) ? this.attributes.get(name) : null;
  }

  setAttribute(name, value) {
    this.attributes.set(name, String(value));
  }

  get parentElement() {
    return this.parentNode;
  }

  append(...items) {
    for (const item of items) {
      const node = typeof item === 'string' ? new FakeText(item) : item;
      if (node.parentNode) {
        const siblings = node.parentNode.childNodes;
        siblings.splice(siblings.indexOf(node), 1);
      }
      node.parentNode = this;
      this.childNodes.push(node);
    }
  }

  replaceChildren(...items) {
    for (const child of this.childNodes)
      child.parentNode = null;
    this.childNodes = [];
    this.append(...items);
  }

  get textContent() {
    return this.childNodes.map(c => c.textContent).join('');
  }

  set textContent(value) {
    this.replaceChildren(...(value === '' ? [] : [String(value)]));
  }

  // Descendant elements in document order.
  * descendants() {
    for (const child of this.childNodes) {
      if (child.nodeType !== 1)
        continue;
      yield child;
      yield* child.descendants();
    }
  }

  matches(selector, scope = this) {
    const steps = parseSelector(selector);
    return matchSteps(this, steps, steps.length - 1, scope);
  }

  querySelectorAll(selector) {
    const steps = parseSelector(selector);
    const found = [];
    for (const el of this.descendants()) {
      if (matchSteps(el, steps, steps.length - 1, this))
        found.push(el);
    }
    return found;
  }

  querySelector(selector) {
    return this.querySelectorAll(selector)[0] || null;
  }

  closest(selector) {
    for (let el = this; el; el = el.parentNode) {
      if (el.matches(selector))
        return el;
    }
    return null;
  }

  addEventListener(type, listener) {
    if (!this.listeners.has(type))
      this.listeners.set(type, []);
    this.listeners.get(type).push(listener);
  }

  // Dispatches an event here and lets it bubble to the document.
  fire(type, props = {}) {
    const event = {
      type,
      target: this,
      detail: 1,
      defaultPrevented: false,
      stopped: false,
      stopPropagation() {
        this.stopped = true;
      },
      preventDefault() {
        this.defaultPrevented = true;
      },
      ...props,
    };
    const path = [];
    for (let el = this; el; el = el.parentNode)
      path.push(el);
    path.push(this.ownerDocument);
    for (const target of path) {
      if (event.stopped)
        break;
      for (const listener of target.listeners.get(type) || [])
        listener(event);
    }
    return event;
  }

  // Like a user's click: a checkbox flips before its handlers run.
  click(props = {}) {
    if (this.tagName === 'input' && this.getAttribute('type') === 'checkbox') {
      this.checked = !this.checked;
      this.indeterminate = false;
    }
    return this.fire('click', props);
  }

  focus() {
    this.ownerDocument.activeElement = this;
  }

  blur() {
    if (this.ownerDocument.activeElement === this) {
      this.ownerDocument.activeElement = this.ownerDocument.body;
    }
  }

  scrollIntoView() {}
}

// ----------------------------------------------------------------- Selectors

function parseSelector(selector) {
  const steps = [];
  let i = 0;
  const fail = () => {
    throw new Error(
        `fake DOM: unsupported selector ${JSON.stringify(selector)} at ${i}`);
  };
  const ident = () => {
    const start = i;
    while (i < selector.length && /[\w-]/.test(selector[i]))
      i++;
    if (i === start)
      fail();
    return selector.slice(start, i);
  };
  const compound = () => {
    const c = {
      tag: null,
      id: null,
      classes: [],
      attrs: [],
      nots: [],
      scope: false,
      empty: true
    };
    for (;; c.empty = false) {
      const ch = selector[i];
      if (ch === '#') {
        i++;
        c.id = ident();
      } else if (ch === '.') {
        i++;
        c.classes.push(ident());
      } else if (ch === '[') {
        i++;
        const name = ident();
        if (selector.slice(i, i + 2) !== '="')
          fail();
        i += 2;
        let value = '';
        while (selector[i] !== '"') {
          if (i >= selector.length)
            fail();
          if (selector[i] === '\\')
            i++;
          value += selector[i++];
        }
        if (selector[++i] !== ']')
          fail();
        i++;
        c.attrs.push([name, value]);
      } else if (selector.startsWith(':scope', i)) {
        i += ':scope'.length;
        c.scope = true;
      } else if (selector.startsWith(':not(', i)) {
        i += ':not('.length;
        c.nots.push(compound());
        if (selector[i] !== ')')
          fail();
        i++;
      } else if (ch !== undefined && /[a-zA-Z]/.test(ch)) {
        if (!c.empty)
          fail();
        c.tag = ident();
      } else {
        break;
      }
    }
    if (c.empty)
      fail();
    return c;
  };
  let combinator = null;
  while (i < selector.length) {
    if (selector[i] === ' ') {
      i++;
      combinator = combinator || ' ';
    } else if (selector[i] === '>') {
      i++;
      combinator = '>';
    } else {
      const c = compound();
      steps.push({
        combinator: steps.length ? combinator || fail() : null,
        compound: c
      });
      combinator = null;
    }
  }
  if (!steps.length || combinator)
    fail();
  return steps;
}

function matchCompound(el, c, scope) {
  if (c.tag && el.tagName !== c.tag)
    return false;
  if (c.id && el.getAttribute('id') !== c.id)
    return false;
  if (c.scope && el !== scope)
    return false;
  if (!c.classes.every(name => el.classList.contains(name)))
    return false;
  if (!c.attrs.every(([name, value]) => el.getAttribute(name) === value))
    return false;
  return !c.nots.some(n => matchCompound(el, n, scope));
}

// Does `el` match steps[0..i], reading the selector right to left?
function matchSteps(el, steps, i, scope) {
  if (!matchCompound(el, steps[i].compound, scope))
    return false;
  if (i === 0)
    return true;
  if (steps[i].combinator === '>') {
    return Boolean(el.parentNode) &&
        matchSteps(el.parentNode, steps, i - 1, scope);
  }
  for (let a = el.parentNode; a; a = a.parentNode) {
    if (matchSteps(a, steps, i - 1, scope))
      return true;
  }
  return false;
}

// -------------------------------------------------------------------- Markup

const decodeEntities = s => s.replace(/&lt;/g, '<')
                                .replace(/&gt;/g, '>')
                                .replace(/&quot;/g, '"')
                                .replace(/&amp;/g, '&');

// Builds the template's static <body> markup under `root`. Handles the
// plain, well-formed HTML the template uses: double-quoted attributes, no
// `<` or `>` inside attribute values.
function parseHtml(html, root) {
  const doc = root.ownerDocument;
  const token =
      /<!--[\s\S]*?-->|<(\/?)([a-zA-Z][\w-]*)((?:\s+[\w-]+(?:="[^"]*")?)*)\s*\/?>|([^<]+)/g;
  const stack = [root];
  for (const m of html.matchAll(token)) {
    const [, closing, tag, attrs, text] = m;
    if (text !== undefined) {
      if (text.trim())
        stack[stack.length - 1].append(decodeEntities(text));
    } else if (closing) {
      if (stack.pop().tagName !== tag.toLowerCase()) {
        throw new Error(`fake DOM: unbalanced </${tag}> in template markup`);
      }
    } else if (tag) {
      const el = doc.createElement(tag);
      for (const a of attrs.matchAll(/([\w-]+)(?:="([^"]*)")?/g)) {
        el.setAttribute(a[1], decodeEntities(a[2] || ''));
      }
      stack[stack.length - 1].append(el);
      if (!VOID_TAGS.has(el.tagName))
        stack.push(el);
    }
  }
  if (stack.length !== 1)
    throw new Error('fake DOM: unclosed element in template markup');
}

// ------------------------------------------------------------------- Globals

// Installs `document` and the other browser globals the viewer reads, with
// the template's <body> markup in place and `hash` as the URL fragment.
// Returns `page`, the helpers test scenarios use to act like a user.
function installFakeDom(bodyHtml, hash) {
  const doc = {listeners: new Map()};
  doc.createElement = tag => new FakeElement(tag, doc);
  doc.createTextNode = data => new FakeText(data);
  doc.documentElement = doc.createElement('html');
  doc.body = doc.createElement('body');
  doc.documentElement.append(doc.body);
  doc.activeElement = doc.body;
  doc.querySelector = selector => doc.documentElement.querySelector(selector);
  doc.querySelectorAll = selector =>
      doc.documentElement.querySelectorAll(selector);
  doc.addEventListener = (type, listener) => {
    if (!doc.listeners.has(type))
      doc.listeners.set(type, []);
    doc.listeners.get(type).push(listener);
  };
  parseHtml(bodyHtml, doc.body);

  // Timers run only when a test says so, never on the wall clock.
  const timers = new Map();
  let nextTimer = 1;
  const stored = new Map();
  Object.assign(globalThis, {
    document: doc,
    location: {hash},
    localStorage: {
      getItem: key => (stored.has(key) ? stored.get(key) : null),
      setItem: (key, value) => stored.set(key, String(value)),
    },
    matchMedia: () => ({matches: false, addEventListener() {}}),
    CSS: {escape: s => String(s).replace(/[^\w-]/g, '\\$&')},
    setTimeout: fn => {
      timers.set(nextTimer, fn);
      return nextTimer++;
    },
    clearTimeout: id => timers.delete(id),
  });

  const $ = selector => doc.querySelector(selector);
  const $$ = selector => doc.querySelectorAll(selector);
  // The stylesheet hides a row when an ancestor `.node` is filtered out
  // (`.node.hidden-by-filter`) or sits in the `.children` of a `.node` that
  // isn't `.open`.
  const shown = el => {
    for (let n = el; n; n = n.parentNode) {
      if (n.classList.contains('hidden-by-filter'))
        return false;
      if (n.classList.contains('children') &&
          !n.parentNode.classList.contains('open')) {
        return false;
      }
    }
    return true;
  };
  return {
    $,
    $$,
    row: path => $(`#tree .row[data-path="${CSS.escape(path)}"]`),
    // Paths of every tree row that exists, shown or not.
    builtRows: () => $$('#tree .row').map(r => r.dataset.path),
    visibleRows: () => $$('#tree .row').filter(shown).map(r => r.dataset.path),
    // Paths whose checkbox is in the given state.
    boxes: state => $$('#tree .row')
                        .filter(r => r.querySelector('.pick')[state])
                        .map(r => r.dataset.path),
    runTimers: () => {
      for (const [id, fn] of [...timers]) {
        timers.delete(id);
        fn();
      }
    },
    // Types into the filter box and lets its debounce timer fire.
    filter(text) {
      $('#filter').value = text;
      $('#filter').fire('input');
      this.runTimers();
    },
    press: key => doc.body.fire('keydown', {key}),
    activeTab: () => $('.tabs button.active').dataset.tab,
    text: selector => $(selector).textContent,
  };
}
