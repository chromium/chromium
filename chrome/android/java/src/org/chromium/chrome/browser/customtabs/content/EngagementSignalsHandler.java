// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs.content;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.os.Bundle;

import androidx.browser.customtabs.CustomTabsSessionToken;
import androidx.browser.customtabs.EngagementSignalsCallback;

import org.chromium.base.Callback;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.customtabs.content.TabObserverRegistrar.CustomTabTabObserver;
import org.chromium.chrome.browser.privacy.settings.PrivacyPreferencesManagerImpl;
import org.chromium.chrome.browser.tab.Tab;

/**
 * Handles the initialization of Engagement Signals when the client sets an {@link
 * androidx.browser.customtabs.EngagementSignalsCallback}.
 */
@NullMarked
public class EngagementSignalsHandler {
    private final CustomTabsSessionToken mSession;
    private @Nullable EngagementSignalsInitialScrollObserver mInitialScrollObserver;
    private @Nullable RealtimeEngagementSignalObserver mObserver;
    private @Nullable TabObserverRegistrar mTabObserverRegistrar;
    private @Nullable CustomTabTabObserver mTabObserver;
    private @Nullable EngagementSignalsCallback mCallback;
    private @Nullable Callback<Boolean> mPrivacyPreferencesObserver;

    public EngagementSignalsHandler(CustomTabsSessionToken session) {
        mSession = session;
    }

    /**
     * Sets the {@link TabObserverRegistrar}. This should be called when the registrar becomes
     * available.
     *
     * @param registrar The {@link TabObserverRegistrar}.
     */
    public void setTabObserverRegistrar(TabObserverRegistrar registrar) {
        if (mInitialScrollObserver != null) {
            mInitialScrollObserver.destroy();
            mInitialScrollObserver = null;
        }
        destroyObservers();
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

    /** Notify that Open in Browser is being invoked on the given tab. */
    public void notifyOpenInBrowser(Tab tab) {
        // When Open in Browser is tapped we need to manually collect user interactions, to ensure
        // the ensuing invocation of EngagementSignalsCallback#onSessionEnded correctly signals
        // whether user interactions occurred. We need to do this manually because the usual
        // triggers for collecting user interactions (TabObserver#webContentsWillSwap,
        // TabObserver#onClosingStateChanged, and TabObserver#onDestroyed) do not get invoked when
        // Open in Browser is used.
        if (mObserver != null) {
            mObserver.collectUserInteraction(tab);
        }
    }

    private void createEngagementSignalsObserver() {
        if (!PrivacyPreferencesManagerImpl.getInstance().isUsageAndCrashReportingPermitted()) {
            return;
        }
        boolean hadScrollDown =
                mInitialScrollObserver != null
                        && mInitialScrollObserver.hasCurrentPageHadScrollDown();
        if (mInitialScrollObserver != null) {
            mInitialScrollObserver.destroy();
            mInitialScrollObserver = null;
        }
        // This can happen if the client app sets a new EngagementSignalsCallback or reuses the
        // session across activities. Clean up any existing observers first.
        destroyObservers();
        assert mTabObserverRegistrar != null;
        assert mCallback != null;
        mObserver =
                new RealtimeEngagementSignalObserver(
                        mTabObserverRegistrar, mSession, mCallback, hadScrollDown);

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
                                .removeObserver(assumeNonNull(mPrivacyPreferencesObserver));
                        mPrivacyPreferencesObserver = null;
                    }
                };
        PrivacyPreferencesManagerImpl.getInstance()
                .getUsageAndCrashReportingPermittedObservableSupplier()
                .addSyncObserverAndPostIfNonNull(mPrivacyPreferencesObserver);
        mTabObserver =
                new CustomTabTabObserver() {
                    @Override
                    public void onDestroyed(Tab tab) {
                        cleanUp();
                    }

                    @Override
                    protected void onAllTabsClosed() {
                        cleanUp();
                    }

                    private void cleanUp() {
                        destroyObservers();
                        mTabObserverRegistrar = null;
                    }
                };
        mTabObserverRegistrar.registerActivityTabObserver(mTabObserver);
    }

    private void destroyObservers() {
        if (mTabObserver != null && mTabObserverRegistrar != null) {
            mTabObserverRegistrar.unregisterActivityTabObserver(mTabObserver);
        }
        mTabObserver = null;
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

    public @Nullable RealtimeEngagementSignalObserver getEngagementSignalsObserverForTesting() {
        return mObserver;
    }
}
