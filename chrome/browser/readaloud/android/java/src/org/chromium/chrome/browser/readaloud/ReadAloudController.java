// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.readaloud;

import android.app.Activity;

import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.Log;
import org.chromium.base.ResettersForTesting;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.supplier.OneShotCallback;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelTabObserver;
import org.chromium.chrome.browser.translate.TranslateBridge;
import org.chromium.chrome.modules.readaloud.Playback;
import org.chromium.chrome.modules.readaloud.PlaybackArgs;
import org.chromium.chrome.modules.readaloud.PlaybackArgs.PlaybackVoice;
import org.chromium.chrome.modules.readaloud.PlaybackListener;
import org.chromium.chrome.modules.readaloud.Player;
import org.chromium.chrome.modules.readaloud.ReadAloudPlaybackHooks;
import org.chromium.chrome.modules.readaloud.ReadAloudPlaybackHooksProvider;
import org.chromium.chrome.modules.readaloud.contentjs.Highlighter;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.components.prefs.PrefService;
import org.chromium.components.user_prefs.UserPrefs;
import org.chromium.content_public.browser.GlobalRenderFrameHostId;
import org.chromium.url.GURL;

import java.util.HashMap;
import java.util.HashSet;
import java.util.List;
import java.util.Map;

/**
 * The main entrypoint component for Read Aloud feature. It's responsible for checking its
 * availability and triggering playback.
 */
public class ReadAloudController implements Player.Observer, Player.Delegate, PlaybackListener {
    private static final String TAG = "ReadAloudController";

    private final Activity mActivity;
    private final ObservableSupplier<Profile> mProfileSupplier;
    private final Map<String, Boolean> mReadabilityMap = new HashMap<>();
    private final Map<String, Boolean> mTimepointsSupportedMap = new HashMap<>();
    private final HashSet<String> mPendingRequests = new HashSet<>();
    private final TabModel mTabModel;
    private Player mPlayerCoordinator;

    private TabModelTabObserver mTabObserver;

    private final BottomSheetController mBottomSheetController;

    private ReadAloudReadabilityHooks mReadabilityHooks;

    @Nullable
    private static ReadAloudReadabilityHooks sReadabilityHooksForTesting;
    @Nullable
    private ReadAloudPlaybackHooks mPlaybackHooks;
    @Nullable
    private static ReadAloudPlaybackHooks sPlaybackHooksForTesting;
    @Nullable private Highlighter mHighligher;

    // Information tied to a playback. When playback is reset it should be set to null together
    //  with the mCurrentlyPlayingTab and mGlobalRenderFrameId
    @Nullable private Playback mPlayback;
    @Nullable
    private Tab mCurrentlyPlayingTab;
    @Nullable private GlobalRenderFrameHostId mGlobalRenderFrameId;

    /**
     * Kicks of readability check on a page load iff: the url is valid, no previous result is
     * available/pending and if a request has to be sent, the necessary conditions are satisfied.
     * TODO: Add optimizations (don't send requests on chrome:// pages, remove password from the
     * url, etc). Also include enterprise policy check.
     */
    private ReadAloudReadabilityHooks.ReadabilityCallback mReadabilityCallback =
            new ReadAloudReadabilityHooks.ReadabilityCallback() {
                @Override
                public void onSuccess(String url, boolean isReadable, boolean timepointsSupported) {
                    Log.d(TAG, "onSuccess called for %s", url);
                    mReadabilityMap.put(url, isReadable);
                    mTimepointsSupportedMap.put(url, timepointsSupported);
                    mPendingRequests.remove(url);
                }

                @Override
                public void onFailure(String url, Throwable t) {
                    Log.d(TAG, "onFailure called for %s because %s", url, t);
                    mPendingRequests.remove(url);
                }
            };

    private ReadAloudPlaybackHooks.CreatePlaybackCallback mPlaybackCallback =
            new ReadAloudPlaybackHooks.CreatePlaybackCallback() {
                @Override
                public void onSuccess(Playback playback) {
                    Log.d(TAG, "Playback created");
                    maybeSetUpHighlighter(playback.getMetadata());
                    mPlayback = playback;
                    mPlayback.addListener(ReadAloudController.this);
                    mPlayerCoordinator.playbackReady(mPlayback, PlaybackListener.State.PLAYING);
                    mPlayback.play();
                }

                @Override
                public void onFailure(Throwable t) {
                    Log.d(TAG, t.getMessage());
                    mPlayerCoordinator.playbackFailed();
                }
            };

