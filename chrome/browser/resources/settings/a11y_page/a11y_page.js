// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'settings-a11y-page' is the small section of advanced settings with
 * a link to the web store accessibility page on most platforms, and
 * a subpage with lots of other settings on Chrome OS.
 */
import 'chrome://resources/cr_elements/cr_link_row/cr_link_row.js';
import '../controls/settings_toggle_button.js';
import '../settings_page/settings_animated_pages.js';
import '../settings_shared_css.js';

// <if expr="not is_macosx and not chromeos">
import './captions_subpage.js';
import '../settings_page/settings_subpage.js';
// </if>

// <if expr="is_win or is_macosx">
import './live_caption_section.js';
// </if>

import {WebUIListenerBehavior} from 'chrome://resources/js/web_ui_listener_behavior.m.js';
import {html, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {loadTimeData} from '../i18n_setup.js';
import {routes} from '../route.js';
import {Router} from '../router.js';

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

    // Enables javascript and gets the screen reader state.
    chrome.send('a11yPageReady');
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
        'https://chrome.google.com/webstore/category/collection/3p_accessibility_extensions');
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
