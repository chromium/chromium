// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {OncMojo} from 'chrome://resources/ash/common/network/onc_mojo.js';

export class NetworkIconElement extends PolymerElement {
  networkState: OncMojo.NetworkStateProperties|undefined;
  isListItem: boolean;
}

declare global {
  interface HTMLElementTagNameMap {
    'network-icon': NetworkIconElement;
  }
}
