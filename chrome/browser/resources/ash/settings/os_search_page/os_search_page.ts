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
import '/shared/settings/controls/extension_controlled_indicator.js';
import '../settings_shared.css.js';
import '../settings_vars.css.js';
import '../os_settings_page/os_settings_animated_pages.js';
import '../os_settings_page/os_settings_subpage.js';
import '../os_settings_page/settings_card.js';
import './os_search_selection_dialog.js';
import './search_engine.js';

import {I18nMixin} from 'chrome://resources/cr_elements/i18n_mixin.js';
import {assert} from 'chrome://resources/js/assert_ts.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {DeepLinkingMixin} from '../deep_linking_mixin.js';
import {Section} from '../mojom-webui/routes.mojom-webui.js';
import {Setting} from '../mojom-webui/setting.mojom-webui.js';
import {RouteOriginMixin} from '../route_origin_mixin.js';
import {Route, Router, routes} from '../router.js';

import {getTemplate} from './os_search_page.html.js';

const OsSettingsSearchPageElementBase =
    DeepLinkingMixin(RouteOriginMixin(I18nMixin(PolymerElement)));

export class OsSettingsSearchPageElement extends
    OsSettingsSearchPageElementBase {
  static get is() {
    return 'os-settings-search-page' as const;
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      section_: {
        type: Number,
        value: Section.kSearchAndAssistant,
        readOnly: true,
      },

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
  private section_: Section;
  private shouldShowQuickAnswersSettings_: boolean;

  constructor() {
    super();

    /** RouteOriginMixin overrde */
    this.route = routes.OS_SEARCH;
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

  private onSearchClick_() {
    Router.getInstance().navigateTo(routes.SEARCH_SUBPAGE);
  }

  private onGoogleAssistantClick_() {
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
    [OsSettingsSearchPageElement.is]: OsSettingsSearchPageElement;
  }
}

customElements.define(
    OsSettingsSearchPageElement.is, OsSettingsSearchPageElement);
