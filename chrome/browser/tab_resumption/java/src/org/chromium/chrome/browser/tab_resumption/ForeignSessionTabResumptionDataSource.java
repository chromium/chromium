// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab_resumption;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.ObserverList;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.recent_tabs.ForeignSessionHelper;
import org.chromium.chrome.browser.recent_tabs.ForeignSessionHelper.ForeignSession;
import org.chromium.chrome.browser.recent_tabs.ForeignSessionHelper.ForeignSessionTab;
import org.chromium.chrome.browser.recent_tabs.ForeignSessionHelper.ForeignSessionWindow;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.browser.signin.services.SigninManager;
import org.chromium.chrome.browser.signin.services.SigninManager.SignInStateObserver;
import org.chromium.chrome.browser.sync.SyncServiceFactory;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.components.signin.identitymanager.ConsentLevel;
import org.chromium.components.signin.identitymanager.IdentityManager;
import org.chromium.components.sync.SyncService;
import org.chromium.components.sync.SyncService.SyncStateChangedListener;

import java.util.ArrayList;
import java.util.Collections;
import java.util.List;
import java.util.concurrent.TimeUnit;

/**
 * Adapter for ForeignSessionHelper with additional features:
 *
 * <pre>
 * * Support to observe "data change events", which combines:
 *   * Permission updates from signin and sync status changes.
 *   * ForeignSessionCallback in response of new data after triggerSessionSync() call.
 * * Synchronous getSuggestions() to get tab resumption module suggestions computed from potentially
 *   stale Foreign Sessions data. This also calls triggerSessionSync(), which allows callers to get
 *   up-to-date results (although this requires another getSuggestions() call).
 * </pre>
 */
