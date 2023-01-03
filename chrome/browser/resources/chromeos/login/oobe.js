// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import {assert} from 'chrome://resources/ash/common/assert.js';
import {$} from 'chrome://resources/ash/common/util.js';

import {Oobe} from './cr_ui.js';
import * as OobeDebugger from './debug/debug.js';
import {invokePolymerMethod} from './display_manager.js';
import {loadTimeData} from './i18n_setup.js';

import 'chrome://oobe/test_api/test_api.js';

import {commonScreensList, loginScreensList, oobeScreensList} from 'chrome://oobe/screens.js';

import {MultiTapDetector} from './multi_tap_detector.js';

import './components/common_styles/oobe_flex_layout_styles.css.js';
// clang-format on

/**
 * Add screens from the given list into the main screen container.
 * Screens are added with the following properties:
 *    - Classes: "step hidden" + any extra classes the screen may have
 *    - Attribute: "hidden"
 *
 * If a screen should be added only under some certain conditions, it must have
 * the `condition` property associated with a boolean flag. If the condition
 * yields true it will be added, otherwise it is skipped.
 * @param {Array<{tag: string, id: string}>}
 */
 function addScreensToMainContainer(screenList) {
  const screenContainer = $('inner-container');
  for (const screen of screenList) {
    if (screen.condition) {
      if (!loadTimeData.getBoolean(screen.condition)) {
        continue;
      }
    }

    const screenElement = document.createElement(screen.tag);
    screenElement.id = screen.id;
    screenElement.classList.add('step', 'hidden');
    screenElement.setAttribute('hidden', '');
    if (screen.extra_classes) {
      screenElement.classList.add(...screen.extra_classes);
    }
    screenContainer.appendChild(screenElement);
    assert(!!$(screen.id).shadowRoot,
           `Error! No shadow root in <${screen.tag}>`);
  }
}

// Create the global values attached to `window` that are used
// for accessing OOBE controls from the browser side.
function prepareGlobalValues(globalValue) {
  // '$(id)' is an alias for 'document.getElementById(id)'. It is defined
  // in chrome://resources/ash/common/util.js. If this function is not exposed
  // via the global object, it would not be available to tests that inject
  // JavaScript directly into the renderer.
  window.$ = $;

  // Expose MultiTapDetector class on window for tests to set static methods.
  window.MultiTapDetector = MultiTapDetector;

  // TODO(crbug.com/1229130) - Remove the necessity for these global objects.
  if (globalValue.cr == undefined) {
    globalValue.cr = {};
  }
  if (globalValue.cr.ui == undefined) {
    globalValue.cr.ui = {};
  }
  if (globalValue.cr.ui.login == undefined) {
    globalValue.cr.ui.login = {};
  }

  // Expose some values in the global object that are needed by OOBE.
  globalValue.cr.ui.Oobe = Oobe;
  globalValue.Oobe = Oobe;
}

function initializeOobe() {
  if (document.readyState === 'loading') {
    return;
  }
  document.removeEventListener('DOMContentLoaded', initializeOobe);

  // Initialize the on-screen debugger if present.
  if (OobeDebugger.DebuggerUI) {
    OobeDebugger.DebuggerUI.getInstance().register(document.body);
  }

  try {
    Oobe.initialize();
  } finally {
    // TODO(crbug.com/712078): Do not set readyForTesting in case of that
    // initialize() is failed. Currently, in some situation, initialize()
    // raises an exception unexpectedly. It means testing APIs should not
    // be called then. However, checking it here now causes bots failures
    // unfortunately. So, as a short term workaround, here set
    // readyForTesting even on failures, just to make test bots happy.
    Oobe.readyForTesting = true;
  }

  // Mark initialization complete and wake any callers that might be waiting
  // for OOBE to load.
  cr.ui.Oobe.initializationComplete = true;
  cr.ui.Oobe.initCallbacks.forEach(resolvePromise => resolvePromise());
}

/**
 * ----------- OOBE Execution Begins -----------
 */
(function () {
    // Ensure that there is a global error listener when OOBE starts.
    // This error listener is added in the main HTML document.
    assert(window.OobeErrorStore, 'OobeErrorStore not present on global object!');

    // Update localized strings at the document level.
    Oobe.updateDocumentLocalizedStrings();

    prepareGlobalValues(window);

    // Add screens to the document.
    addScreensToMainContainer(commonScreensList);
    const isOobeFlow = loadTimeData.getBoolean('isOobeFlow');
    addScreensToMainContainer(isOobeFlow ? oobeScreensList : loginScreensList);

    // The default is to have the class 'oobe-display' in <body> for the OOBE
    // flow. For the 'Add Person' flow, we remove it.
    if (!isOobeFlow) {
      document.body.classList.remove('oobe-display');
    } else {
      assert(
          document.body.classList.contains('oobe-display'),
          'The body of the document must contain oobe-display as a class for the OOBE flow!');
    }

    if (document.readyState === 'loading') {
        document.addEventListener('DOMContentLoaded', initializeOobe);
      } else {
        initializeOobe();
    }
})();
