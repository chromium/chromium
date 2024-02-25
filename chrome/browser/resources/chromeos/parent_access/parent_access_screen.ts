// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * An interface to be implemented by the generic Parent Access screens that
 * render different content depending on flow type.
 */
export interface ParentAccessScreen {
  /** Renders the correct content for the screen depending on the flow type. */
  renderFlowSpecificContent(): void;
}
