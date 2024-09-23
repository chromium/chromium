// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {LegacyElementMixin} from '//resources/polymer/v3_0/polymer/lib/legacy/legacy-element-mixin.js';

interface NetworkConfigToggleElement extends LegacyElementMixin, HTMLElement {
  checked: boolean;
  disabled: boolean;
}

declare global {
  interface HTMLElementTagNameMap {
    'network-config-toggle': NetworkConfigToggleElement;
  }
}

export {NetworkConfigToggleElement};
