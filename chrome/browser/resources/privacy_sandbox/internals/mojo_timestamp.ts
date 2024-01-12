// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
import {CustomElement} from 'chrome://resources/js/custom_element.js';

import {getTemplate} from './mojo_timestamp.html.js';

class MojoTimestampElement extends CustomElement {
  static observedAttributes = ['ts'];

  static override get template() {
    return getTemplate();
  }

  mojoTsToJsDate(mojoTs: bigint) {
    // The Javascript `Date()` is based off of the number of milliseconds since
    // the UNIX epoch (1970-01-01 00::00:00 UTC), while `internalValue``
    // of the `base::Time` (represented in mojom.Time) represents the
    // number of microseconds since the Windows FILETIME epoch
    // (1601-01-01 00:00:00 UTC). This computes the final Javascript time by
    // computing the epoch delta and the conversion from microseconds to
    // milliseconds.
    const windowsEpoch = Date.UTC(1601, 0, 1, 0, 0, 0, 0);
    const unixEpoch = Date.UTC(1970, 0, 1, 0, 0, 0, 0);
    // `epochDeltaInMs` is equal to `base::Time::kTimeTToMicrosecondsOffset`.
    const epochDeltaInMs = unixEpoch - windowsEpoch;
    const timeInMs = Number(mojoTs) / 1000;
    return new Date(timeInMs - epochDeltaInMs);
  }

  attributeChangedCallback(name: string, _oldValue: string, newValue: string) {
    if (name === 'ts') {
      const elem = this.shadowRoot!.querySelector<HTMLElement>('#time')!;
      const ts: bigint = BigInt(newValue);
      if (ts === BigInt(0)) {
        elem.textContent = 'epoch';
        elem.classList.add('none');
      } else {
        const date = this.mojoTsToJsDate(ts);
        elem.textContent = date.toUTCString();
        elem.classList.remove('none');
      }
    }
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'mojo-timestamp': MojoTimestampElement;
  }
}

customElements.define('mojo-timestamp', MojoTimestampElement);
