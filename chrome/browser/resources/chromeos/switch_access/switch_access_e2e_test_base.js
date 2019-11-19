// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

GEN_INCLUDE(['../chromevox/testing/callback_helper.js']);

/**
 * Base class for browser tests for Switch Access.
 * @constructor
 */
function SwitchAccessE2ETest() {
  this.callbackHelper_ = new CallbackHelper(this);
}

SwitchAccessE2ETest.prototype = {
  __proto__: testing.Test.prototype,

  /**
   * @override
   * No UI in the background context.
   */
  runAccessibilityChecks: false,

  /** @override */
  isAsync: true,

  /** @override */
  browsePreload: null,

  /** @override */
  testGenCppIncludes: function() {
    GEN(`
#include "ash/accessibility/accessibility_delegate.h"
#include "ash/shell.h"
#include "base/bind.h"
#include "base/callback.h"
#include "chrome/browser/chromeos/accessibility/accessibility_manager.h"
#include "chrome/common/extensions/extension_constants.h"
#include "ui/accessibility/accessibility_switches.h"
#include "ash/keyboard/ui/keyboard_util.h"
    `);
  },

  /** @override */
  testGenPreamble: function() {
    GEN(`
  //keyboard::SetRequestedKeyboardState(keyboard::KEYBOARD_STATE_ENABLED);
  //ash::Shell::Get()->CreateKeyboard();
  base::Closure load_cb =
      base::Bind(&chromeos::AccessibilityManager::SetSwitchAccessEnabled,
          base::Unretained(chromeos::AccessibilityManager::Get()),
          true);
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      ::switches::kEnableExperimentalAccessibilitySwitchAccess);
  chromeos::AccessibilityManager::Get()->SetSwitchAccessEnabled(true);
  WaitForExtension(extension_misc::kSwitchAccessExtensionId, load_cb);
    `);
  },

  /**
   * Creates a callback that optionally calls {@code opt_callback} when
   * called.  If this method is called one or more times, then
   * {@code testDone()} will be called when all callbacks have been called.
   * @param {Function=} opt_callback Wrapped callback that will have its this
   *        reference bound to the test fixture.
   * @return {Function}
   */
  newCallback: function(opt_callback) {
    return this.callbackHelper_.wrap(opt_callback);
  },

  /**
   * From chromevox_next_e2e_test_base.js
   * Gets the desktop from the automation API and Launches a new tab with
   * the given document, and runs |callback| with the desktop when a load
   * complete fires on the created tab.
   * Arranges to call |testDone()| after |callback| returns.
   * NOTE: Callbacks created inside |callback| must be wrapped with
   * |this.newCallback| if passed to asynchonous calls.  Otherwise, the test
   * will be finished prematurely.
   * @param {string} url Url to load and wait for.
   * @param {function(chrome.automation.AutomationNode)} callback Called with
   *     the desktop node once the document is ready.
   */
  runWithLoadedTree: function(url, callback) {
    const prefix = url.substring(0, 4);
    if (prefix !== 'http' && prefix !== 'data') {
      url = 'data:text/html;charset=utf-8,' + url;
    }

    callback = this.newCallback(callback);
    chrome.automation.getDesktop(function(desktopRootNode) {
      var createParams = {active: true, url: url};
      chrome.tabs.create(createParams, function(unused_tab) {
        chrome.automation.getTree(function(returnedRootNode) {
          rootNode = returnedRootNode;
          if (rootNode.docLoaded) {
            callback && callback(desktopRootNode);
            callback = null;
            return;
          }
          rootNode.addEventListener('loadComplete', function(evt) {
            // URL encoding of newlines in the target root URL were causing
            // tests to fail, so we decode %0A to \n.
            const targetUrl = evt.target.root.url.replace(/%0A/g, '\n');
            if (targetUrl != url) {
              return;
            }
            callback && callback(desktopRootNode);
            callback = null;
          });
        });
      });
    }.bind(this));
  },
};
