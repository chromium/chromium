// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Polymer element for HW data collection screen.
 */

/* #js_imports_placeholder */

/**
 * @constructor
 * @extends {PolymerElement}
 * @implements {LoginScreenBehaviorInterface}
 * @implements {OobeI18nBehaviorInterface}
 */
const HWDataCollectionScreenElementBase = Polymer.mixinBehaviors(
    [OobeDialogHostBehavior, OobeI18nBehavior, LoginScreenBehavior],
    Polymer.Element);

/**
 * @polymer
 */
class HWDataCollectionScreen extends HWDataCollectionScreenElementBase {
  static get is() {
    return 'hw-data-collection-element';
  }

  /* #html_template_placeholder */

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
