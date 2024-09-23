// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from '//resources/js/assert.js';
import {loadTimeData} from '//resources/js/load_time_data.js';
import {$} from '//resources/js/util.js';

import {OobeTypes} from './components/oobe_types.js';

/**
 * Listens to key events on input element.
 */
export function addSubmitListener(
    element: HTMLElement, callback: () => void): void {
  element.addEventListener(
      'keydown', (function(callback: () => void, e: KeyboardEvent) {
                   if (e.code !== 'Enter') {
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
 */
export function addScreensToMainContainer(
    screenList: OobeTypes.ScreensList): void {
  const screenContainer = $('inner-container');
  assert(screenContainer);

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

    const addedScreen = $(screen.id);
    assert(addedScreen, `Error! <${screen.tag}> does not exist`);
    assert(addedScreen.shadowRoot, `Error! No shadow root in <${screen.tag}>`);
  }
}
