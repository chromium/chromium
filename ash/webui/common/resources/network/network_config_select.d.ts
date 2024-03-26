// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {LegacyElementMixin} from '//resources/polymer/v3_0/polymer/lib/legacy/legacy-element-mixin.js';

interface NetworkConfigSelectElement extends LegacyElementMixin, HTMLElement {
  items: Array<string|number>;
  value: string|number;
}

declare global {
  interface HTMLElementTagNameMap {
    'network-config-select': NetworkConfigSelectElement;
  }
}

export {NetworkConfigSelectElement};
