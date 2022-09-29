// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

export interface ArcAccountPickerAppElement extends HTMLElement {
  loadAccounts(): Promise<boolean>;
}

declare global {
  interface HTMLElementTagNameMap {
    'arc-account-picker-app': ArcAccountPickerAppElement;
  }
}
