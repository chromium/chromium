// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './icons.html.js';
import '/strings.m.js';
import '//resources/cr_elements/cr_icon_button/cr_icon_button.js';

import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';
import type {PropertyValues} from '//resources/lit/v3_0/lit.rollup.js';

import {getCss} from './result_text.css.js';
import {getHtml} from './result_text.html.js';
import {WordStreamer} from './word_streamer.js';

export interface ComposeResultTextElement {
  $: {
    resultText: HTMLElement,
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

export class ComposeResultTextElement extends CrLitElement {
  static get is() {
    return 'compose-result-text';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      // Input properties.

      // The text to display.
      textInput: {type: Object},

      // Output properties.

      // The full output shown. False if no output is requested.
      isOutputComplete: {
        type: Boolean,
        notify: true,
      },
      // Is there any output to show.
      hasOutput: {
        type: Boolean,
        notify: true,
      },

      // Private properties.

      hasPartialOutput_: {
        type: Boolean,
        notify: true,
      },
      displayedChunks_: {type: Array},
      displayedFullText_: {type: String},
    };
  }

  accessor textInput: TextInput = {
    text: '',
    isPartial: false,
    streamingEnabled: false,
  };
  accessor isOutputComplete: boolean = false;
  accessor hasOutput: boolean = false;
  protected accessor hasPartialOutput_: boolean = false;
  protected accessor displayedChunks_: StreamChunk[] = [];
  protected accessor displayedFullText_: string = '';

  // Private regular properties.
  private wordStreamer_: WordStreamer;
  private initialText_: string = '';

  constructor() {
    super();
    this.wordStreamer_ = new WordStreamer(this.setStreamedWords_.bind(this));
  }

  override willUpdate(changedProperties: PropertyValues<this>) {
    super.willUpdate(changedProperties);

    if (changedProperties.has('textInput')) {
      this.updateInputs_();
    }

    const changedPrivateProperties =
        changedProperties as Map<PropertyKey, unknown>;

    if (changedPrivateProperties.has('displayedChunks_') ||
        changedPrivateProperties.has('displayedFullText_') ||
        changedProperties.has('isOutputComplete')) {
      this.hasOutput =
          this.displayedChunks_.length > 0 || this.displayedFullText_ !== '';
      this.hasPartialOutput_ = this.hasOutput && !this.isOutputComplete;
    }
  }

  override updated(changedProperties: PropertyValues<this>) {
    super.updated(changedProperties);

    // Making this change in updated() since it touches the DOM.
    if (changedProperties.has('textInput')) {
      this.$.resultText.innerText = this.textInput.text;
    }
  }

  protected updateInputs_() {
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

  protected onFocusIn_() {
    this.fire('set-result-focus', true);

    this.initialText_ = this.textInput.text;
  }

  protected onFocusOut_() {
    this.fire('set-result-focus', false);

    const currentText = this.$.resultText.innerText;
    if (currentText === '') {
      // We disallow the user from saving or using empty text. Instead, replace
      // it with the starting state of the text before it was edited.
      this.$.resultText.innerText = this.initialText_;
      return;
    }
    // Only dispatch event if the text has changed from its initial state.
    if (currentText !== this.initialText_) {
      this.fire('result-edit', this.$.root.innerText);
    }
  }

  protected partialTextCanEdit_() {
    if (this.hasOutput && this.isOutputComplete) {
      return 'plaintext-only';
    } else {
      return 'false';
    }
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
