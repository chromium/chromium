// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_elements/cr_icon_button/cr_icon_button.js';
import '//resources/cr_elements/cr_lazy_render/cr_lazy_render_lit.js';
import '//resources/cr_elements/cr_icon/cr_icon.js';
import '//resources/cr_elements/icons_lit.html.js';
import '//resources/cr_elements/cr_action_menu/cr_action_menu.js';
import '//resources/cr_elements/cr_lazy_render/cr_lazy_render.js';
import './language_menu.js';

import type {CrActionMenuElement} from '//resources/cr_elements/cr_action_menu/cr_action_menu.js';
import type {CrLazyRenderLitElement} from '//resources/cr_elements/cr_lazy_render/cr_lazy_render_lit.js';
import {WebUiListenerMixinLit} from '//resources/cr_elements/web_ui_listener_mixin_lit.js';
import {assert} from '//resources/js/assert.js';
import {loadTimeData} from '//resources/js/load_time_data.js';
import type {PropertyValues} from '//resources/lit/v3_0/lit.rollup.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';

import {openMenu, ToolbarEvent} from './common.js';
import type {LanguageMenuElement} from './language_menu.js';
import {ReadAloudSettingsChange} from './metrics_browser_proxy.js';
import {ReadAnythingLogger} from './read_anything_logger.js';
import {areVoicesEqual, convertLangOrLocaleForVoicePackManager, isNatural, VoiceClientSideStatusCode} from './voice_language_util.js';
import {getCss} from './voice_selection_menu.css.js';
import {getHtml} from './voice_selection_menu.html.js';

export interface VoiceSelectionMenuElement {
  $: {
    voiceSelectionMenu: CrLazyRenderLitElement<CrActionMenuElement>,
    languageMenu: LanguageMenuElement,
  };
}

interface VoiceDropdownGroup {
  language: string;
  voices: VoiceDropdownItem[];
}

interface VoiceDropdownItem {
  title: string;
  voice: SpeechSynthesisVoice;
  selected: boolean;
  previewPlaying: boolean;
  // This ID is currently just used for testing purposes and does not ensure
  // uniqueness
  id: string;
}

const VoiceSelectionMenuElementBase = WebUiListenerMixinLit(CrLitElement);

