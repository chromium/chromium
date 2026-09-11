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

import {areVoicesEqual} from './voice_language_conversions.js';
import type {VoiceDropdownGroup, VoiceDropdownItem} from './voice_menu_display.js';
import {computeVoiceDropdown, isVoicePreviewSpinning} from './voice_menu_display.js';
import {getCss} from './voice_selection_dialog.css.js';
import {getHtml} from './voice_selection_dialog.html.js';

export interface VoiceSelectionDialogElement {
  $: {
    voiceSelectionDialog: CrDialogElement,
    voiceRadioGroup: CrRadioGroupElement,
  };
}

export class VoiceSelectionDialogElement extends CrLitElement {
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
      candidateVoice_: {type: Object},
      localeToDisplayName: {type: Object},
    };
  }

  accessor selectedVoice: SpeechSynthesisVoice|null = null;
  accessor localeToDisplayName: {[lang: string]: string} = {};
  accessor enabledLangs: string[] = [];
  accessor availableVoices: SpeechSynthesisVoice[] = [];

  protected accessor candidateVoice_: SpeechSynthesisVoice|null = null;
  protected errorMessages_: string[] = [];
  protected downloadingMessages_: string[] = [];
  protected voiceGroups_: VoiceDropdownGroup[] = [];

  private hasSelectedVoice_: boolean = false;

  override connectedCallback() {
    super.connectedCallback();
    this.candidateVoice_ = this.selectedVoice;
  }

  override willUpdate(changedProperties: PropertyValues<this>) {
    super.willUpdate(changedProperties);

    if (changedProperties.has('selectedVoice')) {
      this.candidateVoice_ = this.selectedVoice;
    }

    const changedPrivateProperties =
        changedProperties as Map<PropertyKey, unknown>;

    if (changedProperties.has('selectedVoice') ||
        changedPrivateProperties.has('candidateVoice_') ||
        changedProperties.has('availableVoices') ||
        changedProperties.has('enabledLangs') ||
        changedProperties.has('localeToDisplayName')) {
      this.voiceGroups_ = this.computeVoiceDropdown_();
    }
  }

  close() {
    this.$.voiceSelectionDialog.close();
  }

  protected onCancelButtonClick_() {
    this.$.voiceSelectionDialog.cancel();
  }

  protected onDialogCancel_() {
    this.candidateVoice_ = this.selectedVoice;
  }

  protected onDialogClose_() {
    this.onDialogCancel_();
    this.fire('close');
  }

  private computeVoiceDropdown_(): VoiceDropdownGroup[] {
    const {groups, hasSelectedVoice} = computeVoiceDropdown({
      availableVoices: this.availableVoices,
      enabledLangs: this.enabledLangs,
      selectedVoice: this.candidateVoice_,
      localeToDisplayName: this.localeToDisplayName,
    });
    this.hasSelectedVoice_ = hasSelectedVoice;
    return groups;
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
  }

  protected onSaveClick_() {
    if (this.candidateVoice_ &&
        !areVoicesEqual(this.candidateVoice_, this.selectedVoice)) {
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
