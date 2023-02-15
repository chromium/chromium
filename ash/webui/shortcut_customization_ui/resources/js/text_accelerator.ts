// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './input_key.js';
import 'chrome://resources/polymer/v3_0/iron-icon/iron-icon.js';

import {IronIconElement} from 'chrome://resources/polymer/v3_0/iron-icon/iron-icon.js';
import {PolymerElementProperties} from 'chrome://resources/polymer/v3_0/polymer/interfaces.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {InputKeyElement, KeyInputState} from './input_key.js';
import {mojoString16ToString} from './mojo_utils.js';
import {TextAcceleratorPart, TextAcceleratorPartType} from './shortcut_types.js';
import {isCustomizationDisabled} from './shortcut_utils.js';
import {getTemplate} from './text_accelerator.html.js';

/**
 * @fileoverview
 * 'text-accelerator' is a wrapper component for the text of shortcuts that
 * have a kText LayoutStyle. It is responsible for displaying arbitrary text
 * that is passed into it, as well as styling key elements in the text.
 */
export class TextAcceleratorElement extends PolymerElement {
  static get is(): string {
    return 'text-accelerator';
  }

  static get properties(): PolymerElementProperties {
    return {
      parts: {
        type: Array,
        observer: TextAcceleratorElement.prototype.parseAndDisplayTextParts,
      },
    };
  }

  parts: TextAcceleratorPart[];

  private parseAndDisplayTextParts(): void {
    const container =
        this.shadowRoot!.querySelector('.parts-container') as HTMLDivElement;
    container.innerHTML = '';
    const textParts: Node[] = [];
    for (const part of this.parts) {
      const text = mojoString16ToString(part.text);
      if (part.type === TextAcceleratorPartType.kPlainText) {
        textParts.push(this.createPlainTextPart(text));
      } else if (part.type === TextAcceleratorPartType.kDelimiter) {
        textParts.push(this.createDelimiterIconPart());
      } else {
        textParts.push(this.createInputKeyPart(text, part.type));
      }
    }

    container.append(...textParts);
  }

  private createDelimiterIconPart(): IronIconElement {
    const icon = document.createElement('iron-icon');
    icon.classList.add('spacing');
    icon.icon = this.getIconForDelimiter();
    icon.id = 'delimiter-icon';
    return icon;
  }

  private createInputKeyPart(keyText: string, type: TextAcceleratorPartType):
      InputKeyElement {
    const keyState = type === TextAcceleratorPartType.kModifier ?
        KeyInputState.MODIFIER_SELECTED :
        KeyInputState.ALPHANUMERIC_SELECTED;
    const key = document.createElement('input-key');
    key.key = keyText;
    key.keyState = keyState;
    return key;
  }

  private getIconForDelimiter(): string {
    // Update if/when more delimiters are added.
    return 'shortcut-customization-keys:plus';
  }

  private createPlainTextPart(text: string): HTMLSpanElement {
    const span = document.createElement('span');
    span.classList.add('spacing');
    span.innerText = text;
    return span;
  }

  private shouldShowLockIcon(): boolean {
    return !isCustomizationDisabled();
  }

  static get template(): HTMLTemplateElement {
    return getTemplate();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'text-accelerator': TextAcceleratorElement;
  }
}

customElements.define(TextAcceleratorElement.is, TextAcceleratorElement);