export class VoiceSelectionMenuElement extends VoiceSelectionMenuElementBase {
  static get is() {
    return 'voice-selection-menu';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      selectedVoice: {type: Object},
      availableVoices: {type: Array},
      enabledLangs: {type: Array},
      previewVoicePlaying: {type: Object},
      voicePackInstallStatus: {type: Object},
      isSpeechActive: {type: Boolean},
      localeToDisplayName: {type: Object},
      lastDownloadedLang: {type: String},
      showLanguageMenuDialog_: {type: Boolean},
      downloadingMessages_: {type: Boolean},
      voiceGroups_: {type: Object},
    };
  }

  voicePackInstallStatus: {[language: string]: VoiceClientSideStatusCode} = {};
  selectedVoice?: SpeechSynthesisVoice;
  localeToDisplayName: {[lang: string]: string} = {};
  previewVoicePlaying?: SpeechSynthesisVoice;
  enabledLangs: string[] = [];
  availableVoices: SpeechSynthesisVoice[] = [];
  isSpeechActive: boolean = false;
  lastDownloadedLang?: string;

  protected downloadingMessages_: string[] = [];
  protected voiceGroups_: VoiceDropdownGroup[] = [];
  protected showLanguageMenuDialog_: boolean = false;

  private voicePlayingWhenMenuOpened_: boolean = false;
  private readonly spBodyPadding_ = Number.parseInt(
      window.getComputedStyle(document.body)
          .getPropertyValue('--sp-body-padding'),
      10);
  private logger_: ReadAnythingLogger = ReadAnythingLogger.getInstance();


  override willUpdate(changedProperties: PropertyValues<this>) {
    super.willUpdate(changedProperties);

    if (changedProperties.has('voicePackInstallStatus')) {
      this.downloadingMessages_ = this.computeDownloadingMessages_();
    }

    if (changedProperties.has('selectedVoice') ||
        changedProperties.has('availableVoices') ||
        changedProperties.has('enabledLangs') ||
        changedProperties.has('previewVoicePlaying') ||
        changedProperties.has('localeToDisplayName')) {
      this.voiceGroups_ = this.computeVoiceDropdown_();
    }
  }


  onVoiceSelectionMenuClick(targetElement: HTMLElement) {
    this.voicePlayingWhenMenuOpened_ = this.isSpeechActive;

    const menu = this.$.voiceSelectionMenu.get();
    openMenu(menu, targetElement, {
      minX: this.spBodyPadding_,
      maxX: document.body.clientWidth - this.spBodyPadding_,
    });

    // Scroll to the selected voice.
    requestAnimationFrame(() => {
      const selectedItem =
          menu.querySelector<HTMLElement>('.item-invisible-false');
      selectedItem?.scrollIntoViewIfNeeded();
    });
  }

  private computeEnabledVoices_(): SpeechSynthesisVoice[] {
    if (!this.availableVoices || !this.enabledLangs) {
      return [];
    }
    const enablesLangsLowerCase: Set<string> =
        new Set(this.enabledLangs.map(lang => lang.toLowerCase()));
    return this.availableVoices.filter(
        ({lang}) => enablesLangsLowerCase.has(lang.toLowerCase()));
  }

  private getLangDisplayName(lang: string): string {
    const langLower = lang.toLowerCase();
    return this.localeToDisplayName[langLower] || langLower;
  }

  private computeVoiceDropdown_(): VoiceDropdownGroup[] {
    const enabledVoices = this.computeEnabledVoices_();
    if (!enabledVoices) {
      return [];
    }
    const languageToVoices =
        enabledVoices.reduce((languageToDropdownItems, voice) => {
          const dropdownItem: VoiceDropdownItem = {
            title: voice.name,
            voice,
            id: this.stringToHtmlTestId_(voice.name),
            selected: areVoicesEqual(this.selectedVoice, voice),
            previewPlaying: areVoicesEqual(this.previewVoicePlaying, voice),
          };

          const lang = this.getLangDisplayName(voice.lang);

          if (languageToDropdownItems[lang]) {
            languageToDropdownItems[lang].push(dropdownItem);
          } else {
            languageToDropdownItems[lang] = [dropdownItem];
          }

          return languageToDropdownItems;
        }, {} as {[language: string]: VoiceDropdownItem[]});

    for (const lang of Object.keys(languageToVoices)) {
      languageToVoices[lang].sort(voiceQualityRankComparator);
    }

    return Object.entries(languageToVoices).map(([
                                                  language,
                                                  voices,
                                                ]) => ({language, voices}));
  }

  // This ID does not ensure uniqueness and is just used for testing purposes.
  private stringToHtmlTestId_(s: string): string {
    return s.replace(/\s/g, '-').replace(/[()]/g, '');
  }

  protected onVoiceSelectClick_(e: Event) {
    this.logger_.logSpeechSettingsChange(
        ReadAloudSettingsChange.VOICE_NAME_CHANGE);

    const selectedVoice = this.getVoiceItemForEvent_(e).voice;
    this.fire(ToolbarEvent.VOICE, {selectedVoice});
  }

  protected onVoicePreviewClick_(e: Event) {
    // Because the preview button is layered onto the voice-selection button,
    // the onVoiceSelectClick_() listener is also subscribed to this event. This
    // line is to make sure that the voice-selection callback is not triggered.
    e.stopImmediatePropagation();

    const dropdownItem = this.getVoiceItemForEvent_(e);
    this.fire(
        ToolbarEvent.PLAY_PREVIEW,
        // If preview is currently playing, we pass null to indicate the audio
        // should be paused.
        dropdownItem.previewPlaying ? null :
                                      {previewVoice: dropdownItem.voice});
  }

  protected openLanguageMenu_() {
    this.showLanguageMenuDialog_ = true;
  }

  protected onLanguageMenuClose_(event: CustomEvent) {
    event.preventDefault();
    event.stopPropagation();

    this.showLanguageMenuDialog_ = false;
  }

  protected onClose_() {
    this.dispatchEvent(new CustomEvent('voice-menu-close', {
      bubbles: true,
      composed: true,
      detail: {
        voicePlayingWhenMenuOpened: this.voicePlayingWhenMenuOpened_,
      },
    }));
  }

  protected onVoiceMenuKeyDown_(e: KeyboardEvent) {
    const currentElement = e.target as HTMLElement;
    assert(currentElement, 'no key target');
    const targetIsVoiceOption =
        (currentElement.classList.contains('dropdown-voice-selection-button')) ?
        true :
        false;
    const targetIsPreviewButton =
        (currentElement.id === 'preview-icon') ? true : false;

    // For voice options, only handle the right arrow - everything else is
    // default
    if (targetIsVoiceOption && !['ArrowRight'].includes(e.key)) {
      return;
    }
    // For a voice preview, handle up, down and left arrows
    if (targetIsPreviewButton &&
        !['ArrowLeft', 'ArrowUp', 'ArrowDown'].includes(e.key)) {
      return;
    }

    // When the menu first opens, the target is the whole menu.
    // In that case, use default behavior.
    if (!targetIsVoiceOption && !targetIsPreviewButton) return;

    e.preventDefault();

    if (targetIsVoiceOption) {
      // From a voice option, go to its preview button
      const visiblePreviewButton =
          currentElement.querySelector<HTMLElement>('#preview-icon');
      assert(visiblePreviewButton, 'can\'t find preview button');
      visiblePreviewButton!.focus();
    }
    // This action is also handled by the menu itself
    // For left arrow, this takes us to the voice being previewed,
    // For up and down arrows this is combined with the default up/down
    // action, taking us to the next or previous voice.
    currentElement.parentElement!.focus();
  }

  protected previewLabel_(previewPlaying: boolean): string {
    if (previewPlaying) {
      return loadTimeData.getString('stopLabel');
    } else {
      return loadTimeData.getString('previewTooltip');
    }
  }

  protected previewAriaLabel_(previewPlaying: boolean, voiceName: string):
      string {
    let nameSuffix = '';
    if (voiceName.length > 0) {
      nameSuffix = ' ' + voiceName;
    }
    if (previewPlaying) {
      return loadTimeData.getString('stopLabel') + nameSuffix;
    } else {
      return loadTimeData.getStringF(
          'previewVoiceAccessibilityLabel', nameSuffix);
    }
  }

  protected previewIcon_(previewPlaying: boolean): string {
    if (previewPlaying) {
      return 'read-anything-20:stop-circle';
    } else {
      return 'read-anything-20:play-circle';
    }
  }

  private getVoiceItemForEvent_(e: Event): VoiceDropdownItem {
    const groupIndex = Number.parseInt(
        (e.currentTarget as HTMLElement).dataset['groupIndex']!);
    const voiceIndex = Number.parseInt(
        (e.currentTarget as HTMLElement).dataset['voiceIndex']!);

    return this.voiceGroups_[groupIndex].voices[voiceIndex];
  }

  private computeDownloadingMessages_(): string[] {
    return Object.entries(this.voicePackInstallStatus)
        .filter(
            ([_, status]) => status ===
                    VoiceClientSideStatusCode.INSTALLED_AND_UNAVAILABLE ||
                status === VoiceClientSideStatusCode.SENT_INSTALL_REQUEST)
        .map(([lang, _]) => this.getDisplayNameForLocale(lang))
        .filter(possibleName => possibleName.length > 0)
        .map(
            displayName => loadTimeData.getStringF(
                'readingModeVoiceMenuDownloading', displayName));
  }

  private getDisplayNameForLocale(language: string): string {
    const voicePackLang = convertLangOrLocaleForVoicePackManager(language);
    return voicePackLang ? chrome.readingMode.getDisplayNameForLocale(
                               voicePackLang, voicePackLang) :
                           '';
  }
}

function voiceQualityRankComparator(
    voice1: VoiceDropdownItem,
    voice2: VoiceDropdownItem,
    ): number {
  if (isNatural(voice1.voice) && isNatural(voice2.voice)) {
    return 0;
  }

  if (!isNatural(voice1.voice) && !isNatural(voice2.voice)) {
    return 0;
  }

  // voice1 is a Natural voice and voice2 is not
  if (isNatural(voice1.voice)) {
    return -1;
  }

  // voice2 is a Natural voice and voice1 is not
  return 1;
}

declare global {
  interface HTMLElementTagNameMap {
    'voice-selection-menu': VoiceSelectionMenuElement;
  }
}

customElements.define(VoiceSelectionMenuElement.is, VoiceSelectionMenuElement);
