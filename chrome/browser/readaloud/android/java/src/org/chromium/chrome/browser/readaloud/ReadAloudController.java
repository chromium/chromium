// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.readaloud;

import android.content.Context;
import android.view.ViewStub;

import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.Log;
import org.chromium.base.ResettersForTesting;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.readaloud.expandedplayer.ExpandedPlayerCoordinator;
import org.chromium.chrome.browser.readaloud.player.PlayerCoordinator;
import org.chromium.chrome.browser.signin.services.UnifiedConsentServiceBridge;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelTabObserver;
import org.chromium.chrome.browser.translate.TranslateBridge;
import org.chromium.chrome.modules.readaloud.ExpandedPlayer;
import org.chromium.chrome.modules.readaloud.Playback;
import org.chromium.chrome.modules.readaloud.PlaybackListener;
import org.chromium.chrome.modules.readaloud.PlaybackArgs;
import org.chromium.chrome.modules.readaloud.ReadAloudPlaybackHooks;
import org.chromium.chrome.modules.readaloud.ReadAloudPlaybackHooksProvider;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.url.GURL;

import java.util.HashMap;
import java.util.HashSet;
import java.util.Map;

/**
 * The main entrypoint component for Read Aloud feature. It's responsible for
 * checking its availability and triggering playback.
 **/
public class ReadAloudController {
    private static final String TAG = "ReadAloudController";

    private final ObservableSupplier<Profile> mProfileSupplier;
    private final Map<String, Boolean> mReadabilityMap = new HashMap<>();
    private final Map<String, Boolean> mTimepointsSupportedMap = new HashMap<>();
    private final HashSet<String> mPendingRequests = new HashSet<>();
    private final TabModel mTabModel;
    private final ExpandedPlayer mExpandedPlayer;
    private final PlayerCoordinator mPlayerCoordinator;
    @Nullable
    private static PlayerCoordinator sPlayerCoordinatorForTesting;

    private TabModelTabObserver mTabObserver;

    private final ReadAloudReadabilityHooks mReadabilityHooks;
    @Nullable
    private static ReadAloudReadabilityHooks sReadabilityHooksForTesting;
    @Nullable
    private ReadAloudPlaybackHooks mPlaybackHooks;
    @Nullable
    private static ReadAloudPlaybackHooks sPlaybackHooksForTesting;
    // When playback is reset, it should be set to null together with the mCurrentlyPlayingTab
    @Nullable
    private Playback mPlayback;
    @Nullable
    private Tab mCurrentlyPlayingTab;

    /**
     * Kicks of readability check on a page load iff: the url is valid, no previous
     * result is available/pending and if a request has to be sent, the necessary
     * conditions are satisfied.
     * TODO: Add optimizations (don't send requests on chrome:// pages, remove
     * password from the url, etc). Also include enterprise policy check.
     */

    private ReadAloudReadabilityHooks.ReadabilityCallback mReadabilityCallback =
            new ReadAloudReadabilityHooks.ReadabilityCallback() {
                @Override
                public void onSuccess(String url, boolean isReadable, boolean timepointsSupported) {
                    Log.i(TAG, "onSuccess called for %s", url);
                    mReadabilityMap.put(url, isReadable);
                    mTimepointsSupportedMap.put(url, timepointsSupported);
                    mPendingRequests.remove(url);
                }

                @Override
                public void onFailure(String url, Throwable t) {
                    Log.i(TAG, "onFailure called for %s", url);
                    mPendingRequests.remove(url);
                }
            };

    private ReadAloudPlaybackHooks.CreatePlaybackCallback mPlaybackCallback =
            new ReadAloudPlaybackHooks.CreatePlaybackCallback() {
                @Override
                public void onSuccess(Playback playback) {
                    Log.i(TAG, "Playback created");
                    mPlayback = playback;
                    mPlayerCoordinator.playbackReady(mPlayback, PlaybackListener.State.PLAYING);
                    mPlayback.play();
                }
                @Override
                public void onFailure(Throwable t) {
                    Log.i(TAG, t.getMessage());
                    mPlayerCoordinator.playbackFailed();
                }
            };

    public ReadAloudController(Context context, ObservableSupplier<Profile> profileSupplier,
            TabModel tabModel, ViewStub miniPlayerStub,
            BottomSheetController bottomSheetController) {
        mProfileSupplier = profileSupplier;
        mTabModel = tabModel;
        mReadabilityHooks = sReadabilityHooksForTesting != null
                ? sReadabilityHooksForTesting
                : new ReadAloudReadabilityHooksImpl(context, ReadAloudFeatures.getApiKeyOverride());
        mExpandedPlayer = new ExpandedPlayerCoordinator(context, bottomSheetController);
        mPlayerCoordinator = sPlayerCoordinatorForTesting != null
                ? sPlayerCoordinatorForTesting
                : new PlayerCoordinator(context, miniPlayerStub);

        if (mReadabilityHooks.isEnabled()) {
            mTabObserver = new TabModelTabObserver(mTabModel) {
                @Override
                public void onPageLoadStarted(Tab tab, GURL url) {
                    Log.i(TAG, "onPageLoad called for %s", url.getPossiblyInvalidSpec());
                    maybeCheckReadability(url);
                }

                @Override
                protected void onTabSelected(Tab tab) {
                    Log.i(TAG, "onTabSelected called for" + tab.getUrl().getPossiblyInvalidSpec());
                    super.onTabSelected(tab);
                    maybeCheckReadability(tab.getUrl());
                }
            };
        }
    }

