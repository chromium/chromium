// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'settings-a11y-page' is the small section of advanced settings with
 * a link to the web store accessibility page on most platforms, and
 * a subpage with lots of other settings on Chrome OS.
 */
Polymer({
  is: 'settings-a11y-page',

  behaviors: [WebUIListenerBehavior],

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
      value: function() {
        const map = new Map();
        if (settings.routes.CAPTIONS) {
          map.set(settings.routes.CAPTIONS.path, '#captions');
        }
        // <if expr="chromeos">
        if (settings.routes.MANAGE_ACCESSIBILITY) {
          map.set(
              settings.routes.MANAGE_ACCESSIBILITY.path, '#subpage-trigger');
        }
        // </if>
        return map;
      },
    },

    /**
     * Whether to show the link to caption settings.
     * @private {boolean}
     */
    showCaptionSettings_: {
      type: Boolean,
      value: function() {
        return loadTimeData.getBoolean('enableCaptionSettings');
      },
    },

    /**
     * Whether to show OS settings.
     * @private {boolean}
     */
    showOsSettings_: {
      type: Boolean,
      value: function() {
        return loadTimeData.getBoolean('showOSSettings');
      },
    },

    /**
     * Whether the caption settings link opens externally.
     * @private {boolean}
     */
    captionSettingsOpensExternally_: {
      type: Boolean,
      value: function() {
        let opensExternally = false;
        // <if expr="is_macosx">
        opensExternally = true;
        // </if>

        // <if expr="is_win">
        opensExternally = loadTimeData.getBoolean('isWindows10OrNewer');
        // </if>

        return opensExternally;
      },
    },
  },

  /** @override */
  ready: function() {
    this.addWebUIListener(
        'screen-reader-state-changed',
        this.onScreenReaderStateChanged_.bind(this));
    chrome.send('getScreenReaderState');
  },

  /**
   * @private
   * @param {boolean} hasScreenReader Whether a screen reader is enabled.
   */
  onScreenReaderStateChanged_: function(hasScreenReader) {
    // TODO(katie): Remove showExperimentalA11yLabels flag before launch.
    this.showAccessibilityLabelsSetting_ = hasScreenReader &&
        loadTimeData.getBoolean('showExperimentalA11yLabels');
  },

  /** @private */
  onToggleAccessibilityImageLabels_: function() {
    const a11yImageLabelsOn = this.$.a11yImageLabels.checked;
    if (a11yImageLabelsOn) {
      chrome.send('confirmA11yImageLabels');
    }
    chrome.metricsPrivate.recordBoolean(
        'Accessibility.ImageLabels.FromSettings.ToggleSetting',
        a11yImageLabelsOn);
  },

  // <if expr="chromeos">
  /** @private */
  onManageAccessibilityFeaturesTap_: function() {
    settings.navigateTo(settings.routes.MANAGE_ACCESSIBILITY);
  },

  /** @private */
  onManageSystemAccessibilityFeaturesTap_: function() {
    window.location.href = 'chrome://os-settings/manageAccessibility';
  },
  // </if>

  /** private */
  onMoreFeaturesLinkClick_: function() {
    window.open(
        'https://chrome.google.com/webstore/category/collection/accessibility');
  },

  /** @private */
  onCaptionsClick_: function() {
    // Open the system captions dialog for Mac.
    // <if expr="is_macosx">
    settings.CaptionsBrowserProxyImpl.getInstance().openSystemCaptionsDialog();
    // </if>

    // Open the system captions dialog for Windows 10+ or navigate to the
    // caption settings page for older versions of Windows
    // <if expr="is_win">
    if (loadTimeData.getBoolean('isWindows10OrNewer')) {
      settings.CaptionsBrowserProxyImpl.getInstance()
          .openSystemCaptionsDialog();
    } else {
      settings.navigateTo(settings.routes.CAPTIONS);
    }
    // </if>

    // Navigate to the caption settings page for ChromeOS and Linux as they
    // do not have system caption settings.
    // <if expr="chromeos or is_linux">
    settings.navigateTo(settings.routes.CAPTIONS);
    // </if>
  },
});
