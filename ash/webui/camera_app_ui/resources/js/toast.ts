// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import * as animate from './animation.js';
import * as dom from './dom.js';
import {I18nString} from './i18n_string.js';
import * as loadTimeData from './models/load_time_data.js';

/**
 * Updates the toast message.
 * @param message Message to be updated.
 * @param spoken Whether the toast is spoken only.
 */
function update(message: string, spoken: boolean) {
  // TTS speaks changes of on-screen aria-live elements. Force content changes
  // and clear content once inactive to avoid stale content being read out.
  const element = dom.get('#toast', HTMLElement);
  element.textContent = '';  // Force reiterate the same message for a11y.
  element.textContent = message;

  element.classList.toggle('spoken', spoken);
  if (!spoken) {
    element.setAttribute('aria-hidden', 'false');
    animate.play(element).finally(() => {
      element.setAttribute('aria-hidden', 'true');
    });
  }
}

/**
 * Shows a toast with given message which doesn't need i18n.
 * @param message Message to be shown.
 */
export function showDebugMessage(message: string): void {
  update(message, false);
}

/**
 * Shows a toast message.
 * @param label The label of the message to show.
 * @param substitutions The substitutions needed for the given label.
 */
export function show(label: I18nString, ...substitutions: string[]): void {
  update(loadTimeData.getI18nMessage(label, ...substitutions), false);
}

/**
 * Speaks a toast message.
 * @param label The label of the message to show.
 * @param substitutions The substitutions needed for the given label.
 */
export function speak(label: I18nString, ...substitutions: string[]): void {
  update(loadTimeData.getI18nMessage(label, ...substitutions), true);
}