public class ForeignSessionTabResumptionDataSource
        implements SignInStateObserver, SyncStateChangedListener {

    // Suggestions older than 24h are considered stale, and rejected.
    private static final long STALENESS_THRESHOLD_MS = TimeUnit.HOURS.toMillis(24);

    /** Interface to observe "data change events". */
    public interface DataChangedObserver {
        public void onForeignSessionDataChanged(boolean isPermissionUpdate);
    }

    private final SigninManager mSigninManager;
    private final SyncService mSyncService;
    private final ForeignSessionHelper mForeignSessionHelper;

    private final ObserverList<DataChangedObserver> mDataChangedObservers;

    private boolean mIsSignedIn;
    private boolean mIsSynced;

    // Flag to indicate whether a computation of suggestions is needed.
    private boolean mRequireSuggestionsCompute;

    private final ArrayList<SuggestionEntry> mSuggestions;

    /**
     * @param signinManager To observe signin state changes.
     * @param identityManager To get initial signin state.
     * @param syncService To observe sync state changes.
     * @param foreignSessionHelper To fetch ForenSession data.
     */
    @VisibleForTesting
    protected ForeignSessionTabResumptionDataSource(
            SigninManager signinManager,
            IdentityManager identityManager,
            SyncService syncService,
            ForeignSessionHelper foreignSessionHelper) {
        super();
        mSigninManager = signinManager;
        mSyncService = syncService;
        mForeignSessionHelper = foreignSessionHelper;
        mDataChangedObservers = new ObserverList<DataChangedObserver>();

        mSigninManager.addSignInStateObserver(this);
        mSyncService.addSyncStateChangedListener(this);
        mIsSignedIn = identityManager.hasPrimaryAccount(ConsentLevel.SIGNIN);
        mIsSynced = mSyncService.hasKeepEverythingSynced();

        mRequireSuggestionsCompute = true;
        mSuggestions = new ArrayList<SuggestionEntry>();

        mForeignSessionHelper.setOnForeignSessionCallback(
                () -> {
                    dispatchDataChangedObservers(false);
                });
    }

    public static ForeignSessionTabResumptionDataSource createFromProfile(Profile profile) {
        return new ForeignSessionTabResumptionDataSource(
                /* signinManager= */ IdentityServicesProvider.get().getSigninManager(profile),
                /* identityManager= */ IdentityServicesProvider.get().getIdentityManager(profile),
                /* syncService= */ SyncServiceFactory.getForProfile(profile),
                /* foreignSessionHelper= */ new ForeignSessionHelper(profile));
    }

    /** Implements {@link TabResumptionDataProvider} */
    public void destroy() {
        mSyncService.removeSyncStateChangedListener(this);
        mSigninManager.removeSignInStateObserver(this);
        mDataChangedObservers.clear();
        mForeignSessionHelper.destroy();
    }

    /**
     * @return Whether user settings permit Foreign Session data to be used.
     */
    public boolean canUseData() {
        return mIsSignedIn && mIsSynced;
    }

    /** Adds observer for data change events. */
    public void addObserver(DataChangedObserver observer) {
        mDataChangedObservers.addObserver(observer);
    }

    /** Removes observer added by addObserver(). */
    public void removeObserver(DataChangedObserver observer) {
        mDataChangedObservers.removeObserver(observer);
    }

    /** Implements {@link SignInStateObserver} */
    @Override
    public void onSignedIn() {
        mIsSignedIn = true;
        dispatchDataChangedObservers(true);
    }

    /** Implements {@link SignInStateObserver} */
    @Override
    public void onSignedOut() {
        mIsSignedIn = false;
        dispatchDataChangedObservers(true);
    }

    /** Implements {@link SyncStateChangedListener} */
    @Override
    public void syncStateChanged() {
        boolean oldIsSynced = mIsSynced;
        mIsSynced = mSyncService.hasKeepEverythingSynced();
        if (oldIsSynced != mIsSynced) {
            dispatchDataChangedObservers(true);
        }
    }

    /**
     * Computes and returns suggestions entries based on most recently fetched Foreign Session data,
     * and schecules new sync. Returned results can be shared, so the caller should not modify them,
     * except perhaps to add shareable cached data (e.g., favicons).
     */
    List<SuggestionEntry> getSuggestions() {
        if (canUseData()) {
            // Use existing data but trigger sync. If there's no new data then existing data is good
            // enough. Otherwise new data would notify all observers, which might cause caller to
            // call getSuggestion() again for data update.
            mForeignSessionHelper.triggerSessionSync();
            maybeComputeSuggestions();
        } else {
            mSuggestions.clear();
            mRequireSuggestionsCompute = true;
        }
        return mSuggestions;
    }

    /** Returns the current time in ms since the epoch. */
    long getCurrentTimeMs() {
        return System.currentTimeMillis();
    }

    void maybeComputeSuggestions() {
        if (!mRequireSuggestionsCompute) return;

        long currentTimeMs = getCurrentTimeMs();
        mSuggestions.clear();
        List<ForeignSession> foreignSessions = mForeignSessionHelper.getForeignSessions();
        for (ForeignSession session : foreignSessions) {
            for (ForeignSessionWindow window : session.windows) {
                for (ForeignSessionTab tab : window.tabs) {
                    if (isForeignSessionTabUsable(tab)
                            && currentTimeMs - tab.lastActiveTime <= STALENESS_THRESHOLD_MS) {
                        mSuggestions.add(
                                new SuggestionEntry(
                                        session.name,
                                        tab.url,
                                        tab.title,
                                        tab.lastActiveTime,
                                        tab.id));
                    }
                }
            }
        }
        Collections.sort(mSuggestions);

        mRequireSuggestionsCompute = false;
    }

    private boolean isForeignSessionTabUsable(ForeignSessionTab tab) {
        String scheme = tab.url.getScheme();
        return scheme.equals(UrlConstants.HTTP_SCHEME) || scheme.equals(UrlConstants.HTTPS_SCHEME);
    }

    private void dispatchDataChangedObservers(boolean isPermissionUpdate) {
        mRequireSuggestionsCompute = true;
        for (DataChangedObserver observer : mDataChangedObservers) {
            observer.onForeignSessionDataChanged(isPermissionUpdate);
        }
    }
}
