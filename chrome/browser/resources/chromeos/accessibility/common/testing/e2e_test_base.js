// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

GEN_INCLUDE([
  'accessibility_test_base.js',
  'assert_additions.js',
  'callback_helper.js',
  'common.js',
  'doc_utils.js',
]);

/**
 * Base test fixture for end to end tests (tests that need a full extension
 * renderer) for accessibility component extensions. These tests run inside of
 * the extension's background page context.
 */
E2ETestBase = class extends AccessibilityTestBase {
  constructor() {
    super();
    this.callbackHelper_ = new CallbackHelper(this);
    this.desktop_;
  }

  /** @override */
  testGenCppIncludes() {
    GEN(`
  #include "ash/accessibility/accessibility_delegate.h"
  #include "ash/shell.h"
  #include "base/functional/bind.h"
  #include "base/functional/callback.h"
  #include "base/containers/flat_set.h"
  #include "chrome/browser/ash/accessibility/accessibility_manager.h"
  #include "chrome/browser/ash/system_web_apps/system_web_app_manager.h"
  #include "chrome/browser/speech/extension_api/tts_engine_extension_api.h"
  #include "chrome/browser/ui/browser.h"
  #include "chrome/common/extensions/extension_constants.h"
  #include "content/public/test/browser_test.h"
  #include "content/public/test/browser_test_utils.h"
  #include "extensions/browser/test_extension_console_observer.h"
  #include "extensions/browser/process_manager.h"
  #include "ui/accessibility/accessibility_switches.h"
      `);
  }

  /** @override */
  testGenPreamble() {
    GEN(`
    TtsExtensionEngine::GetInstance()->DisableBuiltInTTSEngineForTesting();
      `);
  }

  /** @override */
  testGenPostamble() {
    GEN(`
    if (fail_on_console_error) {
      EXPECT_EQ(0u, console_observer.GetErrorCount())
          << "Found console.warn or console.error with message: "
          << console_observer.GetMessageAt(0);
    }
    `);
  }

  testGenPreambleCommon(
      extensionIdName, failOnConsoleError = true, allowedMessages = []) {
    const messages = allowedMessages.reduce(
        (accumulator, message) => accumulator + `u"${message}",`, '');
    GEN(`
    WaitForExtension(extension_misc::${extensionIdName}, std::move(load_cb));

    bool fail_on_console_error = ${failOnConsoleError};
    // Convert |allowedMessages| into a C++ set.
    base::flat_set<std::u16string> allowed_messages({${messages}});
    extensions::TestExtensionConsoleObserver
        console_observer(GetProfile(), extension_misc::${extensionIdName},
        fail_on_console_error);
    // In most cases, A11y extensions should not log warnings or errors.
    // However, informational messages may be logged in some cases and should
    // be specified in |allowed_messages|. All other messages should cause test
    // failures.
    if (fail_on_console_error) {
      console_observer.SetAllowedErrorMessages(allowed_messages);
    }
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
   * Waits for the given |eventType| to be fired on |node|.
   * @param {!chrome.automation.AutomationNode} node
   * @param {!chrome.automation.EventType} eventType
   * @param {boolean=} capture
   */
  async waitForEvent(node, eventType, capture) {
    return new Promise(this.newCallback(resolve => {
      const callback = this.newCallback(() => {
        node.removeEventListener(eventType, callback, capture);
        resolve();
      });
      node.addEventListener(eventType, callback, capture);
    }));
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
   * Gets the desktop from the automation API and runs |callback|.
   * Arranges to call |testDone()| after |callback| returns.
   * NOTE: Callbacks created inside |callback| must be wrapped with
   * |this.newCallback| if passed to asynchronous calls.  Otherwise, the test
   * will be finished prematurely.
   * @param {function(chrome.automation.AutomationNode)} callback
   *     Called with the desktop node once it's retrieved.
   */
  runWithLoadedDesktop(callback) {
    chrome.automation.getDesktop(this.newCallback(callback));
  }

  /**
   * Gets the desktop from the automation API and Launches a new tab with
   * the given document, and returns the root web area when a load complete
   * fires.
   * @param {string|function(): string} doc An HTML snippet, optionally wrapped
   *     inside of a function.
   * @param {{url: (string=)}}
   *     opt_params
   *           url Optional url to wait for. Defaults to undefined.
   * @return {chrome.automation.AutomationNode} the root web area node, only
   *     returned once the document is ready.
   */
  async runWithLoadedTree(doc, opt_params = {}) {
    return new Promise(this.newCallback(async resolve => {
      // Make sure the test doesn't finish until this function has resolved.
      let callback = this.newCallback(resolve);
      this.desktop_ = await AsyncUtil.getDesktop();
      const url = opt_params.url || DocUtils.createUrlForDoc(doc);

      // The below block handles opening a url via the chrome.tabs api.

      // Listener for both load complete and focus events that eventually
      // triggers the test.
      const listener = async event => {
        // Navigation has occurred, but we need to ensure the url we want has
        // loaded.
        if (event.target.root.url !== url || !event.target.root.docLoaded) {
          return;  // exit listener.
        }

        // Finally, when we get here, we've successfully navigated to
        // the |url|.
        this.desktop_.removeEventListener('focus', listener, true);
        this.desktop_.removeEventListener('loadComplete', listener, true);

        if (callback) {
          callback(event.target.root);
        }
        // Avoid calling |callback| twice (which would cause the test to fail).
        callback = null;
      };  // end listener.

      // Setup the listener above for focus and load complete listening.
      this.desktop_.addEventListener('focus', listener, true);
      this.desktop_.addEventListener('loadComplete', listener, true);

      const createParams = {active: true, url};
      chrome.tabs.create(createParams);
    }));
  }

  /**
   * Gets the desktop from the automation API and launches new tabs with
   * the given url, returns when load complete has fired on each document.
   * @param {Array<string>} urls HTML snippets to open in the URLs.
   * @return {!Promise}
   */
  async runWithLoadedTabs(urls) {
    console.assert(urls.length !== 0);
    for (const url of urls) {
      await this.runWithLoadedTree(url);
    }
  }

  /**
   * Opens the options page for the running extension and calls |callback| with
   * the options page root once ready.
   * @param {function(chrome.automation.AutomationNode)} callback
   * @param {!RegExp} matchUrlRegExp The url pattern of the options page if
   *     different than the supplied default pattern below.
   */
  runWithLoadedOptionsPage(callback, matchUrlRegExp = /options.html/) {
    callback = this.newCallback(callback);
    chrome.automation.getDesktop(desktop => {
      const listener = event => {
        if (!matchUrlRegExp.test(event.target.docUrl) ||
            !event.target.docLoaded) {
          return;
        }

        desktop.removeEventListener(
            chrome.automation.EventType.LOAD_COMPLETE, listener);

        callback(event.target);
      };
      desktop.addEventListener(
          chrome.automation.EventType.LOAD_COMPLETE, listener);
      chrome.runtime.openOptionsPage();
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

  /**
   * Async function to get a preference value from Settings.
   * @param {string} name
   * @return {!Promise<*>}
   */
  async getPref(name) {
    return new Promise(resolve => {
      chrome.settingsPrivate.getPref(name, ret => {
        resolve(ret);
      });
    });
  }

  /**
   * Async function to set a preference value in Settings.
   * @param {string} name
   * @return {!Promise}
   */
  async setPref(name, value) {
    return new Promise(resolve => {
      chrome.settingsPrivate.setPref(name, value, undefined, async () => {
        // Wait for changes to fully propagate.
        const result = await (this.getPref(name));
        assertEquals(result.key, name);
        if (typeof (value) === 'object') {
          assertObjectEquals(value, result.value);
        } else {
          assertEquals(value, result.value);
        }
        resolve();
      });
    });
  }
};

/** @override */
E2ETestBase.prototype.isAsync = true;

/** @override */
E2ETestBase.prototype.paramCommandLineSwitch =
    `::switches::kEnableExperimentalAccessibilityManifestV3`;
