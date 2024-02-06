// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './icons.html.js';
import './strings.m.js';
import '//resources/cr_elements/cr_shared_vars.css.js';
import '//resources/cr_elements/cr_icon_button/cr_icon_button.js';

import {loadTimeData} from '//resources/js/load_time_data.js';
import {PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {ComposeTextareaAnimator} from './animations/textarea_animator.js';
import type {ConfigurableParams} from './compose.mojom-webui.js';
import {getTemplate} from './textarea.html.js';

export interface ComposeTextareaElement {
  $: {
    editButtonContainer: HTMLElement,
    editButton: HTMLElement,
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
  private animator_: ComposeTextareaAnimator;
  inputParams: ConfigurableParams;
  readonly: boolean;
  private invalidInput_: boolean;
  private tooLong_: boolean;
  private tooShort_: boolean;
  private placeholderText_: string;
  value: string;

  constructor() {
    super();
    this.animator_ = new ComposeTextareaAnimator(
        this, loadTimeData.getBoolean('enableAnimations'));
  }

  override ready() {
    super.ready();
    this.placeholderText_ = this.$.input.placeholder;
  }

  focusInput() {
    this.$.input.focus();
  }

  focusEditButton() {
    this.$.editButton.focus();
  }

  private onEditClick_() {
    this.dispatchEvent(
        new CustomEvent('edit-click', {bubbles: true, composed: true}));
  }

  scrollInputToTop() {
    this.$.input.scrollTop = 0;
  }

  private shouldShowEditIcon_(): boolean {
    return this.allowExitingReadonlyMode && this.readonly;
  }

  private onChangeTextArea_() {
    if (this.$.input.value === '') {
      this.$.input.placeholder = this.placeholderText_;
    } else {
      this.$.input.placeholder = '';
    }
  }

  transitionToEditable() {
    this.animator_.transitionToEditable();
  }

  transitionToReadonly(fromHeight?: number) {
    this.animator_.transitionToReadonly(fromHeight);
  }

  transitionToEditing(bodyHeight: number) {
    this.animator_.transitionToEditing(bodyHeight);
  }

  transitionToResult(bodyHeight: number) {
    this.animator_.transitionToResult(bodyHeight);
  }

  validate() {
    const value = this.$.input.value;
    const wordCount = value.match(/\S+/g)?.length || 0;
    this.tooLong_ = value.length > this.inputParams.maxCharacterLimit ||
        wordCount > this.inputParams.maxWordLimit;
    // If it's too long, then it can't be too short.
    this.tooShort_ =
        wordCount < this.inputParams.minWordLimit && !this.tooLong_;
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
