// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {OncMojo} from '//resources/ash/common/network/onc_mojo.js';
import {PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {NetworkList} from './network_list_types.js';

export class NetworkSelectElement extends PolymerElement {
  customItems: NetworkList.CustomItemState[];

  refreshNetworks(): void;
  focus(): void;
  getDefaultNetwork(): OncMojo.NetworkStateProperties;
  getNetwork(guid: string): OncMojo.NetworkStateProperties;
  getNetworkListItemByNameForTest(name: string): null
      |NetworkList.NetworkListItemType;
}

declare global {
  interface HTMLElementTagNameMap {
    'network-select': NetworkSelectElement;
  }
}
