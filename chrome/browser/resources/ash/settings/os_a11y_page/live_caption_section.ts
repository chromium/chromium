// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'settings-live-caption' is a component for showing Live Caption
 * settings in chrome://os-settings/audioAndCaptions and has been forked from
 * the equivalent Browser Settings UI (in chrome://settings/captions).
 */


import 'chrome://resources/ash/common/cr_elements/cr_shared_style.css.js';
import 'chrome://resources/ash/common/cr_elements/cr_action_menu/cr_action_menu.js';
import 'chrome://resources/ash/common/cr_elements/cr_lazy_render/cr_lazy_render.js';
import '../controls/settings_toggle_button.js';
import '../os_languages_page/add_items_dialog.js';
import './live_translate_section.js';

import {CaptionsBrowserProxy, CaptionsBrowserProxyImpl, LiveCaptionLanguage, LiveCaptionLanguageList} from '/shared/settings/a11y_page/captions_browser_proxy.js';
import {PrefsMixin} from '/shared/settings/prefs/prefs_mixin.js';
import type {CrActionMenuElement} from 'chrome://resources/ash/common/cr_elements/cr_action_menu/cr_action_menu.js';
import type {CrLazyRenderElement} from 'chrome://resources/ash/common/cr_elements/cr_lazy_render/cr_lazy_render.js';
import {I18nMixin} from 'chrome://resources/ash/common/cr_elements/i18n_mixin.js';
import {ListPropertyUpdateMixin} from 'chrome://resources/ash/common/cr_elements/list_property_update_mixin.js';
import {WebUiListenerMixin} from 'chrome://resources/ash/common/cr_elements/web_ui_listener_mixin.js';
import {assert} from 'chrome://resources/js/assert.js';
import {focusWithoutInk} from 'chrome://resources/js/focus_without_ink.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {DomRepeatEvent, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {SettingsToggleButtonElement} from '../controls/settings_toggle_button.js';
import type {Item} from '../os_languages_page/add_items_dialog.js';
import type {LanguageHelper, LanguagesModel} from '../os_languages_page/languages_types.js';

import {getTemplate} from './live_caption_section.html.js';

const SettingsLiveCaptionElementBase = WebUiListenerMixin(
    ListPropertyUpdateMixin(PrefsMixin(I18nMixin(PolymerElement))));

export interface SettingsLiveCaptionElement {
  $: {
    menu: CrLazyRenderElement<CrActionMenuElement>,
  };
}

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

      /**
       * Read-only reference to the languages model provided by the
       * 'settings-languages' instance.
       */
      languages: {
        type: Object,
      },

      languageHelper: Object,

      enableLiveTranslate_: {
        type: Boolean,
        value: function() {
          return loadTimeData.getBoolean('enableLiveTranslate');
        },
      },

      installedLanguagePacks_: {
        type: Array,
        value: () => [],
      },

      availableLanguagePacks_: {
        type: Array,
        value: () => [],
      },

      /**
       * The language to display the details for.
       */
      detailLanguage_: Object,

      showAddLanguagesDialog_: Boolean,
    };
  }

  languages: LanguagesModel;
  languageHelper: LanguageHelper;

  private availableLanguagePacks_: LiveCaptionLanguageList;
  private browserProxy_: CaptionsBrowserProxy =
      CaptionsBrowserProxyImpl.getInstance();
  private enableLiveCaptionSubtitle_: string;
  private enableLiveCaptionMultiLanguage_: boolean;
  private enableLiveTranslate_: boolean;
  private installedLanguagePacks_: LiveCaptionLanguageList;
  private detailLanguage_?: LiveCaptionLanguage;
  private showAddLanguagesDialog_: boolean;

  override ready(): void {
    super.ready();
    this.browserProxy_.getInstalledLanguagePacks().then(
        (installedLanguagePacks: LiveCaptionLanguageList) => {
          this.installedLanguagePacks_ = installedLanguagePacks;
        });

    this.browserProxy_.getAvailableLanguagePacks().then(
        (availableLanguagePacks: LiveCaptionLanguageList) => {
          this.availableLanguagePacks_ = availableLanguagePacks;
        });
    this.addWebUiListener(
        'soda-download-progress-changed',
        (sodaDownloadProgress: string, languageCode: string) =>
            this.onSodaDownloadProgressChangedForLanguage_(
                sodaDownloadProgress, languageCode));
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
    if (this.installedLanguagePacks_.length === 0) {
      this.installLanguagePacks_(
          [this.getPref('accessibility.captions.live_caption_language').value]);
    }
  }

  private onAddLanguagesClick_(e: Event): void {
    e.preventDefault();
    this.showAddLanguagesDialog_ = true;
  }

  private onAddLanguagesDialogClose_(): void {
    this.showAddLanguagesDialog_ = false;
    const toFocus = this.shadowRoot!.querySelector<HTMLElement>('#addLanguage');
    assert(toFocus);
    focusWithoutInk(toFocus);
  }


  private onDotsClick_(e: DomRepeatEvent<LiveCaptionLanguage>): void {
    this.detailLanguage_ = Object.assign({}, e.model.item);
    this.$.menu.get().showAt(e.target as HTMLElement);
  }

  private isDefaultLanguage_(languageCode: string): boolean {
    if (this.prefs === undefined) {
      return false;
    }

    return languageCode ===
        this.prefs.accessibility.captions.live_caption_language.value;
  }

  private onMakeDefaultClick_(): void {
    this.$.menu.get().close();
    this.setPrefValue(
        'accessibility.captions.live_caption_language',
        this.detailLanguage_!.code);
  }

  private onRemoveLanguageClick_(): void {
    if (!this.detailLanguage_) {
      return;
    }
    this.$.menu.get().close();
    this.installedLanguagePacks_ = this.installedLanguagePacks_.filter(
        languagePack => languagePack.code !== this.detailLanguage_!.code);
    this.browserProxy_.removeLanguagePack(this.detailLanguage_!.code);

    if (this.installedLanguagePacks_.length === 0) {
      this.setPrefValue('accessibility.captions.live_caption_enabled', false);
      return;
    }

    const liveCapLanguage =
        this.getPref('accessibility.captions.live_caption_language').value;
    if (!this.installedLanguagePacks_.some(
            languagePack => languagePack.code === liveCapLanguage)) {
      this.setPrefValue(
          'accessibility.captions.live_caption_language',
          this.installedLanguagePacks_[0].code);
    }
  }

  private onLanguagesAdded_(e: CustomEvent<string[]>): void {
    this.installLanguagePacks_(e.detail);
  }

  private installLanguagePacks_(languageCodes: string[]): void {
    const newLanguagePacks: LiveCaptionLanguageList = [];
    const newLanguageCodes: string[] = [];
    languageCodes.forEach(languageCode => {
      const languagePackToAdd = this.availableLanguagePacks_.find(
          languagePack => languagePack.code === languageCode);
      if (languagePackToAdd) {
        newLanguagePacks.push(languagePackToAdd);
        newLanguageCodes.push(languageCode);
      }
    });
    this.updateList(
        'installedLanguagePacks_', item => item.code,
        this.installedLanguagePacks_.concat(newLanguagePacks));
    this.browserProxy_.installLanguagePacks(newLanguageCodes);

  }

  private getDisplayText_(language: chrome.languageSettingsPrivate.Language):
      string {
    let displayText = language.displayName;
    // If the native name is different, add it.
    if (language.displayName !== language.nativeDisplayName) {
      displayText += ' - ' + language.nativeDisplayName;
    }
    return displayText;
  }

  private getLiveCaptionLanguages_(): Item[] {
    const installable = this.availableLanguagePacks_.filter(language => {
      return !this.installedLanguagePacks_.some(
          installedLanguagePack =>
              installedLanguagePack.code === language.code);
    });
    return installable.map(
        language => ({
          id: language.code,
          name: this.getDisplayText_(language),
          searchTerms: [language.displayName, language.nativeDisplayName],
          disabledByPolicy: false,
        }));
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
  private onSodaDownloadProgressChangedForLanguage_(
      sodaDownloadProgress: string, languageCode: string): void {
    if (!this.enableLiveCaptionMultiLanguage_) {
      this.enableLiveCaptionSubtitle_ = sodaDownloadProgress;
      return;
    }

    for (let i = 0; i < this.installedLanguagePacks_.length; i++) {
      const language = this.installedLanguagePacks_[i];
      if (language.code === languageCode) {
        language.downloadProgress = sodaDownloadProgress;
        this.notifyPath('installedLanguagePacks_.' + i + '.downloadProgress');
        break;
      }
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
