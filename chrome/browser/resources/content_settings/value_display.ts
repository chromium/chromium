// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './mojo_timestamp.js';
import './expandable_json_viewer.js';

import {CustomElement} from 'chrome://resources/js/custom_element.js';
import type {Value} from 'chrome://resources/mojo/mojo/public/mojom/base/values.mojom-webui.js';

import sheet from './value_display.css' with {type : 'css'};
import {getTemplate} from './value_display.html.js';

export type LogicalFn = (v: Value) => HTMLElement|undefined;

export function defaultLogicalFn(_v: Value): HTMLElement|undefined {
  return undefined;
}

export function timestampLogicalFn(v: Value): HTMLElement|undefined {
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

  constructor() {
    super();
    this.shadowRoot!.adoptedStyleSheets = [sheet];
  }

  flattenValue(value: Value): unknown {
    if (value.listValue != null) {
      return value.listValue.storage.map(v => this.flattenValue(v));
    } else if (value.dictionaryValue != null) {
      const flattenedDictionary: {[key: string]: unknown} = {};
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
    const typeElement = this.getRequiredElement<HTMLElement>('#type');
    const valueElement = this.getRequiredElement<HTMLElement>('#value');
    const logicalValueElement =
        this.getRequiredElement<HTMLElement>('#logical-value');

    const logicalElem = logicalFn(value);
    if (logicalElem !== undefined) {
      logicalValueElement.classList.add('defined');
      logicalValueElement.appendChild(logicalElem);
    }

    if (value.boolValue != null) {
      if (value.boolValue) {
        valueElement.textContent = 'true';
        valueElement.classList.add('bool-true');
      } else {
        valueElement.textContent = 'false';
        valueElement.classList.add('bool-false');
      }
    } else if (value.intValue != null) {
      typeElement.textContent = '(int)';
      valueElement.textContent = '' + value.intValue;
    } else if (value.stringValue != null) {
      typeElement.textContent = '(string)';
      valueElement.textContent = value.stringValue;
    } else if (value.nullValue != null) {
      valueElement.textContent = 'null';
      valueElement.classList.add('none');
    } else if (value.listValue != null || value.dictionaryValue != null) {
      // The pre element is used to preserve line breaks and spaces
      const jsonValueElement = document.createElement('pre');
      jsonValueElement.id = 'json-value';
      jsonValueElement.textContent =
          JSON.stringify(this.flattenValue(value), null, 2);

      const jsonViewerElement =
          document.createElement('expandable-json-viewer');

      valueElement.appendChild(jsonViewerElement);
      jsonViewerElement.configure(jsonValueElement, title);
    } else if (value.binaryValue != null) {
      typeElement.textContent = '(binary)';
      valueElement.textContent = JSON.stringify(value.binaryValue);
    } else {
      typeElement.textContent = '(???)';
      valueElement.textContent = JSON.stringify(value);
    }
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'value-display': ValueDisplayElement;
  }
}

customElements.define('value-display', ValueDisplayElement);
