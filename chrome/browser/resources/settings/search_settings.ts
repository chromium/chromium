// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import {assert} from 'chrome://resources/js/assert.js';

import {loadTimeData} from './i18n_setup.js';

import {PromiseResolver} from 'chrome://resources/js/promise_resolver.js';
import {createEmptySearchBubble, findAndRemoveHighlights, highlight, removeHighlights, stripDiacritics} from 'chrome://resources/js/search_highlight_utils.js';
import {DomIf, microTask} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import type {SettingsSectionElement} from './settings_page/settings_section.js';
import type {SettingsSubpageElement} from './settings_page/settings_subpage.js';

// clang-format on

/**
 * A data structure used by callers to combine the results of multiple search
 * requests.
 */
export interface SearchResult {
  canceled: boolean;
  didFindMatches: boolean;
  wasClearSearch: boolean;
}

/**
 * A CSS attribute indicating that a node should be ignored during searching.
 */
const SKIP_SEARCH_CSS_ATTRIBUTE: string = 'no-search';

/**
 * List of elements types that should not be searched at all.
 * The only DOM-MODULE node is in <body> which is not searched, therefore
 * DOM-MODULE is not needed in this set.
 */
const IGNORED_ELEMENTS: Set<string> = new Set([
  'CONTENT',
  'CR-ACTION-MENU',
  'CR-DIALOG',
  'CR-ICON',
  'CR-ICON-BUTTON',
  'CR-RIPPLE',
  'CR-SLIDER',
  'DIALOG',
  'IMG',
  'IRON-ICON',
  'IRON-LIST',
  'PAPER-RIPPLE',
  'PAPER-SPINNER-LITE',
  'SLOT',
  'STYLE',
  'TEMPLATE',
]);

/**
 * Traverses the entire DOM (including Shadow DOM), finds text nodes that
 * match the given regular expression and applies the highlight UI. It also
 * ensures that <settings-section> instances become visible if any matches
 * occurred under their subtree.
 *
 * @param root The root of the sub-tree to be searched
 * @return Whether or not matches were found.
 */
function findAndHighlightMatches(request: SearchRequest, root: Node): boolean {
  let foundMatches = false;
  const highlights: HTMLElement[] = [];

  // Returns true if the node or any of its ancestors are a settings-subpage.
  function isInSubpage(node: (Node|null)): boolean {
    while (node !== null) {
      if (node.nodeName === 'SETTINGS-SUBPAGE') {
        return true;
      }
      node = node instanceof ShadowRoot ? node.host : node.parentNode;
    }
    return false;
  }

  function doSearch(node: Node) {
    // NOTE: For subpage wrappers <template route-path="..."> when |no-search|
    // participates in a data binding:
    //
    //  - Always use noSearch Polymer property, for example
    //    no-search="[[foo]]"
    //  - *Don't* use a no-search CSS attribute like no-search$="[[foo]]"
    //
    // The latter throws an error during the automatic Polymer 2 conversion to
    // <dom-if><template...></dom-if> syntax.
    if (node.nodeName === 'DOM-IF' &&
        (node as DomIf).hasAttribute('route-path') && !(node as DomIf).if &&
        !(node as any)['noSearch'] &&
        !(node as DomIf).hasAttribute(SKIP_SEARCH_CSS_ATTRIBUTE)) {
      request.queue.addRenderTask(new RenderTask(request, node));
      return;
    }

    if (IGNORED_ELEMENTS.has(node.nodeName)) {
      return;
    }

    if (node instanceof HTMLElement) {
      const element = node as HTMLElement;
      if (element.hasAttribute(SKIP_SEARCH_CSS_ATTRIBUTE) ||
          element.hasAttribute('hidden') || element.style.display === 'none') {
        return;
      }
    }

    if (node.nodeType === Node.TEXT_NODE) {
      const textContent = node.nodeValue;
      if (textContent!.trim().length === 0) {
        return;
      }

      const strippedText = stripDiacritics(textContent!);
      const ranges = [];
      for (let match; match = request.regExp!.exec(strippedText);) {
        ranges.push({start: match.index, length: match[0].length});
      }

      if (ranges.length > 0) {
        foundMatches = true;
        revealParentSection(
            node, /*numResults=*/ ranges.length, request.bubbles);

        if (node.parentNode!.nodeName === 'OPTION') {
          const select = node.parentNode!.parentNode!;
          assert(select.nodeName === 'SELECT');

          // TODO(crbug.com/41096577): support showing bubbles inside subpages.
          // Currently, they're incorrectly positioned and there's no great
          // signal at which to know when to reposition them (because every
          // page asynchronously loads/renders things differently).
          if (isInSubpage(select)) {
            return;
          }

          showBubble(
              select, /*numResults=*/ ranges.length, request.bubbles,
              /*horizontallyCenter=*/ true);
        } else {
          request.addTextObserver(node);
          highlights.push(highlight(node, ranges));
        }
      }

      // Returning early since TEXT_NODE nodes never have children.
      return;
    }

    let child = node.firstChild;
    while (child !== null) {
      // Getting a reference to the |nextSibling| before calling doSearch()
      // because |child| could be removed from the DOM within doSearch().
      const nextSibling = child.nextSibling;
      doSearch(child);
      child = nextSibling;
    }

    const shadowRoot = (node as HTMLElement).shadowRoot;
    if (shadowRoot) {
      doSearch(shadowRoot);
    }
  }

  doSearch(root);
  request.addHighlights(highlights);
  return foundMatches;
}

