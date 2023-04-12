// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview This component is temporarily added to test the screen saver
 * duration settings.
 */

import '../../css/common.css.js';
import 'chrome://resources/cr_elements/cr_radio_button/cr_radio_button.js';
import 'chrome://resources/cr_elements/cr_radio_group/cr_radio_group.js';
import 'chrome://resources/cr_elements/cr_shared_style.css.js';

import {ScreenSaverDuration} from '../../personalization_app.mojom-webui.js';
import {WithPersonalizationStore} from '../personalization_store.js';
import {inBetween} from '../utils.js';

import {getTemplate} from './duration_list_element.html.js';

export class DurationList extends WithPersonalizationStore {
  static get is() {
    return 'duration-list';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      /**
       * Used to refer to the enum values in HTML file.
       */
      durations_: {
        type: Object,
        value: ScreenSaverDuration,
      },

      selectedDuration_: {
        type: String,
        value: ScreenSaverDuration.kForever,
        observer: 'onSelectedDurationChanged_',
      },
    };
  }

  private durations_: ScreenSaverDuration;
  private selectedDuration_: string;

  private onSelectedDurationChanged_(value: string) {
    const num = parseInt(value, 10);
    if (isNaN(num) ||
        !inBetween(
            num, ScreenSaverDuration.MIN_VALUE,
            ScreenSaverDuration.MAX_VALUE)) {
      console.warn('Unexpected duration value received', value);
      return;
    }
    // TODO(b/274175512): call setScreenSaverDuration(num)
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'duration-list': DurationList;
  }
}

customElements.define(DurationList.is, DurationList);
