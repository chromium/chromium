// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_elements/cr_icon_button/cr_icon_button.js';
import '//resources/cr_elements/cr_lazy_render/cr_lazy_render_lit.js';
import '//resources/cr_elements/cr_icon/cr_icon.js';
import '//resources/cr_elements/icons.html.js';
import '//resources/cr_elements/cr_action_menu/cr_action_menu.js';
import './language_menu.js';

import type {CrActionMenuElement} from '//resources/cr_elements/cr_action_menu/cr_action_menu.js';
import type {CrLazyRenderLitElement} from '//resources/cr_elements/cr_lazy_render/cr_lazy_render_lit.js';
import {WebUiListenerMixinLit} from '//resources/cr_elements/web_ui_listener_mixin_lit.js';
import {assert} from '//resources/js/assert.js';
import {loadTimeData} from '//resources/js/load_time_data.js';
import type {PropertyValues} from '//resources/lit/v3_0/lit.rollup.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';

import type {ShowAtConfigPrefs} from '../content/read_anything_types.js';
import {ToolbarEvent} from '../content/read_anything_types.js';
import type {ToolbarMenu} from '../menus/menu_util.js';
import {openMenu, spinnerDebounceTimeout} from '../shared/common.js';
import {ReadAloudSettingsChange} from '../shared/metrics_browser_proxy.js';
import {ReadAnythingLogger} from '../shared/read_anything_logger.js';

import type {AudioBrowserProxy} from './audio_browser_proxy.js';
import {AudioBrowserProxyImpl} from './audio_browser_proxy.js';
import type {LanguageMenuElement} from './language_menu.js';
import type {NotificationType} from './voice_language_conversions.js';
import type {VoiceDropdownGroup, VoiceDropdownItem} from './voice_menu_display.js';
import {computeDownloadingMessages, computeErrorMessages, computeVoiceDropdown, isVoicePreviewSpinning} from './voice_menu_display.js';
import {VoiceNotificationManager} from './voice_notification_manager.js';
import type {VoiceNotificationListener} from './voice_notification_manager.js';
import {getCss} from './voice_selection_menu.css.js';
import {getHtml} from './voice_selection_menu.html.js';

export interface VoiceSelectionMenuElement {
  $: {
    voiceSelectionMenu: CrLazyRenderLitElement<CrActionMenuElement>,
    languageMenu: LanguageMenuElement,
  };
}

const VoiceSelectionMenuElementBase = WebUiListenerMixinLit(CrLitElement);

