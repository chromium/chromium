// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Minimal TypeScript definitions to satisfy cases where
 * multidevice_setup.js is used from TypeScript files.
 */

import {PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

export class MultiDeviceSetup extends PolymerElement {
  updateLocalizedContent(): void;
}

declare global {
  interface HTMLElementTagNameMap {
    'multidevice-setup': MultiDeviceSetup;
  }
}
