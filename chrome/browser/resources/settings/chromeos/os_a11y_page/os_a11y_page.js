// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'os-settings-a11y-page' is the small section of advanced settings containing
 * a subpage with Accessibility settings for ChromeOS.
 */
Polymer({
  is: 'os-settings-a11y-page',

  behaviors: [
    DeepLinkingBehavior,
    settings.RouteObserverBehavior,
    WebUIListenerBehavior,
  ],

  properties: {
    /**
     * The current active route.
     */
    currentRoute: {
      type: Object,
      notify: true,
    },

    /**
     * Preferences state.
     */
    prefs: {
      type: Object,
      notify: true,
    },

    /**
     * Whether to show accessibility labels settings.
     */
    showAccessibilityLabelsSetting_: {
      type: Boolean,
      value: false,
    },

    /** @private {!Map<string, string>} */
    focusConfig_: {
      type: Object,
      value() {
        const map = new Map();
        if (settings.routes.MANAGE_ACCESSIBILITY) {
          map.set(
              settings.routes.MANAGE_ACCESSIBILITY.path, '#subpage-trigger');
        }
        return map;
      },
    },

    /**
     * Whether the user is in kiosk mode.
     * @private
     */
    isKioskModeActive_: {
      type: Boolean,
      value: function() {
        return loadTimeData.getBoolean('isKioskModeActive');
      }
    },

    /**
     * Used by DeepLinkingBehavior to focus this page's deep links.
     * @type {!Set<!chromeos.settings.mojom.Setting>}
     */
    supportedSettingIds: {
      type: Object,
      value: () => new Set([
        chromeos.settings.mojom.Setting.kA11yQuickSettings,
        chromeos.settings.mojom.Setting.kGetImageDescriptionsFromGoogle,
        chromeos.settings.mojom.Setting.kLiveCaption,
      ]),
    },
  },

  /** @private {?OsA11yPageBrowserProxy} */
  browserProxy_: null,

  /** @override */
  created() {
    this.browserProxy_ = OsA11yPageBrowserProxyImpl.getInstance();
  },

  /** @override */
  ready() {
    this.addWebUIListener(
        'screen-reader-state-changed',
        this.onScreenReaderStateChanged_.bind(this));

    // Enables javascript and gets the screen reader state.
    this.browserProxy_.a11yPageReady();
  },

  /**
   * Overridden from DeepLinkingBehavior.
   * @param {!chromeos.settings.mojom.Setting} settingId
   * @return {boolean}
   */
  beforeDeepLinkAttempt(settingId) {
    if (settingId === chromeos.settings.mojom.Setting.kLiveCaption) {
      Polymer.RenderStatus.afterNextRender(this, () => {
        const captionsSubpage = this.$$('settings-captions');
        if (captionsSubpage && captionsSubpage.getLiveCaptionToggle()) {
          this.showDeepLinkElement(captionsSubpage.getLiveCaptionToggle());
          return;
        }
        console.warn(`Element with deep link id ${settingId} not focusable.`);
      });

      // Stop deep link attempt since we completed it manually.
      return false;
    }

    // Continue with deep linking attempt.
    return true;
  },

  /**
   * @param {!settings.Route} route
   * @param {!settings.Route} oldRoute
   */
  currentRouteChanged(route, oldRoute) {
    if (route === settings.routes.OS_ACCESSIBILITY ||
        route === settings.routes.MANAGE_CAPTION_SETTINGS) {
      this.attemptDeepLink();
    }
  },

  /**
   * @private
   * @param {boolean} hasScreenReader Whether a screen reader is enabled.
   */
  onScreenReaderStateChanged_(hasScreenReader) {
    // TODO(katie): Remove showExperimentalA11yLabels flag before launch.
    this.showAccessibilityLabelsSetting_ = hasScreenReader &&
        loadTimeData.getBoolean('showExperimentalA11yLabels');
  },

  /** @private */
  onToggleAccessibilityImageLabels_() {
    const a11yImageLabelsOn = this.$.a11yImageLabels.checked;
    if (a11yImageLabelsOn) {
      this.browserProxy_.confirmA11yImageLabels();
    }
  },

  /** @private */
  onManageAccessibilityFeaturesTap_() {
    settings.Router.getInstance().navigateTo(
        settings.routes.MANAGE_ACCESSIBILITY);
  },

});
