// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'settings-personalization-page' is the settings page containing
 * personalization settings.
 */
import 'chrome://resources/cr_elements/cr_link_row/cr_link_row.js';
import '../../settings_page/settings_animated_pages.js';
import '../../settings_page/settings_subpage.js';
import '../../settings_shared.css.js';

import {I18nMixin, I18nMixinInterface} from 'chrome://resources/js/i18n_mixin.js';
import {mixinBehaviors, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {loadTimeData} from '../../i18n_setup.js';

import {PersonalizationHubBrowserProxy, PersonalizationHubBrowserProxyImpl} from './personalization_hub_browser_proxy.js';
import {getTemplate} from './personalization_page.html.js';

const SettingsPersonalizationPageElementBase =
    mixinBehaviors([], I18nMixin(PolymerElement)) as {
      new (): PolymerElement & I18nMixinInterface,
    };


class SettingsPersonalizationPageElement extends
    SettingsPersonalizationPageElementBase {
  static get is() {
    return 'settings-personalization-page';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      isPersonalizationHubEnabled_: {
        type: Boolean,
        value() {
          return loadTimeData.getBoolean('isPersonalizationHubEnabled');
        },
        readOnly: true,
      },
    };
  }

  private isPersonalizationHubEnabled_: boolean;

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

customElements.define(
    SettingsPersonalizationPageElement.is, SettingsPersonalizationPageElement);
