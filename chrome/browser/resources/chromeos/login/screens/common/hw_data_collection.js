// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Polymer element for HW data collection screen.
 */

import '//resources/cr_elements/cr_checkbox/cr_checkbox.js';
import '//resources/cr_elements/cr_shared_style.css.js';
import '//resources/js/action_link.js';

import '../../components/oobe_icons.m.js';
import '../../components/buttons/oobe_text_button.js';
import '../../components/common_styles/oobe_common_styles.m.js';
import '../../components/common_styles/oobe_dialog_host_styles.m.js';
import '../../components/dialogs/oobe_adaptive_dialog.js';


import {html, mixinBehaviors, Polymer, PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {LoginScreenBehavior, LoginScreenBehaviorInterface} from '../../components/behaviors/login_screen_behavior.m.js';
import {OobeDialogHostBehavior} from '../../components/behaviors/oobe_dialog_host_behavior.js';
import {OobeI18nBehavior, OobeI18nBehaviorInterface} from '../../components/behaviors/oobe_i18n_behavior.js';



/**
 * @constructor
 * @extends {PolymerElement}
 * @implements {LoginScreenBehaviorInterface}
 * @implements {OobeI18nBehaviorInterface}
 */
const HWDataCollectionScreenElementBase = mixinBehaviors(
    [OobeDialogHostBehavior, OobeI18nBehavior, LoginScreenBehavior],
    PolymerElement);

/**
 * @polymer
 */
class HWDataCollectionScreen extends HWDataCollectionScreenElementBase {
  static get is() {
    return 'hw-data-collection-element';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      dataUsageChecked: {
        type: Boolean,
        value: false,
      },
    };
  }

  constructor() {
    super();
  }

  onBeforeShow(data) {
    this.dataUsageChecked =
        'hwDataUsageEnabled' in data && data.hwDataUsageEnabled;
  }

  ready() {
    super.ready();
    this.initializeLoginScreen('HWDataCollectionScreen');
  }

  /**
   * @param {string} locale
   * @return {string}
   * @private
   */
  getHWDataCollectionContent_(locale) {
    return this.i18nAdvanced('HWDataCollectionContent', {tags: ['p']});
  }

  onAcceptButtonClicked_() {
    this.userActed('accept-button');
  }

  /**
   * On-change event handler for dataUsageChecked.
   * @private
   */
  onDataUsageChanged_() {
    this.userActed(['select-hw-data-usage', this.dataUsageChecked]);
  }
}

customElements.define(HWDataCollectionScreen.is, HWDataCollectionScreen);
