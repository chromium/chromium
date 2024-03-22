// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'settings-personalization-page' is the settings page containing
 * personalization settings.
 */
import 'chrome://resources/ash/common/cr_elements/cr_link_row/cr_link_row.js';
import '../os_settings_page/os_settings_animated_pages.js';
import '../os_settings_page/os_settings_subpage.js';
import '../os_settings_page/settings_card.js';
import '../settings_shared.css.js';
import '../controls/settings_toggle_button.js';

import {PrefsMixin} from '/shared/settings/prefs/prefs_mixin.js';
import {I18nMixin} from 'chrome://resources/ash/common/cr_elements/i18n_mixin.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {DeepLinkingMixin} from '../common/deep_linking_mixin.js';
import {isRevampWayfindingEnabled, shouldShowMultitaskingInPersonalization} from '../common/load_time_booleans.js';
import {RouteObserverMixin} from '../common/route_observer_mixin.js';
import {Section} from '../mojom-webui/routes.mojom-webui.js';
import {Setting} from '../mojom-webui/setting.mojom-webui.js';
import {Route, routes} from '../router.js';

import {PersonalizationHubBrowserProxy, PersonalizationHubBrowserProxyImpl} from './personalization_hub_browser_proxy.js';
import {getTemplate} from './personalization_page.html.js';

const SettingsPersonalizationPageElementBase =
    DeepLinkingMixin(RouteObserverMixin(PrefsMixin(I18nMixin(PolymerElement))));

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

      shouldShowMultitaskingInPersonalization_: {
        type: Boolean,
        value() {
          return shouldShowMultitaskingInPersonalization();
        },
      },

      /**
       * Used by DeepLinkingMixin to focus this page's deep links.
       */
      supportedSettingIds: {
        type: Object,
        value: () => new Set<Setting>([
          Setting.kSnapWindowSuggestions,
        ]),
      },
    };
  }

  private isRevampWayfindingEnabled_: boolean;
  private readonly shouldShowMultitaskingInPersonalization_: boolean;
  private personalizationHubBrowserProxy_: PersonalizationHubBrowserProxy;
  private section_: Section;

  constructor() {
    super();

    this.personalizationHubBrowserProxy_ =
        PersonalizationHubBrowserProxyImpl.getInstance();
  }

  private getPersonalizationRowIcon_(): string {
    return this.isRevampWayfindingEnabled_ ?
        'os-settings:personalization-revamp' :
        '';
  }

  private openPersonalizationHub_(): void {
    this.personalizationHubBrowserProxy_.openPersonalizationHub();
  }

  /**
   * Overridden from DeepLinkingMixin.
   */
  override currentRouteChanged(route: Route): void {
    // Does not apply to the personalization page.
    if (route !== routes.PERSONALIZATION) {
      return;
    }

    this.attemptDeepLink();
  }
}


declare global {
  interface HTMLElementTagNameMap {
    'settings-personalization-page': SettingsPersonalizationPageElement;
  }
}

customElements.define(
    SettingsPersonalizationPageElement.is, SettingsPersonalizationPageElement);