export class VoiceSelectionMenuElement extends VoiceSelectionMenuElementBase
    implements VoiceNotificationListener, ToolbarMenu {
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
      currentNotifications_: {type: Object},
      previewVoiceInitiated: {type: Object},
      localeToDisplayName: {type: Object},
      showLanguageMenuDialog_: {type: Boolean},
      downloadingMessages_: {type: Array},
      voiceGroups_: {type: Array},
      nonModal: {type: Boolean},
      errorMessages_: {type: Array},
      webuiRoundedIconsEnabled_: {type: Boolean},
    };
  }

  accessor selectedVoice: SpeechSynthesisVoice|null = null;
  accessor localeToDisplayName: {[lang: string]: string} = {};
  accessor previewVoicePlaying: SpeechSynthesisVoice|null = null;
  accessor enabledLangs: string[] = [];
  accessor availableVoices: SpeechSynthesisVoice[] = [];
  accessor nonModal: boolean = false;

  // The current notifications that should be used in the voice menu.
  private accessor currentNotifications_:
      {[language: string]: NotificationType} = {};

  private accessor previewVoiceInitiated: SpeechSynthesisVoice|null = null;
  protected accessor errorMessages_: string[] = [];
  protected accessor downloadingMessages_: string[] = [];
  protected accessor voiceGroups_: VoiceDropdownGroup[] = [];
  protected accessor showLanguageMenuDialog_: boolean = false;
  protected accessor webuiRoundedIconsEnabled_: boolean =
      loadTimeData.getBoolean('webuiRoundedIconsEnabled');

  private readonly spBodyPadding_ = Number.parseInt(
      window.getComputedStyle(document.body)
          .getPropertyValue('--sp-body-padding'),
      10);
  private logger_: ReadAnythingLogger = ReadAnythingLogger.getInstance();
  private notificationManager_ = VoiceNotificationManager.getInstance();
  private audioBrowserProxy_: AudioBrowserProxy =
      AudioBrowserProxyImpl.getInstance();

  override willUpdate(changedProperties: PropertyValues<this>) {
    super.willUpdate(changedProperties);

    if (changedProperties.has('previewVoicePlaying') &&
        (this.previewVoicePlaying !== this.previewVoiceInitiated)) {
      // When the preview stops, the voice is set to null in app.ts, so
      // we should update the preview voice to null here as well to clear the
      // voice.
      this.previewVoiceInitiated = this.previewVoicePlaying;
    }
    const changedPrivateProperties =
        changedProperties as Map<PropertyKey, unknown>;

    if (changedProperties.has('selectedVoice') ||
        changedProperties.has('availableVoices') ||
        changedProperties.has('enabledLangs') ||
        changedPrivateProperties.has('previewVoiceInitiated') ||
        changedProperties.has('previewVoicePlaying') ||
        changedProperties.has('localeToDisplayName')) {
      this.voiceGroups_ = this.computeVoiceDropdown_();
    }

    if (changedPrivateProperties.has('currentNotifications_')) {
      this.errorMessages_ = this.computeErrorMessages_();
      this.downloadingMessages_ = this.computeDownloadingMessages_();
    }
  }

  notify(type: NotificationType, language?: string) {
    if (!language) {
      return;
    }
    this.currentNotifications_ = {
      ...this.currentNotifications_,
      [language]: type,
    };
  }

  open(anchor: HTMLElement, showAtConfig?: ShowAtConfigPrefs) {
    showAtConfig = {
      ...showAtConfig,
      maxX:
          document.body.clientWidth - this.spBodyPadding_ - anchor.clientWidth,
    };
    this.onVoiceSelectionMenuClick(anchor, showAtConfig);
  }

  close() {
    this.$.voiceSelectionMenu.get().close();
  }

  onVoiceSelectionMenuClick(
      targetElement: HTMLElement, showAtConfig?: ShowAtConfigPrefs) {
    const showAt = {
      minX: this.spBodyPadding_,
      maxX: document.body.clientWidth - this.spBodyPadding_,
      ...(showAtConfig || {}),
    };
    this.notificationManager_.addListener(this);

    const menu = this.$.voiceSelectionMenu.get();
    openMenu(menu, targetElement, showAt, this.onMenuShown.bind(this));
  }

  private onMenuShown() {
    this.fire(ToolbarEvent.VOICE_MENU_OPEN);
    const selectedItem =
        this.$.voiceSelectionMenu.get().querySelector<HTMLElement>(
            '.item-hidden-false.check-mark');
    selectedItem?.scrollIntoViewIfNeeded();
  }

  protected voiceItemTabIndex_(groupIndex: number, voiceIndex: number) {
    return (groupIndex + voiceIndex) === 0 ? 0 : -1;
  }

  private computeVoiceDropdown_(): VoiceDropdownGroup[] {
    const {groups} = computeVoiceDropdown({
      availableVoices: this.availableVoices,
      enabledLangs: this.enabledLangs,
      selectedVoice: this.selectedVoice,
      previewVoicePlaying: this.previewVoicePlaying,
      previewVoiceInitiated: this.previewVoiceInitiated,
      localeToDisplayName: this.localeToDisplayName,
    });
    return groups;
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

    // Set a small timeout to ensure we're not showing the spinner too
    // frequently. If speech starts fairly quickly after the button is
    // pressed, there's no need for a spinner. This timeout should only be
    // set if the preview is starting, not when a preview is stopped.
    if (!dropdownItem.previewActuallyPlaying) {
      setTimeout(() => {
        this.previewVoiceInitiated = dropdownItem.voice;
      }, spinnerDebounceTimeout);
    }
    this.fire(
        ToolbarEvent.PLAY_PREVIEW,
        // If preview is currently playing, we pass null to indicate the audio
        // should be paused.
        {
          previewVoice:
              dropdownItem.previewActuallyPlaying ? null : dropdownItem.voice,
        },
    );
  }

  protected onLanguageMenuClick_() {
    this.showLanguageMenuDialog_ = true;
    this.fire(ToolbarEvent.LANGUAGE_MENU_OPEN);
  }

  protected onLanguageMenuClose_(event: CustomEvent<void>) {
    event.preventDefault();
    event.stopPropagation();

    this.showLanguageMenuDialog_ = false;
    this.fire(ToolbarEvent.LANGUAGE_MENU_CLOSE);
  }

  protected onClose_() {
    this.notificationManager_.removeListener(this);
    this.currentNotifications_ = {};
    this.fire(ToolbarEvent.VOICE_MENU_CLOSE);
  }

  private shouldAllowPropagation_(
      e: KeyboardEvent, currentElement: HTMLElement): boolean {
    // Always allow propagation for keys other than Tab.
    if (e.key !== 'Tab') {
      return true;
    }

    // If the shift key is not pressed with the tab, only allow propagation on
    // the language menu button.
    if (!e.shiftKey) {
      return currentElement.classList.contains('language-menu-button');
    }

    // In the case that shift is pressed, only allow propagation on the first
    // voice option.
    const targetIsVoiceOption =
        currentElement.classList.contains('dropdown-voice-selection-button');
    return targetIsVoiceOption &&
        Number.parseInt(currentElement.dataset['groupIndex']!) === 0 &&
        Number.parseInt(currentElement.dataset['voiceIndex']!) === 0;
  }

  protected onVoiceMenuKeydown_(e: KeyboardEvent) {
    const currentElement = e.target as HTMLElement;
    assert(currentElement, 'no key target');

    // Allowing propagation on Tab closes the menu. We want to stop that
    // propagation unless we're tabbing forward on the last item or tabbing
    // backward on the first item.
    if (!this.shouldAllowPropagation_(e, currentElement)) {
      e.stopImmediatePropagation();
      return;
    }

    const targetIsVoiceOption =
        currentElement.classList.contains('dropdown-voice-selection-button');
    const targetIsPreviewButton = currentElement.id === 'preview-icon';

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
    if (!targetIsVoiceOption && !targetIsPreviewButton) {
      return;
    }

    e.preventDefault();

    if (targetIsVoiceOption) {
      // From a voice option, go to its preview button
      const visiblePreviewButton =
          currentElement.querySelector<HTMLElement>('#preview-icon');
      assert(visiblePreviewButton, 'can\'t find preview button');
      visiblePreviewButton.focus();
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

  protected hideSpinner_(voiceDropdown: VoiceDropdownItem): boolean {
    return !isVoicePreviewSpinning(voiceDropdown);
  }

  protected voiceLabel_(selected: boolean, voiceName: string) {
    const selectedPrefix = selected ? loadTimeData.getString('selected') : '';
    return `${selectedPrefix} ${
        loadTimeData.getStringF(
            'readingModeLanguageMenuItemLabel', voiceName)}`;
  }

  protected shouldDisableButton_(voiceDropdown: VoiceDropdownItem) {
    return isVoicePreviewSpinning(voiceDropdown);
  }

  protected previewIcon_(previewInitiated: boolean): string {
    if (previewInitiated) {
      return loadTimeData.getBoolean('webuiRoundedIconsEnabled') ?
          'read-anything-20:stop-circle' :
          'read-anything-20:stop-circle-old';
    } else {
      return loadTimeData.getBoolean('webuiRoundedIconsEnabled') ?
          'read-anything-20:play-circle' :
          'read-anything-20:play-circle-old';
    }
  }

  private getVoiceItemForEvent_(e: Event): VoiceDropdownItem {
    const groupIndex = Number.parseInt(
        (e.currentTarget as HTMLElement).dataset['groupIndex']!);
    const voiceIndex = Number.parseInt(
        (e.currentTarget as HTMLElement).dataset['voiceIndex']!);

    return this.voiceGroups_[groupIndex]!.voices[voiceIndex]!;
  }

  private computeErrorMessages_(): string[] {
    return computeErrorMessages(
        this.currentNotifications_, this.audioBrowserProxy_);
  }

  private computeDownloadingMessages_(): string[] {
    return computeDownloadingMessages(
        this.currentNotifications_, this.audioBrowserProxy_);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'voice-selection-menu': VoiceSelectionMenuElement;
  }
}

customElements.define(VoiceSelectionMenuElement.is, VoiceSelectionMenuElement);
