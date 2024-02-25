// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.hub;

import com.google.common.collect.ImmutableSet;

/** Interface for specifying the order of {@link Pane}s in the Hub. **/
public interface PaneOrderController {
    /** Returns an ordered set of {@link PaneId} representing the order of Panes in the Hub. **/
    public ImmutableSet<Integer> getPaneOrder();
}
