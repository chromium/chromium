// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_elements/cr_icon_button/cr_icon_button.js';
import '//resources/cr_elements/cr_icons.css.js';
import '//resources/cr_elements/icons.html.js';
import '//resources/cr_elements/cr_action_menu/cr_action_menu.js';
import './icons.html.js';

import type {CrActionMenuElement} from '//resources/cr_elements/cr_action_menu/cr_action_menu.js';
import {AnchorAlignment} from '//resources/cr_elements/cr_action_menu/cr_action_menu.js';
import {WebUiListenerMixin} from '//resources/cr_elements/web_ui_listener_mixin.js';
import type {DomRepeatEvent} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './voice_selection_menu.html.js';


export interface VoiceSelectionMenuElement {
  $: {
    voiceSelectionMenu: CrActionMenuElement,
  };
}

interface VoiceDropdown {
  title: string;
  voice: SpeechSynthesisVoice;
  selected: boolean;
  previewPlaying: boolean;
}

const VoiceSelectionMenuElementBase = WebUiListenerMixin(PolymerElement);

// TODO add tests for this component
export class VoiceSelectionMenuElement extends VoiceSelectionMenuElementBase {
  static get is() {
    return 'voice-selection-menu';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      selectedVoice: Object,
      availableVoices: Array,
      previewVoicePlaying: Object,
      voiceSelectionOptions_: {
        type: Object,
        computed: 'computeVoiceDropdown_(selectedVoice, availableVoices, previewVoicePlaying)',
      },
    };
  }

  private computeVoiceDropdown_(
      selectedVoice: SpeechSynthesisVoice,
      availableVoices: SpeechSynthesisVoice[],
      previewVoicePlaying: SpeechSynthesisVoice|null): VoiceDropdown[] {
    return availableVoices.map(
      voice => ({
        title: voice.name,
        voice,
        selected: voicesAreEqual(selectedVoice, voice),
        previewPlaying: voicesAreEqual(previewVoicePlaying, voice),
      }));
  }

  private onVoiceSelectionMenuClick_(event: MouseEvent) {
    const target = event.target as HTMLElement;
    const minY = target.getBoundingClientRect().bottom;

    this.$.voiceSelectionMenu.showAt(target, {
      minY: minY,
      left: 0,
      anchorAlignmentY: AnchorAlignment.AFTER_END,
      noOffset: true,
    });
  }

  private onVoiceSelectClick_(event: DomRepeatEvent<VoiceDropdown>) {
    const selectedVoice = event.model.item.voice;

    this.dispatchEvent(new CustomEvent('select-voice', {
      bubbles: true,
      composed: true,
      detail: {
        selectedVoice,
      },
    }));
  }

  private onVoicePreviewClick_(event: DomRepeatEvent<VoiceDropdown>) {
    // Because the preview button is layered onto the voice-selection button,
    // the onVoiceSelectClick_() listener is also subscribed to this event. This
    // line is to make sure that the voice-selection callback is not triggered.
    event.stopImmediatePropagation();

    const previewVoice = event.model.item.voice;
    this.dispatchEvent(new CustomEvent('preview-voice', {
      bubbles: true,
      composed: true,
      detail: {
        previewVoice,
      },
    }));
  }
}

function voicesAreEqual(
    voice1: SpeechSynthesisVoice|null,
    voice2: SpeechSynthesisVoice|null): boolean {
  if (!voice1 || !voice2) {
    return false;
  }
  return voice1.default === voice2.default && voice1.lang === voice2.lang &&
      voice1.localService === voice2.localService &&
      voice1.name === voice2.name && voice1.voiceURI === voice2.voiceURI;
}


declare global {
  interface HTMLElementTagNameMap {
    'voice-selection-menu': VoiceSelectionMenuElement;
  }
}

customElements.define('voice-selection-menu', VoiceSelectionMenuElement);
