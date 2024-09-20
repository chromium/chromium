// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

export class SimLockDialogsElement extends PolymerElement {
  isDialogOpen: boolean;
  closeDialogsForTest(): void;
}

declare global {
  interface HTMLElementTagNameMap {
    'sim-lock-dialogs': SimLockDialogsElement;
  }
}
