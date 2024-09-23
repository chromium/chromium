// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs.content;

import android.os.Bundle;

import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;
import androidx.browser.customtabs.CustomTabsSessionToken;
import androidx.browser.customtabs.EngagementSignalsCallback;

import org.chromium.base.Callback;
import org.chromium.chrome.browser.customtabs.CustomTabsConnection;
import org.chromium.chrome.browser.customtabs.content.TabObserverRegistrar.CustomTabTabObserver;
import org.chromium.chrome.browser.privacy.settings.PrivacyPreferencesManagerImpl;

/**
 * Handles the initialization of Engagement Signals when the client sets an
 * {@link androidx.browser.customtabs.EngagementSignalsCallback}.
 */
public class EngagementSignalsHandler {
    private final CustomTabsConnection mConnection;
    private final CustomTabsSessionToken mSession;
    @Nullable private EngagementSignalsInitialScrollObserver mInitialScrollObserver;
    @Nullable private RealtimeEngagementSignalObserver mObserver;
    private TabObserverRegistrar mTabObserverRegistrar;
    private EngagementSignalsCallback mCallback;
    private Callback<Boolean> mPrivacyPreferencesObserver;

    public EngagementSignalsHandler(
            CustomTabsConnection connection, CustomTabsSessionToken session) {
        mConnection = connection;
        mSession = session;
    }

    /**
     * Sets the {@link TabObserverRegistrar}. This should be called when the registrar becomes
     * available.
     * @param registrar The {@link TabObserverRegistrar}.
     */
    public void setTabObserverRegistrar(TabObserverRegistrar registrar) {
        mTabObserverRegistrar = registrar;
        if (mCallback != null) {
            // If the registrar became available after the callback, we don't need to create the
            // EngagementSignalsInitialScrollObserver. We wouldn't be able to observe the scroll
            // gestures without the registrar anyway.
            createEngagementSignalsObserver();
        } else {
            mInitialScrollObserver =
                    new EngagementSignalsInitialScrollObserver(mTabObserverRegistrar);
        }
    }

    /**
     * Sets the {@link EngagementSignalsCallback}. This should be called when the client app sets a
     * callback.
     * @param callback The {@link EngagementSignalsCallback}.
     */
    public void setEngagementSignalsCallback(EngagementSignalsCallback callback) {
        mCallback = callback;
        if (mTabObserverRegistrar != null) {
            createEngagementSignalsObserver();
        }
    }

    public void notifyTabWillCloseAndReopenWithSessionReuse() {
        if (mObserver != null) {
            mObserver.suppressNextSessionEndedCall();
        }
    }

    private void createEngagementSignalsObserver() {
        if (!PrivacyPreferencesManagerImpl.getInstance().isUsageAndCrashReportingPermitted()) {
            return;
        }
        // This can happen if the client app sets a new EngagementSignalsCallback. In that case,
        // we should recreate the observer.
        if (mObserver != null) {
            mObserver.destroy();
        }
        assert mTabObserverRegistrar != null;
        assert mCallback != null;
        boolean hadScrollDown =
                mInitialScrollObserver != null
                        && mInitialScrollObserver.hasCurrentPageHadScrollDown();
        mObserver =
                new RealtimeEngagementSignalObserver(
                        mTabObserverRegistrar, mConnection, mSession, mCallback, hadScrollDown);
        if (mInitialScrollObserver != null) {
            mInitialScrollObserver.destroy();
            mInitialScrollObserver = null;
        }

        mPrivacyPreferencesObserver =
                (permitted) -> {
                    if (!permitted) {
                        if (mObserver != null) {
                            if (mCallback != null) {
                                mCallback.onSessionEnded(false, Bundle.EMPTY);
                            }
                            mObserver.destroy();
                            mObserver = null;
                        }
                        PrivacyPreferencesManagerImpl.getInstance()
                                .getUsageAndCrashReportingPermittedObservableSupplier()
                                .removeObserver(mPrivacyPreferencesObserver);
                        mPrivacyPreferencesObserver = null;
                    }
                };
        PrivacyPreferencesManagerImpl.getInstance()
                .getUsageAndCrashReportingPermittedObservableSupplier()
                .addObserver(mPrivacyPreferencesObserver);
        mTabObserverRegistrar.registerActivityTabObserver(
                new CustomTabTabObserver() {
                    @Override
                    protected void onAllTabsClosed() {
                        if (mTabObserverRegistrar != null) {
                            mTabObserverRegistrar.unregisterActivityTabObserver(this);
                            mTabObserverRegistrar = null;
                        }
                        if (mObserver != null) {
                            mObserver.destroy();
                            mObserver = null;
                        }
                        if (mPrivacyPreferencesObserver != null) {
                            PrivacyPreferencesManagerImpl.getInstance()
                                    .getUsageAndCrashReportingPermittedObservableSupplier()
                                    .removeObserver(mPrivacyPreferencesObserver);
                            mPrivacyPreferencesObserver = null;
                        }
                    }
                });
    }

    @VisibleForTesting
    @Nullable
    public RealtimeEngagementSignalObserver getEngagementSignalsObserverForTesting() {
        return mObserver;
    }
}
