// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'settings-live-caption' is a component for showing Live Caption
 * settings. It appears on the accessibility subpage
 * (chrome://settings/accessibility) on Mac and some versions of Windows and on
 * the captions subpage (chrome://settings/captions) on Linux and other versions
 * of Windows.
 */

import '//resources/cr_elements/cr_button/cr_button.js';
import '//resources/cr_elements/cr_collapse/cr_collapse.js';
import '//resources/cr_elements/cr_icon_button/cr_icon_button.js';
import '../controls/settings_toggle_button.js';

import {WebUiListenerMixinLit} from '//resources/cr_elements/web_ui_listener_mixin_lit.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';
import type {PropertyValues} from '//resources/lit/v3_0/lit.rollup.js';
import type {CaptionsBrowserProxy, LiveCaptionLanguage, LiveCaptionLanguageList} from '/shared/settings/a11y_page/captions_browser_proxy.js';
import {CaptionsBrowserProxyImpl} from '/shared/settings/a11y_page/captions_browser_proxy.js';
import {PrefService} from '/shared/settings/prefs2/pref_service.js';
import {PrefServiceObserverMixinLit} from '/shared/settings/prefs2/pref_service_observer_mixin_lit.js';

import type {SettingsToggleButtonElement} from '../controls/settings_toggle_button.js';
import {loadTimeData} from '../i18n_setup.js';

import {getCss} from './live_caption.css.js';
import {getHtml} from './live_caption.html.js';

// clang-format off

// <if expr="not is_chromeos">
import 'chrome://resources/cr_elements/cr_action_menu/cr_action_menu.js';
import '//resources/cr_elements/cr_lazy_render/cr_lazy_render_lit.js';
import '../languages_page/add_languages_dialog.js';
import './live_translate.js';

import type {CrActionMenuElement} from '//resources/cr_elements/cr_action_menu/cr_action_menu.js';
import type {CrLazyRenderLitElement} from '//resources/cr_elements/cr_lazy_render/cr_lazy_render_lit.js';
import {I18nMixinLit} from 'chrome://resources/cr_elements/i18n_mixin_lit.js';
import {assert} from 'chrome://resources/js/assert.js';
import {focusWithoutInk} from 'chrome://resources/js/focus_without_ink.js';
// </if>
// clang-format on


// <if expr="is_chromeos">
const SettingsLiveCaptionElementBase =
    WebUiListenerMixinLit(PrefServiceObserverMixinLit(CrLitElement));
// </if>
// <if expr="not is_chromeos">
const SettingsLiveCaptionElementBase = WebUiListenerMixinLit(
    PrefServiceObserverMixinLit(I18nMixinLit(CrLitElement)));

export interface SettingsLiveCaptionElement {
  $: {
    menu: CrLazyRenderLitElement<CrActionMenuElement>,
  };
}
// </if>

export class SettingsLiveCaptionElement extends SettingsLiveCaptionElementBase {
  static get is() {
    return 'settings-live-caption';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      isLiveCaptionEnabled_: {type: Boolean},

      /**
       * The subtitle to display under the Live Caption heading. Generally, this
       * is a generic subtitle describing the feature. While the SODA model is
       * being downloading, this displays the download progress.
       */
      enableLiveCaptionSubtitle_: {type: String},
      enableLiveCaptionMultiLanguage_: {type: Boolean},

      // <if expr="not is_chromeos">
      liveCaptionLanguagePref_: {type: Object},
      enableLiveTranslate_: {type: Boolean},
      installedLanguagePacks_: {type: Array},
      availableLanguagePacks_: {type: Array},
      detailLanguage_: {type: Object},
      showAddLanguagesDialog_: {type: Boolean},
      // </if>
    };
  }

  protected accessor isLiveCaptionEnabled_: boolean = false;
  protected accessor enableLiveCaptionSubtitle_: string =
      loadTimeData.getString('captionsEnableLiveCaptionSubtitle');
  protected accessor enableLiveCaptionMultiLanguage_: boolean =
      loadTimeData.getBoolean('enableLiveCaptionMultiLanguage');
  // <if expr="not is_chromeos">
  protected accessor liveCaptionLanguagePref_:
      chrome.settingsPrivate.PrefObject<string>|undefined;
  protected accessor enableLiveTranslate_: boolean =
      loadTimeData.getBoolean('enableLiveTranslate');
  protected accessor installedLanguagePacks_: LiveCaptionLanguageList = [];
  protected accessor availableLanguagePacks_: LiveCaptionLanguageList = [];
  protected accessor detailLanguage_: LiveCaptionLanguage|undefined;
  protected accessor showAddLanguagesDialog_: boolean = false;
  // </if>

  private browserProxy_: CaptionsBrowserProxy =
      CaptionsBrowserProxyImpl.getInstance();

  override connectedCallback() {
    super.connectedCallback();

    this.addPrefObserver<boolean>(
        'accessibility.captions.live_caption_enabled', pref => {
          this.isLiveCaptionEnabled_ = pref.value;
        });
    // <if expr="not is_chromeos">
    this.mirrorPref(
        'accessibility.captions.live_caption_language',
        'liveCaptionLanguagePref_');
    // </if>
  }

  override firstUpdated(changedProperties: PropertyValues<this>) {
    super.firstUpdated(changedProperties);

    // <if expr="not is_chromeos">
    this.browserProxy_.getInstalledLanguagePacks().then(
        (installedLanguagePacks: LiveCaptionLanguageList) => {
          this.installedLanguagePacks_ = installedLanguagePacks;
        });

    this.browserProxy_.getAvailableLanguagePacks().then(
        (availableLanguagePacks: LiveCaptionLanguageList) => {
          this.availableLanguagePacks_ = availableLanguagePacks;
        });
    // </if>

    // <if expr="is_chromeos">
    this.addWebUiListener(
        'soda-download-progress-changed',
        (sodaDownloadProgress: string) =>
            this.onSodaDownloadProgressChanged_(sodaDownloadProgress));
    // </if>
    // <if expr="not is_chromeos">
    this.addWebUiListener(
        'soda-download-progress-changed',
        (sodaDownloadProgress: string, languageCode: string) =>
            this.onSodaDownloadProgressChangedForLanguage_(
                sodaDownloadProgress, languageCode));
    // </if>

    this.browserProxy_.liveCaptionSectionReady();
  }

  /**
   * @return the Live Caption toggle element.
   */
  getLiveCaptionToggle(): SettingsToggleButtonElement {
    return this.shadowRoot.querySelector<SettingsToggleButtonElement>(
        '#liveCaptionToggleButton')!;
  }

  protected computeMoreButtonAriaLabel_(name: string, code: string): string {
    let label = this.i18n('moreActionsFor', name);
    if (this.isDefaultLanguage_(code)) {
      label += ` ${this.i18n('defaultLanguageLabel')}`;
    }
    return label;
  }

  protected onLiveCaptionEnabledChange_(event: Event) {
    const liveCaptionEnabled =
        (event.target as SettingsToggleButtonElement).checked;
    chrome.metricsPrivate.recordBoolean(
        'Accessibility.LiveCaption.EnableFromSettings', liveCaptionEnabled);

    // <if expr="not is_chromeos">
    if (this.installedLanguagePacks_.length === 0) {
      this.installLanguagePacks_([this.liveCaptionLanguagePref_!.value]);
    }
    // </if>
  }

  protected onLiveCaptionMaskOffensiveWordsChange_(event: Event) {
    const liveCaptionMaskOffensiveWords =
        (event.target as SettingsToggleButtonElement).checked;
    chrome.metricsPrivate.recordBoolean(
        'Accessibility.LiveCaption.MaskOffensiveWords',
        liveCaptionMaskOffensiveWords);
  }

  // <if expr="not is_chromeos">
  protected onAddLanguagesClick_(e: Event) {
    e.preventDefault();
    this.showAddLanguagesDialog_ = true;
  }

  protected onAddLanguagesDialogClose_() {
    this.showAddLanguagesDialog_ = false;
    const toFocus = this.shadowRoot.querySelector<HTMLElement>('#addLanguage');
    assert(toFocus);
    focusWithoutInk(toFocus);
  }

  protected onDotsClick_(e: Event) {
    const target = e.currentTarget as HTMLElement;
    const code = target.dataset['code'];
    assert(code);
    const item = this.installedLanguagePacks_.find(p => p.code === code);
    assert(item);
    this.detailLanguage_ = Object.assign({}, item);
    this.$.menu.get().showAt(target);
  }

  protected isDefaultLanguage_(languageCode: string): boolean {
    if (this.liveCaptionLanguagePref_ === undefined) {
      return false;
    }

    return languageCode === this.liveCaptionLanguagePref_.value;
  }

  protected onMakeDefaultClick_() {
    this.$.menu.get().close();
    PrefService.getInstance().setPrefValue(
        'accessibility.captions.live_caption_language',
        this.detailLanguage_!.code);
  }

  protected onRemoveLanguageClick_() {
    if (!this.detailLanguage_) {
      return;
    }

    this.$.menu.get().close();
    this.installedLanguagePacks_ = this.installedLanguagePacks_.filter(
        languagePack => languagePack.code !== this.detailLanguage_!.code);
    this.browserProxy_.removeLanguagePack(this.detailLanguage_.code);

    if (this.installedLanguagePacks_.length === 0) {
      PrefService.getInstance().setPrefValue(
          'accessibility.captions.live_caption_enabled', false);
      return;
    }

    if (!this.installedLanguagePacks_.some(
            languagePack =>
                languagePack.code === this.liveCaptionLanguagePref_!.value)) {
      PrefService.getInstance().setPrefValue(
          'accessibility.captions.live_caption_language',
          this.installedLanguagePacks_[0].code);
    }
  }

  protected onLanguagesAdded_(e: CustomEvent<string[]>) {
    this.installLanguagePacks_(e.detail);
  }

  private installLanguagePacks_(languageCodes: string[]) {
    const newLanguagePacks: LiveCaptionLanguageList = [];
    languageCodes.forEach(languageCode => {
      const languagePackToAdd = this.availableLanguagePacks_.find(
          languagePack => languagePack.code === languageCode);
      if (languagePackToAdd) {
        newLanguagePacks.push(languagePackToAdd);
      }
    });

    this.installedLanguagePacks_ =
        this.installedLanguagePacks_.concat(newLanguagePacks);
    this.browserProxy_.installLanguagePacks(languageCodes);
  }

  protected filterAvailableLanguagePacks_():
      chrome.languageSettingsPrivate.Language[] {
    const filteredLanguagePacks =
        this.availableLanguagePacks_.filter(availableLanguagePack => {
          return !this.installedLanguagePacks_.some(
              installedLanguagePack =>
                  installedLanguagePack.code === availableLanguagePack.code);
        });

    return filteredLanguagePacks.map(
        languagePack => ({
          code: languagePack.code,
          displayName: languagePack.displayName,

          // The native display name for language packs is not shown.
          nativeDisplayName: languagePack.nativeDisplayName,
        }));
  }
  // </if>

  // <if expr="is_chromeos">
  /**
   * Displays SODA download progress in the UI.
   * @param sodaDownloadProgress The message sent from the webui to be displayed
   *     as download progress for Live Caption.
   */
  private onSodaDownloadProgressChanged_(sodaDownloadProgress: string) {
    this.enableLiveCaptionSubtitle_ = sodaDownloadProgress;
  }
  // </if>

  // <if expr="not is_chromeos">
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
      sodaDownloadProgress: string, languageCode: string) {
    if (!this.enableLiveCaptionMultiLanguage_) {
      this.enableLiveCaptionSubtitle_ = sodaDownloadProgress;
      return;
    }

    for (let i = 0; i < this.installedLanguagePacks_.length; i++) {
      const language = this.installedLanguagePacks_[i];
      if (language.code === languageCode) {
        language.downloadProgress = sodaDownloadProgress;
        this.requestUpdate();
        break;
      }
    }
  }
  // </if>
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-live-caption': SettingsLiveCaptionElement;
  }
}

customElements.define(
    SettingsLiveCaptionElement.is, SettingsLiveCaptionElement);
