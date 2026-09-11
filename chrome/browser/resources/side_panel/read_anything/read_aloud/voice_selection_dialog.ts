// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_elements/cr_button/cr_button.js';
import '//resources/cr_elements/cr_dialog/cr_dialog.js';
import '//resources/cr_elements/cr_icon_button/cr_icon_button.js';
import '//resources/cr_elements/cr_radio_group/cr_radio_group.js';
import '//resources/cr_elements/cr_radio_button/cr_radio_button.js';
import '../app/icons.html.js';

import type {CrDialogElement} from '//resources/cr_elements/cr_dialog/cr_dialog.js';
import type {CrRadioGroupElement} from '//resources/cr_elements/cr_radio_group/cr_radio_group.js';
import {loadTimeData} from '//resources/js/load_time_data.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';
import type {PropertyValues} from '//resources/lit/v3_0/lit.rollup.js';

import {ToolbarEvent} from '../content/read_anything_types.js';
import {spinnerDebounceTimeout} from '../shared/common.js';
import {ReadAloudSettingsChange} from '../shared/metrics_browser_proxy.js';
import {ReadAnythingLogger} from '../shared/read_anything_logger.js';

import type {AudioBrowserProxy} from './audio_browser_proxy.js';
import {AudioBrowserProxyImpl} from './audio_browser_proxy.js';
import {areVoicesEqual} from './voice_language_conversions.js';
import type {NotificationType} from './voice_language_conversions.js';
import type {VoiceDropdownGroup, VoiceDropdownItem} from './voice_menu_display.js';
import {computeDownloadingMessages, computeErrorMessages, computeVoiceDropdown, isVoicePreviewSpinning} from './voice_menu_display.js';
import type {VoiceNotificationListener} from './voice_notification_manager.js';
import {VoiceNotificationManager} from './voice_notification_manager.js';
import {getCss} from './voice_selection_dialog.css.js';
import {getHtml} from './voice_selection_dialog.html.js';

export interface VoiceSelectionDialogElement {
  $: {
    voiceSelectionDialog: CrDialogElement,
    voiceRadioGroup: CrRadioGroupElement,
  };
}

