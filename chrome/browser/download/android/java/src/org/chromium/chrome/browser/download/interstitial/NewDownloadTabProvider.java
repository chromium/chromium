// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download.interstitial;

import androidx.annotation.Nullable;

import org.chromium.base.UnownedUserDataKey;
import org.chromium.ui.base.WindowAndroid;

/** Provides the {@link NewDownloadTab} attached to a given {@link WindowAndroid}. */
public class NewDownloadTabProvider {
    private static final UnownedUserDataKey<NewDownloadTab> KEY =
            new UnownedUserDataKey<>(NewDownloadTab.class);

    /**
     * @param windowAndroid The {@link WindowAndroid} the {@link NewDownloadTab} is attached to.
     * @return The {@link NewDownloadTab} attached to a given {@link WindowAndroid}.
     */
    public static @Nullable NewDownloadTab from(WindowAndroid windowAndroid) {
        if (windowAndroid == null) return null;
        return KEY.retrieveDataFromHost(windowAndroid.getUnownedUserDataHost());
    }

    /**
     * Attaches a {@link NewDownloadTab} to a {@link WindowAndroid}.
     * @param windowAndroid The {@link WindowAndroid} to attach to.
     * @param newDownloadTab The {@link NewDownloadTab} to attach.
     */
    static void attach(WindowAndroid windowAndroid, NewDownloadTab newDownloadTab) {
        KEY.attachToHost(windowAndroid.getUnownedUserDataHost(), newDownloadTab);
    }

    /**
     * Detaches a {@link NewDownloadTab} so that it is no longer accessible.
     * @param newDownloadTab The {@link NewDownloadTab} to detach.
     */
    static void detach(NewDownloadTab newDownloadTab) {
        KEY.detachFromAllHosts(newDownloadTab);
    }
}
