// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'settings-live-caption' is a component for showing Live Caption
 * settings. It appears on the accessibility subpage
 * (chrome://settings/accessibility) on Mac and some versions of Windows and on
 * the captions subpage (chrome://settings/captions) on Linux, ChromeOS, and
 * other versions of Windows.
 */

import '//resources/cr_elements/cr_shared_style.css.js';
import '../controls/settings_toggle_button.js';
import '../settings_shared.css.js';

import {WebUiListenerMixin} from '//resources/cr_elements/web_ui_listener_mixin.js';
import {PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {SettingsToggleButtonElement} from '../controls/settings_toggle_button.js';
import {loadTimeData} from '../i18n_setup.js';
import {PrefsMixin} from '../prefs/prefs_mixin.js';

import {getTemplate} from './live_caption_section.html.js';


/**
 * |name| is the display name of a language, ex. German.
 * |code| is the language code, ex. de-DE.
 * |downloadProgress| is the display-friendly download progress as the language
 *     model is being downloaded.
 */
interface LiveCaptionLanguage {
  displayName: string;
  code: string;
  downloadProgress: string;
}

type LiveCaptionLanguageList = LiveCaptionLanguage[];

const SettingsLiveCaptionElementBase =
    WebUiListenerMixin(PrefsMixin(PolymerElement));

export class SettingsLiveCaptionElement extends SettingsLiveCaptionElementBase {
  static get is() {
    return 'settings-live-caption';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      prefs: {
        type: Object,
        notify: true,
      },

      enableLiveCaptionMultiLanguage_: {
        type: Boolean,
        value: function() {
          return loadTimeData.getBoolean('enableLiveCaptionMultiLanguage');
        },
      },

      /**
       * The subtitle to display under the Live Caption heading. Generally, this
       * is a generic subtitle describing the feature. While the SODA model is
       * being downloading, this displays the download progress.
       */
      enableLiveCaptionSubtitle_: {
        type: String,
        value: loadTimeData.getString('captionsEnableLiveCaptionSubtitle'),
      },

      /**
       * List of languages available for Live Caption.
       */
      liveCaptionLanguages_: {
        type: Array,
        value() {
          return [
            {
              displayName:
                  loadTimeData.getString('sodaLanguageDisplayNameEnglish'),
              code: loadTimeData.getString('sodaLanguageCodeEnglish'),
              downloadProgress: '',
            },
            {
              displayName:
                  loadTimeData.getString('sodaLanguageDisplayNameFrench'),
              code: loadTimeData.getString('sodaLanguageCodeFrench'),
              downloadProgress: '',
            },
            {
              displayName:
                  loadTimeData.getString('sodaLanguageDisplayNameGerman'),
              code: loadTimeData.getString('sodaLanguageCodeGerman'),
              downloadProgress: '',
            },
            {
              displayName:
                  loadTimeData.getString('sodaLanguageDisplayNameItalian'),
              code: loadTimeData.getString('sodaLanguageCodeItalian'),
              downloadProgress: '',
            },
            {
              displayName:
                  loadTimeData.getString('sodaLanguageDisplayNameJapanese'),
              code: loadTimeData.getString('sodaLanguageCodeJapanese'),
              downloadProgress: '',
            },
            {
              displayName:
                  loadTimeData.getString('sodaLanguageDisplayNameSpanish'),
              code: loadTimeData.getString('sodaLanguageCodeSpanish'),
              downloadProgress: '',
            },
          ];
        },
      },
    };
  }

  private enableLiveCaptionMultiLanguage_: boolean;
  private enableLiveCaptionSubtitle_: string;
  private liveCaptionLanguages_: LiveCaptionLanguageList;

  override ready() {
    super.ready();

    this.addWebUiListener(
        'soda-download-progress-changed',
        (sodaDownloadProgress: string, languageCode: string) =>
            this.onSodaDownloadProgressChanged_(
                sodaDownloadProgress, languageCode));
    chrome.send('liveCaptionSectionReady');
  }

  /**
   * @return the Live Caption toggle element.
   */
  getLiveCaptionToggle(): SettingsToggleButtonElement {
    return this.shadowRoot!.querySelector<SettingsToggleButtonElement>(
        '#liveCaptionToggleButton')!;
  }

  private onLiveCaptionEnabledChanged_(event: Event) {
    const liveCaptionEnabled =
        (event.target as SettingsToggleButtonElement).checked;
    chrome.metricsPrivate.recordBoolean(
        'Accessibility.LiveCaption.EnableFromSettings', liveCaptionEnabled);
  }

  /**
   * Displays SODA download progress in the UI. When the language UI is visible,
   * which occurs when the kLiveCaptionMultiLanguage feature is enabled and when
   * the kLiveCaptionEnabled pref is true, download progress should appear next
   * to the selected language. Otherwise, the download progress appears as a
   * subtitle below the Live Caption toggle.
   * @param sodaDownloadProgress The message sent from the webui to be displayed
   *     as download progress for Live Caption.
   * @param languageCode The language code indicating which language pack the
   *     message applies to.
   */
  private onSodaDownloadProgressChanged_(
      sodaDownloadProgress: string, languageCode: string) {
    if (this.enableLiveCaptionMultiLanguage_) {
      for (let i = 0; i < this.liveCaptionLanguages_.length; i++) {
        const language = this.liveCaptionLanguages_[i];
        if (language.code === languageCode) {
          language.downloadProgress = sodaDownloadProgress;
          this.notifyPath('liveCaptionLanguages_.' + i + '.downloadProgress');
          return;
        }
      }
    } else {
      this.enableLiveCaptionSubtitle_ = sodaDownloadProgress;
    }
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-live-caption': SettingsLiveCaptionElement;
  }
}

customElements.define(
    SettingsLiveCaptionElement.is, SettingsLiveCaptionElement);
