// Copyright 2020 The Chromium Authors. All rights reserved.
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
  const message = loadTimeData.getI18nMessage(label, ...substitutions);
  const el = dom.get('.snackbar', HTMLElement);
  el.textContent = '';  // Force reiterate the same message for a11y.
  el.textContent = message;
  animate.play(el);
}
