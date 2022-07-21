// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'os-settings-powerwash-dialog-esim-item' is an item showing details of an
 * installed eSIM profile shown in a list in the device reset dialog.
 */
import '../../settings_shared.css.js';

import {I18nBehavior, I18nBehaviorInterface} from 'chrome://resources/js/i18n_behavior.m.js';
import {html, mixinBehaviors, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

/**
 * @constructor
 * @extends {PolymerElement}
 * @implements {I18nBehaviorInterface}
 */
const OsSettingsPowerwashDialogEsimItemElementBase =
    mixinBehaviors([I18nBehavior], PolymerElement);

/** @polymer */
class OsSettingsPowerwashDialogEsimItemElement extends
    OsSettingsPowerwashDialogEsimItemElementBase {
  static get is() {
    return 'os-settings-powerwash-dialog-esim-item';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      /** @type {?ash.cellularSetup.mojom.ESimProfileRemote} */
      profile: {
        type: Object,
        value: null,
        observer: 'onProfileChanged_',
      },

      /**
       * @type {?ash.cellularSetup.mojom.ESimProfileProperties}
       * @private
       */
      profileProperties_: {
        type: Object,
        value: null,
      },
    };
  }

  /** @private */
  onProfileChanged_() {
    if (!this.profile) {
      this.profileProperties_ = null;
      return;
    }
    this.profile.getProperties().then(response => {
      this.profileProperties_ = response.properties;
    });
  }

  /**
   * @return {string}
   * @private
   */
  getItemInnerHtml_() {
    if (!this.profileProperties_) {
      return '';
    }
    const profileName = this.getProfileName_(this.profileProperties_);
    const providerName = this.escapeHtml_(
        String.fromCharCode(...this.profileProperties_.serviceProvider.data));
    if (!providerName) {
      return profileName;
    }
    return this.i18nAdvanced(
        'powerwashDialogESimListItemTitle',
        {attrs: ['id'], substitutions: [profileName, providerName]});
  }

  /**
   * @param {ash.cellularSetup.mojom.ESimProfileProperties} profileProperties
   * @return {string}
   * @private
   */
  getProfileName_(profileProperties) {
    if (!profileProperties.nickname.data ||
        !profileProperties.nickname.data.length) {
      return this.escapeHtml_(
          String.fromCharCode(...profileProperties.name.data));
    }
    return this.escapeHtml_(
        String.fromCharCode(...profileProperties.nickname.data));
  }

  /**
   * @param {string} string
   * @return {string}
   * @private
   */
  escapeHtml_(string) {
    return string.replace(/&/g, '&amp;')
        .replace(/</g, '&lt;')
        .replace(/>/g, '&gt;')
        .replace(/"/g, '&quot;')
        .replace(/'/g, '&#039;');
  }
}

customElements.define(
    OsSettingsPowerwashDialogEsimItemElement.is,
    OsSettingsPowerwashDialogEsimItemElement);