    @VisibleForTesting(otherwise = VisibleForTesting.PACKAGE_PRIVATE)
    public void maybeCheckReadability(GURL url) {
        if (!isURLReadAloudSupported(url)) {
            return;
        }
        String urlSpec = url.getSpec();
        if (mReadabilityMap.containsKey(urlSpec) || mPendingRequests.contains(urlSpec)) {
            return;
        }

        if (!isAvailable()) {
            return;
        }

        mPendingRequests.add(urlSpec);
        mReadabilityHooks.isPageReadable(urlSpec, mReadabilityCallback);
    }

    /**
     * Checks if URL is supported by Read Aloud before sending a readability request.
     * Read Aloud won't be supported on the following URLs:
     * - pages without an HTTP(S) scheme
     * - myaccount.google.com and myactivity.google.com
     * - www.google.com/...
     *   - Based on standards.google/processes/domains/domain-guidelines-by-use-case,
     *     www.google.com/... is reserved for Search features and content.
     */
    public boolean isURLReadAloudSupported(GURL url) {
        return url.isValid() && !url.isEmpty()
                && (url.getScheme().equals(UrlConstants.HTTP_SCHEME)
                        || url.getScheme().equals(UrlConstants.HTTPS_SCHEME))
                && !url.getSpec().startsWith(UrlConstants.GOOGLE_ACCOUNT_HOME_URL)
                && !url.getSpec().startsWith(UrlConstants.MY_ACTIVITY_HOME_URL)
                && !url.getSpec().startsWith(UrlConstants.GOOGLE_URL);
    }

    /**
     * Checks if Read Aloud is supported which is true iff: user is not in the
     * incognito mode and user opted into "Make searches and browsing better".
     */
    public boolean isAvailable() {
        Profile profile = mProfileSupplier.get();
        if (profile == null || profile.isOffTheRecord()) {
            return false;
        }
        // Check whether the user has enabled anonymous URL-keyed data collection.
        // This is surfaced on the relatively new "Make searches and browsing better"
        // user setting.
        return UnifiedConsentServiceBridge.isUrlKeyedAnonymizedDataCollectionEnabled(profile);
    }

    /**
     * Returns true if the web contents within current Tab is readable.
     */
    public boolean isReadable(Tab tab) {
        if (isAvailable() && tab.getUrl().isValid()) {
            Boolean isReadable = mReadabilityMap.get(tab.getUrl().getSpec());
            return isReadable == null ? false : isReadable;
        }
        return false;
    }

    public void playTab(Tab tab) {
        assert tab.getUrl().isValid();
        if (mPlaybackHooks == null) {
            mPlaybackHooks = sPlaybackHooksForTesting != null
                    ? sPlaybackHooksForTesting
                    : ReadAloudPlaybackHooksProvider.getInstance();
        }
        // only start a new playback if different URL or no active playback for that url
        if (mCurrentlyPlayingTab == null || !tab.getUrl().equals(mCurrentlyPlayingTab.getUrl())) {
            mCurrentlyPlayingTab = tab;

            if (mPlayback != null) {
                mPlayback.release();
                mPlayback = null;
            }

            PlaybackArgs args = new PlaybackArgs(tab.getUrl().getSpec(),
                    TranslateBridge.getCurrentLanguage(tab),
                    /* voice=*/null, /* dateModifiedMsSinceEpock=*/0);
            mPlaybackHooks.createPlayback(args, mPlaybackCallback);

            // Notify player UI that playback is happening soon.
            mPlayerCoordinator.playTabRequested();
        }
    }

    /**
     * Whether or not timepoints are supported for the tab's content.
     * Timepoints are needed for word highlighting.
     */
    public boolean timepointsSupported(Tab tab) {
        if (isAvailable() && tab.getUrl().isValid()) {
            Boolean timepointsSuported = mTimepointsSupportedMap.get(tab.getUrl().getSpec());
            return timepointsSuported == null ? false : timepointsSuported;
        }
        return false;
    }

    /** Cleanup: unregister listeners. */
    public void destroy() {
        // Stop playback and hide players.
        mPlayerCoordinator.destroy();

        if (mTabObserver != null) {
            mTabObserver.destroy();
        }

        if (mPlayback != null) {
            mPlayback.release();
            mPlayback = null;
        }
        if (mCurrentlyPlayingTab != null) {
            mCurrentlyPlayingTab = null;
        }
    }

    @VisibleForTesting(otherwise = VisibleForTesting.PACKAGE_PRIVATE)
    public static void setReadabilityHooks(ReadAloudReadabilityHooks hooks) {
        sReadabilityHooksForTesting = hooks;
        ResettersForTesting.register(() -> sReadabilityHooksForTesting = null);
    }

    @VisibleForTesting(otherwise = VisibleForTesting.PACKAGE_PRIVATE)
    public static void setPlaybackHooks(ReadAloudPlaybackHooks hooks) {
        sPlaybackHooksForTesting = hooks;
        ResettersForTesting.register(() -> sPlaybackHooksForTesting = null);
    }

    @VisibleForTesting(otherwise = VisibleForTesting.PACKAGE_PRIVATE)
    public static void setPlayerCoordinator(PlayerCoordinator coordinator) {
        sPlayerCoordinatorForTesting = coordinator;
        ResettersForTesting.register(() -> sPlayerCoordinatorForTesting = null);
    }
}
