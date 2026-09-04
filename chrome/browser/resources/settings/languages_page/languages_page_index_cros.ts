// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '../settings_page/settings_section.js';
import 'chrome://resources/cr_elements/cr_link_row/cr_link_row.js';

import {OpenWindowProxyImpl} from 'chrome://resources/js/open_window_proxy.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {loadTimeData} from '../i18n_setup.js';
import {getSearchManager} from '../search_settings.js';
import type {SettingsPlugin} from '../settings_main/settings_plugin.js';
import {getCss as getSettingsSharedCss} from '../settings_shared_lit.css.js';

import {getHtml} from './languages_page_index_cros.html.js';

export class SettingsLanguagesPageIndexCrosElement extends CrLitElement
    implements SettingsPlugin {
  static get is() {
    return 'settings-languages-page-index-cros';
  }

  static override get styles() {
    return [getSettingsSharedCss()];
  }

  override render() {
    return getHtml.bind(this)();
  }

  protected onOpenChromeOsLanguagesSettingsClick_() {
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
    'settings-languages-page-index-cros': SettingsLanguagesPageIndexCrosElement;
  }
}

customElements.define(
    SettingsLanguagesPageIndexCrosElement.is,
    SettingsLanguagesPageIndexCrosElement);