/**
 * Finds and makes visible the <settings-section> parent of |node|.
 * @param bubbles A map of bubbles created so far.
 */
function revealParentSection(
    node: Node, numResults: number, bubbles: Map<Node, number>) {
  let associatedControl: HTMLElement|null = null;

  // Find corresponding SETTINGS-SECTION parent and make it visible.
  let parent = node;
  while (parent.nodeName !== 'SETTINGS-SECTION') {
    parent = parent.nodeType === Node.DOCUMENT_FRAGMENT_NODE ?
        (parent as ShadowRoot).host :
        parent.parentNode as Node;
    if (!parent) {
      // |node| wasn't inside a SETTINGS-SECTION.
      return;
    }
    if (parent.nodeName === 'SETTINGS-SUBPAGE') {
      const subpage = parent as SettingsSubpageElement;
      assert(
          subpage.associatedControl,
          'An associated control was expected for SETTINGS-SUBPAGE ' +
              subpage.pageTitle + ', but was not found.');
      associatedControl = subpage.associatedControl;
    }
  }
  (parent as SettingsSectionElement).hiddenBySearch = false;

  // Need to add the search bubble after the parent SETTINGS-SECTION has
  // become visible, otherwise |offsetWidth| returns zero.
  if (associatedControl) {
    showBubble(
        associatedControl, numResults, bubbles,
        /* horizontallyCenter= */ false);
  }
}

function showBubble(
    control: Node, numResults: number, bubbles: Map<Node, number>,
    horizontallyCenter: boolean) {
  const bubble = createEmptySearchBubble(control, horizontallyCenter);
  const numHits = numResults + (bubbles.get(bubble) || 0);
  bubbles.set(bubble, numHits);
  const msgName =
      numHits === 1 ? 'searchResultBubbleText' : 'searchResultsBubbleText';
  bubble.firstChild!.textContent = loadTimeData.getStringF(msgName, numHits);
}

abstract class Task {
  protected request: SearchRequest;
  protected node: Node;

  constructor(request: SearchRequest, node: Node) {
    this.request = request;
    this.node = node;
  }

  abstract exec(): Promise<void>;
}

/**
 * A task that takes a <template is="dom-if">...</template> node
 * corresponding to a setting subpage and renders it. A
 * SearchAndHighlightTask is posted for the newly rendered subtree, once
 * rendering is done.
 */
class RenderTask extends Task {
  declare protected node: DomIf;

