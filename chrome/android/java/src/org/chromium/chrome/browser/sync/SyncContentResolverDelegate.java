// Copyright 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.sync;

import android.accounts.Account;
import android.content.SyncStatusObserver;
import android.os.Bundle;

/**
 * Since the ContentResolver in Android has a lot of static methods, it is hard to
 * mock out for tests. This interface wraps all the sync-related methods we use from
 * the Android ContentResolver.
 */
interface SyncContentResolverDelegate {
    Object addStatusChangeListener(int mask, SyncStatusObserver callback);

    void removeStatusChangeListener(Object handle);

    void setMasterSyncAutomatically(boolean sync);

    boolean getMasterSyncAutomatically();

    void setSyncAutomatically(Account account, String authority, boolean sync);

    boolean getSyncAutomatically(Account account, String authority);

    void setIsSyncable(Account account, String authority, int syncable);

    int getIsSyncable(Account account, String authority);

    void removePeriodicSync(Account account, String authority, Bundle extras);
}
