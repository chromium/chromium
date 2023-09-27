// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'settings-live-caption' is a component for showing Live Caption
 * settings in chrome://os-settings/audioAndCaptions and has been forked from
 * the equivalent Browser Settings UI (in chrome://settings/captions).
 */

import '//resources/cr_elements/cr_shared_style.css.js';
import '/shared/settings/controls/settings_toggle_button.js';

import {WebUiListenerMixin} from '//resources/cr_elements/web_ui_listener_mixin.js';
import {loadTimeData} from '//resources/js/load_time_data.js';
import {PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {CaptionsBrowserProxy, CaptionsBrowserProxyImpl} from '/shared/settings/a11y_page/captions_browser_proxy.js';
import {SettingsToggleButtonElement} from '/shared/settings/controls/settings_toggle_button.js';
import {PrefsMixin} from 'chrome://resources/cr_components/settings_prefs/prefs_mixin.js';

import {getTemplate} from './live_caption_section.html.js';

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

      /**
       * The subtitle to display under the Live Caption heading. Generally, this
       * is a generic subtitle describing the feature. While the SODA model is
       * being downloading, this displays the download progress.
       */
      enableLiveCaptionSubtitle_: {
        type: String,
        value: loadTimeData.getString('captionsEnableLiveCaptionSubtitle'),
      },

      enableLiveCaptionMultiLanguage_: {
        type: Boolean,
        value: function() {
          return loadTimeData.getBoolean('enableLiveCaptionMultiLanguage');
        },
      },
    };
  }

  private browserProxy_: CaptionsBrowserProxy =
      CaptionsBrowserProxyImpl.getInstance();
  private enableLiveCaptionSubtitle_: string;
  private enableLiveCaptionMultiLanguage_: boolean;

  override ready(): void {
    super.ready();
    this.addWebUiListener(
        'soda-download-progress-changed',
        (sodaDownloadProgress: string) =>
            this.onSodaDownloadProgressChanged_(sodaDownloadProgress));
    this.browserProxy_.liveCaptionSectionReady();
  }

  /**
   * @return the Live Caption toggle element.
   */
  getLiveCaptionToggle(): SettingsToggleButtonElement {
    return this.shadowRoot!.querySelector<SettingsToggleButtonElement>(
        '#liveCaptionToggleButton')!;
  }

  private onLiveCaptionEnabledChanged_(event: Event): void {
    const liveCaptionEnabled =
        (event.target as SettingsToggleButtonElement).checked;
    chrome.metricsPrivate.recordBoolean(
        'Accessibility.LiveCaption.EnableFromSettings', liveCaptionEnabled);
  }

  /**
   * Displays SODA download progress in the UI.
   * @param sodaDownloadProgress The message sent from the webui to be displayed
   *     as download progress for Live Caption.
   */
  private onSodaDownloadProgressChanged_(sodaDownloadProgress: string): void {
    this.enableLiveCaptionSubtitle_ = sodaDownloadProgress;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-live-caption': SettingsLiveCaptionElement;
  }
}

customElements.define(
    SettingsLiveCaptionElement.is, SettingsLiveCaptionElement);
