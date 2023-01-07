// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import * as animate from './animation.js';
import * as dom from './dom.js';
import {I18nString} from './i18n_string.js';
import * as loadTimeData from './models/load_time_data.js';

/**
 * Shows a snackbar message.
 *
 * @param label The label of the message to show.
 * @param substitutions The substitutions for the label.
 */
export function show(label: I18nString, ...substitutions: string[]): void {
  // TTS speaks changes of on-screen aria-live elements. Force content changes
  // and clear content once inactive to avoid stale content being read out.
  const message = loadTimeData.getI18nMessage(label, ...substitutions);
  const element = dom.get('.snackbar', HTMLElement);
  function changeElement() {
    element.textContent = '';  // Force reiterate the same message for a11y.
    element.textContent = message;
  }
  animate.play(element, changeElement);
}
