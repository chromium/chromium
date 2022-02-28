// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'os-settings-search-page' contains search and assistant settings.
 */
import '//resources/cr_elements/cr_link_row/cr_link_row.js';
import '//resources/cr_elements/icons.m.js';
import '//resources/cr_elements/policy/cr_policy_pref_indicator.m.js';
import '//resources/cr_elements/shared_style_css.m.js';
import '//resources/cr_elements/shared_vars_css.m.js';
import './os_search_selection_dialog.js';
import '../../controls/extension_controlled_indicator.js';
import '../../settings_page/settings_animated_pages.js';
import '../../settings_page/settings_subpage.js';
import '../../settings_shared_css.js';
import '../../settings_vars_css.js';
import '../google_assistant_page/google_assistant_page.js';
import './search_subpage.js';
import './search_engine.js';

import {assert, assertNotReached} from '//resources/js/assert.m.js';
import {addWebUIListener, removeWebUIListener, sendWithPromise, WebUIListener} from '//resources/js/cr.m.js';
import {focusWithoutInk} from '//resources/js/cr/ui/focus_without_ink.m.js';
import {I18nBehavior} from '//resources/js/i18n_behavior.m.js';
import {loadTimeData} from '//resources/js/load_time_data.m.js';
import {afterNextRender, flush, html, Polymer, TemplateInstanceBase, Templatizer} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {Route, Router} from '../../router.js';
import {DeepLinkingBehavior} from '../deep_linking_behavior.js';
import {routes} from '../os_route.m.js';
import {RouteObserverBehavior} from '../route_observer_behavior.js';

import {SearchEngine, SearchEnginesBrowserProxy, SearchEnginesBrowserProxyImpl} from './search_engines_browser_proxy.js';

Polymer({
  _template: html`{__html_template__}`,
  is: 'os-settings-search-page',

  behaviors: [
    DeepLinkingBehavior,
    I18nBehavior,
    RouteObserverBehavior,
  ],

  properties: {
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
     * @type {!Set<!chromeos.settings.mojom.Setting>}
     */
    supportedSettingIds: {
      type: Object,
      value: () =>
          new Set([chromeos.settings.mojom.Setting.kPreferredSearchEngine]),
    },
  },

  /** @override */
  ready() {
    this.focusConfig_ = new Map();
    this.focusConfig_.set(routes.SEARCH_SUBPAGE.path, '#searchSubpageTrigger');
    this.focusConfig_.set(
        routes.GOOGLE_ASSISTANT.path, '#assistantSubpageTrigger');
  },

  /**
   * @param {!Route} route
   * @param {!Route} oldRoute
   */
  currentRouteChanged(route, oldRoute) {
    // Does not apply to this page.
    if (route !== routes.OS_SEARCH) {
      return;
    }

    this.attemptDeepLink();
  },

  /** @private */
  onSearchTap_() {
    Router.getInstance().navigateTo(routes.SEARCH_SUBPAGE);
  },

  /** @private */
  onGoogleAssistantTap_() {
    assert(this.isAssistantAllowed_);
    Router.getInstance().navigateTo(routes.GOOGLE_ASSISTANT);
  },

  /**
   * @param {boolean} toggleValue
   * @return {string}
   * @private
   */
  getAssistantEnabledDisabledLabel_(toggleValue) {
    return this.i18n(
        toggleValue ? 'searchGoogleAssistantEnabled' :
                      'searchGoogleAssistantDisabled');
  },
});
