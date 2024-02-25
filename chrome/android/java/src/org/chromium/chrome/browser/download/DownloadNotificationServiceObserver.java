// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download;

import org.chromium.build.annotations.UsedByReflection;

/** A DownloadForegroundServiceObservers.Observer implementation for DownloadNotificationService. */
@UsedByReflection("DownloadForegroundServiceObservers")
public class DownloadNotificationServiceObserver
        implements DownloadForegroundServiceObservers.Observer {
    @UsedByReflection("DownloadForegroundServiceObservers")
    public DownloadNotificationServiceObserver() {}

    @Override
    public void onForegroundServiceTaskRemoved() {
        DownloadNotificationService.getInstance().onForegroundServiceTaskRemoved();
    }

    @Override
    public void onForegroundServiceDestroyed() {
        DownloadNotificationService.getInstance().onForegroundServiceDestroyed();
    }
}
