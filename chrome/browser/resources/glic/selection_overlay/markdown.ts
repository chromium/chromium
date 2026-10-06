// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Holds a link's place in the text while bold and italic are added. It's a
// private-use character, so it has no meaning of its own.
const LINK_MARKER = '\uE000';

// Renders the basic Markdown in explanations: `- ` and `1. ` lists,
// `**bold**`, `*italic*` and `[links](url)`. Other Markdown shows as is.
//
// - 1), 2) etc are not supported.
// - Sublists are not supported.
// - Numbered lists are only supported from 0 to 9.
// - _ and __ are not supported.
// - #, ## etc are not supported.
// - ` / ``` are not supported.
export function renderMarkdown(markdown: string): DocumentFragment {
  // Lists come before italic so `* item` isn't italic.
  const root = renderLines(markdown);
  // Links come before bold and italic. Each link waits as one LINK_MARKER
  // character in the text, so bold and italic can go around it, like
  // `**[a](url)**`, but a `*` inside it can't pair with one outside, like in
  // `*[a*](url)`. CommonMark works the same way.
  const links = takeLinks(root);
  addBoldAndItalic(root);
  // A link's own text gets bold and italic too, like `[**a**](url)`.
  for (const link of links) {
    addBoldAndItalic(link);
  }
  putLinksBack(root, links);
  return root;
}

// Makes each line a paragraph, or a list item if it starts with `- `, `* `,
// or one digit like `1. `.
function renderLines(markdown: string): DocumentFragment {
  const root = document.createDocumentFragment();
  for (const line of markdown.split('\n')) {
    const text = line.trim();
    if (text === '') {
      continue;
    }
    if (text.startsWith('- ') || text.startsWith('* ')) {
      // Process "- ..." and "* ..."
      appendListItem(root, 'ul', text.slice(2));
    } else if (isNumbered(text)) {
      // Process "0. ..." to "9. ...". For example, "3. a" is item "a" with
      // number 3.
      appendListItem(root, 'ol', text.slice(3), text.charAt(0));
    } else {
      // Process regular text.
      root.append(createElement('p', text));
    }
  }
  return root;
}

// Adds an <li> with `text` to the list on the previous non-blank line, or to a
// new list.
//
//   - a       <ul><li>a</li>
//   - b           <li>b</li></ul>
//
//   - a       <ul><li>a</li></ul>
//   b         <p>b</p>
//   - c       <ul><li>c</li></ul>
//
//   - a       <ul><li>a</li></ul>
//   1. b      <ol start="1"><li>b</li></ol>
//
// `start` is the number of the first item of a new <ol>. For example:
//
//   1. a      <ol start="1"><li>a</li></ol>
//   - note    <ul><li>note</li></ul>
//   2. b      <ol start="2"><li>b</li></ol>
//
// "- note" ends the first <ol>, so "2. b" starts a new <ol start="2">, which
// still shows "2." instead of "1.".
function appendListItem(
    root: DocumentFragment, tag: 'ul'|'ol', text: string, start?: string) {
  // The last element can be a <p>, <ul> or <ol>, or null if nothing was added.
  let list = root.lastElementChild;
  // If nothing was added, or the previous line was a <p>, create a new list;
  // else, join the list.
  if (list?.localName !== tag) {
    list = root.appendChild(document.createElement(tag));
    if (start) {
      list.setAttribute('start', start);
    }
  }
  list.append(createElement('li', text));
}

// Whether `text` starts with one digit and ". ". For example, "1. a" and
// "9. b" are numbered, but "10. c", "2046. It is a big year." and "1.5 kg"
// aren't.
function isNumbered(text: string): boolean {
  const first = text.charAt(0);
  return first >= '0' && first <= '9' && text.slice(1, 3) === '. ';
}

// Takes each `[text](url)` out of the text as a link, and leaves LINK in its
// place. Returns the links in order.
function takeLinks(root: Node): HTMLElement[] {
  const links: HTMLElement[] = [];
  for (const node of textNodes(root)) {
    // Drop any LINK_MARKER already in the text (very unlikely). After splitting
    // on `[`, each part but the first is `text](url)...` if it's a link.
    const [first, ...parts] = node.data.replaceAll(LINK_MARKER, '').split('[');
    let text = first!;
    for (const part of parts) {
      const textEnd = part.indexOf('](');
      const urlEnd = part.indexOf(')', textEnd);
      if (textEnd === -1 || urlEnd === -1) {
        // Not a link, like `[1]`.
        text += '[' + part;
        continue;
      }
      const link = createElement('a', part.slice(0, textEnd));
      link.setAttribute('href', '#');
      links.push(link);
      text += LINK_MARKER + part.slice(urlEnd + 1);
    }
    node.data = text;
  }
  return links;
}

// Adds bold and italic to the text in `root`.
function addBoldAndItalic(root: Node) {
  // `**text**`. In `***a***`, the closing `**` is the last two `*`s, so the
  // bold text is `*a*`, which then becomes italic.
  const bold = /\*\*(.+?)\*\*(?!\*)/;
  // `*text*`, where neither `*` touches another `*`. So a lone `**`, which
  // `bold` left as text, isn't an empty pair.
  const italic = /(?<!\*)\*(?!\*)(.+?)(?<!\*)\*(?!\*)/;
  // Bold comes before italic so italic can go inside bold, like `**a *b* c**`.
  wrapPairs(root, bold, 'strong');
  wrapPairs(root, italic, 'em');
}

// Puts the text of each `pair` match in a `tag` element. `pair` has one group:
// the text between the markers. A marker without a partner, like in "2 * 3",
// stays as text.
function wrapPairs(root: Node, pair: RegExp, tag: string) {
  for (const node of textNodes(root)) {
    const nodes: Array<Node|string> = [];
    // split() puts each match's group between the text around it, so the parts
    // switch between outside and inside a pair. For example, "a *b* c" splits
    // into "a " (outside), "b" (inside) and " c" (outside).
    let inside = false;
    for (const part of node.data.split(pair)) {
      if (inside) {
        nodes.push(createElement(tag, part));
      } else {
        nodes.push(part);
      }
      inside = !inside;
    }
    node.replaceWith(...nodes);
  }
}

function putLinksBack(root: Node, links: HTMLElement[]) {
  let next = 0;
  for (const node of textNodes(root)) {
    const [first, ...parts] = node.data.split(LINK_MARKER);
    const nodes: Array<Node|string> = [first!];
    for (const part of parts) {
      nodes.push(links[next++]!, part);
    }
    node.replaceWith(...nodes);
  }
}

function textNodes(root: Node): Text[] {
  const walker = document.createTreeWalker(root, NodeFilter.SHOW_TEXT);
  const nodes: Text[] = [];
  while (walker.nextNode()) {
    nodes.push(walker.currentNode as Text);
  }
  return nodes;
}

function createElement(tag: string, text: string): HTMLElement {
  const element = document.createElement(tag);
  element.textContent = text;
  return element;
}
