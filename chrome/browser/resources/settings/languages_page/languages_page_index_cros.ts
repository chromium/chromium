// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '../settings_page/settings_section.js';
import '../settings_shared.css.js';
import 'chrome://resources/cr_elements/cr_link_row/cr_link_row.js';

import {OpenWindowProxyImpl} from 'chrome://resources/js/open_window_proxy.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {loadTimeData} from '../i18n_setup.js';
import {getSearchManager} from '../search_settings.js';
import type {SettingsPlugin} from '../settings_main/settings_plugin.js';

import {getTemplate} from './languages_page_index_cros.html.js';


export class SettingsLanguagesPageIndexElement extends PolymerElement implements
    SettingsPlugin {
  static get is() {
    return 'settings-languages-page-index-cros';
  }

  static get template() {
    return getTemplate();
  }

  private onOpenChromeOsLanguagesSettingsClick_() {
    OpenWindowProxyImpl.getInstance().openUrl(
        loadTimeData.getString('osSettingsLanguagesPageUrl'));
  }

  // SettingsPlugin implementation
  async searchContents(query: string) {
    const searchRequest = await getSearchManager().search(query, this);
    return searchRequest.getSearchResult();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-languages-page-index-cros': SettingsLanguagesPageIndexElement;
  }
}

customElements.define(
    SettingsLanguagesPageIndexElement.is, SettingsLanguagesPageIndexElement);