    public ReadAloudController(
            Activity activity,
            ObservableSupplier<Profile> profileSupplier,
            TabModel tabModel,
            BottomSheetController bottomSheetController) {
        mActivity = activity;
        mProfileSupplier = profileSupplier;
        new OneShotCallback<Profile>(mProfileSupplier, this::onProfileAvailable);
        mTabModel = tabModel;
        mBottomSheetController = bottomSheetController;
    }

    private void onProfileAvailable(Profile profile) {
        mReadabilityHooks =
                sReadabilityHooksForTesting != null
                        ? sReadabilityHooksForTesting
                        : new ReadAloudReadabilityHooksImpl(
                                mActivity, profile, ReadAloudFeatures.getApiKeyOverride());

        if (mReadabilityHooks.isEnabled()) {
            mTabObserver =
                    new TabModelTabObserver(mTabModel) {
                        @Override
                        public void onPageLoadStarted(Tab tab, GURL url) {
                            Log.d(TAG, "onPageLoad called for %s", url.getPossiblyInvalidSpec());
                            maybeCheckReadability(url);
                            maybeHandleTabReload(tab, url);
                        }

                        @Override
                        protected void onTabSelected(Tab tab) {
                            Log.d(
                                    TAG,
                                    "onTabSelected called for"
                                            + tab.getUrl().getPossiblyInvalidSpec());
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

        String urlSpec = stripUserData(url).getSpec();
        if (mReadabilityMap.containsKey(urlSpec) || mPendingRequests.contains(urlSpec)) {
            return;
        }

        if (!isAvailable()) {
            return;
        }

        if (mReadabilityHooks == null) {
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
        return ReadAloudFeatures.isAllowed(mProfileSupplier.get());
    }

    /**
     * Returns true if the web contents within current Tab is readable.
     */
    public boolean isReadable(Tab tab) {
        if (isAvailable() && tab.getUrl().isValid()) {
            Boolean isReadable = mReadabilityMap.get(stripUserData(tab.getUrl()).getSpec());
            return isReadable == null ? false : isReadable;
        }
        return false;
    }

    public void playTab(Tab tab) {
        assert tab.getUrl().isValid();
        if (mPlaybackHooks == null) {
            mPlaybackHooks =
                    sPlaybackHooksForTesting != null
                            ? sPlaybackHooksForTesting
                            : ReadAloudPlaybackHooksProvider.getForProfile(mProfileSupplier.get());
            mPlayerCoordinator = mPlaybackHooks.createPlayer(/* delegate= */ this);
        }
        // only start a new playback if different URL or no active playback for that url
        if (mCurrentlyPlayingTab == null || !tab.getUrl().equals(mCurrentlyPlayingTab.getUrl())) {
            mCurrentlyPlayingTab = tab;

            if (mPlayback != null) {
                mPlayback.release();
                mPlayback = null;
            }

            // TODO Create voice list from settings.
            PlaybackArgs args =
                    new PlaybackArgs(
                            stripUserData(tab.getUrl()).getSpec(),
                            TranslateBridge.getCurrentLanguage(tab),
                            /* voice= */ null,
                            /* dateModifiedMsSinceEpock= */ 0);
            mPlaybackHooks.createPlayback(args, mPlaybackCallback);

            // Notify player UI that playback is happening soon.
            mPlayerCoordinator.playTabRequested();
            mPlayerCoordinator.addObserver(this);
        }
    }

    /**
     * Whether or not timepoints are supported for the tab's content.
     * Timepoints are needed for word highlighting.
     */
    public boolean timepointsSupported(Tab tab) {
        if (isAvailable() && tab.getUrl().isValid()) {
            Boolean timepointsSuported =
                    mTimepointsSupportedMap.get(stripUserData(tab.getUrl()).getSpec());
            return timepointsSuported == null ? false : timepointsSuported;
        }
        return false;
    }

    private void resetCurrentPlayback() {
        // TODO(b/303294007): Investigate exception sometimes thrown by release().
        if (mPlayback != null) {
            maybeClearHighlights();
            mPlayback.removeListener(this);
            mPlayback.release();
            mPlayback = null;
        }
        if (mCurrentlyPlayingTab != null) {
            // TODO: remove translation observer
            mCurrentlyPlayingTab = null;
        }
        mGlobalRenderFrameId = null;
    }

    /** Stops the playback, hides the UI and resets its state. */
    public void stopPlayback() {
        // Players should be dismissed before releasing playback so that the playback
        // listener can be removed.
        mPlayerCoordinator.removeObserver(this);
        mPlayerCoordinator.dismissPlayers();

        resetCurrentPlayback();
    }

    /** Cleanup: unregister listeners. */
    public void destroy() {
        // Stop playback and hide players.
        if (mPlayerCoordinator != null) {
            mPlayerCoordinator.destroy();
        }

        if (mTabObserver != null) {
            mTabObserver.destroy();
        }

        resetCurrentPlayback();
    }

    private void maybeSetUpHighlighter(Playback.Metadata metadata) {
        if (timepointsSupported(mCurrentlyPlayingTab)) {
            if (mHighligher == null) {
                mHighligher = mPlaybackHooks.createHighlighter();
            }

            mHighligher.initializeJs(
                    mCurrentlyPlayingTab, metadata, new Highlighter.Config(mActivity));
            assert (mCurrentlyPlayingTab.getWebContents() != null
                    && mCurrentlyPlayingTab.getWebContents().getMainFrame() != null);
            if (mCurrentlyPlayingTab.getWebContents() != null
                    && mCurrentlyPlayingTab.getWebContents().getMainFrame() != null) {
                mGlobalRenderFrameId =
                        mCurrentlyPlayingTab
                                .getWebContents()
                                .getMainFrame()
                                .getGlobalRenderFrameHostId();
            }
        }
    }

    private void maybeClearHighlights() {
        if (mHighligher != null && mGlobalRenderFrameId != null && mCurrentlyPlayingTab != null) {
            mHighligher.clearHighlights(mGlobalRenderFrameId, mCurrentlyPlayingTab);
        }
    }

    private void maybeHighlightText(PhraseTiming phraseTiming) {
        if (mHighligher != null && mGlobalRenderFrameId != null && mCurrentlyPlayingTab != null) {
            mHighligher.highlightText(mGlobalRenderFrameId, mCurrentlyPlayingTab, phraseTiming);
        }
    }

    private void maybeHandleTabReload(Tab tab, GURL newUrl) {
        if (mHighligher != null
                && tab.getUrl() != null
                && tab.getUrl().getSpec().equals(newUrl.getSpec())) {
            mHighligher.handleTabReloaded(tab);
        }
    }

    private GURL stripUserData(GURL in) {
        if (!in.isValid()
                || in.isEmpty()
                || (in.getUsername().isEmpty() && in.getPassword().isEmpty())) {
            return in;
        }
        return in.replaceComponents(
                /* username= */ null,
                /* clearUsername= */ true,
                /* password= */ null,
                /* clearPassword= */ true);
    }

    // Player.Delegate
    @Override
    public BottomSheetController getBottomSheetController() {
        return mBottomSheetController;
    }

    @Override
    public boolean isHighlightingSupported() {
        // TODO: implement
        return false;
    }

    @Override
    public ObservableSupplierImpl<Boolean> getHighlightingEnabledSupplier() {
        // TODO: implement
        return new ObservableSupplierImpl<Boolean>();
    }

    @Override
    public ObservableSupplier<List<PlaybackVoice>> getCurrentLanguageVoicesSupplier() {
        // TODO: implement
        return new ObservableSupplierImpl<List<PlaybackVoice>>();
    }

    @Override
    public ObservableSupplier<String> getVoiceIdSupplier() {
        // TODO: implement
        return new ObservableSupplierImpl<String>();
    }

    @Override
    public Map<String, String> getVoiceOverrides() {
        // TODO: implement
        return new HashMap<String, String>();
    }

    @Override
    public void setVoiceOverride(PlaybackVoice voice) {
        // TODO: implement
    }

    @Override
    public void previewVoice(PlaybackVoice voice) {
        // TODO: implement
    }

    @Override
    public void navigateToPlayingTab() {
        // TODO: implement
    }

    @Override
    public Activity getActivity() {
        return mActivity;
    }

    @Override
    public PrefService getPrefService() {
        return UserPrefs.get(mProfileSupplier.get());
    }

    // Player.Observer
    @Override
    public void onRequestClosePlayers() {
        stopPlayback();
    }

    // PlaybackListener methods
    @Override
    public void onPhraseChanged(PhraseTiming phraseTiming) {
        maybeHighlightText(phraseTiming);
    }

    // Tests.
    public void setHighlighterForTests(Highlighter highighter) {
        mHighligher = highighter;
    }

    public void setTimepointsSupportedForTest(String url, boolean supported) {
        mTimepointsSupportedMap.put(url, supported);
    }

    public TabModelTabObserver getTabModelTabObserverforTests() {
        return mTabObserver;
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
}
