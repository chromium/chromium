// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab_resumption;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.Callback;
import org.chromium.base.ObserverList;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.browser.signin.services.SigninManager;
import org.chromium.chrome.browser.signin.services.SigninManager.SignInStateObserver;
import org.chromium.chrome.browser.sync.SyncServiceFactory;
import org.chromium.components.signin.identitymanager.ConsentLevel;
import org.chromium.components.signin.identitymanager.IdentityManager;
import org.chromium.components.sync.SyncService;
import org.chromium.components.sync.SyncService.SyncStateChangedListener;
import org.chromium.components.sync.UserSelectableType;

import java.util.ArrayList;
import java.util.List;

/**
 * A shareable, self-updating, and cached source of SuggestionEntry instances that also observes
 * permission updates from signin and sync status changes.
 */
public class SyncDerivedSuggestionEntrySource
        implements SignInStateObserver, SyncStateChangedListener {

    /**
     * Interface for Source caller to observe "data change events", which can be non-permission
     * updates (suggestion changes) or permission updates (signin or sync status changes).
     */
    public interface SourceDataChangedObserver {
        public void onDataChanged(boolean isPermissionUpdate);
    }

    private final SigninManager mSigninManager;
    private final SyncService mSyncService;
    private final SuggestionBackend mSuggestionBackend;
    private final boolean mServesLocalTabs;

    private final ObserverList<SourceDataChangedObserver> mSourceDataChangedObservers;

    private boolean mIsSignedIn;
    private boolean mIsSynced;

    // Flag to indicate whether `mCachedSuggestionEntries` can be passed without update.
    private boolean mPassUseCachedResults;

    private final ArrayList<SuggestionEntry> mCachedSuggestionEntries;

    /**
     * @param signinManager To observe signin state changes.
     * @param identityManager To get initial signin state.
     * @param syncService To observe sync state changes.
     * @param suggestionBackend To get suggestions and trigger update.
     * @param servesLocalTabs Whether Local Tabs may be served as results.
     */
    @VisibleForTesting
    protected SyncDerivedSuggestionEntrySource(
            SigninManager signinManager,
            IdentityManager identityManager,
            SyncService syncService,
            SuggestionBackend suggestionBackend,
            boolean servesLocalTabs) {
        super();
        mSigninManager = signinManager;
        mSyncService = syncService;
        mSuggestionBackend = suggestionBackend;
        mServesLocalTabs = servesLocalTabs;

        mSourceDataChangedObservers = new ObserverList<SourceDataChangedObserver>();

        mSigninManager.addSignInStateObserver(this);
        mSyncService.addSyncStateChangedListener(this);
        mIsSignedIn = identityManager.hasPrimaryAccount(ConsentLevel.SIGNIN);
        mIsSynced = mSyncService.getSelectedTypes().contains(UserSelectableType.TABS);

        mPassUseCachedResults = false;
        mCachedSuggestionEntries = new ArrayList<SuggestionEntry>();

        mSuggestionBackend.setUpdateObserver(
                () -> {
                    dispatchSourceDataChangedObservers(false);
                });
    }

    public static SyncDerivedSuggestionEntrySource createFromProfile(
            Profile profile, SuggestionBackend suggestionBackend, boolean servesLocalTabs) {
        return new SyncDerivedSuggestionEntrySource(
                /* signinManager= */ IdentityServicesProvider.get().getSigninManager(profile),
                /* identityManager= */ IdentityServicesProvider.get().getIdentityManager(profile),
                /* syncService= */ SyncServiceFactory.getForProfile(profile),
                suggestionBackend,
                servesLocalTabs);
    }

    public void destroy() {
        mSyncService.removeSyncStateChangedListener(this);
        mSigninManager.removeSignInStateObserver(this);
        mSourceDataChangedObservers.clear();
        mSuggestionBackend.destroy();
    }

    /**
     * @return Whether user settings permit SuggestionBackend data to be used.
     */
    public boolean canUseData() {
        return mServesLocalTabs || (mIsSignedIn && mIsSynced);
    }

    /** Adds observer for data change events. */
    public void addObserver(SourceDataChangedObserver observer) {
        mSourceDataChangedObservers.addObserver(observer);
    }

    /** Removes observer added by addObserver(). */
    public void removeObserver(SourceDataChangedObserver observer) {
        mSourceDataChangedObservers.removeObserver(observer);
    }

    /** Implements {@link SignInStateObserver} */
    @Override
    public void onSignedIn() {
        mIsSignedIn = true;
        dispatchSourceDataChangedObservers(true);
    }

    @Override
    public void onSignedOut() {
        mIsSignedIn = false;
        dispatchSourceDataChangedObservers(true);
    }

    /** Implements {@link SyncStateChangedListener} */
    @Override
    public void syncStateChanged() {
        boolean oldIsSynced = mIsSynced;
        mIsSynced = mSyncService.getSelectedTypes().contains(UserSelectableType.TABS);
        if (oldIsSynced != mIsSynced) {
            dispatchSourceDataChangedObservers(true);
        }
    }

    /**
     * Computes and passes back suggestions entries based on most recently fetched SuggestionBackend
     * data, and schecules new sync. Returned results may be shared, so the caller should not modify
     * them.
     */
    void getSuggestions(Callback<List<SuggestionEntry>> callback) {
        if (canUseData()) {
            // Read results and trigger sync. If there's no new data then results is good enough.
            // Otherwise new data would notify all observers, which might cause caller to call
            // getSuggestion() again for data update.
            mSuggestionBackend.triggerUpdate();

            if (mPassUseCachedResults) {
                callback.onResult(mCachedSuggestionEntries);
            } else {
                // Local tab info can update quickly, so don't cache if they're served.
                mPassUseCachedResults = !mServesLocalTabs;

                mSuggestionBackend.read(
                        (List<SuggestionEntry> suggestions) -> {
                            mCachedSuggestionEntries.clear();
                            mCachedSuggestionEntries.addAll(suggestions);
                            callback.onResult(mCachedSuggestionEntries);
                        });
            }

        } else {
            mPassUseCachedResults = false;
            mCachedSuggestionEntries.clear();
            callback.onResult(mCachedSuggestionEntries);
        }
    }

    private void dispatchSourceDataChangedObservers(boolean isPermissionUpdate) {
        mPassUseCachedResults = false;
        for (SourceDataChangedObserver observer : mSourceDataChangedObservers) {
            observer.onDataChanged(isPermissionUpdate);
        }
    }
}
