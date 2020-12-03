// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.sync;

import android.accounts.Account;
import android.annotation.SuppressLint;
import android.content.ContentResolver;
import android.content.SyncStatusObserver;
import android.os.Bundle;

import androidx.annotation.MainThread;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.ContextUtils;
import org.chromium.base.ThreadUtils;
import org.chromium.base.task.PostTask;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.components.signin.AccountManagerFacadeProvider;
import org.chromium.components.signin.base.CoreAccountInfo;
import org.chromium.components.signin.identitymanager.ConsentLevel;
import org.chromium.components.signin.identitymanager.IdentityManager;
import org.chromium.content_public.browser.UiThreadTaskTraits;

/**
 * WARNING: Chrome will be decoupled from Android auto-sync (crbug.com/1105795).
 * Some documentation in this class may be outdated or may not be coherent when
 * DecoupleSyncFromAndroidMasterSync is enabled.
 *
 * A helper class to handle the current status of sync for Chrome in Android settings.
 *
 * {@link #updateAccount(Account)} should be invoked whenever sync account is changed.
 */
@MainThread
public class AndroidSyncSettings {
    @SuppressLint("StaticFieldLeak")
    private static AndroidSyncSettings sInstance;

    // Cached value of the static |getContractAuthority()|.
    private final String mContractAuthority;

    private final SyncContentResolverDelegate mSyncContentResolverDelegate;

    private Account mAccount;

    private boolean mIsSyncable;

    private boolean mChromeSyncEnabled;

    private boolean mMasterSyncEnabled;

    private boolean mShouldDecoupleSyncFromMasterSync;

    // Is set at most once.
    @Nullable
    private Delegate mDelegate;

    /**
     * Propagates changes from Android sync settings to the native code.
     */
    public interface Delegate {
        void androidSyncSettingsChanged();
    }

