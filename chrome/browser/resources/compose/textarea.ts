// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './icons.html.js';
import '/strings.m.js';
import '//resources/cr_elements/cr_icon_button/cr_icon_button.js';

import {loadTimeData} from '//resources/js/load_time_data.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';

import {ComposeTextareaAnimator} from './animations/textarea_animator.js';
import type {ConfigurableParams} from './compose.mojom-webui.js';
import {getCss} from './textarea.css.js';
import {getHtml} from './textarea.html.js';

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

export class ComposeTextareaElement extends CrLitElement {
  static get is() {
    return 'compose-textarea';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      allowExitingReadonlyMode: {
        type: Boolean,
        reflect: true,
      },
      inputParams: {type: Object},
      readonly: {
        type: Boolean,
        reflect: true,
      },
      invalidInput_: {
        type: Boolean,
        reflect: true,
      },
      tooLong_: {type: Boolean},
      tooShort_: {type: Boolean},
      value: {
        type: String,
        notify: true,
      },
    };
  }

  accessor allowExitingReadonlyMode: boolean = false;
  accessor inputParams: ConfigurableParams = {
    minWordLimit: 0,
    maxWordLimit: 0,
    maxCharacterLimit: 0,
  };
  accessor readonly: boolean = false;
  protected accessor invalidInput_: boolean = false;
  protected accessor tooLong_: boolean = false;
  protected accessor tooShort_: boolean = false;
  accessor value: string = '';

  private animator_: ComposeTextareaAnimator;
  private placeholderText_: string = '';

  constructor() {
    super();
    this.animator_ = new ComposeTextareaAnimator(
        this, loadTimeData.getBoolean('enableAnimations'));
  }

  override firstUpdated() {
    this.placeholderText_ = this.$.input.placeholder;
  }

  focusInput() {
    this.$.input.focus();
  }

  focusEditButton() {
    this.$.editButton.focus();
  }

  protected onEditClick_() {
    this.fire('edit-click');
  }

  scrollInputToTop() {
    this.$.input.scrollTop = 0;
  }

  protected shouldShowEditIcon_(): boolean {
    return this.allowExitingReadonlyMode && this.readonly;
  }

  protected onInput_() {
    this.value = this.$.input.value;
  }

  protected onChangeTextArea_() {
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
    // Ensure that any changes to |value| have propagated to the native input.
    this.performUpdate();
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
