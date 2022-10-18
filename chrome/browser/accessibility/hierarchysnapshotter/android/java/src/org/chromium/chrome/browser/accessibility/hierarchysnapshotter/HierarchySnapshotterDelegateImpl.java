// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.accessibility.hierarchysnapshotter;

/**
 * Instantiable version of {@link HierarchySnapshotterDelegate}, don't add anything to this class.
 * Downstream targets may provide a different implementation. In GN, we specify that {@link
 * HierarchySnapshotterDelegate} is compiled separately from its implementation; other targets
 * may specify a different HierarchySnapshotterDelegate implementation via GN.
 */
class HierarchySnapshotterDelegateImpl extends HierarchySnapshotterDelegate {}
