// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'settings-site-data' is the polymer element for showing the
 * settings for site data under Site Settings.
 */
import '/shared/settings/prefs/prefs.js';
import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';
import '../controls/settings_radio_group.js';
import '../privacy_page/collapse_radio_button.js';
import './site_list.js';

import {PrefsMixin} from '/shared/settings/prefs/prefs_mixin.js';
import {focusWithoutInk} from 'chrome://resources/js/focus_without_ink.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import type {SettingsRadioGroupElement} from '../controls/settings_radio_group.js';
import type {SettingsCollapseRadioButtonElement} from '../privacy_page/collapse_radio_button.js';

import {ContentSetting, ContentSettingsTypes} from './constants.js';
import {getTemplate} from './site_data.html.js';

export interface SettingsSiteDataElement {
  $: {
    defaultGroup: SettingsRadioGroupElement,
    defaultAllow: SettingsCollapseRadioButtonElement,
    defaultSessionOnly: SettingsCollapseRadioButtonElement,
    defaultBlock: SettingsCollapseRadioButtonElement,
  };
}

const SettingsSiteDataElementBase = PrefsMixin(PolymerElement);

export class SettingsSiteDataElement extends SettingsSiteDataElementBase {
  static get is() {
    return 'settings-site-data';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      prefs: {
        type: Object,
        notify: true,
      },

      /** Current search term. */
      searchTerm: {
        type: String,
        notify: true,
        value: '',
      },

      cookiesContentSettingType_: {
        type: String,
        value: ContentSettingsTypes.COOKIES,
      },

      /** Expose ContentSetting enum to HTML bindings. */
      contentSettingEnum_: {
        type: Object,
        value: ContentSetting,
      },

      exceptionListsReadOnly_: {
        type: Boolean,
        value: false,
      },

      showDefaultBlockDialog_: Boolean,
    };
  }

  static get observers() {
    return [`onGeneratedPrefsUpdated_(
        prefs.generated.cookie_default_content_setting)`];
  }

  searchTerm: string;
  private cookiesContentSettingType_: ContentSettingsTypes;
  private exceptionListsReadOnly_: boolean;
  private showDefaultBlockDialog_: boolean;

  private onGeneratedPrefsUpdated_() {
    const pref = this.getPref('generated.cookie_default_content_setting');

    // If the pref is managed this implies a content setting policy is present
    // and the exception lists should be disabled.
    this.exceptionListsReadOnly_ =
        pref.enforcement === chrome.settingsPrivate.Enforcement.ENFORCED;
  }

  private onDefaultRadioChange_() {
    const selected = this.$.defaultGroup.selected;
    if (selected === ContentSetting.BLOCK) {
      this.showDefaultBlockDialog_ = true;
    } else {
      this.$.defaultGroup.sendPrefChange();
    }
  }

  private onDefaultBlockDialogCancel_() {
    this.$.defaultGroup.resetToPrefValue();

    this.showDefaultBlockDialog_ = false;

    // Set focus back to the block button regardless of user interaction
    // with the dialog, as it was the entry point to the dialog.
    focusWithoutInk(this.$.defaultBlock);
  }

  private onDefaultBlockDialogConfirm_() {
    this.$.defaultGroup.sendPrefChange();

    this.showDefaultBlockDialog_ = false;

    // Set focus back to the block button regardless of user interaction
    // with the dialog, as it was the entry point to the dialog.
    focusWithoutInk(this.$.defaultBlock);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-site-data': SettingsSiteDataElement;
  }
}

customElements.define(SettingsSiteDataElement.is, SettingsSiteDataElement);
