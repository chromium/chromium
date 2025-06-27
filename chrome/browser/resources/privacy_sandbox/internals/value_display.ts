// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
import './mojo_timestamp.js';
import './expandable_json_viewer.js';

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

// TODO(crbug.com/427549893): Don't use expandable-json-viewer if JSON content
// is {} or [].
export class ValueDisplayElement extends CustomElement {
  static override get template() {
    return getTemplate();
  }

  flattenValue(value: Value): any {
    if (value.listValue != null) {
      return value.listValue.storage.map(v => this.flattenValue(v));
    } else if (value.dictionaryValue != null) {
      const flattenedDictionary: {[key: string]: any} = {};
      for (const [k, v] of Object.entries(value.dictionaryValue.storage)) {
        flattenedDictionary[k] = this.flattenValue(v);
      }
      return flattenedDictionary;
    } else {
      return value;
    }
  }

  configure(
      value: Value, logicalFn: LogicalFn = defaultLogicalFn,
      title: string = '') {
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

    } else if (value.listValue != null || value.dictionaryValue != null) {
      // The pre element is used to preserve line breaks and spaces
      const jsonValueElement = document.createElement('pre');
      jsonValueElement.id = 'json-value';
      jsonValueElement.textContent =
          JSON.stringify(this.flattenValue(value), null, 2);

      const jsonViewerElement =
          document.createElement('expandable-json-viewer');

      vElem.appendChild(jsonViewerElement);
      jsonViewerElement.configure(jsonValueElement, title);

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