  exec() {
    const routePath = this.node.getAttribute('route-path')!;

    const content = DomIf._contentForTemplate(
        this.node.firstElementChild as HTMLTemplateElement);
    const subpageTemplate = content!.querySelector('settings-subpage')!;
    subpageTemplate.setAttribute('route-path', routePath);
    assert(!this.node.if);
    this.node.if = true;

    return new Promise<void>(resolve => {
      const parent = this.node.parentNode!;
      microTask.run(() => {
        const renderedNode =
            parent.querySelector('[route-path="' + routePath + '"]');
        assert(renderedNode);
        // Register a SearchAndHighlightTask for the part of the DOM that was
        // just rendered.
        this.request.queue.addSearchAndHighlightTask(
            new SearchAndHighlightTask(this.request, renderedNode));
        resolve();
      });
    });
  }
}

class SearchAndHighlightTask extends Task {
  exec() {
    const foundMatches = findAndHighlightMatches(this.request, this.node);
    this.request.updateMatches(foundMatches);
    return Promise.resolve();
  }
}

class TopLevelSearchTask extends Task {
  declare protected node: HTMLElement;

  exec() {
    const shouldSearch = this.request.regExp !== null;
    this.setSectionsVisibility_(!shouldSearch);
    if (shouldSearch) {
      const foundMatches = findAndHighlightMatches(this.request, this.node);
      this.request.updateMatches(foundMatches);
    }

    return Promise.resolve();
  }

  private setSectionsVisibility_(visible: boolean) {
    const sections = this.node.querySelectorAll('settings-section');

    for (let i = 0; i < sections.length; i++) {
      sections[i].hiddenBySearch = !visible;
    }
  }
}

interface Queues {
  high: Task[];
  middle: Task[];
  low: Task[];
}

class TaskQueue {
  private request_: SearchRequest;
  private queues_: Queues;
  private running_: boolean;
  private onEmptyCallback_: (() => void)|null = null;

  constructor(request: SearchRequest) {
    this.request_ = request;

    this.reset();

    /**
     * Whether a task is currently running.
     */
    this.running_ = false;
  }

  /** Drops all tasks. */
  reset() {
    this.queues_ = {high: [], middle: [], low: []};
  }

  addTopLevelSearchTask(task: TopLevelSearchTask) {
    this.queues_.high.push(task);
    this.consumePending_();
  }

  addSearchAndHighlightTask(task: SearchAndHighlightTask) {
    this.queues_.middle.push(task);
    this.consumePending_();
  }

  addRenderTask(task: RenderTask) {
    this.queues_.low.push(task);
    this.consumePending_();
  }

  /**
   * Registers a callback to be called every time the queue becomes empty.
   */
  onEmpty(onEmptyCallback: () => void) {
    this.onEmptyCallback_ = onEmptyCallback;
  }

  private popNextTask_(): Task|undefined {
    return this.queues_.high.shift() || this.queues_.middle.shift() ||
        this.queues_.low.shift();
  }

  private consumePending_() {
    if (this.running_) {
      return;
    }

    const task = this.popNextTask_();
    if (!task) {
      this.running_ = false;
      if (this.onEmptyCallback_) {
        this.onEmptyCallback_();
      }
      return;
    }

    this.running_ = true;
    requestIdleCallback(() => {
      if (!this.request_.canceled) {
        task.exec().then(() => {
          this.running_ = false;
          this.consumePending_();
        });
      }
      // Nothing to do otherwise. Since the request corresponding to this
      // queue was canceled, the queue is disposed along with the request.
    });
  }
}

export class SearchRequest {
  private rawQuery_: string;
  private root_: Element;
  regExp: RegExp|null;
  canceled: boolean;
  private foundMatches_: boolean;
  resolver: PromiseResolver<SearchRequest>;
  queue: TaskQueue;
  private textObservers_: Set<MutationObserver>;
  private highlights_: HTMLElement[];
  bubbles: Map<HTMLElement, number>;

  constructor(rawQuery: string, root: Element) {
    this.rawQuery_ = rawQuery;
    this.root_ = root;
    this.regExp = this.generateRegExp_();

    /**
     * Whether this request was canceled before completing.
     */
    this.canceled = false;

    this.foundMatches_ = false;
    this.resolver = new PromiseResolver();

    this.queue = new TaskQueue(this);
    this.queue.onEmpty(() => {
      this.resolver.resolve(this);
    });

    this.textObservers_ = new Set();
    this.highlights_ = [];
    this.bubbles = new Map();
  }

