// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'os-settings-powerwash-dialog-esim-item' is an item showing details of an
 * installed eSIM profile shown in a list in the device reset dialog.
 */
import '../settings_shared.css.js';

import {I18nMixin} from 'chrome://resources/ash/common/cr_elements/i18n_mixin.js';
import {mojoString16ToString} from 'chrome://resources/js/mojo_type_util.js';
import {sanitizeInnerHtml} from 'chrome://resources/js/parse_html_subset.js';
import {ESimProfileProperties, ESimProfileRemote} from 'chrome://resources/mojo/chromeos/ash/services/cellular_setup/public/mojom/esim_manager.mojom-webui.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './os_powerwash_dialog_esim_item.html.js';

const OsSettingsPowerwashDialogEsimItemElementBase = I18nMixin(PolymerElement);

class OsSettingsPowerwashDialogEsimItemElement extends
    OsSettingsPowerwashDialogEsimItemElementBase {
  static get is() {
    return 'os-settings-powerwash-dialog-esim-item';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      profile: {
        type: Object,
        value: null,
        observer: 'onProfileChanged_',
      },

      profileProperties_: {
        type: Object,
        value: null,
      },
    };
  }

  profile: ESimProfileRemote|null;
  private profileProperties_: ESimProfileProperties|null;

  private onProfileChanged_(): void {
    if (!this.profile) {
      this.profileProperties_ = null;
      return;
    }
    this.profile.getProperties().then(response => {
      this.profileProperties_ = response.properties;
    });
  }

  private getItemInnerHtml_(): TrustedHTML {
    if (!this.profileProperties_) {
      return window.trustedTypes!.emptyHTML;
    }
    const profileName = this.getProfileName_(this.profileProperties_);
    const providerName = this.escapeHtml_(
        mojoString16ToString(this.profileProperties_.serviceProvider));
    if (!providerName) {
      return sanitizeInnerHtml(profileName);
    }
    return this.i18nAdvanced(
        'powerwashDialogESimListItemTitle',
        {attrs: ['id'], substitutions: [profileName, providerName]});
  }

  private getProfileName_(profileProperties: ESimProfileProperties): string {
    if (!profileProperties.nickname.data ||
        !profileProperties.nickname.data.length) {
      return this.escapeHtml_(mojoString16ToString(profileProperties.name));
    }
    return this.escapeHtml_(mojoString16ToString(profileProperties.nickname));
  }

  private escapeHtml_(string: string): string {
    return string.replace(/&/g, '&amp;')
        .replace(/</g, '&lt;')
        .replace(/>/g, '&gt;')
        .replace(/"/g, '&quot;')
        .replace(/'/g, '&#039;');
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'os-settings-powerwash-dialog-esim-item':
        OsSettingsPowerwashDialogEsimItemElement;
  }
}

customElements.define(
    OsSettingsPowerwashDialogEsimItemElement.is,
    OsSettingsPowerwashDialogEsimItemElement);
