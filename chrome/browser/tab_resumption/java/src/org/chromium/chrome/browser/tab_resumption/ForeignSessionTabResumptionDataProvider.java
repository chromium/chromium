// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab_resumption;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.Callback;
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

/** TabResumptionDataProvider using ForeignSession data. */
public class ForeignSessionTabResumptionDataProvider extends TabResumptionDataProvider
        implements SignInStateObserver, SyncStateChangedListener {
    // Suggestions older than 24h are considered stale, and rejected.
    private static final long STALENESS_THRESHOLD_MS = 24L * 60L * 60L * 1000L;

    private final SigninManager mSigninManager;
    private final SyncService mSyncService;
    private final ForeignSessionHelper mForeignSessionHelper;

    /** 0 means to use actual time. */
    private final long mForcedCurrentTimeMs;

    private boolean mIsSignedIn;
    private boolean mIsSynced;

    /**
     * @param signinManager To observe signin state changes.
     * @param identityManager To get initial signin state.
     * @param syncService To observe sync state changes.
     * @param foreignSessionHelper To fetch ForenSession data.
     * @param forcedCurrentTimeMs To override current time (in ms since the epoch) for testing. 0L
     *     specifies that actual time should be used.
     */
    @VisibleForTesting
    protected ForeignSessionTabResumptionDataProvider(
            SigninManager signinManager,
            IdentityManager identityManager,
            SyncService syncService,
            ForeignSessionHelper foreignSessionHelper,
            long forcedCurrentTimeMs) {
        super();
        mSigninManager = signinManager;
        mSyncService = syncService;
        mForeignSessionHelper = foreignSessionHelper;
        mForcedCurrentTimeMs = forcedCurrentTimeMs;

        mSigninManager.addSignInStateObserver(this);
        mSyncService.addSyncStateChangedListener(this);
        mIsSignedIn = identityManager.hasPrimaryAccount(ConsentLevel.SIGNIN);
        mIsSynced = mSyncService.hasKeepEverythingSynced();
    }

    public static ForeignSessionTabResumptionDataProvider createFromProfile(Profile profile) {
        return new ForeignSessionTabResumptionDataProvider(
                /* signinManager= */ IdentityServicesProvider.get().getSigninManager(profile),
                /* identityManager= */ IdentityServicesProvider.get().getIdentityManager(profile),
                /* syncService= */ SyncServiceFactory.getForProfile(profile),
                /* foreignSessionHelper= */ new ForeignSessionHelper(profile),
                /* forcedCurrentTimeMs= */ 0L);
    }

    /** Implements {@link TabResumptionDataProvider} */
    @Override
    public void destroy() {
        mSyncService.removeSyncStateChangedListener(this);
        mSigninManager.removeSignInStateObserver(this);
        mForeignSessionHelper.destroy();
    }

    private boolean isForeignSessionTabUsable(ForeignSessionTab tab, long currentTimeMs) {
        if (currentTimeMs - tab.timestamp > STALENESS_THRESHOLD_MS) return false;
        String scheme = tab.url.getScheme();
        return scheme.equals(UrlConstants.HTTP_SCHEME) || scheme.equals(UrlConstants.HTTPS_SCHEME);
    }

    private void updateFromForeignSessions(Callback<List<SuggestionEntry>> suggestionsCallback) {
        ArrayList<SuggestionEntry> suggestions = new ArrayList<SuggestionEntry>();
        long currentTimeMs =
                (mForcedCurrentTimeMs == 0) ? System.currentTimeMillis() : mForcedCurrentTimeMs;

        List<ForeignSession> foreignSessions = mForeignSessionHelper.getForeignSessions();
        for (ForeignSession session : foreignSessions) {
            for (ForeignSessionWindow window : session.windows) {
                for (ForeignSessionTab tab : window.tabs) {
                    if (isForeignSessionTabUsable(tab, currentTimeMs)) {
                        suggestions.add(
                                new SuggestionEntry(
                                        session.name, tab.url, tab.title, tab.timestamp, tab.id));
                    }
                }
            }
        }
        Collections.sort(suggestions);

        suggestionsCallback.onResult(suggestions);
    }

    /** Implements {@link TabResumptionDataProvider} */
    @Override
    public void fetchSuggestions(Callback<List<SuggestionEntry>> suggestionsCallback) {
        boolean canFetch = mIsSignedIn && mIsSynced;
        if (!canFetch) {
            suggestionsCallback.onResult(null);
            return;
        }

        mForeignSessionHelper.setOnForeignSessionCallback(
                () -> {
                    updateFromForeignSessions(suggestionsCallback);
                });
        mForeignSessionHelper.triggerSessionSync();
        updateFromForeignSessions(suggestionsCallback);
    }

    /** Implements {@link SignInStateObserver} */
    @Override
    public void onSignedIn() {
        mIsSignedIn = true;
        dispatchStatusChangedCallback();
    }

    /** Implements {@link SignInStateObserver} */
    @Override
    public void onSignedOut() {
        mIsSignedIn = false;
        dispatchStatusChangedCallback();
    }

    /** Implements {@link SyncStateChangedListener} */
    @Override
    public void syncStateChanged() {
        boolean oldHasKeepEverythingSynced = mIsSynced;
        mIsSynced = mSyncService.hasKeepEverythingSynced();
        if (oldHasKeepEverythingSynced != mIsSynced) {
            dispatchStatusChangedCallback();
        }
    }
}
