// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
import './value_display.js';
import './mojo_timestamp.js';

import {CustomElement} from 'chrome://resources/js/custom_element.js';
import type {Value} from 'chrome://resources/mojo/mojo/public/mojom/base/values.mojom-webui.js';

import {getTemplate} from './pref_display.html.js';
import {defaultLogicalFn} from './value_display.js';

export class PrefDisplayElement extends CustomElement {
  static override get template() {
    return getTemplate();
  }

  getElement(key: string) {
    return this.shadowRoot!.querySelector<HTMLElement>(`.id-${key}`)!;
  }

  configure(
      prefName: string, value: Value,
      valueLogicalFn: (v: Value) => HTMLElement |
          undefined = defaultLogicalFn) {
    const nameElem = this.getElement('pref-name');
    nameElem.textContent = prefName;
    const valueElem = this.getElement('pref-value');

    const v = document.createElement('value-display');
    v.configure(value, valueLogicalFn);
    valueElem.appendChild(v);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'pref-display': PrefDisplayElement;
  }
}

customElements.define('pref-display', PrefDisplayElement);
