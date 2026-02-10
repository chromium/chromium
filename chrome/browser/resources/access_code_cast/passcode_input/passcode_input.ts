// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';
import type {PropertyValues} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {getCss} from './passcode_input.css.js';
import {getHtml} from './passcode_input.html.js';

export interface PasscodeInputElement {
  $: {
    inputElement: HTMLInputElement,
    container: HTMLElement,
  };
}

export class PasscodeInputElement extends CrLitElement {
  static get is() {
    return 'c2c-passcode-input';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      charDisplayBoxes: {type: Array},
      disabled: {type: Boolean},
      focused: {type: Boolean},
      length: {type: Number},
      startIndex: {type: Number},
      endIndex: {type: Number},
      value: {type: String, notify: true, reflect: true},
    };
  }

  protected accessor charDisplayBoxes: string[] = [];
  accessor disabled: boolean = false;
  accessor length: number = 0;
  accessor value: string = '';
  accessor focused: boolean = false;
  private accessor startIndex: number = -1;
  private accessor endIndex: number = -1;

  private static readonly PASSCODE_INPUT_SIZE = 40;
  private static readonly PASSCODE_BOX_SPACING = 8;

  override willUpdate(changedProperties: PropertyValues<this>) {
    super.willUpdate(changedProperties);
    if (changedProperties.has('length') || changedProperties.has('value')) {
      const newBoxes = Array(this.length).fill('');
      for (let i = 0; i < this.length; i++) {
        newBoxes[i] = i < this.value.length ? this.value[i] : '';
      }
      this.charDisplayBoxes = newBoxes;
    }
  }

  override firstUpdated(changedProperties: PropertyValues<this>) {
    super.firstUpdated(changedProperties);
    const boxWithMarginWidth = PasscodeInputElement.PASSCODE_INPUT_SIZE +
      PasscodeInputElement.PASSCODE_BOX_SPACING;
    const elementBaseWidth = boxWithMarginWidth * this.length;
    this.$.container.style.width = elementBaseWidth +
      (0.5 * PasscodeInputElement.PASSCODE_INPUT_SIZE) +
      PasscodeInputElement.PASSCODE_BOX_SPACING + 'px';
    const inputEl = this.$.inputElement;
    inputEl.style.width = (elementBaseWidth + /* input border */ 2) + 'px';
  }

  override updated(changedProperties: PropertyValues<this>) {
    super.updated(changedProperties);

    if (changedProperties.has('value')) {
      if (this.$.inputElement.value.toUpperCase() !== this.value) {
        this.$.inputElement.value = this.value;
      }
      // Make sure value is all uppercase.
      this.value = this.value.toUpperCase();
    }
  }

  getDisplayChar(charIndex: number) {
    return this.shadowRoot.querySelector<HTMLElement>('#char-' + charIndex)!;
  }

  focusInput() {
    this.$.inputElement.focus();
    this.focused = true;
    this.renderSelection();
  }

  protected getDisabledClass(): string {
    return this.disabled ? 'disabled' : '';
  }

  protected getCharBoxClass(index: number): string {
    const focused = this.focused ? 'focused' : '';
    const active = (this.endIndex === -1 && index === this.startIndex) ||
            (this.endIndex > -1 && index >= this.startIndex &&
             index < this.endIndex) ?
        'active' :
        '';
    return [focused, active].join(' ');
  }

  // Make the char boxes from startIndex to endIndex (including startIndex and
  // not including endIndex) active. This highlights these boxes. If only
  // startIndex is passed, then make active only that char box. Passing -1
  // makes all boxes inactive.
  private makeActive(startIndex: number, endIndex?: number) {
    this.startIndex = startIndex;
    this.endIndex = endIndex === undefined ? -1 : endIndex;
  }

  protected renderSelection() {
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
    const chars = this.shadowRoot.querySelectorAll<HTMLElement>('.char');
    chars.forEach(char => {
      char.classList.remove('cursor-filled', 'cursor-empty', 'cursor-start');
    });
  }

  protected handleOnFocus() {
    this.focused = true;
  }

  protected handleOnBlur() {
    this.focused = false;
    this.removeCursor();
    this.makeActive(-1);
  }

  protected async handleOnInput() {
    this.value = this.$.inputElement.value.toUpperCase();
    await this.updateComplete;
    this.renderSelection();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'c2c-passcode-input': PasscodeInputElement;
  }
}

customElements.define(PasscodeInputElement.is, PasscodeInputElement);
