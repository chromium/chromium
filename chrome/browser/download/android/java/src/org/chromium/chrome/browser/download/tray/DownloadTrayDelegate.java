// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download.tray;

import org.chromium.build.annotations.NullMarked;
import org.chromium.components.offline_items_collection.OfflineItem;

/**
 * Actions the download tray needs from outside the download module, e.g. opening files, which
 * depends on {@code //chrome/android}.
 */
@NullMarked
public interface DownloadTrayDelegate {
    /** Opens a completed download. */
    void openItem(OfflineItem item);
}
