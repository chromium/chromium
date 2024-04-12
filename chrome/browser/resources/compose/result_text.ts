// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './icons.html.js';
import './strings.m.js';
import '//resources/cr_elements/cr_shared_vars.css.js';
import '//resources/cr_elements/cr_icon_button/cr_icon_button.js';

import {loadTimeData} from '//resources/js/load_time_data.js';
import {PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './result_text.html.js';
import {WordStreamer} from './word_streamer.js';

export interface ComposeResultTextElement {
  $: {
    partialResultText: HTMLElement,
    root: HTMLElement,
  };
}

interface StreamChunk {
  text: string;
}

export interface TextInput {
  text: string;
  isPartial: boolean;
  streamingEnabled: boolean;
}

export class ComposeResultTextElement extends PolymerElement {
  static get is() {
    return 'compose-result-text';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      // Input properties.

      // The text to display.
      textInput: {
        type: Object,
      },

      // Output properties.

      // The full output shown. False if no output is requested.
      isOutputComplete: {
        type: Boolean,
        notify: true,
      },
      // Is there any output to show.
      hasOutput: {
        type: Boolean,
        computed: 'hasOutput_(displayedChunks_, displayedFullText_)',
        notify: true,
      },

      // Private properties.

      hasPartialOutput_: {
        type: Boolean,
        computed: 'getHasPartialOutput_(displayedChunks_, displayedFullText_)',
        notify: true,
      },
      displayedChunks_: {
        type: Object,
        readOnly: true,
      },
      displayedFullText_: {
        type: String,
        readOnly: true,
      },
      editingEnabled_: {
        type: Boolean,
      },
    };
  }

  static get observers() {
    return ['updateInputs(textInput)'];
  }

  textInput: TextInput = {text: '', isPartial: false, streamingEnabled: false};
  isOutputComplete: boolean = false;
  hasOutput: boolean;

  // Private regular properties.
  private wordStreamer_: WordStreamer;
  private displayedChunks_: StreamChunk[] = [];
  private displayedFullText_: string = '';
  private editingEnabled_: boolean;
  // Tracking whether the value has changed
  private isDirty_: boolean = false;

  constructor() {
    super();
    this.wordStreamer_ = new WordStreamer(this.setStreamedWords_.bind(this));
    this.editingEnabled_ = loadTimeData.getBoolean('enableRefinedUi');
  }

  updateInputs() {
    if (this.textInput.streamingEnabled) {
      this.isOutputComplete = false;
      this.wordStreamer_.setText(
          this.textInput.text.trim(), !this.textInput.isPartial);
    } else {
      this.wordStreamer_.reset();
      if (!this.textInput.isPartial) {
        this.displayedFullText_ = this.textInput.text;
        this.isOutputComplete = this.displayedFullText_ !== '';
        this.displayedChunks_ = [];
      } else {
        this.displayedFullText_ = '';
        this.isOutputComplete = false;
      }
    }
  }

  enableInstantStreamingForTesting() {
    this.wordStreamer_.setMsPerTickForTesting(0);
    this.wordStreamer_.setMsWaitBeforeCompleteForTesting(0);
    this.wordStreamer_.setCharsPerTickForTesting(5);
  }

  private onFocusOut_() {
    // Only dispatch event if user has typed something.
    if (this.editingEnabled_ && this.isDirty_) {
      this.isDirty_ = false;
      this.dispatchEvent(new CustomEvent(
          'result-edit',
          {bubbles: true, composed: true, detail: this.$.root.innerText}));
    }
  }

  private onInput_() {
    this.isDirty_ = true;
  }

  private canEdit_() {
    if (this.editingEnabled_) {
      return 'plaintext-only';
    } else {
      return 'false';
    }
  }

  private partialTextCanEdit_() {
    if (this.editingEnabled_ && this.hasOutput && this.isOutputComplete) {
      return 'plaintext-only';
    } else {
      return 'false';
    }
  }

  private hasOutput_(): boolean {
    return this.displayedChunks_.length > 0 || this.displayedFullText_ !== '';
  }

  private getHasPartialOutput_(): boolean {
    return this.hasOutput && !this.isOutputComplete;
  }

  private setStreamedWords_(words: string[], isComplete: boolean) {
    this.displayedChunks_ = words.map(text => ({text}));
    this.isOutputComplete = isComplete && words.length > 0;
    if (isComplete) {
      this.wordStreamer_.reset();
    }
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'compose-result-text': ComposeResultTextElement;
  }
}

customElements.define(ComposeResultTextElement.is, ComposeResultTextElement);
