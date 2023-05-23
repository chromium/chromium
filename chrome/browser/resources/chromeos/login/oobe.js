// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.



import './components/common_styles/oobe_flex_layout_styles.css.js';
import './components/api_keys_notice.js';
// clang-format on


import {assert} from '//resources/ash/common/assert.js';
import {$} from '//resources/ash/common/util.js';
import {refreshColorCss, startColorChangeUpdater} from '//resources/cr_components/color_change_listener/colors_css_updater.js';
import {getTrustedScriptURL} from '//resources/js/static_types.js';

import {Oobe} from './cr_ui.js';
import * as OobeDebugger from './debug/debug.js';
import * as QuickStartDebugger from './debug/quick_start_debugger.js';
import * as OobeTestApi from './test_api/test_api.js';
import {loadTimeData} from './i18n_setup.js';
import {addScreensToMainContainer} from './login_ui_tools.js';
import {MultiTapDetector} from './multi_tap_detector.js';
import {TraceEvent, traceExecution} from './oobe_trace.js';
import {priorityOobeScreenList} from './priority_screens_oobe_flow.js';

// Everything has been imported at this point.
traceExecution(TraceEvent.FIRST_LINE_AFTER_IMPORTS);

// Create the global values attached to `window` that are used
// for accessing OOBE controls from the browser side.
function prepareGlobalValues() {
  // '$(id)' is an alias for 'document.getElementById(id)'. It is defined
  // in //resources/ash/common/util.js. If this function is not exposed
  // via the global object, it would not be available to tests that inject
  // JavaScript directly into the renderer.
  window.$ = $;

  // Expose MultiTapDetector class on window for tests to set static methods.
  window.MultiTapDetector = MultiTapDetector;

  // TODO(crbug.com/1229130) - Remove the necessity for these global objects.
  if (window.cr == undefined) {
    window.cr = {};
  }
  if (window.cr.ui == undefined) {
    window.cr.ui = {};
  }
  if (window.cr.ui.login == undefined) {
    window.cr.ui.login = {};
  }

  // Expose some values in the global object that are needed by OOBE.
  window.cr.ui.Oobe = Oobe;
  window.Oobe = Oobe;
}

function initializeOobe() {
  if (document.readyState === 'loading') {
    return;
  }
  document.removeEventListener('DOMContentLoaded', initializeOobe);
  traceExecution(TraceEvent.DOM_CONTENT_LOADED);

  const isOobeJellyEnabled = loadTimeData.getBoolean('isOobeJellyEnabled');
  if (isOobeJellyEnabled) {
    // Start listening for color changes in 'chrome://theme/colors.css'. Force
    // reload it once to account for any missed color change events between
    // loading oobe.html and here.
    startColorChangeUpdater();
    refreshColorCss();
  }

  // Initialize the on-screen debugger if present.
  if (OobeDebugger.DebuggerUI) {
    OobeDebugger.DebuggerUI.getInstance().register(document.body);
  }
  // Add the QuickStart debugger if present.
  if (QuickStartDebugger.addDebugger) {
    QuickStartDebugger.addDebugger();
  }

  // Add the OOBE Test API
  if (OobeTestApi.OobeApiProvider) {
    window.OobeAPI = new OobeTestApi.OobeApiProvider();
  }

  Oobe.initialize();
  Oobe.readyForTesting = true;
  traceExecution(TraceEvent.OOBE_INITIALIZED);
}

function initAfterDomLoaded() {
  document.removeEventListener('oobe-screens-loaded', initAfterDomLoaded);
  if (document.readyState === 'loading') {
    document.addEventListener('DOMContentLoaded', initializeOobe);
  } else {
    initializeOobe();
  }
}

/**
 * Adds a separate script to the page that imports most of the screens used
 * during OOBE. Once this script finishes, it fires the 'oobe-screens-loaded'
 * event, which is then handled by |initAfterDomLoaded| function. This function
 * fires the |priorityScreensLoaded| signal which is used to notify the browser
 * that critical screens (Welcome) have been added to the document.
 */
function lazyLoadOobe() {
  chrome.send('priorityScreensLoaded');
  document.addEventListener('oobe-screens-loaded', initAfterDomLoaded);
  const script = document.createElement('script');
  script.type = 'module';
  script.src = getTrustedScriptURL`./lazy_load_screens.js`;
  document.body.appendChild(script);
}

/**
 * ----------- OOBE Execution Begins -----------
 * |startOobe| is the main entry point for OOBE and it is invoked at the bottom
 * of this file. Depending on the flow (OOBE vs. LOGIN) the following steps are
 * performed:
 *
 * OOBE:
 * 1. Add priority screens (Only 'Welcome' at this time) to the HTML document.
 * 2. Jump to |lazyLoadOobe| which will add the remaining screens using a
 *    separate script and trigger the 'oobe-screens-loaded' event.
 * 3. The 'oobe-screens-loaded' event is then handled by |initAfterDomLoaded|,
 *    which calls |initializeOobe|.
 *
 * LOGIN:
 * For the 'login' flow, the steps are almost the same, with the only exception
 * that there are no priority screens to be added, so it jumps immediately to
 * step (2) and skips step (1).
 */
function startOobe() {
  // Ensure that there is a global error listener when OOBE starts.
  // This error listener is added in the main HTML document.
  assert(window.OobeErrorStore, 'OobeErrorStore not present on global object!');

  chrome.send('initializeCoreHandler');

  // Update localized strings at the document level.
  Oobe.updateDocumentLocalizedStrings();

  prepareGlobalValues();

  // Add OOBE or LOGIN screens to the document.
  const isOobeFlow = loadTimeData.getBoolean('isOobeFlow');

  // The default is to have the class 'oobe-display' in <body> for the OOBE
  // flow. For the 'Add Person' flow, we remove it.
  if (!isOobeFlow) {
    document.body.classList.remove('oobe-display');
  } else {
    assert(
        document.body.classList.contains('oobe-display'),
        'The body of the document must contain oobe-display as a class for the OOBE flow!');
  }

  // For the OOBE flow, we prioritize the loading of the Welcome screen.
  if (isOobeFlow) {
    addScreensToMainContainer(priorityOobeScreenList);
    traceExecution(TraceEvent.PRIORITY_SCREENS_ADDED);
  }

  if (document.readyState === 'loading') {
    document.addEventListener('DOMContentLoaded', lazyLoadOobe);
  } else {
    lazyLoadOobe();
  }
}

startOobe();
