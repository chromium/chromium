// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'cr-textarea' is a component similar to native textarea,
 * and inherits styling from cr-input.
 *
 * Forked from ui/webui/resources/cr_elements/cr_textarea/cr_textarea.ts
 */
import '../cr_hidden_style.css.js';
import '../cr_shared_style.css.js';
import '../cr_input/cr_input_style.css.js';

import {PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './cr_textarea.html.js';

export interface CrTextareaElement {
  $: {
    firstFooter: HTMLElement,
    footerContainer: HTMLElement,
    input: HTMLTextAreaElement,
    label: HTMLElement,
    mirror: HTMLElement,
    secondFooter: HTMLElement,
    underline: HTMLElement,
  };
}

export class CrTextareaElement extends PolymerElement {
  static get is() {
    return 'cr-textarea';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      /**
       * Whether the text area should automatically get focus when the page
       * loads.
       */
      autofocus: {
        type: Boolean,
        value: false,
        reflectToAttribute: true,
      },

      /**
       * Whether the text area is disabled. When disabled, the text area loses
       * focus and is not reachable by tabbing.
       */
      disabled: {
        type: Boolean,
        value: false,
        reflectToAttribute: true,
        observer: 'onDisabledChanged_',
      },

      /** Whether the text area is required. */
      required: {
        type: Boolean,
        value: false,
        reflectToAttribute: true,
      },

      /** Maximum length (in characters) of the text area. */
      maxlength: {
        type: Number,
      },

      /**
       * Whether the text area is read only. If read-only, content cannot be
       * changed.
       */
      readonly: Boolean,

      /** Number of rows (lines) of the text area. */
      rows: {
        type: Number,
        value: 3,
        reflectToAttribute: true,
      },

      /** Caption of the text area. */
      label: {
        type: String,
        value: '',
      },

      /**
       * Text inside the text area. If the text exceeds the bounds of the text
       * area, i.e. if it has more than |rows| lines, a scrollbar is shown by
       * default when autogrow is not set.
       */
      value: {
        type: String,
        value: '',
        notify: true,
      },

      /**
       * Placeholder text that is shown when no value is present.
       */
      placeholder: {
        type: String,
        value: '',
      },

      /** Whether the textarea can auto-grow vertically or not. */
      autogrow: {
        type: Boolean,
        value: false,
        reflectToAttribute: true,
      },

      /**
       * Attribute to enable limiting the maximum height of a autogrow textarea.
       * Use --cr-textarea-autogrow-max-height to set the height.
       */
      hasMaxHeight: {
        type: Boolean,
        value: false,
        reflectToAttribute: true,
      },

      /** Whether the textarea is invalid or not. */
      invalid: {
        type: Boolean,
        value: false,
        reflectToAttribute: true,
      },

      /**
       * First footer text below the text area. Can be used to warn user about
       * character limits.
       */
      firstFooter: {
        type: String,
        value: '',
      },

      /**
       * Second footer text below the text area. Can be used to show current
       * character count.
       */
      secondFooter: {
        type: String,
        value: '',
      },
    };
  }

  override autofocus: boolean;
  disabled: boolean;
  readonly: boolean;
  required: boolean;
  rows: number;
  label: string;
  value: string;
  placeholder: string;
  autogrow: boolean;
  hasMaxHeight: boolean;
  invalid: boolean;
  firstFooter: string;
  secondFooter: string;

  focusInput() {
    this.$.input.focus();
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

  private calculateMirror_(): string {
    if (!this.autogrow) {
      return '';
    }
    // Browsers do not render empty divs. The extra space is used to render the
    // div when empty.
    const tokens = this.value ? this.value.split('\n') : [''];

    while (this.rows > 0 && tokens.length < this.rows) {
      tokens.push('');
    }
    return tokens.join('\n') + '&nbsp;';
  }

  private onInputFocusChange_() {
    // focused_ is used instead of :focus-within, so focus on elements within
    // the suffix slot does not trigger a change in input styles.
    if (this.shadowRoot!.activeElement === this.$.input) {
      this.setAttribute('focused_', '');
    } else {
      this.removeAttribute('focused_');
    }
  }

  private onDisabledChanged_() {
    this.setAttribute('aria-disabled', this.disabled ? 'true' : 'false');
  }

  private getFooterAria_(): string {
    return this.invalid ? 'assertive' : 'polite';
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'cr-textarea': CrTextareaElement;
  }
}

customElements.define(CrTextareaElement.is, CrTextareaElement);
