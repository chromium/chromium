// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download;

import org.chromium.chrome.browser.profiles.OtrProfileId;
import org.chromium.components.offline_items_collection.ContentId;

/** Interface for classes implementing concrete implementation of UI behavior. */
public interface DownloadServiceDelegate {
    /**
     * Called to cancel a download.
     *
     * @param id The {@link ContentId} of the download to cancel.
     * @param otrProfileId The {@link OtrProfileId} of the download. Null if in regular mode.
     */
    void cancelDownload(ContentId id, OtrProfileId otrProfileId);

    /**
     * Called to pause a download.
     *
     * @param id The {@link ContentId} of the download to pause.
     * @param otrProfileId The {@link OtrProfileId} of the download. Null if in regular mode.
     */
    void pauseDownload(ContentId id, OtrProfileId otrProfileId);

    /**
     * Called to resume a paused download.
     *
     * @param id The {@link ContentId} of the download to cancel.
     * @param item Download item to resume. TODO(fgorski): Update the interface to not require
     *     download item.
     */
    void resumeDownload(ContentId id, DownloadItem item);

    /** Called to destroy the delegate, in case it needs to be destroyed. */
    void destroyServiceDelegate();
}
