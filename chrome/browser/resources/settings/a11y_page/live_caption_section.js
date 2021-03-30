// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'settings-live-caption' is a component for showing Live Caption
 * settings. It appears on the accessibility subpage
 * (chrome://settings/accessibility) on Mac and some versions of Windows and on
 * the captions subpage (chrome://settings/captions) on Linux, ChromeOS, and
 * other versions of Windows.
 */

import '//resources/cr_elements/shared_style_css.m.js';
import '../controls/settings_toggle_button.js';
import '../settings_shared_css.js';

import {I18nBehavior} from '//resources/js/i18n_behavior.m.js';
import {WebUIListenerBehavior} from '//resources/js/web_ui_listener_behavior.m.js';
import {html, Polymer} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {loadTimeData} from '../i18n_setup.js';
import {PrefsBehavior} from '../prefs/prefs_behavior.js';


/**
 * |name| is the display name of a language, ex. German.
 * |code| is the language code, ex. de-DE.
 * |downloadProgress| is the display-friendly download progress as the language
 *     model is being downloaded.
 * @typedef {{
 *   display_name: string,
 *   code: string,
 *   downloadProgress: string,
 * }}
 */
let LiveCaptionLanguage;

/**
 * @typedef {!Array<!LiveCaptionLanguage>}
 */
let LiveCaptionLanguageList;

Polymer({
  is: 'settings-live-caption',

  _template: html`{__html_template__}`,

  behaviors: [
    I18nBehavior,
    WebUIListenerBehavior,
    PrefsBehavior,
  ],

  properties: {
    prefs: {
      type: Object,
      notify: true,
    },

    /** @private */
    enableLiveCaptionMultiLanguage_: {
      type: Boolean,
      value: function() {
        return loadTimeData.getBoolean('enableLiveCaptionMultiLanguage');
      }
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
     * List of languages available for Live Caption.
     * @type {!LiveCaptionLanguageList}
     * @private
     */
    liveCaptionLanguages_: {
      type: Array,
      value() {
        return [
          {
            display_name:
                loadTimeData.getString('sodaLanguageDisplayNameEnglish'),
            code: loadTimeData.getString('sodaLanguageCodeEnglish'),
            downloadProgress: '',
          },
          {
            display_name:
                loadTimeData.getString('sodaLanguageDisplayNameFrench'),
            code: loadTimeData.getString('sodaLanguageCodeFrench'),
            downloadProgress: '',
          },
          {
            display_name:
                loadTimeData.getString('sodaLanguageDisplayNameGerman'),
            code: loadTimeData.getString('sodaLanguageCodeGerman'),
            downloadProgress: '',
          },
          {
            display_name:
                loadTimeData.getString('sodaLanguageDisplayNameItalian'),
            code: loadTimeData.getString('sodaLanguageCodeItalian'),
            downloadProgress: '',
          },
          {
            display_name:
                loadTimeData.getString('sodaLanguageDisplayNameJapanese'),
            code: loadTimeData.getString('sodaLanguageCodeJapanese'),
            downloadProgress: '',
          },
          {
            display_name:
                loadTimeData.getString('sodaLanguageDisplayNameSpanish'),
            code: loadTimeData.getString('sodaLanguageCodeSpanish'),
            downloadProgress: '',
          },
        ];
      },
    },
  },

  /** @override */
  ready() {
    this.addWebUIListener(
        'soda-download-progress-changed',
        this.onSodaDownloadProgressChanged_.bind(this));
    chrome.send('liveCaptionSectionReady');
  },

  /**
   * Returns the Live Caption toggle element.
   * @return {?CrToggleElement}
   */
  getLiveCaptionToggle() {
    return /** @type {?CrToggleElement} */ (
        this.$$('#liveCaptionToggleButton'));
  },

  /**
   * @param {!Event} event
   * @private
   */
  onLiveCaptionEnabledChanged_(event) {
    const liveCaptionEnabled = event.target.checked;
    chrome.metricsPrivate.recordBoolean(
        'Accessibility.LiveCaption.EnableFromSettings', liveCaptionEnabled);
  },

  /**
   * Displays SODA download progress in the UI. When the language UI is visible,
   * which occurs when the kLiveCaptionMultiLanguage feature is enabled and when
   * the kLiveCaptionEnabled pref is true, download progress should appear next
   * to the selected language. Otherwise, the download progress appears as a
   * subtitle below the Live Caption toggle.
   * @param {!string} sodaDownloadProgress The message sent from the webui
   *     to be displayed as download progress for Live Caption.
   * @private
   */
  onSodaDownloadProgressChanged_(sodaDownloadProgress) {
    if (this.enableLiveCaptionMultiLanguage_ &&
        this.getPref('accessibility.captions.live_caption_enabled').value) {
      for (let i = 0; i < this.liveCaptionLanguages_.length; i++) {
        const language = this.liveCaptionLanguages_[i];
        if (language.code ===
            this.getPref('accessibility.captions.live_caption_language')
                .value) {
          language.downloadProgress = sodaDownloadProgress;
          this.notifyPath('liveCaptionLanguages_.' + i + '.downloadProgress');
          return;
        }
      }
    } else {
      this.enableLiveCaptionSubtitle_ = sodaDownloadProgress;
    }
  },
});
