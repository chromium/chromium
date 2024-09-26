// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.accessibility.hierarchysnapshotter;

import org.chromium.base.ServiceLoaderUtil;

/**
 * Base class to handle the HierarchySnapshotter functionality. This will allow Chrome to output
 * custom AccessibilityNodeInfo attributes during ui dumps, such as those used by uiautomator or
 * go/HSV.
 */
public class HierarchySnapshotter {
    /** Initialize a HierarchySnapshotter. */
    public static void initialize() {
        HierarchySnapshotterDelegate delegate =
                ServiceLoaderUtil.maybeCreate(HierarchySnapshotterDelegate.class);
        if (delegate != null) {
            delegate.initialize();
        }
    }
}
