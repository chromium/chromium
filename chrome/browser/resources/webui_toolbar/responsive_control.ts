// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {OverflowMenuItem} from '/shared/toolbar_ui_api.mojom-webui.js';

/**
 * Interface for elements whose sizing or visibility responds dynamically
 * to toolbar layout width changes. When a ResponsiveControl's minumum
 * or preferred width changes (or the behavior of its expandUpToPreferredWidth()
 * method), it must trigger a layout by dispayching a `request-layout`
 * CustomEvent with `{bubbles: true, composed: true}` to cause the toolbar to
 * layout its controls again.
 */
export interface ResponsiveControl extends EventTarget {
  /**
   * Whether the control is pinned/enabled and should be shown if it fits.
   */
  shouldBeShown(): boolean;

  /**
   * Sets the control to its minimum width. If the element can be hidden, it is.
   */
  setToMinWidth(): void;

  /**
   * Unconditionally sets the control to its preferred width without considering
   * window sizing or other control state.
   */
  setToPreferredWidth(): void;

  /**
   * Expands the control up to its preferred width. If expanding causes the
   * container to overflow, the control is responsible for shrinking back.
   */
  expandUpToPreferredWidth(): void;

  /**
   * Returns items of controls that are hidden and therefore
   * need to be added to the overflow menu. Returns an empty Array if there are
   * no such controls managed by this ResponsiveControl.
   */
  controlsToAddToOverflowMenu(): OverflowMenuItem[];
}
