// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {CrButtonElement} from 'chrome://resources/cr_elements/cr_button/cr_button.js';
import {CrToggleElement} from 'chrome://resources/cr_elements/cr_toggle/cr_toggle.js';
import {LegacyElementMixin} from 'chrome://resources/polymer/v3_0/polymer/lib/legacy/legacy-element-mixin.js';

interface NetworkSiminfoElement extends LegacyElementMixin, HTMLElement {
  getUnlockButton(): CrButtonElement|null;
  getSimLockToggle(): CrToggleElement|null;
}

declare global {
  interface HTMLElementTagNameMap {
    'network-siminfo': NetworkSiminfoElement;
  }
}

export {NetworkSiminfoElement};
