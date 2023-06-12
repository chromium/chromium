// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_shared_vars.css.js';
import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_components/localized_link/localized_link.js';
import 'chrome://resources/cr_elements/cr_checkbox/cr_checkbox.js';
import './strings.m.js';

import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {afterNextRender, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './app.html.js';
import {WaffleChoice} from './browser_proxy';
import {WaffleBrowserProxy} from './browser_proxy.js';

export class WaffleAppElement extends PolymerElement {
  static get is() {
    return 'waffle-app';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      /**
       * We pass the choice list as JSON because it doesn't change
       * dynamically, so it would be better to have it available as loadtime
       * data.
       */
      choiceList_: {
        type: Array,
        value() {
          return JSON.parse(loadTimeData.getString('choiceList'));
        },
      },
    };
  }

  private choiceList_: WaffleChoice[];

  override connectedCallback() {
    super.connectedCallback();

    afterNextRender(this, () => {
      WaffleBrowserProxy.getInstance().handler.displayDialog();
    });
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'waffle-app': WaffleAppElement;
  }
}

customElements.define(WaffleAppElement.is, WaffleAppElement);
