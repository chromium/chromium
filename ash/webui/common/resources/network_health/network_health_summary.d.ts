// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

export class NetworkHealthSummaryElement extends PolymerElement {}

declare global {
  interface HTMLElementTagNameMap {
    'network-health-summary': NetworkHealthSummaryElement;
  }
}
