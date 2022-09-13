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
import '../../settings_page/settings_animated_pages.js';
import '../../settings_page/settings_subpage.js';
import '../../settings_shared.css.js';
import '../../settings_vars.css.js';
import '../google_assistant_page/google_assistant_page.js';
import './search_subpage.js';
import './search_engine.js';

import {assert} from 'chrome://resources/js/assert.m.js';
import {I18nBehavior, I18nBehaviorInterface} from 'chrome://resources/cr_elements/i18n_behavior.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
import {html, mixinBehaviors, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {Setting} from '../../mojom-webui/setting.mojom-webui.js';
import {Route, Router} from '../../router.js';
import {DeepLinkingBehavior, DeepLinkingBehaviorInterface} from '../deep_linking_behavior.js';
import {routes} from '../os_route.js';
import {RouteObserverBehavior, RouteObserverBehaviorInterface} from '../route_observer_behavior.js';

/**
 * @constructor
 * @extends {PolymerElement}
 * @implements {DeepLinkingBehaviorInterface}
 * @implements {I18nBehaviorInterface}
 * @implements {RouteObserverBehaviorInterface}
 */
const OsSettingsSearchPageElementBase = mixinBehaviors(
    [DeepLinkingBehavior, I18nBehavior, RouteObserverBehavior], PolymerElement);

/** @polymer */
class OsSettingsSearchPageElement extends OsSettingsSearchPageElementBase {
  static get is() {
    return 'os-settings-search-page';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      /** @type {?Map<string, string>} */
      focusConfig_: Object,

      /** @private */
      shouldShowQuickAnswersSettings_: {
        type: Boolean,
        value() {
          return loadTimeData.getBoolean('shouldShowQuickAnswersSettings');
        },
      },

      /** @private Can be disallowed due to flag, policy, locale, etc. */
      isAssistantAllowed_: {
        type: Boolean,
        value() {
          return loadTimeData.getBoolean('isAssistantAllowed');
        },
      },

      /**
       * Used by DeepLinkingBehavior to focus this page's deep links.
       * @type {!Set<!Setting>}
       */
      supportedSettingIds: {
        type: Object,
        value: () => new Set([Setting.kPreferredSearchEngine]),
      },
    };
  }

  /** @override */
  ready() {
    super.ready();

    this.focusConfig_ = new Map();
    this.focusConfig_.set(routes.SEARCH_SUBPAGE.path, '#searchSubpageTrigger');
    this.focusConfig_.set(
        routes.GOOGLE_ASSISTANT.path, '#assistantSubpageTrigger');
  }

  /**
   * @param {!Route} route
   * @param {!Route=} oldRoute
   */
  currentRouteChanged(route, oldRoute) {
    // Does not apply to this page.
    if (route !== routes.OS_SEARCH) {
      return;
    }

    this.attemptDeepLink();
  }

  /** @private */
  onSearchTap_() {
    Router.getInstance().navigateTo(routes.SEARCH_SUBPAGE);
  }

  /** @private */
  onGoogleAssistantTap_() {
    assert(this.isAssistantAllowed_);
    Router.getInstance().navigateTo(routes.GOOGLE_ASSISTANT);
  }

  /**
   * @param {boolean} toggleValue
   * @return {string}
   * @private
   */
  getAssistantEnabledDisabledLabel_(toggleValue) {
    return this.i18n(
        toggleValue ? 'searchGoogleAssistantEnabled' :
                      'searchGoogleAssistantDisabled');
  }
}

customElements.define(
    OsSettingsSearchPageElement.is, OsSettingsSearchPageElement);
