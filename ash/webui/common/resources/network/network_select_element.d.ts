// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
import {OncMojo} from '//resources/ash/common/network/onc_mojo.js';

import {NetworkList} from './network_list_types';

interface NetworkSelectElement extends HTMLElement {
  refreshNetworks(): void;
  focus(): void;
  getDefaultNetwork(): OncMojo.NetworkStateProperties;
  getNetwork(guid: string): OncMojo.NetworkStateProperties;
  getNetworkListItemByNameForTest(name: string): null
      |NetworkList.NetworkListItemType;
}
