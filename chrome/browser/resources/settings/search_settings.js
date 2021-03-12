// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import {assert} from 'chrome://resources/js/assert.m.js';
import {createEmptySearchBubble, findAndRemoveHighlights, highlight, removeHighlights, stripDiacritics} from 'chrome://resources/js/search_highlight_utils.m.js';
import {findAncestor} from 'chrome://resources/js/util.m.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
import {DomIf} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {PromiseResolver} from 'chrome://resources/js/promise_resolver.m.js';
// clang-format on

  /**
   * A data structure used by callers to combine the results of multiple search
   * requests.
   *
   * @typedef {{
   *   canceled: Boolean,
   *   didFindMatches: Boolean,
   *   wasClearSearch: Boolean,
   * }}
   */
  export let SearchResult;

  /**
   * A CSS attribute indicating that a node should be ignored during searching.
   * @type {string}
   */
  const SKIP_SEARCH_CSS_ATTRIBUTE = 'no-search';

  /**
   * List of elements types that should not be searched at all.
   * The only DOM-MODULE node is in <body> which is not searched, therefore
   * DOM-MODULE is not needed in this set.
   * @type {!Set<string>}
   */
  const IGNORED_ELEMENTS = new Set([
    'CONTENT',
    'CR-ACTION-MENU',
    'CR-DIALOG',
    'CR-ICON-BUTTON',
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
   * @param {!SearchRequest} request
   * @param {!Node} root The root of the sub-tree to be searched
   * @return {boolean} Whether or not matches were found.
   * @private
   */
  function findAndHighlightMatches_(request, root) {
    let foundMatches = false;
    const highlights = [];

    function doSearch(node) {
      // NOTE: For subpage wrappers <template route-path="..."> when |no-search|
      // participates in a data binding:
      //
      //  - Always use noSearch Polymer property, for example
      //    no-search="[[foo]]"
      //  - *Don't* use a no-search CSS attribute like no-search$="[[foo]]"
      //
      // The latter throws an error during the automatic Polymer 2 conversion to
      // <dom-if><template...></dom-if> syntax.
      if (node.nodeName === 'DOM-IF' && node.hasAttribute('route-path') &&
          !node.if && !node['noSearch'] &&
          !node.hasAttribute(SKIP_SEARCH_CSS_ATTRIBUTE)) {
        request.queue_.addRenderTask(new RenderTask(request, node));
        return;
      }

      if (IGNORED_ELEMENTS.has(node.nodeName)) {
        return;
      }

      if (node instanceof HTMLElement) {
        const element = /** @type {HTMLElement} */ (node);
        if (element.hasAttribute(SKIP_SEARCH_CSS_ATTRIBUTE) ||
            element.hasAttribute('hidden') ||
            element.style.display === 'none') {
          return;
        }
      }

      if (node.nodeType === Node.TEXT_NODE) {
        const textContent = node.nodeValue;
        if (textContent.trim().length === 0) {
          return;
        }

        const strippedText =
            stripDiacritics(textContent);
        const ranges = [];
        for (let match; match = request.regExp.exec(strippedText);) {
          ranges.push({start: match.index, length: match[0].length});
        }

        if (ranges.length > 0) {
          foundMatches = true;
          revealParentSection_(
              node, /*numResults=*/ ranges.length, request.bubbles);

          if (node.parentNode.nodeName === 'OPTION') {
            const select = node.parentNode.parentNode;
            assert(select.nodeName === 'SELECT');

            // TODO(crbug.com/355446): support showing bubbles inside subpages.
            // Currently, they're incorrectly positioned and there's no great
            // signal at which to know when to reposition them (because every
            // page asynchronously loads/renders things differently).
            const isSubpage = n => n.nodeName === 'SETTINGS-SUBPAGE';
            if (findAncestor(select, isSubpage, true)) {
              return;
            }

            showBubble_(
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

      const shadowRoot = node.shadowRoot;
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
   * @param {!Node} node
   * @param {number} numResults
   * @param {!Map<!Node, number>} bubbles A map of bubbles created so far.
   * @private
   */
  function revealParentSection_(node, numResults, bubbles) {
    let associatedControl = null;

    // Find corresponding SETTINGS-SECTION parent and make it visible.
    let parent = node;
    while (parent.nodeName !== 'SETTINGS-SECTION') {
      parent = parent.nodeType === Node.DOCUMENT_FRAGMENT_NODE ?
          parent.host :
          parent.parentNode;
      if (!parent) {
        // |node| wasn't inside a SETTINGS-SECTION.
        return;
      }
      if (parent.nodeName === 'SETTINGS-SUBPAGE') {
        // TODO(dpapad): Cast to SettingsSubpageElement here.
        associatedControl = assert(
            parent.associatedControl,
            'An associated control was expected for SETTINGS-SUBPAGE ' +
                parent.pageTitle + ', but was not found.');
      }
    }
    parent.hiddenBySearch = false;

    // Need to add the search bubble after the parent SETTINGS-SECTION has
    // become visible, otherwise |offsetWidth| returns zero.
    if (associatedControl) {
      showBubble_(
          associatedControl, numResults, bubbles,
          /* horizontallyCenter= */ false);
    }
  }

  /**
   * @param {!Node} control
   * @param {number} numResults
   * @param {!Map<!Node, number>} bubbles
   * @param {boolean} horizontallyCenter
   */
  function showBubble_(control, numResults, bubbles, horizontallyCenter) {
    const bubble = createEmptySearchBubble(
        control, horizontallyCenter);
    const numHits = numResults + (bubbles.get(bubble) || 0);
    bubbles.set(bubble, numHits);
    const msgName =
        numHits === 1 ? 'searchResultBubbleText' : 'searchResultsBubbleText';
    bubble.firstChild.textContent = loadTimeData.getStringF(msgName, numHits);
  }

  /** @abstract */
  class Task {
    /**
     * @param {!SearchRequest} request
     * @param {!Node} node
     */
    constructor(request, node) {
      /** @protected {!SearchRequest} */
      this.request = request;

      /** @protected {!Node} */
      this.node = node;
    }

    /**
     * @abstract
     * @return {!Promise}
     */
    exec() {}
  }

  class RenderTask extends Task {
    /**
     * A task that takes a <template is="dom-if">...</template> node
     * corresponding to a setting subpage and renders it. A
     * SearchAndHighlightTask is posted for the newly rendered subtree, once
     * rendering is done.
     *
     * @param {!SearchRequest} request
     * @param {!Node} node
     */
    constructor(request, node) {
      super(request, node);
    }

    /** @override */
    exec() {
      const routePath = this.node.getAttribute('route-path');

      const content =
          /**
            @type {!{_contentForTemplate:
                function(!HTMLTemplateElement):!HTMLElement}}
          */
          (DomIf)
              ._contentForTemplate(
                  /** @type {!HTMLTemplateElement} */ (
                      this.node.firstElementChild));
      const subpageTemplate = content.querySelector('settings-subpage');
      subpageTemplate.setAttribute('route-path', routePath);
      assert(!this.node.if);
      this.node.if = true;

      return new Promise((resolve, reject) => {
        const parent = this.node.parentNode;
        parent.async(() => {
          const renderedNode =
              parent.querySelector('[route-path="' + routePath + '"]');
          // Register a SearchAndHighlightTask for the part of the DOM that was
          // just rendered.
          this.request.queue_.addSearchAndHighlightTask(
              new SearchAndHighlightTask(this.request, assert(renderedNode)));
          resolve();
        });
      });
    }
  }

  class SearchAndHighlightTask extends Task {
    /**
     * @param {!SearchRequest} request
     * @param {!Node} node
     */
    constructor(request, node) {
      super(request, node);
    }

    /** @override */
    exec() {
      const foundMatches = findAndHighlightMatches_(this.request, this.node);
      this.request.updateMatches(foundMatches);
      return Promise.resolve();
    }
  }

  class TopLevelSearchTask extends Task {
    /**
     * @param {!SearchRequest} request
     * @param {!Node} page
     */
    constructor(request, page) {
      super(request, page);
    }

    /** @override */
    exec() {
      const shouldSearch = this.request.regExp !== null;
      this.setSectionsVisibility_(!shouldSearch);
      if (shouldSearch) {
        const foundMatches = findAndHighlightMatches_(this.request, this.node);
        this.request.updateMatches(foundMatches);
      }

      return Promise.resolve();
    }

    /**
     * @param {boolean} visible
     * @private
     */
    setSectionsVisibility_(visible) {
      const sections = this.node.querySelectorAll('settings-section');

      for (let i = 0; i < sections.length; i++) {
        sections[i].hiddenBySearch = !visible;
      }
    }
  }

  class TaskQueue {
    /** @param {!SearchRequest} request */
    constructor(request) {
      /** @private {!SearchRequest} */
      this.request_ = request;

      /**
       * @private {{
       *   high: !Array<!Task>,
       *   middle: !Array<!Task>,
       *   low: !Array<!Task>
       * }}
       */
      this.queues_;
      this.reset();

      /** @private {?Function} */
      this.onEmptyCallback_ = null;

      /**
       * Whether a task is currently running.
       * @private {boolean}
       */
      this.running_ = false;
    }

    /** Drops all tasks. */
    reset() {
      this.queues_ = {high: [], middle: [], low: []};
    }

    /** @param {!TopLevelSearchTask} task */
    addTopLevelSearchTask(task) {
      this.queues_.high.push(task);
      this.consumePending_();
    }

    /** @param {!SearchAndHighlightTask} task */
    addSearchAndHighlightTask(task) {
      this.queues_.middle.push(task);
      this.consumePending_();
    }

    /** @param {!RenderTask} task */
    addRenderTask(task) {
      this.queues_.low.push(task);
      this.consumePending_();
    }

    /**
     * Registers a callback to be called every time the queue becomes empty.
     * @param {function():void} onEmptyCallback
     */
    onEmpty(onEmptyCallback) {
      this.onEmptyCallback_ = onEmptyCallback;
    }

    /**
     * @return {!Task|undefined}
     * @private
     */
    popNextTask_() {
      return this.queues_.high.shift() || this.queues_.middle.shift() ||
          this.queues_.low.shift();
    }

    /** @private */
    consumePending_() {
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
      window.requestIdleCallback(() => {
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
    /**
     * @param {string} rawQuery
     * @param {!Element} root
     */
    constructor(rawQuery, root) {
      /** @private {string} */
      this.rawQuery_ = rawQuery;

      /** @private {!Element} */
      this.root_ = root;

      /** @type {?RegExp} */
      this.regExp = this.generateRegExp_();

      /**
       * Whether this request was canceled before completing.
       * @type {boolean}
       */
      this.canceled = false;

      /** @private {boolean} */
      this.foundMatches_ = false;

      /** @type {!PromiseResolver} */
      this.resolver = new PromiseResolver();

      /** @private {!TaskQueue} */
      this.queue_ = new TaskQueue(this);
      this.queue_.onEmpty(() => {
        this.resolver.resolve(this);
      });

      /** @private {!Set<!MutationObserver>} */
      this.textObservers_ = new Set();

      /** @private {!Array<!Node>} */
      this.highlights_ = [];

      /** @type {!Map<!Node, number>} */
      this.bubbles = new Map;
    }

    /** @param {!Array<!Node>} highlights The highlight wrappers to add */
    addHighlights(highlights) {
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
      this.bubbles.forEach((count, bubble) => bubble.remove());
      this.highlights_ = [];
      this.bubbles.clear();
    }

    /** @param {!Node} textNode */
    addTextObserver(textNode) {
      const originalParentNode = /** @type {!Node} */ (textNode.parentNode);
      const observer = new MutationObserver(mutations => {
        const oldValue = mutations[0].oldValue.trim();
        const newValue = textNode.nodeValue.trim();
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
      this.queue_.addTopLevelSearchTask(
          new TopLevelSearchTask(this, this.root_));
    }

    /**
     * @return {?RegExp}
     * @private
     */
    generateRegExp_() {
      let regExp = null;
      // Generate search text by escaping any characters that would be
      // problematic for regular expressions.
      const strippedQuery =
          stripDiacritics(this.rawQuery_.trim());
      const sanitizedQuery = strippedQuery.replace(SANITIZE_REGEX, '\\$&');
      if (sanitizedQuery.length > 0) {
        regExp = new RegExp(`(${sanitizedQuery})`, 'ig');
      }
      return regExp;
    }

    /**
     * @param {string} rawQuery
     * @return {boolean} Whether this SearchRequest refers to an identical
     *     query.
     */
    isSame(rawQuery) {
      return this.rawQuery_ === rawQuery;
    }

    /**
     * Updates the result for this search request.
     * @param {boolean} found
     */
    updateMatches(found) {
      this.foundMatches_ = this.foundMatches_ || found;
    }

    /** @return {boolean} Whether any matches were found. */
    didFindMatches() {
      return this.foundMatches_;
    }
  }

  /** @type {!RegExp} */
  const SANITIZE_REGEX = /[-[\]{}()*+?.,\\^$|#\s]/g;

  /** @interface */
  class SearchManager {
    /**
     * @param {string} text The text to search for.
     * @param {!Element} page
     * @return {!Promise<!SearchRequest>} A signal indicating that
     *     searching finished.
     */
    search(text, page) {}
  }

  /** @implements {SearchManager} */
  class SearchManagerImpl {
    constructor() {
      /** @private {!Set<!SearchRequest>} */
      this.activeRequests_ = new Set();

      /** @private {!Set<!SearchRequest>} */
      this.completedRequests_ = new Set();

      /** @private {?string} */
      this.lastSearchedText_ = null;
    }

    /** @override */
    search(text, page) {
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

  /** @type {?SearchManager} */
  let instance = null;

  /** @return {!SearchManager} */
  export function getSearchManager() {
    if (instance === null) {
      instance = new SearchManagerImpl();
    }
    return instance;
  }

  /**
   * Sets the SearchManager singleton instance, useful for testing.
   * @param {!SearchManager} searchManager
   */
  export function setSearchManagerForTesting(searchManager) {
    instance = searchManager;
  }

