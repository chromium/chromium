// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './mini_page.js';
import 'chrome://resources/cr_elements/cr_icons_css.m.js';
import 'chrome://resources/cr_elements/cr_toggle/cr_toggle.m.js';
import 'chrome://resources/cr_elements/cr_button/cr_button.m.js';
import 'chrome://resources/cr_elements/policy/cr_policy_indicator.m.js';

import {assert} from 'chrome://resources/js/assert.m.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
import {html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {BrowserProxy} from './browser_proxy.js';

/** Element that lets the user configure modules settings. */
class CustomizeModulesElement extends PolymerElement {
  static get is() {
    return 'ntp-customize-modules';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      /** @private */
      hide_: {
        type: Boolean,
        reflectToAttribute: true,
      },

      /** @private */
      hideManagedByPolicy_: {
        type: Boolean,
        value: () => loadTimeData.getBoolean('modulesVisibleManagedByPolicy'),
      },

      /** @private */
      selected_: {
        type: Boolean,
        reflectToAttribute: true,
        computed: 'computeSelected_(hide_, hideManagedByPolicy_)',
      },
    };
  }

  constructor() {
    super();
    /** @private {?number} */
    this.setModulesVisibleListenerId_ = null;
  }

  /** @override */
  connectedCallback() {
    super.connectedCallback();
    this.setModulesVisibleListenerId_ =
        BrowserProxy.getInstance().callbackRouter.setModulesVisible.addListener(
            visible => {
              this.hide_ = !visible;
            });
    BrowserProxy.getInstance().handler.updateModulesVisible();
  }

  /** @override */
  disconnectedCallback() {
    super.disconnectedCallback();
    BrowserProxy.getInstance().callbackRouter.removeListener(
        assert(this.setModulesVisibleListenerId_));
  }

  /** @override */
  ready() {
    // |window.CrPolicyStrings.controlledSettingPolicy| populates the tooltip
    // text of <cr-policy-indicator indicator-type="devicePolicy" /> elements.
    // Needs to be called before |super.ready()| so that the string is available
    // when <cr-policy-indicator> gets instantiated.
    window.CrPolicyStrings = {
      controlledSettingPolicy:
          loadTimeData.getString('controlledSettingPolicy'),
    };
    super.ready();
  }

  apply() {
    BrowserProxy.getInstance().handler.setModulesVisible(!this.hide_);
  }

  /**
   * @return {boolean}
   * @private
   */
  computeSelected_() {
    return this.hide_ && !this.hideManagedByPolicy_;
  }

  /**
   * @param {!CustomEvent<boolean>} e
   * @private
   */
  onHideChange_(e) {
    this.hide_ = e.detail;
  }
}

customElements.define(CustomizeModulesElement.is, CustomizeModulesElement);
