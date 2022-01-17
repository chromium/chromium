// Copyright 2021 The Chromium Authors. All rights reserved.
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

  get EXTERNAL_API() {
    return ['setHWDataUsage'];
  }

  /**
   * Called to restore the data usage checkbox.
   * @param {boolean} checked Is the checkbox checked?
   */
  setHWDataUsage(checked) {
    this.dataUsageChecked = checked;
  }

  ready() {
    super.ready();
    this.initializeLoginScreen('HWDataCollectionScreen', {
      resetAllowed: true,
    });
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
    if (this.dataUsageChecked) {
      this.userActed('select-hw-data-usage');
    } else {
      this.userActed('unselect-hw-data-usage');
    }
  }
}

customElements.define(HWDataCollectionScreen.is, HWDataCollectionScreen);
