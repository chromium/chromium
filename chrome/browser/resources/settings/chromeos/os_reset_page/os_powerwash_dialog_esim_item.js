// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'os-settings-powerwash-dialog-esim-item' is an item showing details of an
 * installed eSIM profile shown in a list in the device reset dialog.
 */
import '../../settings_shared.css.js';

import {I18nBehavior, I18nBehaviorInterface} from 'chrome://resources/cr_elements/i18n_behavior.js';
import {ESimProfileProperties, ESimProfileRemote} from 'chrome://resources/mojo/chromeos/ash/services/cellular_setup/public/mojom/esim_manager.mojom-webui.js';
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
      /** @type {?ESimProfileRemote} */
      profile: {
        type: Object,
        value: null,
        observer: 'onProfileChanged_',
      },

      /**
       * @type {?ESimProfileProperties}
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
   * @param {ESimProfileProperties} profileProperties
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