export class VoiceSelectionDialogElement extends CrLitElement implements
    VoiceNotificationListener {
  static get is() {
    return 'voice-selection-dialog';
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
      candidateVoice_: {type: Object},
      previewVoiceInitiated_: {type: Object},
      currentNotifications_: {type: Object},
      localeToDisplayName: {type: Object},
    };
  }

  accessor selectedVoice: SpeechSynthesisVoice|null = null;
  accessor localeToDisplayName: {[lang: string]: string} = {};
  accessor enabledLangs: string[] = [];
  accessor availableVoices: SpeechSynthesisVoice[] = [];
  accessor previewVoicePlaying: SpeechSynthesisVoice|null = null;

  protected accessor candidateVoice_: SpeechSynthesisVoice|null = null;
  protected accessor previewVoiceInitiated_: SpeechSynthesisVoice|null = null;
  protected accessor currentNotifications_:
      {[language: string]: NotificationType} = {};

  protected errorMessages_: string[] = [];
  protected downloadingMessages_: string[] = [];
  protected voiceGroups_: VoiceDropdownGroup[] = [];

  private audioBrowserProxy_: AudioBrowserProxy =
      AudioBrowserProxyImpl.getInstance();
  private notificationManager_: VoiceNotificationManager =
      VoiceNotificationManager.getInstance();
  private logger_: ReadAnythingLogger = ReadAnythingLogger.getInstance();

  private previewTimer_: number|null = null;
  private previewRequestId_: number = 0;
  private previewPendingVoice_: SpeechSynthesisVoice|null = null;
  private hasSelectedVoice_: boolean = false;

  override connectedCallback() {
    super.connectedCallback();
    this.candidateVoice_ = this.selectedVoice;
    this.notificationManager_.addListener(this);
  }

  override disconnectedCallback() {
    super.disconnectedCallback();
    this.stopActivePreview_();
    this.notificationManager_.removeListener(this);
  }

  override willUpdate(changedProperties: PropertyValues<this>) {
    super.willUpdate(changedProperties);

    if (changedProperties.has('selectedVoice')) {
      this.candidateVoice_ = this.selectedVoice;
    }

    if (changedProperties.has('previewVoicePlaying') &&
        (this.previewVoicePlaying !== this.previewVoiceInitiated_)) {
      if (this.previewVoicePlaying) {
        this.clearPreviewTimeout_();
      }
      this.previewVoiceInitiated_ = this.previewVoicePlaying;
    }

    const changedPrivateProperties =
        changedProperties as Map<PropertyKey, unknown>;

    if (changedPrivateProperties.has('currentNotifications_')) {
      this.errorMessages_ = this.computeErrorMessages_();
      this.downloadingMessages_ = this.computeDownloadingMessages_();
    }

    if (changedProperties.has('selectedVoice') ||
        changedPrivateProperties.has('candidateVoice_') ||
        changedProperties.has('availableVoices') ||
        changedProperties.has('enabledLangs') ||
        changedProperties.has('localeToDisplayName') ||
        changedProperties.has('previewVoicePlaying') ||
        changedPrivateProperties.has('previewVoiceInitiated_')) {
      this.voiceGroups_ = this.computeVoiceDropdown_();
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

  close() {
    this.$.voiceSelectionDialog.close();
  }

  protected onCancelButtonClick_() {
    this.$.voiceSelectionDialog.cancel();
  }

  protected onDialogCancel_() {
    this.candidateVoice_ = this.selectedVoice;
    this.stopActivePreview_();
  }

  protected onDialogClose_() {
    this.onDialogCancel_();
    this.currentNotifications_ = {};
    this.fire('close');
  }

  private computeVoiceDropdown_(): VoiceDropdownGroup[] {
    const {groups, hasSelectedVoice} = computeVoiceDropdown({
      availableVoices: this.availableVoices,
      enabledLangs: this.enabledLangs,
      selectedVoice: this.candidateVoice_,
      previewVoicePlaying: this.previewVoicePlaying,
      previewVoiceInitiated: this.previewVoiceInitiated_,
      localeToDisplayName: this.localeToDisplayName,
    });
    this.hasSelectedVoice_ = hasSelectedVoice;
    return groups;
  }

  private computeErrorMessages_(): string[] {
    return computeErrorMessages(
        this.currentNotifications_, this.audioBrowserProxy_);
  }

  private computeDownloadingMessages_(): string[] {
    return computeDownloadingMessages(
        this.currentNotifications_, this.audioBrowserProxy_);
  }

  protected previewButtonTabIndex_(
      voiceItem: VoiceDropdownItem, isFirstVoice: boolean): number {
    // Allows keyboard users to preview the active/candidate voice without
    // cycling through 50 unselected buttons. If no candidate is selected in
    // the visible list, fallback to the first visible voice's preview button.
    return voiceItem.selected || (!this.hasSelectedVoice_ && isFirstVoice) ? 0 :
                                                                             -1;
  }

  protected onVoiceRadioGroupSelectedChanged_(e: CustomEvent<{value: string}>) {
    const selectedVoiceName = e.detail.value;
    if (!selectedVoiceName) {
      return;
    }
    for (const group of this.voiceGroups_) {
      const match = group.voices.find(v => v.voice.name === selectedVoiceName);
      if (match) {
        this.candidateVoice_ = match.voice;
        return;
      }
    }
  }

  protected onVoicePreviewClick_(e: Event) {
    e.stopImmediatePropagation();

    const currentTarget = e.currentTarget as HTMLElement;
    const groupIndex = Number(currentTarget.dataset['groupIndex']);
    const voiceIndex = Number(currentTarget.dataset['voiceIndex']);
    const voiceItem = this.voiceGroups_[groupIndex]?.voices[voiceIndex];
    if (!voiceItem) {
      return;
    }

    const clickedVoice = voiceItem.voice;
    const isPlaying = voiceItem.previewActuallyPlaying;
    const isInitiated = voiceItem.previewInitiated;
    const isPending = this.previewTimer_ !== null &&
        areVoicesEqual(this.previewPendingVoice_, clickedVoice);

    this.clearPreviewTimeout_();

    if (isPlaying || isInitiated || isPending) {
      this.stopActivePreview_();
      return;
    }

    this.previewPendingVoice_ = clickedVoice;
    const requestId = ++this.previewRequestId_;
    this.previewTimer_ = window.setTimeout(() => {
      if (this.previewRequestId_ === requestId) {
        this.previewVoiceInitiated_ = clickedVoice;
        this.previewTimer_ = null;
        this.previewPendingVoice_ = null;
      }
    }, spinnerDebounceTimeout);

    this.fire(ToolbarEvent.PLAY_PREVIEW, {previewVoice: clickedVoice});
  }

  private clearPreviewTimeout_() {
    if (this.previewTimer_ !== null) {
      window.clearTimeout(this.previewTimer_);
      this.previewTimer_ = null;
      this.previewPendingVoice_ = null;
    }
  }

  private stopActivePreview_() {
    this.clearPreviewTimeout_();
    this.previewRequestId_++;
    this.previewVoiceInitiated_ = null;
    this.fire(ToolbarEvent.PLAY_PREVIEW, {previewVoice: null});
  }

  protected onSaveClick_() {
    if (this.candidateVoice_ &&
        !areVoicesEqual(this.candidateVoice_, this.selectedVoice)) {
      this.logger_.logSpeechSettingsChange(
          ReadAloudSettingsChange.VOICE_NAME_CHANGE);
      this.fire(ToolbarEvent.VOICE, {selectedVoice: this.candidateVoice_});
      this.selectedVoice = this.candidateVoice_;
    }
    this.close();
  }

  protected previewLabel_(previewPlaying: boolean): string {
    return loadTimeData.getString(
        previewPlaying ? 'stopLabel' : 'previewTooltip');
  }

  protected hideSpinner_(voiceDropdown: VoiceDropdownItem): boolean {
    return !isVoicePreviewSpinning(voiceDropdown);
  }

  protected voiceLabel_(voiceName: string): string {
    return loadTimeData.getStringF(
        'readingModeLanguageMenuItemLabel', voiceName);
  }

  protected previewIcon_(previewInitiated: boolean): string {
    return previewInitiated ? 'read-anything-20:stop-circle' :
                              'read-anything-20:play-circle';
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'voice-selection-dialog': VoiceSelectionDialogElement;
  }
}

customElements.define(
    VoiceSelectionDialogElement.is, VoiceSelectionDialogElement);
