// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
import {CustomElement} from 'chrome://resources/js/custom_element.js';

import {getTemplate} from './mojo_timedelta.html.js';

class MojoTimeDeltaElement extends CustomElement {
  static observedAttributes = ['duration'];

  static override get template() {
    return getTemplate();
  }

  attributeChangedCallback(name: string, _oldValue: string, newValue: string) {
    // The "duration" attribute should be specified in microseconds.
    if (name === 'duration') {
      const elem = this.shadowRoot!.querySelector<HTMLElement>('#duration')!;
      // TODO(b/308167671): format the microseconds duration as human-readable.
      elem.textContent = newValue + ' microseconds';
    }
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'mojo-timedelta': MojoTimeDeltaElement;
  }
}

customElements.define('mojo-timedelta', MojoTimeDeltaElement);
