// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'settings-ambient-mode-page' is the settings page containing
 * ambient mode settings.
 */
import '//resources/cr_elements/cr_radio_button/cr_radio_button.m.js';
import '//resources/cr_elements/shared_style_css.m.js';
import '//resources/cr_elements/shared_vars_css.m.js';
import '//resources/polymer/v3_0/paper-spinner/paper-spinner-lite.js';
import './topic_source_list.js';
import '../../prefs/prefs.js';
import '../../controls/settings_radio_group.js';
import '../../controls/settings_toggle_button.js';
import '../../settings_shared_css.js';

import {I18nBehavior, I18nBehaviorInterface} from '//resources/js/i18n_behavior.m.js';
import {WebUIListenerBehavior, WebUIListenerBehaviorInterface} from '//resources/js/web_ui_listener_behavior.m.js';
import {afterNextRender, flush, html, mixinBehaviors, PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {Route, Router} from '../../router.js';
import {DeepLinkingBehavior, DeepLinkingBehaviorInterface} from '../deep_linking_behavior.js';
import {routes} from '../os_route.js';
import {PrefsBehavior, PrefsBehaviorInterface} from '../prefs_behavior.js';
import {RouteObserverBehavior, RouteObserverBehaviorInterface} from '../route_observer_behavior.js';

import {AmbientModeBrowserProxy, AmbientModeBrowserProxyImpl} from './ambient_mode_browser_proxy.js';
import {AmbientModeTemperatureUnit, AmbientModeTopicSource, TopicSourceItem} from './constants.js';

/**
 * @constructor
 * @extends {PolymerElement}
 * @implements {DeepLinkingBehaviorInterface}
 * @implements {I18nBehaviorInterface}
 * @implements {PrefsBehaviorInterface}
 * @implements {RouteObserverBehaviorInterface}
 * @implements {WebUIListenerBehaviorInterface}
 */
const SettingsAmbientModePageElementBase = mixinBehaviors(
    [
      DeepLinkingBehavior, I18nBehavior, PrefsBehavior, RouteObserverBehavior,
      WebUIListenerBehavior
    ],
    PolymerElement);

/** @polymer */
class SettingsAmbientModePageElement extends
    SettingsAmbientModePageElementBase {
  static get is() {
    return 'settings-ambient-mode-page';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      /** Preferences state. */
      prefs: Object,

      /**
       * Used to refer to the enum values in the HTML.
       * @private {!Object}
       */
      AmbientModeTopicSource: {
        type: Object,
        value: AmbientModeTopicSource,
      },

      /**
       * Used to refer to the enum values in the HTML.
       * @private {!Object<string, AmbientModeTemperatureUnit>}
       */
      AmbientModeTemperatureUnit_: {
        type: Object,
        value: AmbientModeTemperatureUnit,
      },

      // TODO(b/160632748): Dynamically generate topic source of Google Photos.
      /** @private {!Array<!AmbientModeTopicSource>} */
      topicSources_: {
        type: Array,
        value: [
          AmbientModeTopicSource.GOOGLE_PHOTOS,
          AmbientModeTopicSource.ART_GALLERY
        ],
      },

      /** @private {!AmbientModeTopicSource} */
      selectedTopicSource_: {
        type: AmbientModeTopicSource,
        value: AmbientModeTopicSource.UNKNOWN,
      },

      /** @private */
      hasGooglePhotosAlbums_: Boolean,

      /** @private {!AmbientModeTemperatureUnit} */
      selectedTemperatureUnit_: {
        type: AmbientModeTemperatureUnit,
        value: AmbientModeTemperatureUnit.UNKNOWN,
        observer: 'onSelectedTemperatureUnitChanged_'
      },

      /**
       * Used by DeepLinkingBehavior to focus this page's deep links.
       * @type {!Set<!chromeos.settings.mojom.Setting>}
       */
      supportedSettingIds: {
        type: Object,
        value: () => new Set([
          chromeos.settings.mojom.Setting.kAmbientModeOnOff,
          chromeos.settings.mojom.Setting.kAmbientModeSource,
        ]),
      },

      /** @private */
      showSettings_: {
        type: Boolean,
        computed: 'computeShowSettings_(' +
            'selectedTopicSource_, selectedTemperatureUnit_)',
      },

      /** @private */
      disableSettings_: {
        type: Boolean,
        computed: 'computeDisableSettings_(prefs.settings.ambient_mode.*)',
      }
    };
  }

  /** @override */
  constructor() {
    super();

    /** @private {!AmbientModeBrowserProxy} */
    this.browserProxy_ = AmbientModeBrowserProxyImpl.getInstance();
  }

  /** @override */
  ready() {
    super.ready();

    this.addWebUIListener(
        'topic-source-changed',
        (/** @type {!TopicSourceItem} */ topicSourceItem) => {
          this.selectedTopicSource_ = topicSourceItem.topicSource;
          this.hasGooglePhotosAlbums_ = topicSourceItem.hasGooglePhotosAlbums;
        },
    );
    this.addWebUIListener(
        'temperature-unit-changed',
        (/** @type {!AmbientModeTemperatureUnit} */ temperatureUnit) => {
          this.selectedTemperatureUnit_ = temperatureUnit;
        },
    );

    this.addEventListener('show-albums', this.onShowAlbums_);
  }

  /**
   * Overridden from DeepLinkingBehavior.
   * @param {!chromeos.settings.mojom.Setting} settingId
   */
  beforeDeepLinkAttempt(settingId) {
    if (settingId !== chromeos.settings.mojom.Setting.kAmbientModeSource) {
      // Continue with deep link attempt.
      return true;
    }

    // Wait for element to load.
    afterNextRender(this, () => {
      flush();

      const topicList = this.shadowRoot.querySelector('topic-source-list');
      const listItem =
          topicList && topicList.shadowRoot.querySelector('topic-source-item');
      if (listItem) {
        this.showDeepLinkElement(listItem);
        return;
      }

      console.warn(`Element with deep link id ${settingId} not focusable.`);
    });
    // Stop deep link attempt since we completed it manually.
    return false;
  }

  /**
   * RouteObserverBehavior
   * @param {!Route} currentRoute
   * @param {!Route=} prevRoute
   * @protected
   */
  currentRouteChanged(currentRoute, prevRoute) {
    if (currentRoute !== routes.AMBIENT_MODE) {
      return;
    }

    this.browserProxy_.requestSettings();
    this.attemptDeepLink();
  }

  /**
   * @param {boolean} toggleValue
   * @return {string}
   * @private
   */
  getAmbientModeOnOffLabel_(toggleValue) {
    return this.i18n(toggleValue ? 'ambientModeOn' : 'ambientModeOff');
  }

  /**
   * @param {!AmbientModeTemperatureUnit} temperatureUnit
   * @return {boolean}
   * @private
   */
  isValidTemperatureUnit_(temperatureUnit) {
    return temperatureUnit === AmbientModeTemperatureUnit.FAHRENHEIT ||
        temperatureUnit === AmbientModeTemperatureUnit.CELSIUS;
  }

  /**
   * @param {number} topicSource
   * @return {boolean}
   * @private
   */
  isValidTopicSource_(topicSource) {
    return topicSource !== AmbientModeTopicSource.UNKNOWN;
  }

  /**
   * @param {!AmbientModeTemperatureUnit} newValue
   * @param {!AmbientModeTemperatureUnit} oldValue
   * @private
   */
  onSelectedTemperatureUnitChanged_(newValue, oldValue) {
    if (newValue && newValue !== AmbientModeTemperatureUnit.UNKNOWN &&
        newValue !== oldValue) {
      this.browserProxy_.setSelectedTemperatureUnit(newValue);
    }
  }

  /**
   * Open ambientMode/photos subpage.
   * @param {!Event} event
   * @private
   */
  onShowAlbums_(
      /** @type {CustomEvent<{item: !AmbientModeTopicSource}>} */ event) {
    const params = new URLSearchParams();
    params.append('topicSource', JSON.stringify(event.detail));
    Router.getInstance().navigateTo(routes.AMBIENT_MODE_PHOTOS, params);
  }

  /**
   * Whether to show settings.
   * @return {boolean}
   * @private
   */
  computeShowSettings_() {
    return this.isValidTopicSource_(this.selectedTopicSource_) &&
        this.isValidTemperatureUnit_(this.selectedTemperatureUnit_);
  }

  /**
   * Whether to disable settings.
   * @return {boolean}
   * @private
   */
  computeDisableSettings_() {
    return !this.getPref('settings.ambient_mode.enabled').value;
  }
}

customElements.define(
    SettingsAmbientModePageElement.is, SettingsAmbientModePageElement);
