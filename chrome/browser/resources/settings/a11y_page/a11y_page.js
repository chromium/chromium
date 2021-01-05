// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'settings-a11y-page' is the small section of advanced settings with
 * a link to the web store accessibility page on most platforms, and
 * a subpage with lots of other settings on Chrome OS.
 */
import 'chrome://resources/cr_elements/cr_link_row/cr_link_row.m.js';
import '../controls/settings_toggle_button.m.js';
import '../settings_page/settings_animated_pages.m.js';
import '../settings_shared_css.m.js';

// <if expr="not is_macosx and not chromeos">
import './captions_subpage.m.js';
import '../settings_page/settings_subpage.m.js';
// </if>

import {WebUIListenerBehavior} from 'chrome://resources/js/web_ui_listener_behavior.m.js';
import {html, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {loadTimeData} from '../i18n_setup.js';
import {routes} from '../route.js';
import {Router} from '../router.m.js';

// <if expr="is_win or is_macosx">
import {CaptionsBrowserProxyImpl} from './captions_browser_proxy.js';
// </if>


Polymer({
  is: 'settings-a11y-page',

  _template: html`{__html_template__}`,

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

    // <if expr="not chromeos">
    /** @private */
    enableLiveCaption_: {
      type: Boolean,
      value: function() {
        return loadTimeData.getBoolean('enableLiveCaption');
      },
    },

    /**
     * The subtitle to display under the Live Caption heading. Generally, this
     * is a generic subtitle describing the feature. While the SODA model is
     * being downloading, this displays the download progress.
     * @private
     */
    enableLiveCaptionSubtitle_: {
      type: String,
      value: loadTimeData.getString('captionsEnableLiveCaptionSubtitle'),
    },

    /**
     * Whether to show the focus highlight setting.
     * Depends on feature flag for focus highlight.
     * @private {boolean}
     */
    showFocusHighlightOption_: {
      type: Boolean,
      value: function() {
        return loadTimeData.getBoolean('showFocusHighlightOption');
      }
    },
    // </if>

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
        if (routes.CAPTIONS) {
          map.set(routes.CAPTIONS.path, '#captions');
        }
        return map;
      },
    },

    /**
     * Whether the caption settings link opens externally.
     * @private {boolean}
     */
    captionSettingsOpensExternally_: {
      type: Boolean,
      value() {
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
  ready() {
    this.addWebUIListener(
        'screen-reader-state-changed',
        this.onScreenReaderStateChanged_.bind(this));

    // <if expr="not chromeos">
    this.addWebUIListener(
        'enable-live-caption-subtitle-changed',
        this.onEnableLiveCaptionSubtitleChanged_.bind(this));
    // </if>

    // Enables javascript and gets the screen reader state.
    chrome.send('a11yPageReady');

    if (this.captionSettingsOpensExternally_) {
      // If captions settings open externally, then this page doesn't have a
      // separate captions subpage. Send a captionsSubpageReady notification in
      // order to start observing SODA events.
      chrome.send('captionsSubpageReady');
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

  /**
   * @private
   * @param {!Event} event
   */
  onA11yCaretBrowsingChange_(event) {
    if (event.target.checked) {
      chrome.metricsPrivate.recordUserAction(
          'Accessibility.CaretBrowsing.EnableWithSettings');
    } else {
      chrome.metricsPrivate.recordUserAction(
          'Accessibility.CaretBrowsing.DisableWithSettings');
    }
  },

  /**
   * @private
   * @param {!Event} event
   */
  onA11yImageLabelsChange_(event) {
    const a11yImageLabelsOn = event.target.checked;
    if (a11yImageLabelsOn) {
      chrome.send('confirmA11yImageLabels');
    }
  },

  // <if expr="not chromeos">
  /**
   * @param {!Event} event
   * @private
   */
  onA11yLiveCaptionChange_(event) {
    const a11yLiveCaptionOn = event.target.checked;
    chrome.metricsPrivate.recordBoolean(
        'Accessibility.LiveCaption.EnableFromSettings', a11yLiveCaptionOn);
  },

  /**
   * @private
   * @param {!string} enableLiveCaptionSubtitle The message sent from the webui
   *     to be displayed as a subtitle to Live Captions.
   */
  onEnableLiveCaptionSubtitleChanged_(enableLiveCaptionSubtitle) {
    this.enableLiveCaptionSubtitle_ = enableLiveCaptionSubtitle;
  },

  /**
   * @private
   * @param {!Event} event
   */
  onFocusHighlightChange_(event) {
    chrome.metricsPrivate.recordBoolean(
        'Accessibility.FocusHighlight.ToggleEnabled', event.target.checked);
  },
  // </if>

  // <if expr="chromeos">
  /** @private */
  onManageSystemAccessibilityFeaturesTap_() {
    window.location.href = 'chrome://os-settings/manageAccessibility';
  },
  // </if>

  /** private */
  onMoreFeaturesLinkClick_() {
    window.open(
        'https://chrome.google.com/webstore/category/collection/accessibility');
  },

  /** @private */
  onCaptionsClick_() {
    if (this.captionSettingsOpensExternally_) {
      CaptionsBrowserProxyImpl.getInstance().openSystemCaptionsDialog();
    } else {
      Router.getInstance().navigateTo(routes.CAPTIONS);
    }
  },
});
