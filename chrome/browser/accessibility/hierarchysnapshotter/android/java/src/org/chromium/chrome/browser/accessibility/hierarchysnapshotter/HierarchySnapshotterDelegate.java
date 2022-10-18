// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.accessibility.hierarchysnapshotter;

/**
 * Base class for defining methods where different behavior is required by downstream targets for
 * the HierarchySnapshotter. The correct version of {@link HierarchySnapshotterDelegateImpl} will
 * be determined at compile time via build runs.
 */
public class HierarchySnapshotterDelegate {
    /** @see {@link HierarchySnapshotter#initialize()} */
    public void initialize() {}
}
