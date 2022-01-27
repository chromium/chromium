// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.sync;

import android.accounts.Account;
import android.annotation.SuppressLint;

import androidx.annotation.MainThread;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.ThreadUtils;

/** TODO(crbug.com/1107904): Delete and migrate callers to use SyncService. */
@MainThread
public class AndroidSyncSettings {
    @SuppressLint("StaticFieldLeak")
    private static AndroidSyncSettings sInstance;

    /**
      Singleton instance getter. Will initialize the singleton if it hasn't been initialized before.
     */
    @MainThread
    public static AndroidSyncSettings get() {
        ThreadUtils.assertOnUiThread();
        if (sInstance == null) {
            sInstance = new AndroidSyncSettings();
        }
        return sInstance;
    }

    /**
     * Overrides AndroidSyncSettings instance for tests.
     */
    @MainThread
    @VisibleForTesting
    public static void overrideForTests(AndroidSyncSettings instance) {
        ThreadUtils.assertOnUiThread();
        sInstance = instance;
    }

    /**
     * DEPRECATED - DO NOT USE! You probably want SyncService.isSyncRequested() instead.
     *
     * @return The state of the Chrome sync setting for the given account,
     * *ignoring* the master sync setting.
     */
    @Deprecated
    public boolean isChromeSyncEnabled() {
        ThreadUtils.assertOnUiThread();
        return SyncService.get() != null && SyncService.get().isSyncRequested();
    }

    /**
     * Enables Chrome sync for |mAccount| if it's non-null.
     */
    public void enableChromeSync() {
        ThreadUtils.assertOnUiThread();
        assert SyncService.get() != null;
        SyncService.get().setSyncRequested(true);
    }

    /**
     * Must be called with the new account on sign-in and with null on sign-out.
     */
    public void updateAccount(Account account) {}
}