  /** @param highlights The highlight wrappers to add */
  addHighlights(highlights: HTMLElement[]) {
    this.highlights_.push(...highlights);
  }

  removeAllTextObservers() {
    this.textObservers_.forEach(observer => {
      observer.disconnect();
    });
    this.textObservers_.clear();
  }

  removeAllHighlightsAndBubbles() {
    removeHighlights(this.highlights_);
    this.bubbles.forEach((_count, bubble) => bubble.remove());
    this.highlights_ = [];
    this.bubbles.clear();
  }

  addTextObserver(textNode: Node) {
    const originalParentNode = textNode.parentNode as Node;
    const observer = new MutationObserver(mutations => {
      const oldValue = mutations[0].oldValue!.trim();
      const newValue = textNode.nodeValue!.trim();
      if (oldValue !== newValue) {
        observer.disconnect();
        this.textObservers_.delete(observer);
        findAndRemoveHighlights(originalParentNode);
      }
    });
    observer.observe(
        textNode, {characterData: true, characterDataOldValue: true});
    this.textObservers_.add(observer);
  }

  /**
   * Fires this search request.
   */
  start() {
    this.queue.addTopLevelSearchTask(new TopLevelSearchTask(this, this.root_));
  }

  private generateRegExp_(): RegExp|null {
    let regExp = null;
    // Generate search text by escaping any characters that would be
    // problematic for regular expressions.
    const strippedQuery = stripDiacritics(this.rawQuery_.trim());
    const sanitizedQuery = strippedQuery.replace(SANITIZE_REGEX, '\\$&');
    if (sanitizedQuery.length > 0) {
      regExp = new RegExp(`(${sanitizedQuery})`, 'ig');
    }
    return regExp;
  }

  /**
   * @return Whether this SearchRequest refers to an identical query.
   */
  isSame(rawQuery: string): boolean {
    return this.rawQuery_ === rawQuery;
  }

  /**
   * Updates the result for this search request.
   */
  updateMatches(found: boolean) {
    this.foundMatches_ = this.foundMatches_ || found;
  }

  /** @return Whether any matches were found. */
  didFindMatches(): boolean {
    return this.foundMatches_;
  }
}

const SANITIZE_REGEX: RegExp = /[-[\]{}()*+?.,\\^$|#\s]/g;

export interface SearchManager {
  /**
   * @param text The text to search for.
   * @param page
   * @return A signal indicating that searching finished.
   */
  search(text: string, page: Element): Promise<SearchRequest>;
}

class SearchManagerImpl implements SearchManager {
  private activeRequests_: Set<SearchRequest> = new Set();
  private completedRequests_: Set<SearchRequest> = new Set();
  private lastSearchedText_: string|null = null;

  search(text: string, page: Element) {
    // Cancel any pending requests if a request with different text is
    // submitted.
    if (text !== this.lastSearchedText_) {
      this.activeRequests_.forEach(function(request) {
        request.removeAllTextObservers();
        request.removeAllHighlightsAndBubbles();
        request.canceled = true;
        request.resolver.resolve(request);
      });
      this.activeRequests_.clear();
      this.completedRequests_.forEach(request => {
        request.removeAllTextObservers();
        request.removeAllHighlightsAndBubbles();
      });
      this.completedRequests_.clear();
    }

    this.lastSearchedText_ = text;
    const request = new SearchRequest(text, page);
    this.activeRequests_.add(request);
    request.start();
    return request.resolver.promise.then(() => {
      this.activeRequests_.delete(request);
      this.completedRequests_.add(request);
      return request;
    });
  }
}

let instance: SearchManager|null = null;

export function getSearchManager(): SearchManager {
  if (instance === null) {
    instance = new SearchManagerImpl();
  }
  return instance;
}

/**
 * Sets the SearchManager singleton instance, useful for testing.
 */
export function setSearchManagerForTesting(searchManager: SearchManager) {
  instance = searchManager;
}
