// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
import './mojo_timestamp.js';

import {CustomElement} from 'chrome://resources/js/custom_element.js';
import type {Value} from 'chrome://resources/mojo/mojo/public/mojom/base/values.mojom-webui.js';

import {getTemplate} from './value_display.html.js';

export type LogicalFn = (v: Value) => HTMLElement|undefined;

export function defaultLogicalFn(_v: Value) {
  return undefined;
}
export function timestampLogicalFn(v: Value) {
  if (!v.stringValue) {
    return undefined;
  }
  const tsElement = document.createElement('mojo-timestamp');
  tsElement.setAttribute('ts', v.stringValue);
  return tsElement;
}

export class ValueDisplayElement extends CustomElement {
  static override get template() {
    return getTemplate();
  }

  configure(value: Value, logicalFn: LogicalFn = defaultLogicalFn) {
    const tElem = this.shadowRoot!.querySelector<HTMLElement>(`#type`)!;
    const vElem = this.shadowRoot!.querySelector<HTMLElement>(`#value`)!;
    const lElem =
        this.shadowRoot!.querySelector<HTMLElement>(`#logical-value`)!;

    const logicalElem = logicalFn(value);
    if (logicalElem !== undefined) {
      lElem.classList.add('defined');
      lElem.appendChild(logicalElem);
    }

    if (value.boolValue != null) {
      if (value.boolValue) {
        vElem.textContent = 'true';
        vElem.classList.add('bool-true');
      } else {
        vElem.textContent = 'false';
        vElem.classList.add('bool-false');
      }

    } else if (value.intValue != null) {
      tElem.textContent = '(int)';
      vElem.textContent = '' + value.intValue;

    } else if (value.stringValue != null) {
      tElem.textContent = '(string)';
      vElem.textContent = value.stringValue;

    } else if (value.nullValue != null) {
      vElem.textContent = 'null';
      vElem.classList.add('none');

    } else if (value.listValue != null) {
      tElem.textContent = '(list)';
      vElem.textContent = JSON.stringify(value.listValue.storage);

    } else if (value.dictionaryValue != null) {
      tElem.textContent = '(dictionary)';
      vElem.textContent = JSON.stringify(value.dictionaryValue.storage);

    } else if (value.binaryValue != null) {
      tElem.textContent = '(binary)';
      vElem.textContent = JSON.stringify(value.binaryValue);

    } else {
      tElem.textContent = '(???)';
      vElem.textContent = JSON.stringify(value);
    }
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'value-display': ValueDisplayElement;
  }
}

customElements.define('value-display', ValueDisplayElement);
