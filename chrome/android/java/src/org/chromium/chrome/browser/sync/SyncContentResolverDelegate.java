// Copyright 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.sync;

import android.accounts.Account;
import android.content.ContentResolver;
import android.content.SyncStatusObserver;
import android.os.Bundle;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.ThreadUtils;

/**
 * Since the ContentResolver in Android has a lot of static methods, it is hard to
 * mock out for tests. This class wraps all the sync-related methods we use from
 * the Android ContentResolver.
 * Note that SyncContentResolverDelegate is not an Android concept. In
 * particular, it's not this class that will notify observers, Android will
 * directly do that.
 */
class SyncContentResolverDelegate {
    private static SyncContentResolverDelegate sInstance;

    public static SyncContentResolverDelegate get() {
        ThreadUtils.assertOnUiThread();
        if (sInstance == null) {
            sInstance = new SyncContentResolverDelegate();
        }
        return sInstance;
    }

    public Object addStatusChangeListener(int mask, SyncStatusObserver callback) {
        return ContentResolver.addStatusChangeListener(mask, callback);
    }

    public void removeStatusChangeListener(Object handle) {
        ContentResolver.removeStatusChangeListener(handle);
    }

    public void setMasterSyncAutomatically(boolean sync) {
        ContentResolver.setMasterSyncAutomatically(sync);
    }

    public boolean getMasterSyncAutomatically() {
        return ContentResolver.getMasterSyncAutomatically();
    }

    public boolean getSyncAutomatically(Account account, String authority) {
        return ContentResolver.getSyncAutomatically(account, authority);
    }

    public void setSyncAutomatically(Account account, String authority, boolean sync) {
        ContentResolver.setSyncAutomatically(account, authority, sync);
    }

    public void setIsSyncable(Account account, String authority, int syncable) {
        ContentResolver.setIsSyncable(account, authority, syncable);
    }

    public int getIsSyncable(Account account, String authority) {
        return ContentResolver.getIsSyncable(account, authority);
    }

    public void removePeriodicSync(Account account, String authority, Bundle extras) {
        ContentResolver.removePeriodicSync(account, authority, extras);
    }

    @VisibleForTesting
    public static void overrideForTests(SyncContentResolverDelegate delegate) {
        ThreadUtils.assertOnUiThread();
        sInstance = delegate;
    }

    protected SyncContentResolverDelegate() {}
}
