// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp;

import org.chromium.build.annotations.NullMarked;

import java.util.List;

/** Interface for entries that contain a list of {@link RecentlyClosedTab}s. */
@NullMarked
public interface TabGroupContainer {
    /** Returns the list of tabs in this container. */
    List<RecentlyClosedTab> getTabs();
}
