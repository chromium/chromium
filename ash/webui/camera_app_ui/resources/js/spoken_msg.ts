// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import * as dom from './dom.js';
import {I18nString} from './i18n_string.js';
import * as loadTimeData from './models/load_time_data.js';

/**
 * Speaks a message.
 *
 * @param label The label of the message to speak.
 * @param substitutions The substitutions needed for the given label.
 */
export function speak(label: I18nString, ...substitutions: string[]): void {
  speakMessage(loadTimeData.getI18nMessage(label, ...substitutions));
}

/**
 * Speaks a message.
 */
export function speakMessage(message: string): void {
  // TTS speaks changes of on-screen aria-live elements. Force content changes
  // and clear content once inactive to avoid stale content being read out.
  const element = dom.get('#spoken_msg', HTMLElement);
  element.textContent = '';  // Force reiterate the same message for a11y.
  element.textContent = message;
}
