// Copyright (c) 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import {assert} from 'chrome://resources/js/assert.m.js';
import {$} from 'chrome://resources/js/util.m.js';
import {Oobe} from './cr_ui.m.js';
import * as OobeDebugger from './debug/debug.m.js';
import {invokePolymerMethod} from './display_manager.m.js';
import {loadTimeData} from './i18n_setup.js';
import 'chrome://oobe/components/test_util.m.js';
import 'chrome://oobe/test_api/test_api.m.js';
import {i18nTemplate} from 'chrome://resources/js/i18n_template_no_process.m.js';
import {commonScreensList, loginScreensList, oobeScreensList} from 'chrome://oobe/screens.js';
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

function initializeDebugger() {
  if (document.readyState === 'loading')
    return;
  document.removeEventListener('DOMContentLoaded', initializeDebugger);
  OobeDebugger.DebuggerUI.getInstance().register(document.body);
}

// Create the global values attached to `window` that are used
// for accessing OOBE controls from the browser side.
function prepareGlobalValues(globalValue) {
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

(function (root) {
    i18nTemplate.process(document, loadTimeData);
    prepareGlobalValues(window);

    // Add screens to the document.
    addScreensToMainContainer(commonScreensList);
    const isOobeFlow = loadTimeData.getBoolean('isOobeFlow');
    addScreensToMainContainer(isOobeFlow ? oobeScreensList : loginScreensList);

    Oobe.initialize();

    // Initialize the debugger if it has been defined.
    if (OobeDebugger.DebuggerUI) {
      if (document.readyState === 'loading') {
          document.addEventListener('DOMContentLoaded', initializeDebugger);
        } else {
          initializeDebugger();
      }
    }
})(window);
