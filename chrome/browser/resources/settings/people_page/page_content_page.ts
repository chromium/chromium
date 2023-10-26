// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'settings-page-content-page' contains settings related to features accessing
 * page content.
 */
import 'chrome://resources/polymer/v3_0/iron-icon/iron-icon.js';
import '/shared/settings/controls/settings_toggle_button.js';
import '../settings_columned_section.css.js';
import '../settings_shared.css.js';

import {PrefsMixin} from 'chrome://resources/cr_components/settings_prefs/prefs_mixin.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './page_content_page.html.js';

const SettingsPageContentPageElementBase = PrefsMixin(PolymerElement);

export class SettingsPageContentPageElement extends
    SettingsPageContentPageElementBase {
  static get is() {
    return 'settings-page-content-page';
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
      showComposeToggle_: {
        type: Boolean,
        computed:
            `computeShowComposeToggle(prefs.page_content_collection.enabled.value)`,
      },
    };
  }

  private computeShowComposeToggle(): boolean {
    // <if expr="enable_compose">
    return loadTimeData.getBoolean('enableComposeSetting') &&
        this.prefs.page_content_collection.enabled.value;
    // </if>
    // <if expr="not enable_compose">
    return false;
    // </if>
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-page-content-page': SettingsPageContentPageElement;
  }
}

customElements.define(
    SettingsPageContentPageElement.is, SettingsPageContentPageElement);
