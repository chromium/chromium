// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Interface for elements whose sizing or visibility responds dynamically
 * to toolbar layout width changes.
 */
export interface ResponsiveControl {
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
   * window sizing or other control state. This is exposed to all tests to
   * determine the preferred size of individual controls.
   */
  setToPreferredWidth(): void;

  /**
   * Expands the control up to its preferred width. If expanding causes the
   * container to overflow, the control is responsible for shrinking back.
   */
  expandUpToPreferredWidth(): void;

  /**
   * Returns true if the state of the control has changed in a way likely to
   * require a new layout of the toolbar, which generally means its min or
   * preferred size has changed. When this returns true, subsequent calls will
   * return false until another change occurs that may require another layout.
   * Return value may only be changed when controls are all being updated in an
   * updated() call, as the only time the return value is checked is immediately
   * after all updated() calls have completed.
   */
  consumeNeedsLayout(): boolean;
}
