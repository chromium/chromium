// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from '//resources/ash/common/assert.js';
import { loadTimeData } from './i18n_setup.js';
import { $ } from '//resources/ash/common/util.js';

/**
 * @fileoverview JS helpers used on login.
 */

/**
 * Listens to key events on input element.
 * @param {Element} element DOM element
 * @param {Object} callback
 */
export function addSubmitListener(element, callback) {
  element.addEventListener('keydown', (function(callback, e) {
                                        if (e.keyCode != 13) {
                                          return;
                                        }
                                        callback();
                                      }).bind(undefined, callback));
}

/**
 * Add screens from the given list into the main screen container.
 * Screens are added with the following properties:
 *    - Classes: "step hidden" + any extra classes the screen may have
 *    - Attribute: "hidden"
 *
 * If a screen should be added only under some certain conditions, it must have
 * the `condition` property associated with a boolean flag. If the condition
 * yields true it will be added, otherwise it is skipped.
 * @param {!Array<{tag: string, id: string}>} screenList
 */
export function addScreensToMainContainer(screenList) {
  const screenContainer = ($('inner-container'));
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
    assert(
        !!$(screen.id).shadowRoot, `Error! No shadow root in <${screen.tag}>`);
  }
}
