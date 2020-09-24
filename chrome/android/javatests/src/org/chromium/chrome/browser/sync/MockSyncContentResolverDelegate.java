// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.sync;

import android.accounts.Account;
import android.content.ContentResolver;
import android.content.SyncStatusObserver;
import android.os.Bundle;

import androidx.annotation.VisibleForTesting;

import java.util.HashMap;
import java.util.HashSet;
import java.util.Map;
import java.util.Set;

/**
 * Fake thread-safe implementation of {@link SyncContentResolverDelegate}, so tests can
 * emulate certain auto-sync settings (e.g. mimic a user disabling the master sync toggle).
 * Synchronously notifies observers every time a setter is called, even if the state didn't
 * change. Differently from the actual android.os.ContentResolver APIs, it only supports
 * observers for the SYNC_OBSERVER_TYPE_SETTINGS type and it doesn't allow querying
 * settings for a null account.
 */
class MockSyncContentResolverDelegate extends SyncContentResolverDelegate {
    private final Set<String> mSyncAutomaticallySet = new HashSet<String>();
    private final Map<String, Boolean> mIsSyncableMap = new HashMap<String, Boolean>();
    private final Set<SyncStatusObserver> mObservers = new HashSet<SyncStatusObserver>();
    private boolean mMasterSyncAutomatically;

    private Object mLock = new Object();

    @Override
    public Object addStatusChangeListener(int mask, SyncStatusObserver observer) {
        if (mask != ContentResolver.SYNC_OBSERVER_TYPE_SETTINGS) {
            throw new IllegalArgumentException("This implementation only supports "
                    + "ContentResolver.SYNC_OBSERVER_TYPE_SETTINGS as the mask");
        }
        synchronized (mLock) {
            mObservers.add(observer);
        }
        return observer;
    }

    @Override
    public void removeStatusChangeListener(Object handle) {
        synchronized (mLock) {
            mObservers.remove(handle);
        }
    }

    // Once this returns, observers are guaranteed to have been synchronously notified.
    @Override
    @VisibleForTesting
    public void setMasterSyncAutomatically(boolean sync) {
        synchronized (mLock) {
            if (mMasterSyncAutomatically == sync) return;
            mMasterSyncAutomatically = sync;
        }
        notifyObservers();
    }

    @Override
    public boolean getMasterSyncAutomatically() {
        synchronized (mLock) {
            return mMasterSyncAutomatically;
        }
    }

    @Override
    public boolean getSyncAutomatically(Account account, String authority) {
        synchronized (mLock) {
            return mSyncAutomaticallySet.contains(createKey(account, authority));
        }
    }

    // Once this returns, observers are guaranteed to have been synchronously notified.
    @Override
    public void setSyncAutomatically(Account account, String authority, boolean sync) {
        synchronized (mLock) {
            String key = createKey(account, authority);
            if (sync) {
                mSyncAutomaticallySet.add(key);
            } else if (mSyncAutomaticallySet.contains(key)) {
                mSyncAutomaticallySet.remove(key);
            }
        }
        notifyObservers();
    }

    // Once this returns, observers are guaranteed to have been synchronously notified.
    @Override
    public void setIsSyncable(Account account, String authority, int syncable) {
        synchronized (mLock) {
            String key = createKey(account, authority);
            if (syncable > 0) {
                mIsSyncableMap.put(key, true);
            } else if (syncable == 0) {
                mIsSyncableMap.put(key, false);
            } else if (mIsSyncableMap.containsKey(key)) {
                mIsSyncableMap.remove(key);
            }
        }
        notifyObservers();
    }

    @Override
    public int getIsSyncable(Account account, String authority) {
        synchronized (mLock) {
            String key = createKey(account, authority);
            if (mIsSyncableMap.containsKey(key)) {
                return mIsSyncableMap.get(key) ? 1 : 0;
            }
            return -1;
        }
    }

    @Override
    public void removePeriodicSync(Account account, String authority, Bundle extras) {}

    private static String createKey(Account account, String authority) {
        return account.name + "@@@" + account.type + "@@@" + authority;
    }

    private void notifyObservers() {
        synchronized (mLock) {
            for (SyncStatusObserver observer : mObservers) {
                observer.onStatusChanged(ContentResolver.SYNC_OBSERVER_TYPE_SETTINGS);
            }
        }
    }
}
