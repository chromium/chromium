// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview The personalization-main component displays the main content of
 * the personalization hub.
 */

import '../css/cros_button_style.css.js';

import {loadTimeData} from 'chrome://resources/ash/common/load_time_data.m.js';

import {getTemplate} from './personalization_main_element.html.js';
import {isAmbientModeAllowed, Paths, PersonalizationRouter} from './personalization_router_element.js';
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
      clickable_: {
        type: Boolean,
        value: true,
      },
      isAmbientModeManaged_: {
        type: Boolean,
        value: loadTimeData.getBoolean('isAmbientModeManaged'),
      },
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

  private onClickAmbientSubpageLink_() {
    PersonalizationRouter.instance().goToRoute(Paths.AMBIENT);
  }
}

customElements.define(PersonalizationMain.is, PersonalizationMain);
