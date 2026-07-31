// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab_bottom_sheet;

import org.chromium.build.annotations.NullMarked;

/** Factory for creating {@link ResizingStrategy} instances. */
@NullMarked
public class ResizingStrategyFactory {
    /**
     * Creates a default strategy for managing resizing mode on {@link WebViewResizingHelper}.
     *
     * @param helper The {@link WebViewResizingHelper} to control resizing mode on.
     */
    public static ResizingStrategy create(WebViewResizingHelper helper) {
        return new DefaultResizingStrategy(helper);
    }
}
