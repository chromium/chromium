// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview The personalization-main component displays the main content of
 * the personalization hub.
 */

import './ambient/ambient_preview_large_element.js';

import {getShouldShowTimeOfDayBanner} from './ambient/ambient_controller.js';
import {isRgbKeyboardSupported} from './load_time_booleans.js';
import {getTemplate} from './personalization_main_element.html.js';
import {WithPersonalizationStore} from './personalization_store.js';

export class PersonalizationMainElement extends WithPersonalizationStore {
  static get is() {
    return 'personalization-main';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      path: String,
      isRgbKeyboardSupported_: {
        type: Boolean,
        value() {
          return isRgbKeyboardSupported();
        },
      },
      shouldShowTimeOfDayBanner_: Boolean,
    };
  }

  private shouldShowTimeOfDayBanner_: boolean;

  override connectedCallback() {
    super.connectedCallback();
    this.watch<PersonalizationMainElement['shouldShowTimeOfDayBanner_']>(
        'shouldShowTimeOfDayBanner_',
        state => state.ambient.shouldShowTimeOfDayBanner);
    this.updateFromStore();

    getShouldShowTimeOfDayBanner(this.getStore());
  }
}

customElements.define(
    PersonalizationMainElement.is, PersonalizationMainElement);
