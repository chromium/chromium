// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'os-settings-search-page' contains search and assistant settings.
 */
import 'chrome://resources/cr_elements/cr_link_row/cr_link_row.js';
import 'chrome://resources/cr_elements/icons.html.js';
import 'chrome://resources/cr_elements/policy/cr_policy_pref_indicator.js';
import 'chrome://resources/cr_elements/cr_shared_style.css.js';
import 'chrome://resources/cr_elements/cr_shared_vars.css.js';
import './os_search_selection_dialog.js';
import '../../controls/extension_controlled_indicator.js';
import '../os_settings_page/os_settings_animated_pages.js';
import '../os_settings_page/os_settings_subpage.js';
import '../../settings_shared.css.js';
import '../../settings_vars.css.js';
import '../google_assistant_page/google_assistant_page.js';
import './search_subpage.js';
import './search_engine.js';

import {I18nMixin} from 'chrome://resources/cr_elements/i18n_mixin.js';
import {assert} from 'chrome://resources/js/assert_ts.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {Setting} from '../../mojom-webui/setting.mojom-webui.js';
import {DeepLinkingMixin} from '../deep_linking_mixin.js';
import {routes} from '../os_settings_routes.js';
import {RouteObserverMixin} from '../route_observer_mixin.js';
import {Route, Router} from '../router.js';

import {getTemplate} from './os_search_page.html.js';

const OsSettingsSearchPageElementBase =
    DeepLinkingMixin(RouteObserverMixin(I18nMixin(PolymerElement)));

class OsSettingsSearchPageElement extends OsSettingsSearchPageElementBase {
  static get is() {
    return 'os-settings-search-page';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      focusConfig_: Object,

      shouldShowQuickAnswersSettings_: {
        type: Boolean,
        value() {
          return loadTimeData.getBoolean('shouldShowQuickAnswersSettings');
        },
      },

      /** Can be disallowed due to flag, policy, locale, etc. */
      isAssistantAllowed_: {
        type: Boolean,
        value() {
          return loadTimeData.getBoolean('isAssistantAllowed');
        },
      },

      /**
       * Used by DeepLinkingMixin to focus this page's deep links.
       */
      supportedSettingIds: {
        type: Object,
        value: () => new Set<Setting>([Setting.kPreferredSearchEngine]),
      },
    };
  }

  private isAssistantAllowed_: boolean;
  private focusConfig_: Map<string, string>;
  private shouldShowQuickAnswersSettings_: boolean;

  override ready() {
    super.ready();

    this.focusConfig_ = new Map();
    this.focusConfig_.set(routes.SEARCH_SUBPAGE.path, '#searchSubpageTrigger');
    this.focusConfig_.set(
        routes.GOOGLE_ASSISTANT.path, '#assistantSubpageTrigger');
  }

  override currentRouteChanged(route: Route, _oldRoute?: Route) {
    // Does not apply to this page.
    if (route !== routes.OS_SEARCH) {
      return;
    }

    this.attemptDeepLink();
  }

  private onSearchTap_() {
    Router.getInstance().navigateTo(routes.SEARCH_SUBPAGE);
  }

  private onGoogleAssistantTap_() {
    assert(this.isAssistantAllowed_);
    Router.getInstance().navigateTo(routes.GOOGLE_ASSISTANT);
  }

  private getAssistantEnabledDisabledLabel_(toggleValue: boolean): string {
    return this.i18n(
        toggleValue ? 'searchGoogleAssistantEnabled' :
                      'searchGoogleAssistantDisabled');
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'os-settings-search-page': OsSettingsSearchPageElement;
  }
}

customElements.define(
    OsSettingsSearchPageElement.is, OsSettingsSearchPageElement);
