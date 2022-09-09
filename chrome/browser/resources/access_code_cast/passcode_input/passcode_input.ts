// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {afterNextRender, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './passcode_input.html.js';

type ForEachCallback = (el: HTMLParagraphElement|HTMLDivElement, index: number) => void;

export interface PasscodeInputElement {
  $: {
    inputElement: HTMLInputElement,
    container: HTMLDivElement,
  };
}

export class PasscodeInputElement extends PolymerElement {
  static get is() {
    return 'c2c-passcode-input';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      ariaLabel: {
        type: String,
        value: '',
      },
      disabled: {
        type: Boolean,
        observer: 'disabledChange',
      },
      length: Number,
      value: {
        type: String,
        value: '',
        observer: 'valueChange',
        notify: true,
        reflectToAttribute: true,
      },
    };
  }

  length: number;
  value: string;
  focused: boolean;
  disabled: boolean;
  private afterFirstRender: boolean;
  private charDisplayBoxes: string[];

  private static readonly PASSCODE_INPUT_SIZE = 40;
  private static readonly PASSCODE_BOX_SPACING = 8;

  constructor() {
    super();
    this.focused = false;
    this.afterFirstRender = false;
    afterNextRender(this, () => {
      this.afterFirstRender = true;
    });
  }

  override ready() {
    super.ready();
    this.charDisplayBoxes = Array(this.length).fill('');
    const boxWithMarginWidth = PasscodeInputElement.PASSCODE_INPUT_SIZE +
      PasscodeInputElement.PASSCODE_BOX_SPACING;
    const elementBaseWidth = boxWithMarginWidth * this.length;
    this.$.container.style.width = elementBaseWidth +
      (0.5 * PasscodeInputElement.PASSCODE_INPUT_SIZE) +
      PasscodeInputElement.PASSCODE_BOX_SPACING + 'px';
    const inputEl = this.$.inputElement;
    inputEl.style.width = (elementBaseWidth + /* input border */ 2) + 'px';
    inputEl.maxLength = this.length;

    // Set event listeners
    inputEl.addEventListener('blur', () => {
      this.handleOnBlur();
    });
    inputEl.addEventListener('click', () => {
      this.renderSelection();
    });
    inputEl.addEventListener('focus', () => {
      this.handleOnFocus();
    });
    inputEl.addEventListener('input', () => {
      this.handleOnInput();
    });
    inputEl.addEventListener('keyup', () => {
      this.renderSelection();
    });
    inputEl.addEventListener('select', () => {
      this.renderSelection();
    });
  }

  getCharBox(boxIndex: number) {
    const el = this.shadowRoot!.querySelector('#char-box-' + boxIndex)!;
    return el as HTMLDivElement;
  }

  getDisplayChar(charIndex: number) {
    const el = this.shadowRoot!.querySelector('#char-' + charIndex)!;
    return el as HTMLParagraphElement;
  }

  focusInput() {
    this.afterPageLoaded(() => {
      this.$.inputElement.focus();
      this.handleOnFocus();
      this.renderSelection();
    });
  }

  private disabledChange() {
    this.afterPageLoaded(() => {
      if (this.disabled) {
        this.forEach('char', (char) => {
          char.classList.add('disabled');
        });
      } else {
        this.forEach('char', (char) => {
          char.classList.remove('disabled');
        });
      }
    });
  }

  private valueChange() {
    if (this.$.inputElement.value.toUpperCase() !== this.value) {
      this.$.inputElement.value = this.value;
    }
    this.afterPageLoaded(() => {
      this.displayChars();
    });
  }

  // Make the char boxes from startIndex to endIndex (including startIndex and
  // not including endIndex) active. This highlights these boxes. If only
  // startIndex is passed, then make active only that char box. Passing -1
  // makes all boxes inactive.
  private makeActive(startIndex: number, endIndex?: number) {
    this.forEach('char-box', (charbox, index) => {
      if ((!endIndex && index === startIndex) ||
          (endIndex && index >= startIndex && index < endIndex)) {
        charbox.classList.add('active');
      } else {
        charbox.classList.remove('active');
      }
    });
  }

  private renderSelection() {
    if (!this.focused) {
      return;
    }

    const selectionStart = this.$.inputElement.selectionStart;
    const selectionEnd = this.$.inputElement.selectionEnd;
    if (selectionStart === null || selectionEnd === null) {
      return;
    }

    if (selectionStart !== null && selectionStart === selectionEnd) {
      if (selectionStart === 0) {
        this.makeActive(0);
      } else if (selectionStart === this.length ||
          this.getDisplayChar(selectionStart).innerText.length) {
        this.makeActive(selectionStart - 1);
      } else {
        this.makeActive(selectionStart);
      }
      this.placeCursor(selectionStart);
    } else {
      this.removeCursor();
      this.makeActive(selectionStart, selectionEnd);
    }
  }

  private placeCursor(cursorIndex: number) {
    this.removeCursor();

    if (cursorIndex < this.length && cursorIndex === this.value.length) {
      this.getDisplayChar(cursorIndex).classList.add('cursor-empty');
      return;
    }
    if (cursorIndex === 0) {
      this.getDisplayChar(0).classList.add('cursor-start');
      return;
    }
    this.getDisplayChar(cursorIndex - 1).classList.add('cursor-filled');
  }

  private removeCursor() {
    this.forEach('char', (char) => {
      char.classList.remove('cursor-filled', 'cursor-empty', 'cursor-start');
    });
  }

  private forEach(elementType: 'char'|'char-box', callback: ForEachCallback) {
    let el: HTMLDivElement | HTMLParagraphElement | null;
    for (let i = 0; i < this.length; i++) {
      el = this.shadowRoot!.querySelector('#' + elementType + '-' + i);
      if (el !== null) {
        callback(el, i);
      }
    }
  }

  private handleOnFocus() {
    this.focused = true;
    this.forEach('char-box', (charBox) => {
      charBox.classList.add('focused');
    });
  }

  private handleOnBlur() {
    this.focused = false;
    this.removeCursor();
    this.makeActive(-1);
    this.forEach('char-box', (charBox) => {
      charBox.classList.remove('focused');
    });
  }

  private handleOnInput() {
    this.displayChars();
    this.renderSelection();
  }

  private displayChars() {
    const input = this.$.inputElement;
    this.set('value', input.value.toUpperCase());
    this.forEach('char', (char, index) => {
      char.innerText = index < this.value.length ? this.value[index] : '';
    });
  }

  private async afterPageLoaded(callback: () => void) {
    if (this.afterFirstRender) {
      callback();
    } else {
      afterNextRender(this, callback);
    }
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'c2c-passcode-input': PasscodeInputElement;
  }
}

customElements.define(PasscodeInputElement.is, PasscodeInputElement);