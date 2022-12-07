// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'settings-personalization-page' is the settings page containing
 * personalization settings.
 */
import 'chrome://resources/cr_elements/cr_link_row/cr_link_row.js';
import '../os_settings_page/os_settings_animated_pages.js';
import '../os_settings_page/os_settings_subpage.js';
import '../../settings_shared.css.js';

import {I18nMixin} from 'chrome://resources/cr_elements/i18n_mixin.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {PersonalizationHubBrowserProxy, PersonalizationHubBrowserProxyImpl} from './personalization_hub_browser_proxy.js';
import {getTemplate} from './personalization_page.html.js';

const SettingsPersonalizationPageElementBase = I18nMixin(PolymerElement);

class SettingsPersonalizationPageElement extends
    SettingsPersonalizationPageElementBase {
  static get is() {
    return 'settings-personalization-page';
  }

  static get template() {
    return getTemplate();
  }

  private personalizationHubBrowserProxy_: PersonalizationHubBrowserProxy;

  constructor() {
    super();

    this.personalizationHubBrowserProxy_ =
        PersonalizationHubBrowserProxyImpl.getInstance();
  }

  private openPersonalizationHub_() {
    this.personalizationHubBrowserProxy_.openPersonalizationHub();
  }
}


declare global {
  interface HTMLElementTagNameMap {
    'settings-personalization-page': SettingsPersonalizationPageElement;
  }
}

customElements.define(
    SettingsPersonalizationPageElement.is, SettingsPersonalizationPageElement);
