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

const VoiceSelectionMenuElementBase = WebUiListenerMixin(PolymerElement);

export class VoiceSelectionMenuElement extends VoiceSelectionMenuElementBase {
  // If Read Aloud is in the paused state. This is set from the parent element
  // via one way data binding.
  private readonly paused: boolean;

  private voicePlayingWhenMenuOpened_: boolean = false;

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
      paused: Boolean,
      localeToDisplayName: Object,
      voiceSelectionOptions_: {
        type: Object,
        computed: 'computeVoiceDropdown_(selectedVoice, availableVoices,' +
            ' previewVoicePlaying, localeToDisplayName)',
      },
    };
  }

  private computeVoiceDropdown_(
      selectedVoice: SpeechSynthesisVoice,
      availableVoices: SpeechSynthesisVoice[],
      previewVoicePlaying: SpeechSynthesisVoice|null,
      localeToDisplayName: {[lang: string]: string}): VoiceDropdownGroup[] {
    const languageToVoices =
        availableVoices.reduce((languageToDropdownItems, voice) => {
          const dropdownItem: VoiceDropdownItem = {
            title: voice.name,
            voice,
            id: this.stringToHtmlTestId_(voice.name),
            selected: voicesAreEqual(selectedVoice, voice),
            previewPlaying: voicesAreEqual(previewVoicePlaying, voice),
          };

          const lang =
              (localeToDisplayName && voice.lang in localeToDisplayName) ?
              localeToDisplayName[voice.lang] :
              voice.lang;

          if (languageToDropdownItems[lang]) {
            languageToDropdownItems[lang].push(dropdownItem);
          } else {
            languageToDropdownItems[lang] = [dropdownItem];
          }

          return languageToDropdownItems;
        }, {} as {[language: string]: VoiceDropdownItem[]});

    return Object.entries(languageToVoices).map(([
                                                  language,
                                                  voices,
                                                ]) => ({language, voices}));
  }

  // This ID does not ensure uniqueness and is just used for testing purposes.
  private stringToHtmlTestId_(s: string): string {
    return s.replace(/\s/g, '-').replace(/[()]/g, '');
  }

  private onVoiceSelectionMenuClick_(event: MouseEvent) {
    const target = event.target as HTMLElement;
    const minY = target.getBoundingClientRect().bottom;

    this.voicePlayingWhenMenuOpened_ = !this.paused;

    this.$.voiceSelectionMenu.showAt(target, {
      minY: minY,
      left: 0,
      anchorAlignmentY: AnchorAlignment.AFTER_END,
      noOffset: true,
    });
  }

  private onVoiceSelectClick_(event: DomRepeatEvent<VoiceDropdownItem>) {
    const selectedVoice = event.model.item.voice;

    this.dispatchEvent(new CustomEvent('select-voice', {
      bubbles: true,
      composed: true,
      detail: {
        selectedVoice,
      },
    }));
  }

  private onVoicePreviewClick_(event: DomRepeatEvent<VoiceDropdownItem>) {
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

  private onClose_() {
    this.dispatchEvent(new CustomEvent('voice-menu-close', {
      bubbles: true,
      composed: true,
      detail: {
        voicePlayingWhenMenuOpened: this.voicePlayingWhenMenuOpened_,
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
