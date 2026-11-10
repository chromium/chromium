// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.app.download;

import android.content.Context;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.download.DownloadOpenSource;
import org.chromium.chrome.browser.download.DownloadUtils;
import org.chromium.chrome.browser.download.tray.DownloadTrayDelegate;
import org.chromium.chrome.browser.profiles.OtrProfileId;
import org.chromium.components.offline_items_collection.OfflineItem;

/** Provides the download toolbar button tray with dependencies from {@code //chrome/android}. */
@NullMarked
public class DownloadTrayDelegateImpl implements DownloadTrayDelegate {
    private final Context mContext;

    /**
     * Creates an instance of {@link DownloadTrayDelegateImpl}.
     *
     * @param context The context of the window hosting the tray, used to open downloads.
     */
    public DownloadTrayDelegateImpl(Context context) {
        mContext = context;
    }

    @Override
    public void openItem(OfflineItem item) {
        // Unverified (as the download message does), as the incognito profile may have just been
        // closed.
        OtrProfileId otrProfileId = OtrProfileId.deserializeWithoutVerify(item.otrProfileId);
        @DownloadOpenSource int source = DownloadOpenSource.DOWNLOAD_TRAY;
        // The tray opens a file in a new Chrome tab when Chrome supports its format, and otherwise
        // falls back to the default app, as the download message does.
        if (DownloadUtils.openItemInChrome(item, mContext, otrProfileId, source)) return;
        DownloadUtils.openItem(item, otrProfileId, source, mContext);
    }
}
