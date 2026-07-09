// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/// BEGIN_GENERATED - DO NOT MODIFY BELOW
import * as generated from './check_api_cases_imported.js';
export import InterfaceA = generated.InterfaceA;
export import InterfaceB = generated.InterfaceB;
export import ExtEnum1 = generated.ExtEnum1;
export import ExtEnum2 = generated.ExtEnum2;
export import ClosedEnum1 = generated.ClosedEnum1;
/// END_GENERATED - DO NOT MODIFY ABOVE

declare module './check_api_cases_imported.js' {
  export interface InterfaceB {
    // ErrorAddRequiredMethodInAugmentation:edit-add-lines:
    // newRequiredInAugmentation(): void;

    // OkAddOptionalMethodInAugmentation:edit-add-lines:
    // newOptionalInAugmentation?(): void;
  }
}

export interface PrivateTypes {
  privateTypes: PrivateTypes;
  closedEnums: ClosedEnums;
}

export interface ClosedEnums {
  closedEnum1: typeof ClosedEnum1;
}