    /**
      Singleton instance getter. Will initialize the singleton if it hasn't been initialized before.
     */
    @MainThread
    public static AndroidSyncSettings get() {
        ThreadUtils.assertOnUiThread();
        if (sInstance == null) {
            sInstance = new AndroidSyncSettings(getSyncAccount());
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

    // TODO(crbug.com/1125622): Exposing these testing constructors that don't register the
    // singleton instance can be dangerous when there's code that explicitly calls |get()|
    // (in that case, a new object would be returned, not the one constructed by the test).
    // Consider exposing them as static methods that also register a singleton instance.
    /**
     * WARNING: Consider using |overrideForTests()| to inject a mock instead.
     * @param account The sync account if sync is enabled, null otherwise.
     */
    @VisibleForTesting
    @Deprecated
    public AndroidSyncSettings(@Nullable Account account) {
        ThreadUtils.assertOnUiThread();
        mContractAuthority = getContractAuthority();
        mSyncContentResolverDelegate = SyncContentResolverDelegate.get();

        mAccount = account;
        updateCachedSettings();
        updateSyncability();

        ProfileSyncService syncService = ProfileSyncService.get();
        if (syncService != null
                && ChromeFeatureList.isEnabled(
                        ChromeFeatureList.DECOUPLE_SYNC_FROM_ANDROID_MASTER_SYNC)) {
            // Read initial persisted value.
            mShouldDecoupleSyncFromMasterSync = syncService.getDecoupledFromAndroidMasterSync();
        }

        SyncStatusObserver androidOsListener = new SyncStatusObserver() {
            @Override
            public void onStatusChanged(int which) {
                if (which != ContentResolver.SYNC_OBSERVER_TYPE_SETTINGS) return;
                // This is called by Android on a background thread, but AndroidSyncSettings
                // methods should be called from the UI thread, so post a task.
                PostTask.postTask(UiThreadTaskTraits.DEFAULT, () -> {
                    if (updateCachedSettings()) maybeNotifyDelegate();
                });
            }
        };
        mSyncContentResolverDelegate.addStatusChangeListener(
                ContentResolver.SYNC_OBSERVER_TYPE_SETTINGS, androidOsListener);
    }

    /**
     * DEPRECATED - DO NOT USE! You probably want ProfileSyncService.isSyncRequested() instead.
     *
     * @return The state of the Chrome sync setting for the given account,
     * *ignoring* the master sync setting.
     */
    @Deprecated
    public boolean isChromeSyncEnabled() {
        ThreadUtils.assertOnUiThread();
        return mChromeSyncEnabled;
    }

    /**
     * Checks whether the master sync flag for Android allows syncing Chrome
     * data.
     */
    public boolean doesMasterSyncSettingAllowChromeSync() {
        ThreadUtils.assertOnUiThread();
        return mMasterSyncEnabled || mShouldDecoupleSyncFromMasterSync;
    }

    /**
     * Enables Chrome sync for |mAccount| if it's non-null.
     */
    public void enableChromeSync() {
        ThreadUtils.assertOnUiThread();
        setChromeSyncEnabled(true);
    }

    /**
     * Disables Chrome sync for |mAccount| if it's non-null.
     */
    public void disableChromeSync() {
        ThreadUtils.assertOnUiThread();
        setChromeSyncEnabled(false);
    }

    /**
     * Must be called with the new account on sign-in and with null on sign-out.
     */
    public void updateAccount(Account account) {
        ThreadUtils.assertOnUiThread();
        mAccount = account;
        updateSyncability();
        if (updateCachedSettings()) maybeNotifyDelegate();
    }

    /**
     * Returns the contract authority used by Chrome when talking to auto-sync.
     * Exposed only to tests, so they can fake user interaction with the
     * auto-sync UI.
     */
    @VisibleForTesting
    public static String getContractAuthority() {
        return ContextUtils.getApplicationContext().getPackageName();
    }

    /**
     * Must be called at most once to set a (non-null) delegate.
     */
    public void setDelegate(Delegate delegate) {
        ThreadUtils.assertOnUiThread();
        assert delegate != null;
        assert mDelegate == null;
        mDelegate = delegate;
    }

    private void setChromeSyncEnabled(boolean value) {
        updateSyncability();
        if (value == mChromeSyncEnabled || mAccount == null) return;
        mChromeSyncEnabled = value;

        mSyncContentResolverDelegate.setSyncAutomatically(mAccount, mContractAuthority, value);
        maybeNotifyDelegate();
    }

    /**
     * Updates whether Chrome is registered with the Android Auto-Sync Manager.
     *
     * This is what causes the "Chrome" option to appear in Settings -> Accounts -> Sync .
     */
    private void updateSyncability() {
        boolean shouldBeSyncable = mAccount != null
                && !ChromeFeatureList.isEnabled(
                        ChromeFeatureList.DECOUPLE_SYNC_FROM_ANDROID_MASTER_SYNC);
        if (mIsSyncable == shouldBeSyncable) return;

        mIsSyncable = shouldBeSyncable;

        // Make account syncable if there is one.
        if (shouldBeSyncable) {
            mSyncContentResolverDelegate.setIsSyncable(mAccount, mContractAuthority, 1);
            // This reduces unnecessary resource usage. See http://crbug.com/480688 for details.
            mSyncContentResolverDelegate.removePeriodicSync(
                    mAccount, mContractAuthority, Bundle.EMPTY);
        } else if (mAccount != null) {
            mSyncContentResolverDelegate.setIsSyncable(mAccount, mContractAuthority, 0);
        }

        // Disable the syncability of Chrome for all other accounts.
        AccountManagerFacadeProvider.getInstance().tryGetGoogleAccounts(accounts -> {
            for (Account account : accounts) {
                if (account.equals(mAccount)) continue;
                if (mSyncContentResolverDelegate.getIsSyncable(account, mContractAuthority) <= 0) {
                    continue;
                }
                mSyncContentResolverDelegate.setIsSyncable(account, mContractAuthority, 0);
            }
        });
    }

    /**
     * Update the three cached settings from the content resolver and the
     * master sync decoupling setting.
     *
     * @return Whether either chromeSyncEnabled or masterSyncEnabled changed.
     */
    private boolean updateCachedSettings() {
        boolean oldChromeSyncEnabled = mChromeSyncEnabled;
        boolean oldMasterSyncEnabled = mMasterSyncEnabled;

        if (mAccount != null) {
            mIsSyncable =
                    mSyncContentResolverDelegate.getIsSyncable(mAccount, mContractAuthority) > 0;
            mChromeSyncEnabled =
                    mSyncContentResolverDelegate.getSyncAutomatically(mAccount, mContractAuthority);
        } else {
            mIsSyncable = false;
            mChromeSyncEnabled = false;
        }
        mMasterSyncEnabled = mSyncContentResolverDelegate.getMasterSyncAutomatically();

        if (mAccount != null && ProfileSyncService.get() != null
                && ChromeFeatureList.isEnabled(
                        ChromeFeatureList.DECOUPLE_SYNC_FROM_ANDROID_MASTER_SYNC)
                && mMasterSyncEnabled && !mShouldDecoupleSyncFromMasterSync) {
            // Re-enabling master sync at least once should cause Sync to no longer care whether
            // the former is enabled or not. This fact should be persisted via ProfileSyncService
            // so it's known on the next startup.
            mShouldDecoupleSyncFromMasterSync = true;
            ProfileSyncService.get().setDecoupledFromAndroidMasterSync();
        }

        return oldChromeSyncEnabled != mChromeSyncEnabled
                || oldMasterSyncEnabled != mMasterSyncEnabled;
    }

    private void maybeNotifyDelegate() {
        if (mDelegate != null) mDelegate.androidSyncSettingsChanged();
    }

    /**
     * Returns the sync account in the last used regular profile.
     */
    private static @Nullable Account getSyncAccount() {
        IdentityManager identityManager = IdentityServicesProvider.get().getIdentityManager(
                Profile.getLastUsedRegularProfile());
        return CoreAccountInfo.getAndroidAccountFrom(
                identityManager.getPrimaryAccountInfo(ConsentLevel.SYNC));
    }
}
