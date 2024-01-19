// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Interface which provides alternate UI behaviors for the Cellular Setup
 * flows.
 */
export interface CellularSetupDelegate {
  /**
   * Return true if base page title text should be visible.
   */
  shouldShowPageTitle(): boolean;

  /**
   * Return true if cancel button should be visible.
   */
  shouldShowCancelButton(): boolean;
}
