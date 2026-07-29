// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '../site_settings/settings_category_default_radio_group.js';
import '../site_settings/site_settings_shared.css.js';
import '../settings_page/settings_subpage.js';
import '../settings_shared.css.js';
import '../site_settings/category_setting_exceptions.js';

import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {SettingsViewMixin} from '../settings_page/settings_view_mixin.js';
import {ContentSettingsTypes} from '../site_settings/constants.js';

import {getTemplate} from './inline_cue_menu_page.html.js';

const InlineCueMenuPageElementBase = SettingsViewMixin(PolymerElement);

export class InlineCueMenuPageElement extends InlineCueMenuPageElementBase {
  static get is() {
    return 'settings-inline-cue-menu-page';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      searchTerm: String,

      // Expose ContentSettingsTypes enum to the HTML template.
      contentSettingsTypesEnum_: {
        type: Object,
        value: ContentSettingsTypes,
      },
    };
  }

  declare searchTerm: string;

  // SettingsViewMixin implementation.
  override focusBackButton() {
    this.shadowRoot!.querySelector('settings-subpage')!.focusBackButton();
  }

  protected getOpenInNewOffIcon_(): string {
    return loadTimeData.getBoolean('webuiRoundedIconsEnabled') ?
        'privacy:open-in-new-off' :
        'privacy:open-in-new-off-old';
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-inline-cue-menu-page': InlineCueMenuPageElement;
  }
}

customElements.define(InlineCueMenuPageElement.is, InlineCueMenuPageElement);
