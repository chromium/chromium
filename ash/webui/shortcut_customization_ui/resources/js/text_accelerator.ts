// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './input_key.js';

import {StrictQueryMixin} from 'chrome://resources/ash/common/typescript_utils/strict_query_mixin.js';
import {PolymerElementProperties} from 'chrome://resources/polymer/v3_0/polymer/interfaces.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {KeyInputState} from './input_key.js';
import {mojoString16ToString} from './mojo_utils.js';
import {TextAcceleratorPart, TextAcceleratorPartType} from './shortcut_types.js';
import {getTemplate} from './text_accelerator.html.js';

/**
 * @fileoverview
 * 'text-accelerator' is a wrapper component for the text of shortcuts that
 * have a kText LayoutStyle. It is responsible for displaying arbitrary text
 * that is passed into it, as well as styling key elements in the text.
 */

const TextAcceleratorElementBase = StrictQueryMixin(PolymerElement);

export class TextAcceleratorElement extends TextAcceleratorElementBase {
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
    let finalHtml = '';
    for (const part of this.parts) {
      const text = mojoString16ToString(part.text);
      if (part.type === TextAcceleratorPartType.kPlainText) {
        finalHtml += text;
      } else {
        finalHtml += this.createInputKeyHtmlString(text, part.type);
      }
    }
    this.strictQueryDiv('#text-wrapper').innerHTML = finalHtml;
  }

  private createInputKeyHtmlString(text: string, type: TextAcceleratorPartType):
      string {
    const keyState = type === TextAcceleratorPartType.kModifier ?
        KeyInputState.MODIFIER_SELECTED :
        KeyInputState.ALPHANUMERIC_SELECTED;
    return `<input-key key="${text}" key-state=${keyState}></input-key>`;
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