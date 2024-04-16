// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Forked from ui/webui/resources/cr_elements/cr_input/cr_input.ts

import '../cr_hidden_style.css.js';
import '../cr_shared_style.css.js';
import '../cr_shared_vars.css.js';
import './cr_input_style.css.js';

import {assert} from '//resources/js/assert.js';
import {PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './cr_input.html.js';


/**
 * Input types supported by cr-input.
 */
const SUPPORTED_INPUT_TYPES: Set<string> = new Set([
  'number',
  'password',
  'search',
  'text',
  'url',
]);

/**
 * @fileoverview 'cr-input' is a component similar to native input.
 *
 * Native input attributes that are currently supported by cr-inputs are:
 *   autofocus
 *   disabled
 *   max (only applicable when type="number")
 *   min (only applicable when type="number")
 *   maxlength
 *   minlength
 *   pattern
 *   placeholder
 *   readonly
 *   required
 *   tabindex (set through input-tabindex)
 *   type (see |SUPPORTED_INPUT_TYPES| above)
 *   value
 *
 * Additional attributes that you can use with cr-input:
 *   label
 *   auto-validate - triggers validation based on |pattern| and |required|,
 *                   whenever |value| changes.
 *   error-message - message displayed under the input when |invalid| is true.
 *   invalid
 *
 * You may pass an element into cr-input via [slot="suffix"] to be vertically
 * center-aligned with the input field, regardless of position of the label and
 * error-message. Example:
 *   <cr-input>
 *     <cr-button slot="suffix"></cr-button>
 *   </cr-input>
 */
export interface CrInputElement {
  $: {
    error: HTMLElement,
    label: HTMLElement,
    input: HTMLInputElement,
    underline: HTMLElement,
  };
}

export class CrInputElement extends PolymerElement {
  static get is() {
    return 'cr-input';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      ariaDescription: {
        type: String,
      },

      ariaLabel: {
        type: String,
        value: '',
      },

      autofocus: {
        type: Boolean,
        value: false,
        reflectToAttribute: true,
      },

      autoValidate: Boolean,

      disabled: {
        type: Boolean,
        value: false,
        reflectToAttribute: true,
      },

      errorMessage: {
        type: String,
        value: '',
        observer: 'onInvalidOrErrorMessageChanged_',
      },

      displayErrorMessage_: {
        type: String,
        value: '',
      },

      /**
       * This is strictly used internally for styling, do not attempt to use
       * this to set focus.
       */
      focused_: {
        type: Boolean,
        value: false,
        reflectToAttribute: true,
      },

      invalid: {
        type: Boolean,
        value: false,
        notify: true,
        reflectToAttribute: true,
        observer: 'onInvalidOrErrorMessageChanged_',
      },

      max: {
        type: Number,
        reflectToAttribute: true,
      },

      min: {
        type: Number,
        reflectToAttribute: true,
      },

      maxlength: {
        type: Number,
        reflectToAttribute: true,
      },

      minlength: {
        type: Number,
        reflectToAttribute: true,
      },

      pattern: {
        type: String,
        reflectToAttribute: true,
      },

      inputmode: String,

      label: {
        type: String,
        value: '',
      },

      placeholder: {
        type: String,
        value: null,
        observer: 'placeholderChanged_',
      },

      readonly: {
        type: Boolean,
        reflectToAttribute: true,
      },

      required: {
        type: Boolean,
        reflectToAttribute: true,
      },

      inputTabindex: {
        type: Number,
        value: 0,
        observer: 'onInputTabindexChanged_',
      },

      type: {
        type: String,
        value: 'text',
        observer: 'onTypeChanged_',
      },

      value: {
        type: String,
        value: '',
        notify: true,
        observer: 'onValueChanged_',
      },
    };
  }

  override ariaDescription: string|null;
  autoFocus: boolean;
  autoValidate: boolean;
  disabled: boolean;
  errorMessage: string;
  inputmode: string;
  inputTabindex: number;
  invalid: boolean;
  label: string;
  max: number;
  min: number;
  maxlength: number;
  minlength: number;
  pattern: string;
  placeholder: string|null;
  readonly: boolean;
  required: boolean;
  type: string;
  value: string;

  private displayErrorMessage_: string;
  private focused_: boolean;

  override ready() {
    super.ready();

    // Use inputTabindex instead.
    assert(!this.hasAttribute('tabindex'));
  }

  private onInputTabindexChanged_() {
    // CrInput only supports 0 or -1 values for the input's tabindex to allow
    // having the input in tab order or not. Values greater than 0 will not work
    // as the shadow root encapsulates tabindices.
    assert(this.inputTabindex === 0 || this.inputTabindex === -1);
  }

  private onTypeChanged_() {
    // Check that the 'type' is one of the supported types.
    assert(SUPPORTED_INPUT_TYPES.has(this.type));
  }

  get inputElement(): HTMLInputElement {
    return this.$.input;
  }

  /**
   * Returns the aria label to be used with the input element.
   */
  private getAriaLabel_(ariaLabel: string, label: string, placeholder: string):
      string {
    return ariaLabel || label || placeholder;
  }

  /**
   * Returns 'true' or 'false' as a string for the aria-invalid attribute.
   */
  private getAriaInvalid_(invalid: boolean): string {
    return invalid ? 'true' : 'false';
  }

  private onInvalidOrErrorMessageChanged_() {
    this.displayErrorMessage_ = this.invalid ? this.errorMessage : '';

    // On VoiceOver role="alert" is not consistently announced when its content
    // changes. Adding and removing the |role| attribute every time there
    // is an error, triggers VoiceOver to consistently announce.
    const ERROR_ID = 'error';
    const errorElement =
        this.shadowRoot!.querySelector<HTMLElement>(`#${ERROR_ID}`);
    assert(errorElement);
    if (this.invalid) {
      errorElement.setAttribute('role', 'alert');
      this.inputElement.setAttribute('aria-errormessage', ERROR_ID);
    } else {
      errorElement.removeAttribute('role');
      this.inputElement.removeAttribute('aria-errormessage');
    }
  }

  /**
   * This is necessary instead of doing <input placeholder="[[placeholder]]">
   * because if this.placeholder is set to a truthy value then removed, it
   * would show "null" as placeholder.
   */
  private placeholderChanged_() {
    if (this.placeholder || this.placeholder === '') {
      this.inputElement.setAttribute('placeholder', this.placeholder);
    } else {
      this.inputElement.removeAttribute('placeholder');
    }
  }

  override focus() {
    this.focusInput();
  }

  /**
   * Focuses the input element.
   * TODO(crbug.com/40593040): Replace this with focus() after resolving the
   * text selection issue described in onFocus_().
   * @return Whether the <input> element was focused.
   */
  focusInput(): boolean {
    if (this.shadowRoot!.activeElement === this.inputElement) {
      return false;
    }
    this.inputElement.focus();
    return true;
  }

  private onValueChanged_(newValue: string, oldValue: string) {
    if (!newValue && !oldValue) {
      return;
    }
    if (this.autoValidate) {
      this.validate();
    }
  }

  /**
   * 'change' event fires when <input> value changes and user presses 'Enter'.
   * This function helps propagate it to host since change events don't
   * propagate across Shadow DOM boundary by default.
   */
  private onInputChange_(e: Event) {
    this.dispatchEvent(new CustomEvent(
        'change', {bubbles: true, composed: true, detail: {sourceEvent: e}}));
  }

  private onInputFocus_() {
    this.focused_ = true;
  }

  private onInputBlur_() {
    this.focused_ = false;
  }

  /**
   * Selects the text within the input. If no parameters are passed, it will
   * select the entire string. Either no params or both params should be passed.
   * Publicly, this function should be used instead of inputElement.select() or
   * manipulating inputElement.selectionStart/selectionEnd because the order of
   * execution between focus() and select() is sensitive.
   */
  select(start?: number, end?: number) {
    this.inputElement.focus();
    if (start !== undefined && end !== undefined) {
      this.inputElement.setSelectionRange(start, end);
    } else {
      // Can't just pass one param.
      assert(start === undefined && end === undefined);
      this.inputElement.select();
    }
  }

  validate(): boolean {
    this.invalid = !this.inputElement.checkValidity();
    return !this.invalid;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'cr-input': CrInputElement;
  }
}

customElements.define(CrInputElement.is, CrInputElement);
