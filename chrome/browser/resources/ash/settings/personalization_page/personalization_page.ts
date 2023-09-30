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
import '../os_settings_page/settings_card.js';
import '../settings_shared.css.js';

import {I18nMixin} from 'chrome://resources/cr_elements/i18n_mixin.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {isRevampWayfindingEnabled} from '../common/load_time_booleans.js';
import {Section} from '../mojom-webui/routes.mojom-webui.js';

import {PersonalizationHubBrowserProxy, PersonalizationHubBrowserProxyImpl} from './personalization_hub_browser_proxy.js';
import {getTemplate} from './personalization_page.html.js';

const SettingsPersonalizationPageElementBase = I18nMixin(PolymerElement);

export class SettingsPersonalizationPageElement extends
    SettingsPersonalizationPageElementBase {
  static get is() {
    return 'settings-personalization-page';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      section_: {
        type: Number,
        value: Section.kPersonalization,
        readOnly: true,
      },

      isRevampWayfindingEnabled_: {
        type: Boolean,
        value() {
          return isRevampWayfindingEnabled();
        },
      },
    };
  }

  private isRevampWayfindingEnabled_: boolean;
  private personalizationHubBrowserProxy_: PersonalizationHubBrowserProxy;
  private section_: Section;

  constructor() {
    super();

    this.personalizationHubBrowserProxy_ =
        PersonalizationHubBrowserProxyImpl.getInstance();
  }

  private getSublabel_(): string|null {
    return this.isRevampWayfindingEnabled_ ?
        null :
        this.i18n('personalizationHubSubtitle');
  }

  private openPersonalizationHub_(): void {
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
