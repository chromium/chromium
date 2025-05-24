// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'search-and-assistant-settings-card' is the card element containing search
 * and assistant settings.
 */

import 'chrome://resources/ash/common/cr_elements/cr_link_row/cr_link_row.js';
import './magic_boost_review_terms_banner.js';
import '../os_settings_page/settings_card.js';
import '../settings_shared.css.js';
import './search_engine.js';
// <if expr="_google_chrome" >
import 'chrome://resources/ash/common/internal/ash_internal_icons.html.js';

// </if>

import {I18nMixin} from 'chrome://resources/ash/common/cr_elements/i18n_mixin.js';
import {assert} from 'chrome://resources/js/assert.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {DeepLinkingMixin} from '../common/deep_linking_mixin.js';
import {isAssistantAllowed, isLobsterSettingsToggleVisible, isMagicBoostFeatureEnabled, isMagicBoostNoticeBannerVisible, isQuickAnswersSupported, isScannerSettingsToggleVisible} from '../common/load_time_booleans.js';
import {RouteOriginMixin} from '../common/route_origin_mixin.js';
import type {PrefsState} from '../common/types.js';
import {Setting} from '../mojom-webui/setting.mojom-webui.js';
import type {Route} from '../router.js';
import {Router, routes} from '../router.js';

import {getTemplate} from './search_and_assistant_settings_card.html.js';

const ENTERPRISE_POLICY_DISALLOWED = 2;

const SearchAndAssistantSettingsCardElementBase =
    DeepLinkingMixin(RouteOriginMixin(I18nMixin(PolymerElement)));

export class SearchAndAssistantSettingsCardElement extends
    SearchAndAssistantSettingsCardElementBase {
  static get is() {
    return 'search-and-assistant-settings-card' as const;
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

      isQuickAnswersSupported_: {
        type: Boolean,
        value: () => {
          return isQuickAnswersSupported();
        },
      },

      isMagicBoostFeatureEnabled_: {
        type: Boolean,
        value: () => {
          return isMagicBoostFeatureEnabled();
        },
      },

      isMagicBoostNoticeBannerVisible_: {
        type: Boolean,
        value: () => {
          return isMagicBoostNoticeBannerVisible();
        },
      },

      isLobsterSettingsToggleVisible_: {
        type: Boolean,
        value: () => {
          return isLobsterSettingsToggleVisible();
        },
      },

      isScannerSettingsToggleVisible_: {
        type: Boolean,
        readOnly: true,
        value: () => {
          return isScannerSettingsToggleVisible();
        },
      },

      isLobsterAllowedByEnterprisePolicy_: {
        type: Boolean,
        computed: 'isEnterprisePolicyAllowed_(' +
            'prefs.settings.lobster.enterprise_settings.value)',
      },

      isScannerAllowedByEnterprisePolicy_: {
        type: Boolean,
        computed: 'isEnterprisePolicyAllowed_(' +
            'prefs.ash.scanner.enterprise_policy_allowed.value)',
      },

      isHmrAllowedByEnterprisePolicy_: {
        type: Boolean,
        computed: 'isEnterprisePolicyAllowed_(' +
            'prefs.settings.managed.help_me_read.value)',
      },

      isHmwAllowedByEnterprisePolicy_: {
        type: Boolean,
        computed: 'isEnterprisePolicyAllowed_(' +
            'prefs.settings.managed.help_me_write.value)',
      },

      enterprisePolicyToggleUncheckedValues_: {
        type: Array,
        readOnly: true,
        value: () => [ENTERPRISE_POLICY_DISALLOWED],
      },

      /** Can be disallowed due to flag, policy, locale, etc. */
      isAssistantAllowed_: {
        type: Boolean,
        value: () => {
          return isAssistantAllowed();
        },
      },
    };
  }

  prefs: PrefsState;

  // DeepLinkingMixin override
  override supportedSettingIds = new Set<Setting>([
    Setting.kPreferredSearchEngine,
    Setting.kMagicBoostOnOff,
    Setting.kMahiOnOff,
    Setting.kShowOrca,
    Setting.kLobsterOnOff,
    Setting.kSunfishOnOff,
    Setting.kScannerOnOff,
  ]);

  private readonly enterprisePolicyToggleUncheckedValues_: number[];
  private isAssistantAllowed_: boolean;
  private isHmrAllowedByEnterprisePolicy_: boolean;
  private isHmwAllowedByEnterprisePolicy_: boolean;
  private isLobsterAllowedByEnterprisePolicy_: boolean;
  private readonly isLobsterSettingsToggleVisible_: boolean;
  private readonly isMagicBoostNoticeBannerVisible_: boolean;
  private isMagicBoostFeatureEnabled_: boolean;
  private isQuickAnswersSupported_: boolean;
  private isScannerAllowedByEnterprisePolicy_: boolean;
  private readonly isScannerSettingsToggleVisible_: boolean;

  constructor() {
    super();

    /** RouteOriginMixin overrde */
    this.route = routes.SYSTEM_PREFERENCES;
  }

  override ready(): void {
    super.ready();

    this.addFocusConfig(routes.SEARCH_SUBPAGE, '#searchRow');
    this.addFocusConfig(routes.GOOGLE_ASSISTANT, '#assistantRow');
  }

  override currentRouteChanged(newRoute: Route, oldRoute?: Route): void {
    super.currentRouteChanged(newRoute, oldRoute);

    // Does not apply to this page.
    if (newRoute !== this.route) {
      return;
    }

    this.attemptDeepLink();
  }

  private onSearchClick_(): void {
    assert(this.isQuickAnswersSupported_);
    Router.getInstance().navigateTo(routes.SEARCH_SUBPAGE);
  }

  private onGoogleAssistantClick_(): void {
    assert(this.isAssistantAllowed_);
    Router.getInstance().navigateTo(routes.GOOGLE_ASSISTANT);
  }

  private getAssistantEnabledDisabledLabel_(isAssistantEnabled: boolean):
      string {
    return this.i18n(
        isAssistantEnabled ? 'searchGoogleAssistantEnabled' :
                             'searchGoogleAssistantDisabled');
  }

  private isEnterprisePolicyAllowed_(value: number): boolean {
    return value !== ENTERPRISE_POLICY_DISALLOWED;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [SearchAndAssistantSettingsCardElement.is]:
        SearchAndAssistantSettingsCardElement;
  }
}

customElements.define(
    SearchAndAssistantSettingsCardElement.is,
    SearchAndAssistantSettingsCardElement);
