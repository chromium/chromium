// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_elements/cr_icon_button/cr_icon_button.js';
import '//resources/cr_elements/cr_icons.css.js';
import '//resources/cr_elements/icons.html.js';
import '//resources/cr_elements/cr_action_menu/cr_action_menu.js';
import '//resources/cr_elements/cr_lazy_render/cr_lazy_render.js';
import './icons.html.js';

import type {CrActionMenuElement} from '//resources/cr_elements/cr_action_menu/cr_action_menu.js';
import {AnchorAlignment} from '//resources/cr_elements/cr_action_menu/cr_action_menu.js';
import type {CrLazyRenderElement} from '//resources/cr_elements/cr_lazy_render/cr_lazy_render.js';
import {WebUiListenerMixin} from '//resources/cr_elements/web_ui_listener_mixin.js';
import {assert} from '//resources/js/assert.js';
import type {DomRepeatEvent} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './voice_selection_menu.html.js';

export interface VoiceSelectionMenuElement {
  $: {
    voiceSelectionMenu: CrLazyRenderElement<CrActionMenuElement>,
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

// This string is not localized and will be in English, even for non-English
// Natural voices.
const NATURAL_STRING_IDENTIFIER = '(Natural)';

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

  onVoiceSelectionMenuClick(event: MouseEvent) {
    const target = event.target as HTMLElement;
    const minY = target.getBoundingClientRect().bottom;

    this.voicePlayingWhenMenuOpened_ = !this.paused;

    this.$.voiceSelectionMenu.get().showAt(target, {
      minY: minY,
      left: 0,
      anchorAlignmentY: AnchorAlignment.AFTER_END,
      noOffset: true,
    });
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

  private onVoiceMenuKeyDown_(e: KeyboardEvent) {
    const currentElement = e.target as HTMLElement;
    assert(currentElement, 'no key target');
    const targetIsVoiceOption =
        (currentElement.classList.contains('dropdown-voice-selection-button')) ?
        true :
        false;
    const targetIsPreviewButton = (currentElement.id === 'play-icon' ||
                                   currentElement.id === 'pause-icon') ?
        true :
        false;
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
      // From a voice option, go to whichever preview button is visible
      // There are 'play-icon' and 'pause-icon' preview buttons, and
      // only one is visible at a time. The one that's visible has style
      // display-true, so use that to directly select the right button
      const visiblePreviewButton =
          currentElement.querySelector<HTMLElement>('#play-icon');
      assert(visiblePreviewButton, 'can\'t find preview button');
      visiblePreviewButton!.focus();
    } else {  // Voice preview button - go to voice entry
      currentElement.parentElement!.focus();
    }
  }
}

function voiceQualityRankComparator(
    voice1: VoiceDropdownItem,
    voice2: VoiceDropdownItem,
    ): number {
  if (isNatural(voice1) && isNatural(voice2)) {
    return 0;
  }

  if (!isNatural(voice1) && !isNatural(voice2)) {
    return 0;
  }

  // voice1 is a Natural voice and voice2 is not
  if (isNatural(voice1)) {
    return -1;
  }

  // voice2 is a Natural voice and voice1 is not
  return 1;
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

function isNatural(voiceDropdownItem: VoiceDropdownItem) {
  return voiceDropdownItem.voice.name.includes(NATURAL_STRING_IDENTIFIER);
}

declare global {
  interface HTMLElementTagNameMap {
    'voice-selection-menu': VoiceSelectionMenuElement;
  }
}

customElements.define('voice-selection-menu', VoiceSelectionMenuElement);
