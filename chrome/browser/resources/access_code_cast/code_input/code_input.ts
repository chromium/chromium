// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_input/cr_input.m.js';

import {CrInputElement} from 'chrome://resources/cr_elements/cr_input/cr_input.m.js';
import {I18nMixin} from 'chrome://resources/js/i18n_mixin.js';
import {WebUIListenerMixin} from 'chrome://resources/js/web_ui_listener_mixin.js';
import {afterNextRender, html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

const CodeInputElementBase = WebUIListenerMixin(I18nMixin(PolymerElement));

export class CodeInputElement extends CodeInputElementBase {
  static get is() {
    return 'c2c-code-input';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      disabled: Boolean,
      length: Number,
      value: {
        type: String,
        value: '',
      }
    };
  }

  length: number;
  value: string;
  private inputs: string[];
  private inputElementsLoaded: boolean;

  constructor() {
    super();
    this.inputElementsLoaded = false;

    afterNextRender(this, () => {
      this.inputElementsLoaded = true;

      for(let i = 0; i < this.length; i++) {
        const index = i;
        this.getInput(i).addEventListener('input', (e: Event) => {
          const input = e.target as CrInputElement;
          this.handleInput(input.value, index);
        });
        this.getInput(i).addEventListener('keydown', (k: KeyboardEvent) => {
          if (k.key === 'Backspace') {
            this.handleBackspace(index);
          }
        });
      }
    });
  }

  ready() {
    super.ready();
    this.inputs = Array(this.length).fill('');
  }

  clearInput() {
    const clear = () => {
      for (let i = 0; i < this.length; i++) {
        this.getInput(i).value = '';
      }
    };

    this.afterInputsLoaded(clear);
  }

  focusInput() {
    const focus = () => {
      for (let i = 0; i < this.length; i++) {
        if (this.getInput(i).value === '') {
          this.getInput(i).focusInput();
          return;
        }
      }
      this.getInput(this.length - 1).focusInput();
    };
    
    this.afterInputsLoaded(focus);
  }

  getFocusedIndex() {
    for (let i = 0; i < this.length; i++) {
      if (this.getInput(i) === this.shadowRoot!.activeElement) {
        return i;
      }
    }

    return -1;
  }

  getInput(index: number): CrInputElement {
    const el = this.shadowRoot!.querySelector('#input-' + index);
    return el as CrInputElement;
  }

  setValue(value: string) {
    if (value.length > this.length) {
      return;
    }

    this.clearInput();

    for (let i = 0 ; i < value.length; i++) {
      this.getInput(i).value = value[i];
    }

    this.updateValue();
  }

  private afterInputsLoaded(callback: Function) {
    if (this.inputElementsLoaded) {
      callback();
    } else {
      afterNextRender(this, () => {
        callback();
      });
    }
  }

  private focusNext(index: number) {
    if (index + 1 < this.length) {
      this.getInput(index + 1).focusInput();
    }
  }

  private focusPrev(index: number) {
    if (index > 0) {
      this.getInput(index - 1).focusInput();
    }
  }

  private getInputLabel(index: number) {
    return this.i18n('enterCharacter', index + 1, this.length);
  }

  private handleInput(value: string, index: number) {
    if (value.length) {
      this.focusNext(index);
      this.getInput(index).value = value.trim().toUpperCase()[0];
    }

    this.updateValue();
  }

  private handleBackspace(index: number) {
    if (index > 0 && !this.getInput(index).value.length) {
      this.getInput(index - 1).value = '';
      this.focusPrev(index);
    } else if (this.getInput(index).inputElement.selectionStart === 0) {
      this.focusPrev(index);
    }

    this.updateValue();
  }

  private updateValue() {
    for (let i = 0; i < this.length; i++) {
      this.inputs[i] = this.getInput(i).value;
    }
    this.value = ''.concat(...this.inputs);
    this.dispatchEvent(new CustomEvent('access-code-input', {
      detail: {value: this.value}
    }));
  }
}

customElements.define(CodeInputElement.is, CodeInputElement);
