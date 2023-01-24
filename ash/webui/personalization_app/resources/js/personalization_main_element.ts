// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview The personalization-main component displays the main content of
 * the personalization hub.
 */

import './ambient/ambient_preview_large_element.js';

import {loadTimeData} from 'chrome://resources/ash/common/load_time_data.m.js';

import {getTemplate} from './personalization_main_element.html.js';
import {isAmbientModeAllowed} from './personalization_router_element.js';
import {WithPersonalizationStore} from './personalization_store.js';

export class PersonalizationMain extends WithPersonalizationStore {
  static get is() {
    return 'personalization-main';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      path: String,
    };
  }

  private isDarkLightModeEnabled_(): boolean {
    return loadTimeData.getBoolean('isDarkLightModeEnabled');
  }

  private isAmbientModeAllowed_(): boolean {
    return isAmbientModeAllowed();
  }

  private isRgbKeyboardSupported_(): boolean {
    return loadTimeData.getBoolean('isRgbKeyboardSupported');
  }
}

customElements.define(PersonalizationMain.is, PersonalizationMain);
