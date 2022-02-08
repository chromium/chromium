// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview The personalization-main component displays the main content of
 * the personalization hub.
 */

import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
import {html} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {Paths, PersonalizationRouter} from './personalization_router_element.js';
import {WithPersonalizationStore} from './personalization_store.js';

export class PersonalizationMain extends WithPersonalizationStore {
  static get is() {
    return 'personalization-main';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      clickable_: {
        type: Boolean,
        value: true,
      },
    };
  }

  private isDarkLightModeEnabled_(): boolean {
    return loadTimeData.getBoolean('isDarkLightModeEnabled');
  }

  private onClickUserSubpageLink_() {
    PersonalizationRouter.instance().goToRoute(Paths.User);
  }

  private onClickAmbientSubpageLink_() {
    PersonalizationRouter.instance().goToRoute(Paths.Ambient);
  }
}

customElements.define(PersonalizationMain.is, PersonalizationMain);
