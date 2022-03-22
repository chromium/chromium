// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'settings-search-engine' is the settings module for setting
 * the preferred search engine.
 */
import '//resources/cr_elements/cr_link_row/cr_link_row.js';
import '//resources/cr_elements/icons.m.js';
import '//resources/cr_elements/policy/cr_policy_pref_indicator.m.js';
import '//resources/cr_elements/shared_style_css.m.js';
import '//resources/cr_elements/shared_vars_css.m.js';
import './os_search_selection_dialog.js';
import '../../controls/extension_controlled_indicator.js';
import '../../controls/controlled_button.js';
import '../../controls/settings_toggle_button.js';
import '../../prefs/prefs.js';
import '../../prefs/pref_util.js';
import '../../settings_shared_css.js';
import '../../settings_vars_css.js';

import {assert, assertNotReached} from '//resources/js/assert.m.js';
import {addWebUIListener, removeWebUIListener, sendWithPromise, WebUIListener} from '//resources/js/cr.m.js';
import {focusWithoutInk} from '//resources/js/cr/ui/focus_without_ink.m.js';
import {I18nBehavior} from '//resources/js/i18n_behavior.m.js';
import {loadTimeData} from '//resources/js/load_time_data.m.js';
import {afterNextRender, flush, html, Polymer, TemplateInstanceBase, Templatizer} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {Route, Router} from '../../router.js';
import {routes} from '../os_route.js';
import {PrefsBehavior} from '../prefs_behavior.js';
import {RouteObserverBehavior} from '../route_observer_behavior.js';

import {SearchEngine, SearchEnginesBrowserProxy, SearchEnginesBrowserProxyImpl} from './search_engines_browser_proxy.js';

Polymer({
  _template: html`{__html_template__}`,
  is: 'settings-search-engine',

  behaviors: [
    I18nBehavior,
  ],

  properties: {
    prefs: Object,

    /** @private {!SearchEngine} The current selected search engine. */
    currentSearchEngine_: Object,

    /** @private */
    showSearchSelectionDialog_: Boolean,

    /** @private */
    syncSettingsCategorizationEnabled_: {
      type: Boolean,
      value() {
        return loadTimeData.getBoolean('syncSettingsCategorizationEnabled');
      },
    },
  },

  /** @private {?SearchEnginesBrowserProxy} */
  browserProxy_: null,

  /** @override */
  created() {
    this.browserProxy_ = SearchEnginesBrowserProxyImpl.getInstance();
  },

  /** @override */
  ready() {
    const updateCurrentSearchEngine = searchEngines => {
      this.currentSearchEngine_ =
          searchEngines.defaults.find(searchEngine => searchEngine.default);
    };
    this.browserProxy_.getSearchEnginesList().then(updateCurrentSearchEngine);
    addWebUIListener('search-engines-changed', updateCurrentSearchEngine);
  },

  /** @override */
  focus() {
    if (loadTimeData.getBoolean('syncSettingsCategorizationEnabled')) {
      this.$$('#browserSearchSettingsLink').focus();
    } else {
      this.$$('#searchSelectionDialogButton').focus();
    }
  },

  /** @private */
  onDisableExtension_() {
    this.fire('refresh-pref', 'default_search_provider.enabled');
  },

  /** @private */
  onShowSearchSelectionDialogClick_() {
    this.showSearchSelectionDialog_ = true;
  },

  /** @private */
  onSearchSelectionDialogClose_() {
    this.showSearchSelectionDialog_ = false;
    focusWithoutInk(assert(this.$$('#searchSelectionDialogButton')));
  },

  /** @private */
  onSearchEngineLinkClick_() {
    window.open('chrome://settings/search');
  },

  /**
   * @param {!chrome.settingsPrivate.PrefObject} pref
   * @return {boolean}
   * @private
   */
  isDefaultSearchControlledByPolicy_(pref) {
    return pref.controlledBy ===
        chrome.settingsPrivate.ControlledBy.USER_POLICY;
  },

  /**
   * @param {!chrome.settingsPrivate.PrefObject} pref
   * @return {boolean}
   * @private
   */
  isDefaultSearchEngineEnforced_(pref) {
    return pref.enforcement === chrome.settingsPrivate.Enforcement.ENFORCED;
  },
});
