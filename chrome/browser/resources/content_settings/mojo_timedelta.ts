// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {CustomElement} from 'chrome://resources/js/custom_element.js';

import sheet from './mojo_shared.css' with {type : 'css'};
import {getTemplate} from './mojo_timedelta.html.js';

export class MojoTimedeltaElement extends CustomElement {
  static observedAttributes = ['duration'];

  static override get template() {
    return getTemplate();
  }

  constructor() {
    super();
    this.shadowRoot!.adoptedStyleSheets = [sheet];
  }

  attributeChangedCallback(name: string, _oldValue: string, newValue: string) {
    // The "duration" attribute should be specified in microseconds.
    if (name === 'duration') {
      const elem = this.getRequiredElement<HTMLElement>('#duration');
      elem.textContent = `${newValue} microseconds`;
    }
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'mojo-timedelta': MojoTimedeltaElement;
  }
}

customElements.define('mojo-timedelta', MojoTimedeltaElement);
