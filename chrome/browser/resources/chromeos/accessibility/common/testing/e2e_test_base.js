// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

GEN_INCLUDE(['assert_additions.js', 'callback_helper.js', 'doc_utils.js']);

/**
 * Base test fixture for end to end tests (tests that need a full extension
 * renderer) for accessibility component extensions. These tests run inside of
 * the extension's background page context.
 */
E2ETestBase = class extends testing.Test {
  constructor() {
    super();
    this.callbackHelper_ = new CallbackHelper(this);
    this.desktop_;
  }

  /** @override */
  testGenCppIncludes() {
    GEN(`
  #include "chrome/browser/speech/extension_api/tts_engine_extension_api.h"
      `);
  }

  /** @override */
  testGenPreamble() {
    GEN(`
    TtsExtensionEngine::GetInstance()->DisableBuiltInTTSEngineForTesting();
      `);
  }

  /**
   * Listens and waits for the first event on the given node of the given type.
   * @param {!chrome.automation.AutomationNode} node
   * @param {!chrome.automation.EventType} eventType
   * @param {!function()} callback
   * @param {boolean} capture
   */
  listenOnce(node, eventType, callback, capture) {
    const innerCallback = this.newCallback(function() {
      node.removeEventListener(eventType, innerCallback, capture);
      callback.apply(this, arguments);
    });
    node.addEventListener(eventType, innerCallback, capture);
  }

  /**
   * Listens to and waits for the specified event type on the given node until
   * |predicate| is satisfied.
   * @param {!function(): boolean} predicate
   * @param {!chrome.automation.AutomationNode} node
   * @param {!chrome.automation.EventType} eventType
   * @param {!function()} callback
   * @param {boolean} capture
   */
  listenUntil(predicate, node, eventType, callback, capture = false) {
    callback = this.newCallback(callback);
    if (predicate()) {
      callback();
      return;
    }

    const listener = () => {
      if (predicate()) {
        node.removeEventListener(eventType, listener, capture);
        callback.apply(this, arguments);
      }
    };
    node.addEventListener(eventType, listener, capture);
  }

  /**
   * Creates a callback that optionally calls {@code opt_callback} when
   * called.  If this method is called one or more times, then
   * {@code testDone()} will be called when all callbacks have been called.
   * @param {Function=} opt_callback Wrapped callback that will have its this
   *        reference bound to the test fixture. Optionally, return a promise to
   * defer completion.
   * @return {Function}
   */
  newCallback(opt_callback) {
    return this.callbackHelper_.wrap(opt_callback);
  }

  /**
   * Gets the desktop from the automation API and Launches a new tab with
   * the given document, and runs |callback| when a load complete fires.
   * Arranges to call |testDone()| after |callback| returns.
   * NOTE: Callbacks created inside |opt_callback| must be wrapped with
   * |this.newCallback| if passed to asynchronous calls.  Otherwise, the test
   * will be finished prematurely.
   * @param {string|function(): string} doc An HTML snippet, optionally wrapped
   *     inside of a function.
   * @param {function(chrome.automation.AutomationNode)} callback
   *     Called once the document is ready.
   * @param {{url: (string=), returnPage: (boolean=)}}
   *     opt_params
   *           url Optional url to wait for. Defaults to undefined.
   *           returnPage True if the node for the root web area should be
   *               returned; otherwise the desktop will be returned.
   */
  runWithLoadedTree(doc, callback, opt_params = {}) {
    callback = this.newCallback(callback);
    chrome.automation.getDesktop((desktop) => {
      const url = opt_params.url || DocUtils.createUrlForDoc(doc);
      const listener = (event) => {
        if (event.target.root.url !== url || !event.target.root.docLoaded) {
          return;
        }

        desktop.removeEventListener('focus', listener, true);
        desktop.removeEventListener('loadComplete', listener, true);
        callback && callback(opt_params.returnPage ? event.target : desktop);
        callback = null;
      };
      this.desktop_ = desktop;
      desktop.addEventListener('focus', listener, true);
      desktop.addEventListener('loadComplete', listener, true);

      const createParams = {active: true, url};
      chrome.tabs.create(createParams);
    });
  }

  /**
   * Finds one specific node in the automation tree.
   * This function is expected to run within a callback passed to
   *     runWithLoadedTree().
   * @param {function(chrome.automation.AutomationNode): boolean} predicate A
   *     predicate that uniquely specifies one automation node.
   * @param {string=} nodeDescription An optional description of what node was
   *     being looked for.
   * @return {!chrome.automation.AutomationNode}
   */
  findNodeMatchingPredicate(
      predicate, nodeDescription = 'node matching the predicate') {
    assertNotNullNorUndefined(
        this.desktop_,
        'findNodeMatchingPredicate called from invalid location.');
    const treeWalker = new AutomationTreeWalker(
        this.desktop_, constants.Dir.FORWARD, {visit: predicate});
    const node = treeWalker.next().node;
    assertNotNullNorUndefined(node, 'Could not find ' + nodeDescription + '.');
    assertNullOrUndefined(
        treeWalker.next().node, 'Found more than one ' + nodeDescription + '.');
    return node;
  }
};

/** @override */
E2ETestBase.prototype.isAsync = true;
/**
 * @override
 * No UI in the background context.
 */
E2ETestBase.prototype.runAccessibilityChecks = false;
