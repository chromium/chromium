// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'settings-search-engine' is the settings module for setting
 * the preferred search engine.
 */
import 'chrome://resources/cr_elements/cr_link_row/cr_link_row.js';
import 'chrome://resources/cr_elements/icons.html.js';
import 'chrome://resources/cr_elements/policy/cr_policy_pref_indicator.js';
import 'chrome://resources/cr_elements/cr_shared_style.css.js';
import 'chrome://resources/cr_elements/cr_shared_vars.css.js';
import './os_search_selection_dialog.js';
import '../../controls/extension_controlled_indicator.js';
import '../../controls/controlled_button.js';
import '../../controls/settings_toggle_button.js';
import '../../prefs/prefs.js';
import '../../prefs/pref_util.js';
import '../../settings_shared.css.js';
import '../../settings_vars.css.js';

import {assert} from 'chrome://resources/js/assert.m.js';
import {focusWithoutInk} from 'chrome://resources/js/cr/ui/focus_without_ink_js.js';
import {I18nBehavior, I18nBehaviorInterface} from 'chrome://resources/cr_elements/i18n_behavior.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
import {WebUIListenerBehavior, WebUIListenerBehaviorInterface} from 'chrome://resources/cr_elements/web_ui_listener_behavior.js';
import {html, mixinBehaviors, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {SearchEngine, SearchEnginesBrowserProxy, SearchEnginesBrowserProxyImpl, SearchEnginesInfo} from './search_engines_browser_proxy.js';

/**
 * @constructor
 * @extends {PolymerElement}
 * @implements {I18nBehaviorInterface}
 * @implements {WebUIListenerBehaviorInterface}
 */
const SettingsSearchEngineElementBase =
    mixinBehaviors([I18nBehavior, WebUIListenerBehavior], PolymerElement);

/** @polymer */
class SettingsSearchEngineElement extends SettingsSearchEngineElementBase {
  static get is() {
    return 'settings-search-engine';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
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
    };
  }

  /** @override */
  constructor() {
    super();

    /** @private {!SearchEnginesBrowserProxy} */
    this.browserProxy_ = SearchEnginesBrowserProxyImpl.getInstance();
  }

  /** @override */
  ready() {
    super.ready();

    this.browserProxy_.getSearchEnginesList().then(
        this.updateCurrentSearchEngine_.bind(this));
    this.addWebUIListener(
        'search-engines-changed', this.updateCurrentSearchEngine_.bind(this));
  }

  /**
   * @param {!SearchEnginesInfo} searchEngines
   * @private
   */
  updateCurrentSearchEngine_(searchEngines) {
    this.currentSearchEngine_ =
        /** @type {!SearchEngine} */ (
            searchEngines.defaults.find(searchEngine => searchEngine.default));
  }

  /** @override */
  focus() {
    if (loadTimeData.getBoolean('syncSettingsCategorizationEnabled')) {
      this.shadowRoot.querySelector('#browserSearchSettingsLink').focus();
    } else {
      this.shadowRoot.querySelector('#searchSelectionDialogButton').focus();
    }
  }

  /** @private */
  onDisableExtension_() {
    const event = new CustomEvent('refresh-pref', {
      bubbles: true,
      composed: true,
      detail: 'default_search_provider.enabled',
    });
    this.dispatchEvent(event);
  }

  /** @private */
  onShowSearchSelectionDialogClick_() {
    this.showSearchSelectionDialog_ = true;
  }

  /** @private */
  onSearchSelectionDialogClose_() {
    this.showSearchSelectionDialog_ = false;
    focusWithoutInk(
        assert(this.shadowRoot.querySelector('#searchSelectionDialogButton')));
  }

  /** @private */
  onSearchEngineLinkClick_() {
    window.open('chrome://settings/search');
  }

  /**
   * @param {!chrome.settingsPrivate.PrefObject} pref
   * @return {boolean}
   * @private
   */
  isDefaultSearchControlledByPolicy_(pref) {
    return pref.controlledBy ===
        chrome.settingsPrivate.ControlledBy.USER_POLICY;
  }

  /**
   * @param {!chrome.settingsPrivate.PrefObject} pref
   * @return {boolean}
   * @private
   */
  isDefaultSearchEngineEnforced_(pref) {
    return pref.enforcement === chrome.settingsPrivate.Enforcement.ENFORCED;
  }
}

customElements.define(
    SettingsSearchEngineElement.is, SettingsSearchEngineElement);
