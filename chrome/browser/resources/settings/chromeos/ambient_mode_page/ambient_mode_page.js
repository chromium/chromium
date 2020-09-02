// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'settings-ambient-mode-page' is the settings page containing
 * ambient mode settings.
 */
Polymer({
  is: 'settings-ambient-mode-page',

  behaviors: [
    DeepLinkingBehavior, I18nBehavior, PrefsBehavior,
    settings.RouteObserverBehavior, WebUIListenerBehavior
  ],

  properties: {
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
        AmbientModeTopicSource.GOOGLE_PHOTOS, AmbientModeTopicSource.ART_GALLERY
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
  },

  listeners: {
    'show-albums': 'onShowAlbums_',
  },

  /** @private {?settings.AmbientModeBrowserProxy} */
  browserProxy_: null,

  /** @override */
  created() {
    this.browserProxy_ = settings.AmbientModeBrowserProxyImpl.getInstance();
  },

  /** @override */
  ready() {
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
  },

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
    Polymer.RenderStatus.afterNextRender(this, () => {
      Polymer.dom.flush();

      const topicList = this.$$('topic-source-list');
      const listItem = topicList && topicList.$$('topic-source-item');
      if (listItem) {
        this.showDeepLinkElement(listItem);
        return;
      }

      console.warn(`Element with deep link id ${settingId} not focusable.`);
    });
    // Stop deep link attempt since we completed it manually.
    return false;
  },

  /**
   * RouteObserverBehavior
   * @param {!settings.Route} currentRoute
   * @protected
   */
  currentRouteChanged(currentRoute) {
    if (currentRoute !== settings.routes.AMBIENT_MODE) {
      return;
    }

    this.browserProxy_.requestSettings();
    this.attemptDeepLink();
  },

  /**
   * @param {boolean} toggleValue
   * @return {string}
   * @private
   */
  getAmbientModeOnOffLabel_(toggleValue) {
    return this.i18n(toggleValue ? 'ambientModeOn' : 'ambientModeOff');
  },

  /**
   * @param {!AmbientModeTemperatureUnit} temperatureUnit
   * @return {boolean}
   * @private
   */
  isValidTemperatureUnit_(temperatureUnit) {
    return temperatureUnit === AmbientModeTemperatureUnit.FAHRENHEIT ||
        temperatureUnit === AmbientModeTemperatureUnit.CELSIUS;
  },

  /**
   * @param {number} topicSource
   * @return {boolean}
   * @private
   */
  isValidTopicSource_(topicSource) {
    return topicSource !== AmbientModeTopicSource.UNKNOWN;
  },

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
  },

  /**
   * Open ambientMode/photos subpage.
   * @param {!CustomEvent<{item: !AmbientModeTopicSource}>} event
   * @private
   */
  onShowAlbums_(event) {
    const params = new URLSearchParams();
    params.append('topicSource', JSON.stringify(event.detail));
    settings.Router.getInstance().navigateTo(
        settings.routes.AMBIENT_MODE_PHOTOS, params);
  }
});
