// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/shared_vars_css.m.js';

import {afterNextRender, html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

export interface CodeInputElement {
  $: {
    inputElement: HTMLInputElement;
    container: HTMLDivElement;
  }
}

export class CodeInputElement extends PolymerElement {
  static get is() {
    return 'c2c-code-input';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      disabled: {
        type: Boolean,
        observer: 'disabledChange'
      },
      length: Number,
      value: {
        type: String,
        value: '',
      }
    };
  }

  length: number;
  value: string;
  focused: boolean;
  disabled: boolean;
  private charDisplayBoxes: string[];
  private boxesLoaded: boolean;

  constructor() {
    super();
    this.boxesLoaded = false;
    this.focused = false;

    afterNextRender(this, () => {
      this.boxesLoaded = true;
    });
  }

  ready() {
    super.ready();
    this.charDisplayBoxes = Array(this.length).fill('');
    this.$.container.style.width = (48 * this.length + 28) + 'px';
    this.$.inputElement.style.width = (48 * this.length + 2) + 'px';
    this.$.inputElement.maxLength = this.length;
  }

  clearInput() {
    this.setValue('');
  }

  focusInput() {
    this.afterBoxesLoaded(() => {
      this.$.inputElement.focus();
      this.showSelection();
    });
  }

  getBox(boxIndex: number) {
    return this.shadowRoot!.querySelector('#box-' + boxIndex)!;
  }

  getDisplayChar(charIndex: number) {
    return this.shadowRoot!.querySelector('#char-' + charIndex)!;
  }

  setValue(value: string) {
    if (value.length > this.length) return;

    this.afterBoxesLoaded(() => {
      this.$.inputElement.value = value;
      this.renderCharDisplay();
    });
  }

  private afterBoxesLoaded(callback: Function) {
    if (this.boxesLoaded) {
      callback();
    } else {
      afterNextRender(this, () => {
        callback();
      });
    }
  }

  private disabledChange() {
    if (this.disabled) {
      this.$.container.classList.add('disabled');
    } else {
      this.$.container.classList.remove('disabled');
    }
  }

  private selectAndRender() {
    this.showSelection();
    this.renderCharDisplay();
  }

  private hideFocus() {
    this.focused = false;
    for (let i = 0; i < this.length; i++) {
      this.getBox(i).classList.remove('focused');
      this.getBox(i).classList.remove('selected');
      this.getBox(i).classList.remove('cursor');
      this.getBox(i).classList.remove('cursor-start');
    }
  }

  private highlight(start: number, end: number) {
    for (let i = 0; i < this.length; i++) {
      this.getBox(i).classList.remove('cursor');
      if (i >= start && i < end) {
        this.getBox(i).classList.add('selected');
      } else {
        this.getBox(i).classList.remove('selected');
      }
    }
  }

  private placeCursor(cursorIndex: number) {
    for (let i = 0; i < this.length; i++) {
      this.getBox(i).classList.remove('selected');
      this.getBox(i).classList.remove('cursor');
    }
    if (cursorIndex - 1 >= 0) {
        this.getBox(cursorIndex - 1).classList.add('cursor');
    }
    if (cursorIndex === 0) {
      this.getBox(0).classList.add('cursor-start');
    } else {
      this.getBox(0).classList.remove('cursor-start');
    }
  }

  private renderCharDisplay() {
    const input = this.$.inputElement;
    this.value = input.value.toUpperCase();
    this.dispatchEvent(new CustomEvent('access-code-input', {
      detail: {value: this.value}
    }));
    for (let i = 0; i < this.length; i++) {
      if (i < this.value.length) {
        this.getDisplayChar(i).innerHTML = this.value[i];
      } else {
        this.getDisplayChar(i).innerHTML = '';
      }
    }
  }

  private showFocus() {
    this.focused = true;
    for (let i = 0; i < this.length; i++) {
        this.getBox(i).classList.add('focused');
    }
  }

  private showSelection() {
    const startIndex = this.$.inputElement.selectionStart!;
    const endIndex = this.$.inputElement.selectionEnd!;
    if (startIndex === endIndex) {
        this.placeCursor(startIndex);
    } else {
        this.highlight(startIndex, endIndex);
    }
  }
}

customElements.define(CodeInputElement.is, CodeInputElement);
