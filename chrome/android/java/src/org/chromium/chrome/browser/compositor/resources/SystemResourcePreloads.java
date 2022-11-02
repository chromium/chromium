// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor.resources;

import org.chromium.chrome.browser.toolbar.ToolbarFeatures;
import org.chromium.ui.resources.SystemUIResourceType;

/**
 * List of high priority system resources that should be loaded at startup to be used by CC layers.
 */
public class SystemResourcePreloads {
    private static final int[] sEmptyList = new int[] {};

    private static final int[] sAsynchronousResources =
            new int[] {SystemUIResourceType.OVERSCROLL_GLOW};

    public static int[] getSynchronousResources() {
        return sEmptyList;
    }

    public static int[] getAsynchronousResources() {
        return ToolbarFeatures.shouldSuppressCaptures() ? sAsynchronousResources : sEmptyList;
    }
}
