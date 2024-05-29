// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {LegacyElementMixin} from '//resources/polymer/v3_0/polymer/lib/legacy/legacy-element-mixin.js';

import {NetworkList} from './network_list_types.js';
import {OncMojo} from './onc_mojo.js';

interface NetworkListElement extends LegacyElementMixin, HTMLElement {
  networks: OncMojo.NetworkStateProperties[];
  customItems: NetworkList.CustomItemState[];
  disabled: boolean;
  isBuiltInVpnManagementBlocked: boolean;
}

declare global {
  interface HTMLElementTagNameMap {
    'network-list': NetworkListElement;
  }
}

export {NetworkListElement};
