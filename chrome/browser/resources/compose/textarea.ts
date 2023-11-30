// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './icons.html.js';
import '//resources/cr_elements/cr_hidden_style.css.js';
import '//resources/cr_elements/cr_shared_vars.css.js';
import '//resources/cr_elements/cr_icon_button/cr_icon_button.js';

import {PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {ConfigurableParams} from './compose.mojom-webui.js';
import {getTemplate} from './textarea.html.js';

export interface ComposeTextareaElement {
  $: {
    editButtonContainer: HTMLElement,
    tooShortError: HTMLElement,
    tooLongError: HTMLElement,
    input: HTMLTextAreaElement,
    readonlyText: HTMLElement,
  };
}

export class ComposeTextareaElement extends PolymerElement {
  static get is() {
    return 'compose-textarea';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      allowExitingReadonlyMode: {
        type: Boolean,
        value: false,
        reflectToAttribute: true,
      },
      inputParams: Object,
      readonly: {
        type: Boolean,
        value: false,
        reflectToAttribute: true,
      },
      invalidInput_: {
        type: Boolean,
        value: false,
        reflectToAttribute: true,
      },
      tooLong_: {
        type: Boolean,
        value: false,
      },
      tooShort_: {
        type: Boolean,
        value: false,
      },
      value: {
        type: String,
        notify: true,
      },
    };
  }

  allowExitingReadonlyMode: boolean;
  inputParams: ConfigurableParams;
  readonly: boolean;
  private invalidInput_: boolean;
  private tooLong_: boolean;
  private tooShort_: boolean;
  value: string;

  focusInput() {
    this.$.input.focus();
  }

  private onEditClick_() {
    this.dispatchEvent(
        new CustomEvent('edit-click', {bubbles: true, composed: true}));
  }

  private shouldShowEditIcon_(): boolean {
    return this.allowExitingReadonlyMode && this.readonly;
  }

  validate() {
    const value = this.$.input.value;
    const wordCount = value.match(/\S+/g)?.length || 0;
    this.tooShort_ = wordCount < this.inputParams.minWordLimit;
    this.tooLong_ = value.length > this.inputParams.maxCharacterLimit ||
        wordCount > this.inputParams.maxWordLimit;
    this.invalidInput_ = this.tooLong_ || this.tooShort_;
    return !this.invalidInput_;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'compose-textarea': ComposeTextareaElement;
  }
}

customElements.define(ComposeTextareaElement.is, ComposeTextareaElement);
