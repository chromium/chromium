// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.readaloud;

import static org.chromium.chrome.modules.readaloud.PlaybackListener.State.PAUSED;
import static org.chromium.chrome.modules.readaloud.PlaybackListener.State.PLAYING;
import static org.chromium.chrome.modules.readaloud.PlaybackListener.State.STOPPED;

import android.app.Activity;
import android.content.Intent;

import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.ApplicationState;
import org.chromium.base.ApplicationStatus;
import org.chromium.base.Log;
import org.chromium.base.Promise;
import org.chromium.base.ResettersForTesting;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.supplier.OneShotCallback;
import org.chromium.chrome.browser.browser_controls.BrowserControlsSizer;
import org.chromium.chrome.browser.device.DeviceConditions;
import org.chromium.chrome.browser.language.AppLocaleUtils;
import org.chromium.chrome.browser.layouts.LayoutManager;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabSelectionType;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelTabObserver;
import org.chromium.chrome.browser.translate.TranslateBridge;
import org.chromium.chrome.browser.translate.TranslationObserver;
import org.chromium.chrome.modules.readaloud.Playback;
import org.chromium.chrome.modules.readaloud.PlaybackArgs;
import org.chromium.chrome.modules.readaloud.PlaybackArgs.PlaybackVoice;
import org.chromium.chrome.modules.readaloud.PlaybackListener;
import org.chromium.chrome.modules.readaloud.Player;
import org.chromium.chrome.modules.readaloud.ReadAloudPlaybackHooks;
import org.chromium.chrome.modules.readaloud.ReadAloudPlaybackHooksProvider;
import org.chromium.chrome.modules.readaloud.contentjs.Highlighter;
import org.chromium.chrome.modules.readaloud.contentjs.Highlighter.Mode;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.widget.InsetObserver;
import org.chromium.components.browser_ui.widget.InsetObserverSupplier;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.components.prefs.PrefService;
import org.chromium.components.user_prefs.UserPrefs;
import org.chromium.content_public.browser.GlobalRenderFrameHostId;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.ActivityWindowAndroid;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.url.GURL;

import java.util.HashMap;
import java.util.HashSet;
import java.util.List;
import java.util.Map;

/**
 * The main entrypoint component for Read Aloud feature. It's responsible for checking its
 * availability and triggering playback. Only instantiate after native is initialized.
 */
public class ReadAloudController
        implements Player.Observer,
                Player.Delegate,
                PlaybackListener,
                ApplicationStatus.ApplicationStateListener,
                InsetObserver.WindowInsetObserver {
    private static final String TAG = "ReadAloudController";

    private final Activity mActivity;
    private final ObservableSupplier<Profile> mProfileSupplier;
    private final ObservableSupplierImpl<String> mReadabilitySupplier =
            new ObservableSupplierImpl();
    private final Map<String, String> mSanitizedToFullUrlMap = new HashMap<>();
    private final Map<String, Boolean> mReadabilityMap = new HashMap<>();
    private final Map<String, Boolean> mTimepointsSupportedMap = new HashMap<>();
    private final HashSet<String> mPendingRequests = new HashSet<>();
    private final TabModel mTabModel;
    private final TabModel mIncognitoTabModel;
    @Nullable private Player mPlayerCoordinator;
    private final ObservableSupplier<LayoutManager> mLayoutManagerSupplier;

    private TabModelTabObserver mTabObserver;
    private TabModelTabObserver mIncognitoTabObserver;

    private boolean mPausedForIncognito;

    private final BottomSheetController mBottomSheetController;
    private final BrowserControlsSizer mBrowserControlsSizer;
    private final ActivityLifecycleDispatcher mActivityLifecycleDispatcher;
    private ReadAloudReadabilityHooks mReadabilityHooks;

    @Nullable private static ReadAloudReadabilityHooks sReadabilityHooksForTesting;
    @Nullable private ReadAloudPlaybackHooks mPlaybackHooks;
    @Nullable private static ReadAloudPlaybackHooks sPlaybackHooksForTesting;
    @Nullable private Highlighter mHighlighter;
    @Nullable private Highlighter.Config mHighlighterConfig;

    // Information tied to a playback. When playback is reset it should be set to null together
    //  with the mCurrentlyPlayingTab and mGlobalRenderFrameId
    @Nullable private Playback mPlayback;
    @Nullable private Tab mCurrentlyPlayingTab;
    @Nullable private GlobalRenderFrameHostId mGlobalRenderFrameId;
    // Current tab playback data, or null if there is no playback.
    @Nullable private PlaybackData mCurrentPlaybackData;

    // Playback for voice previews.
    @Nullable private Playback mVoicePreviewPlayback;

    // Information about a tab playback necessary for resuming later. Does not
    // include language or voice which should come from current tab state or
    // settings respectively.
    private class RestoreState {
        // Tab to play.
        private final Tab mTab;
        // Paragraph index to resume from.
        private final int mParagraphIndex;
        // Optional - position within the paragraph to resume from.
        private final long mOffsetNanos;
        // True if audio should start playing immediately when this state is restored.
        private final boolean mPlaying;

        /**
         * Constructor.
         *
         * @param tab Tab to play.
         * @param data Current PlaybackData which may be null if playback hasn't started yet.
         */
        RestoreState(Tab tab, @Nullable PlaybackData data) {
            this(tab, data, /* useOffsetInParagraph= */ true, /* shouldPlayOverride= */ null);
        }

        /**
         * Constructor.
         *
         * @param tab Tab to play.
         * @param data Current PlaybackData which may be null if playback hasn't started yet.
         */
        RestoreState(
                Tab tab,
                @Nullable PlaybackData data,
                boolean useOffsetInParagraph,
                @Nullable Boolean shouldPlayOverride) {
            mTab = tab;
            if (data == null) {
                mParagraphIndex = 0;
                mOffsetNanos = 0L;
            } else {
                mParagraphIndex = data.paragraphIndex();
                mOffsetNanos = data.positionInParagraphNanos();
            }

            if (shouldPlayOverride != null) {
                mPlaying = shouldPlayOverride;
            } else {
                mPlaying = data == null ? true : data.state() != PAUSED && data.state() != STOPPED;
            }
        }

        /** Apply the saved playback state. */
        void restore() {
            createTabPlayback(mTab)
                    .then(
                            playback -> {
                                if (mPlaying) {
                                    mPlayerCoordinator.playbackReady(playback, PLAYING);
                                    playback.play();
                                } else {
                                    mPlayerCoordinator.playbackReady(playback, PAUSED);
                                }

                                if (mParagraphIndex != 0 || mOffsetNanos != 0) {
                                    playback.seekToParagraph(
                                            mParagraphIndex, /* offsetNanos= */ mOffsetNanos);
                                }
                            },
                            exception -> {
                                Log.d(
                                        TAG,
                                        "Failed to restore playback state: %s",
                                        exception.getMessage());
                            });
        }
    }

    // State of playback that was interrupted by a voice preview and should be
    // restored when closing the voice menu.
    @Nullable private RestoreState mStateToRestoreOnVoiceMenuClose;
    // State of playback that was interrupted by backgrounding Chrome.
    @Nullable private RestoreState mStateToRestoreOnBringingToForeground;

    // Whether or not to highlight the page. Change will only have effect if
    // isHighlightingSupported() returns true.
    private final ObservableSupplierImpl<Boolean> mHighlightingEnabled;
    // Voices to show in voice selection menu.
    private final ObservableSupplierImpl<List<PlaybackVoice>> mCurrentLanguageVoices;
    // Selected voice ID.
    private final ObservableSupplierImpl<String> mSelectedVoiceId;
    private final ActivityWindowAndroid mActivityWindowAndroid;

    private long mTranslationObserverHandle;
    private final TranslationObserver mTranslationObserver =
            new TranslationObserver() {
                @Override
                public void onIsPageTranslatedChanged(WebContents webContents) {
                    if (mCurrentlyPlayingTab != null) {
                        maybeStopPlayback(mCurrentlyPlayingTab);
                    }
                }

                @Override
                public void onPageTranslated(
                        String sourceLanguage, String translatedLanguage, int errorCode) {
                    if (mCurrentlyPlayingTab != null && errorCode == 0) {
                        maybeStopPlayback(mCurrentlyPlayingTab);
                    }
                }
            };

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
                    ReadAloudMetrics.recordIsPageReadable(isReadable);
                    ReadAloudMetrics.recordIsPageReadabilitySuccessful(true);

                    // Register _KnownReadable trial before checking more playback conditions
                    if (isReadable) {
                        ReadAloudFeatures.activateKnownReadableTrial();
                    }

                    // isPlaybackEnabled() should only be checked if isReadable == true.
                    isReadable = isReadable && ReadAloudFeatures.isPlaybackEnabled();

                    mReadabilityMap.put(url, isReadable);
                    mTimepointsSupportedMap.put(url, timepointsSupported);
                    mPendingRequests.remove(url);
                    mReadabilitySupplier.set(mSanitizedToFullUrlMap.get(url));
                }

                @Override
                public void onFailure(String url, Throwable t) {
                    Log.d(TAG, "onFailure called for %s because %s", url, t);
                    ReadAloudMetrics.recordIsPageReadabilitySuccessful(false);
                    mPendingRequests.remove(url);
                }
            };

    private PlaybackListener mVoicePreviewPlaybackListener =
            new PlaybackListener() {
                @Override
                public void onPlaybackDataChanged(PlaybackData data) {
                    if (data.state() == PlaybackListener.State.STOPPED) {
                        destroyVoicePreview();
                    }
                }
            };

    public ReadAloudController(
            Activity activity,
            ObservableSupplier<Profile> profileSupplier,
            TabModel tabModel,
            TabModel incognitoTabModel,
            BottomSheetController bottomSheetController,
            BrowserControlsSizer browserControlsSizer,
            ObservableSupplier<LayoutManager> layoutManagerSupplier,
            ActivityWindowAndroid activityWindowAndroid,
            ActivityLifecycleDispatcher activityLifecycleDispatcher) {
        ReadAloudFeatures.init();
        mActivity = activity;
        mProfileSupplier = profileSupplier;
        new OneShotCallback<Profile>(mProfileSupplier, this::onProfileAvailable);
        mTabModel = tabModel;
        mIncognitoTabModel = incognitoTabModel;
        mBottomSheetController = bottomSheetController;
        mCurrentLanguageVoices = new ObservableSupplierImpl<>();
        mSelectedVoiceId = new ObservableSupplierImpl<>();
        mBrowserControlsSizer = browserControlsSizer;
        mLayoutManagerSupplier = layoutManagerSupplier;
        mHighlightingEnabled = new ObservableSupplierImpl<>(false);
        ApplicationStatus.registerApplicationStateListener(this);
        mActivityWindowAndroid = activityWindowAndroid;
        mActivityLifecycleDispatcher = activityLifecycleDispatcher;
    }

    public ObservableSupplier<String> getReadabilitySupplier() {
        return mReadabilitySupplier;
    }

    private void onProfileAvailable(Profile profile) {
        mReadabilityHooks =
                sReadabilityHooksForTesting != null
                        ? sReadabilityHooksForTesting
                        : new ReadAloudReadabilityHooksImpl(mActivity, profile);
        if (mReadabilityHooks.isEnabled()) {
            mHighlightingEnabled.addObserver(
                    ReadAloudController.this::onHighlightingEnabledChanged);
            mHighlightingEnabled.set(ReadAloudPrefs.isHighlightingEnabled(getPrefService()));
            ReadAloudMetrics.recordHighlightingEnabledOnStartup(mHighlightingEnabled.get());
            mTabObserver =
                    new TabModelTabObserver(mTabModel) {
                        @Override
                        public void onPageLoadStarted(Tab tab, GURL url) {
                            Log.d(TAG, "onPageLoad called for %s", url.getPossiblyInvalidSpec());
                            maybeCheckReadability(url);
                            maybeHandleTabReload(tab, url);
                            maybeStopPlayback(tab);
                            boolean isAllowed = ReadAloudFeatures.isAllowed(mProfileSupplier.get());
                            ReadAloudMetrics.recordIsUserEligible(isAllowed);
                            if (!isAllowed) {
                                ReadAloudMetrics.recordIneligibilityReason(
                                        ReadAloudFeatures.getIneligibilityReason());
                            }
                        }

                        @Override
                        public void onActivityAttachmentChanged(
                                Tab tab, @Nullable WindowAndroid window) {
                            super.onActivityAttachmentChanged(tab, window);
                            Log.d(TAG, "onActivityAttachmentChanged");
                            maybeStopPlayback(tab);
                        }

                        @Override
                        protected void onTabSelected(Tab tab) {
                            super.onTabSelected(tab);
                            if (tab != null && tab.getUrl() != null) {
                                Log.d(
                                        TAG,
                                        "onTabSelected called for "
                                                + tab.getUrl().getPossiblyInvalidSpec());
                                maybeCheckReadability(tab.getUrl());

                                if (mPausedForIncognito) {
                                    mPausedForIncognito = false;
                                    if (mPlayback != null) {
                                        mPlayerCoordinator.restorePlayers();
                                    }
                                }
                            }
                        }

                        @Override
                        public void willCloseTab(Tab tab) {
                            Log.d(TAG, "Tab closed");
                            maybeStopPlayback(tab);
                        }
                    };

            mIncognitoTabObserver =
                    new TabModelTabObserver(mIncognitoTabModel) {
                        @Override
                        protected void onTabSelected(Tab tab) {
                            super.onTabSelected(tab);
                            if (tab == null || !tab.isIncognito()) {
                                return;
                            }

                            if (mPlayback != null && !mPausedForIncognito) {
                                mPlayback.pause();
                                mPlayerCoordinator.hidePlayers();
                                mPausedForIncognito = true;
                            }
                        }
                    };

            InsetObserver insetObserver =
                    InsetObserverSupplier.getValueOrNullFrom(mActivityWindowAndroid);
            if (insetObserver != null) {
                insetObserver.addObserver(this);
            }
        }
    }

    @VisibleForTesting(otherwise = VisibleForTesting.PACKAGE_PRIVATE)
    public void maybeCheckReadability(GURL url) {
        if (!isURLReadAloudSupported(url)) {
            ReadAloudMetrics.recordIsPageReadable(false);
            return;
        }

        String urlSpec = stripUserData(url).getSpec();
        // TODO: 2 different URLs can have the same sanitized URL
        mSanitizedToFullUrlMap.put(url.getSpec(), urlSpec);
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
        return url.isValid()
                && !url.isEmpty()
                && (url.getScheme().equals(UrlConstants.HTTP_SCHEME)
                        || url.getScheme().equals(UrlConstants.HTTPS_SCHEME))
                && !url.getSpec().startsWith(UrlConstants.GOOGLE_ACCOUNT_HOME_URL)
                && !url.getSpec().startsWith(UrlConstants.MY_ACTIVITY_HOME_URL)
                && !url.getSpec().startsWith(UrlConstants.GOOGLE_URL);
    }

    /**
     * Checks if Read Aloud is supported which is true iff: user is not in the incognito mode and
     * user opted into "Make searches and browsing better". If the ReadAloudInMultiWindow flag is
     * disabled, this will return false if the activity is in multi window mode.
     */
    public boolean isAvailable() {
        return ReadAloudFeatures.isAllowed(mProfileSupplier.get())
                && !ReadAloudFeatures.isInMultiWindowAndDisabled(mActivity);
    }

    /** Returns true if the web contents within current Tab is readable. */
    public boolean isReadable(Tab tab) {
        if (isTabLanguageSupported(tab) && isAvailable() && tab.getUrl().isValid()) {
            Boolean isReadable = mReadabilityMap.get(stripUserData(tab.getUrl()).getSpec());
            return isReadable == null ? false : isReadable;
        }
        return false;
    }

    /** Returns true if the tab's current language is supported by the available voices. */
    private boolean isTabLanguageSupported(Tab tab) {
        if (mReadabilityHooks == null) {
            return false;
        }

        String playbackLanguage = getLanguageForNewPlayback(tab);
        return mReadabilityHooks.getCompatibleLanguages().contains(playbackLanguage);
    }

    /**
     * Play the tab, creating and showing the player if it isn't already showing. No effect if tab's
     * URL is the same as the URL that is already playing.
     *
     * @param tab Tab to play.
     */
    public void playTab(Tab tab) {
        createTabPlayback(tab)
                .then(
                        playback -> {
                            mPlayerCoordinator.playbackReady(playback, PLAYING);
                            playback.play();
                            ReadAloudMetrics.recordPlaybackStarted();
                        },
                        exception -> {
                            Log.d(TAG, "playTab failed: %s", exception.getMessage());
                        });
    }

    private Promise<Playback> createTabPlayback(Tab tab) {
        assert tab.getUrl().isValid();
        if (mPlaybackHooks == null) {
            mPlaybackHooks =
                    sPlaybackHooksForTesting != null
                            ? sPlaybackHooksForTesting
                            : ReadAloudPlaybackHooksProvider.getForProfile(mProfileSupplier.get());
            mPlayerCoordinator = mPlaybackHooks.createPlayer(/* delegate= */ this);
            mPlayerCoordinator.addObserver(this);
        }
        // only start a new playback if different URL or no active playback for that url
        if (mCurrentlyPlayingTab != null && tab.getUrl().equals(mCurrentlyPlayingTab.getUrl())) {
            var promise = new Promise<Playback>();
            promise.reject(new Exception("Tab already playing"));
            return promise;
        }

        resetCurrentPlayback();
        mCurrentlyPlayingTab = tab;
        mTranslationObserverHandle =
                TranslateBridge.addTranslationObserver(
                        mCurrentlyPlayingTab.getWebContents(), mTranslationObserver);
        if (!mPlaybackHooks.voicesInitialized()) {
            mPlaybackHooks.initVoices();
        }

        // Notify player UI that playback is happening soon and show UI in case there's an error
        // coming.
        mPlayerCoordinator.playTabRequested();

        final String playbackLanguage = getLanguageForNewPlayback(tab);
        boolean isTranslated = TranslateBridge.isPageTranslated(tab.getWebContents());
        var voices = mPlaybackHooks.getVoicesFor(playbackLanguage);
        // TODO: Don't show entrypoints for unsupported languages
        if (voices == null || voices.isEmpty()) {
            onCreatePlaybackFailed();
            var promise = new Promise<Playback>();
            promise.reject(new Exception("Unsupported language"));
            return promise;
        }

        PlaybackArgs args =
                new PlaybackArgs(
                        stripUserData(tab.getUrl()).getSpec(),
                        isTranslated ? playbackLanguage : null,
                        mPlaybackHooks.getPlaybackVoiceList(
                                ReadAloudPrefs.getVoices(getPrefService())),
                        /* dateModifiedMsSinceEpoch= */ 0);
        Log.d(TAG, "Creating playback with args: %s", args);

        Promise<Playback> promise = createPlayback(args);
        promise.then(
                playback -> {
                    ReadAloudMetrics.recordIsTabPlaybackCreationSuccessful(true);
                    maybeSetUpHighlighter(playback.getMetadata());
                    updateVoiceMenu(
                            isTranslated
                                    ? playbackLanguage
                                    : getLanguage(playback.getMetadata().languageCode()));
                    mPlayback = playback;
                    mPlayback.addListener(ReadAloudController.this);
                },
                exception -> {
                    Log.e(TAG, exception.getMessage());
                    onCreatePlaybackFailed();
                });
        return promise;
    }

    private void onCreatePlaybackFailed() {
        ReadAloudMetrics.recordIsTabPlaybackCreationSuccessful(false);
        mPlayerCoordinator.playbackFailed();
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
            mPlayerCoordinator.recordPlaybackDuration();
        }
        if (mTranslationObserverHandle != 0L) {
            assert mCurrentlyPlayingTab != null;
            TranslateBridge.removeTranslationObserver(
                    mCurrentlyPlayingTab.getWebContents(), mTranslationObserverHandle);
            mTranslationObserverHandle = 0L;
        }
        mCurrentlyPlayingTab = null;
        mGlobalRenderFrameId = null;
        mCurrentPlaybackData = null;
        mPausedForIncognito = false;
    }

    /** Cleanup: unregister listeners. */
    public void destroy() {
        if (mVoicePreviewPlayback != null) {
            destroyVoicePreview();
        }

        // Stop playback and hide players.
        if (mPlayerCoordinator != null) {
            mPlayerCoordinator.destroy();
        }

        if (mTabObserver != null) {
            mTabObserver.destroy();
        }
        mHighlightingEnabled.removeObserver(ReadAloudController.this::onHighlightingEnabledChanged);
        ApplicationStatus.unregisterApplicationStateListener(this);
        resetCurrentPlayback();
        mStateToRestoreOnBringingToForeground = null;
        ReadAloudFeatures.shutdown();
        InsetObserver insetObserver =
                InsetObserverSupplier.getValueOrNullFrom(mActivityWindowAndroid);
        if (insetObserver != null) {
            insetObserver.removeObserver(this);
        }
    }

    private void maybeSetUpHighlighter(Playback.Metadata metadata) {
        boolean highlightingSupported = isHighlightingSupported();
        ReadAloudMetrics.recordHighlightingSupported(highlightingSupported);
        if (highlightingSupported) {
            if (mHighlighter == null) {
                mHighlighter = mPlaybackHooks.createHighlighter();
            }
            mHighlighterConfig = new Highlighter.Config(mActivity);
            mHighlighterConfig.setMode(Mode.TEXT_HIGHLIGHTING_MODE_WORD);
            mHighlighter.initializeJs(mCurrentlyPlayingTab, metadata, mHighlighterConfig);
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

    /** Update the page highlighting setting. */
    private void onHighlightingEnabledChanged(boolean enabled) {
        ReadAloudPrefs.setHighlightingEnabled(getPrefService(), enabled);
        if (!enabled) {
            // clear highlighting
            maybeClearHighlights();
        }
    }

    private void maybeClearHighlights() {
        if (mHighlighter != null && mGlobalRenderFrameId != null && mCurrentlyPlayingTab != null) {
            mHighlighter.clearHighlights(mGlobalRenderFrameId, mCurrentlyPlayingTab);
        }
    }

    private void maybeHighlightText(PhraseTiming phraseTiming) {
        if (mHighlightingEnabled.get()
                && mHighlighter != null
                && mGlobalRenderFrameId != null
                && mCurrentlyPlayingTab != null) {
            mHighlighter.highlightText(mGlobalRenderFrameId, mCurrentlyPlayingTab, phraseTiming);
        }
    }

    /**
     * Dismiss the player UI if present and stop and release playback if playing.
     *
     * @param tab if specified, a playback will be stopped if it was triggered for this tab; if null
     *     any active playback will be stopped.
     */
    public void maybeStopPlayback(@Nullable Tab tab) {
        if (mCurrentlyPlayingTab == null && mPlayerCoordinator != null) {
            // in case there's an error and UI is drawn
            mPlayerCoordinator.dismissPlayers();
        } else if (mCurrentlyPlayingTab != null
                && (tab == null || mCurrentlyPlayingTab.getId() == tab.getId())) {
            mPlayerCoordinator.dismissPlayers();
            resetCurrentPlayback();
        }
    }

    /** Pause audio if playing. */
    public void pause() {
        if (mPlayback != null && mCurrentPlaybackData.state() == PLAYING) {
            mPlayback.pause();
        }
    }

    private void maybeHandleTabReload(Tab tab, GURL newUrl) {
        if (mHighlighter != null
                && tab.getUrl() != null
                && tab.getUrl().getSpec().equals(newUrl.getSpec())) {
            mHighlighter.handleTabReloaded(tab);
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

    private String getLanguageForNewPlayback(Tab tab) {
        String language = TranslateBridge.getCurrentLanguage(tab);
        if (language == null || language.isEmpty() || language.equals("und")) {
            language = AppLocaleUtils.getAppLanguagePref();
        }

        if (language == null) {
            Log.d(TAG, "Neither page nor app language known. Falling back to en.");
            language = "en";
        }

        // If language string is a locale like "en-US", strip the "-US" part.
        return getLanguage(language);
    }

    /** Is language string includes locale, strip it */
    private String getLanguage(String language) {
        if (language.contains("-")) {
            return language.split("-")[0];
        }
        return language;
    }

    private void updateVoiceMenu(@Nullable String language) {
        if (language == null) {
            return;
        }

        List<PlaybackVoice> voices = mPlaybackHooks.getVoicesFor(language);
        mCurrentLanguageVoices.set(voices);

        String selectedVoiceId = ReadAloudPrefs.getVoices(getPrefService()).get(language);
        if (selectedVoiceId == null) {
            selectedVoiceId = voices.get(0).getVoiceId();
        }
        mSelectedVoiceId.set(selectedVoiceId);
    }

    /**
     * Pause if the given intent is for processing text.
     *
     * @param intent Intent being sent by Chrome.
     */
    public void maybePauseForOutgoingIntent(@Nullable Intent intent) {
        if (intent != null && intent.getAction().equals(Intent.ACTION_PROCESS_TEXT)) {
            pause();
        }
    }

    // Player.Delegate
    @Override
    public BottomSheetController getBottomSheetController() {
        return mBottomSheetController;
    }

    @Override
    public boolean isHighlightingSupported() {
        if (mCurrentlyPlayingTab == null) {
            return false;
        }
        return timepointsSupported(mCurrentlyPlayingTab)
                && !TranslateBridge.isPageTranslated(mCurrentlyPlayingTab.getWebContents());
    }

    @Override
    public ObservableSupplierImpl<Boolean> getHighlightingEnabledSupplier() {
        return mHighlightingEnabled;
    }

    @Override
    public void setHighlighterMode(@Highlighter.Mode int mode) {
        // Highlighter initialization is expensive, so only do it if necessary
        if (mHighlighter != null
                && mHighlighterConfig != null
                && mode != mHighlighterConfig.getMode()) {
            mHighlighterConfig.setMode(mode);
            mHighlighter.handleTabReloaded(mCurrentlyPlayingTab);
            mHighlighter.initializeJs(
                    mCurrentlyPlayingTab, mPlayback.getMetadata(), mHighlighterConfig);
        }
    }

    @Override
    public ObservableSupplier<List<PlaybackVoice>> getCurrentLanguageVoicesSupplier() {
        return mCurrentLanguageVoices;
    }

    @Override
    public ObservableSupplier<String> getVoiceIdSupplier() {
        return mSelectedVoiceId;
    }

    @Override
    public void setVoiceOverrideAndApplyToPlayback(PlaybackVoice voice) {
        ReadAloudPrefs.setVoice(getPrefService(), voice.getLanguage(), voice.getVoiceId());
        mSelectedVoiceId.set(voice.getVoiceId());

        if (mCurrentlyPlayingTab != null && mPlayback != null) {
            RestoreState state = new RestoreState(mCurrentlyPlayingTab, mCurrentPlaybackData);
            resetCurrentPlayback();
            // This should re-request playback with the same playback state and paragraph
            // and the new voice.
            state.restore();
        }
    }

    @Override
    public Promise<Playback> previewVoice(PlaybackVoice voice) {
        // Only one playback possible at a time, so current playback must be stopped and
        // cleaned up. May be null if the most recent playback was a voice preview.
        if (mCurrentlyPlayingTab != null) {
            mStateToRestoreOnVoiceMenuClose =
                    new RestoreState(mCurrentlyPlayingTab, mCurrentPlaybackData);
            resetCurrentPlayback();
        }

        if (mVoicePreviewPlayback != null) {
            destroyVoicePreview();
        }

        Log.d(
                TAG,
                "Requested preview of voice %s from language %s",
                voice.getVoiceId(),
                voice.getLanguage());

        PlaybackArgs args =
                new PlaybackArgs(
                        mActivity.getString(R.string.readaloud_voice_preview_message),
                        /* isUrl= */ false,
                        voice.getLanguage(),
                        mPlaybackHooks.getPlaybackVoiceList(
                                Map.of(voice.getLanguage(), voice.getVoiceId())),
                        /* dateModifiedMsSinceEpoch= */ 0);
        Log.d(TAG, "Voice preview args: %s", args);

        Promise<Playback> promise = createPlayback(args);
        promise.then(
                playback -> {
                    Log.d(TAG, "Voice preview playback created.");
                    ReadAloudMetrics.recordVoicePreviewed(voice.getVoiceId());
                    mVoicePreviewPlayback = playback;
                    playback.addListener(mVoicePreviewPlaybackListener);
                    mVoicePreviewPlayback.play();
                },
                exception -> {
                    Log.e(TAG, "Failed to create voice preview: %s", exception.getMessage());
                });
        return promise;
    }

    private void destroyVoicePreview() {
        mVoicePreviewPlayback.removeListener(mVoicePreviewPlaybackListener);
        mVoicePreviewPlayback.release();
        mVoicePreviewPlayback = null;
    }

    private Promise<Playback> createPlayback(PlaybackArgs args) {
        final var promise = new Promise<Playback>();
        mPlaybackHooks.createPlayback(
                args,
                new ReadAloudPlaybackHooks.CreatePlaybackCallback() {
                    @Override
                    public void onSuccess(Playback playback) {
                        // Check if in multi-window mode and not supporting multi-window
                        // This failure will also trigger when the user goes into multi-window mode
                        // with a playback since we will attempt to restore
                        if (ReadAloudFeatures.isInMultiWindowAndDisabled(mActivity)) {
                            playback.release();
                            promise.reject(new Exception("In multi window mode"));
                            return;
                        }
                        // If we rely on the backend to detect page language, ensure it is supported
                        if (args.getLanguage() == null
                                && !mReadabilityHooks
                                        .getCompatibleLanguages()
                                        .contains(
                                                getLanguage(
                                                        playback.getMetadata().languageCode()))) {
                            playback.release();
                            promise.reject(new Exception("Unsupported language"));
                            return;
                        }

                        promise.fulfill(playback);
                    }

                    @Override
                    public void onFailure(Throwable throwable) {
                        promise.reject(new Exception(throwable));
                    }
                });
        return promise;
    }

    @Override
    public ActivityLifecycleDispatcher getActivityLifecycleDispatcher() {
        return mActivityLifecycleDispatcher;
    }

    @Override
    public void navigateToPlayingTab() {
        if (mCurrentlyPlayingTab == null) {
            return;
        }
        if (mTabModel.indexOf(mCurrentlyPlayingTab) != TabModel.INVALID_TAB_INDEX) {
            mTabModel.setIndex(
                    mTabModel.indexOf(mCurrentlyPlayingTab), TabSelectionType.FROM_USER, false);
        }
    }

    @Override
    public Activity getActivity() {
        return mActivity;
    }

    @Override
    public PrefService getPrefService() {
        return UserPrefs.get(mProfileSupplier.get());
    }

    @Override
    public BrowserControlsSizer getBrowserControlsSizer() {
        return mBrowserControlsSizer;
    }

    @Override
    @Nullable
    public LayoutManager getLayoutManager() {
        return mLayoutManagerSupplier.get();
    }

    // Player.Observer
    @Override
    public void onRequestClosePlayers() {
        maybeStopPlayback(mCurrentlyPlayingTab);
    }

    @Override
    public void onVoiceMenuClosed() {
        if (mVoicePreviewPlayback != null) {
            destroyVoicePreview();
        }

        if (mStateToRestoreOnVoiceMenuClose != null) {
            mStateToRestoreOnVoiceMenuClose.restore();
            mStateToRestoreOnVoiceMenuClose = null;
        }
    }

    // InsetObserver.WindowInsetObserver
    @Override
    public void onKeyboardInsetChanged(int inset) {
        if (inset > 0) {
            maybeHidePlayer();
        } else {
            maybeShowPlayer();
        }
    }

    /** Show mini player if there is an active playback. */
    public void maybeShowPlayer() {
        if (mPlayback != null) {
            mPlayerCoordinator.restorePlayers();
        }
    }

    /**
     * If there's an active playback, this method will hide the player (either the mini player or
     * the expanded player - whichever is showing) without stopping audio. To bring back the player
     * UI, call {@link #maybeShowPlayer() maybeShowPlayer}
     */
    public void maybeHidePlayer() {
        if (mPlayback != null) {
            mPlayerCoordinator.hidePlayers();
        }
    }

    // PlaybackListener methods
    @Override
    public void onPhraseChanged(PhraseTiming phraseTiming) {
        maybeHighlightText(phraseTiming);
    }

    @Override
    public void onPlaybackDataChanged(PlaybackData data) {
        mCurrentPlaybackData = data;
    }

    @Override
    public void onApplicationStateChange(@ApplicationState int newState) {
        boolean isScreenLocked =
                DeviceConditions.isCurrentlyScreenOnAndUnlocked(mActivity.getApplicationContext());
        // stop any playback if user left Chrome while screen is on and unlocked
        if (newState == ApplicationState.HAS_STOPPED_ACTIVITIES && isScreenLocked) {
            if (mCurrentlyPlayingTab != null) {
                mStateToRestoreOnBringingToForeground =
                        new RestoreState(
                                mCurrentlyPlayingTab,
                                mCurrentPlaybackData,
                                /* useOffsetInParagraph= */ true,
                                /* shouldPlayOverride= */ false);
            }
            resetCurrentPlayback();
        } else if (newState == ApplicationState.HAS_RUNNING_ACTIVITIES
                && mStateToRestoreOnBringingToForeground != null) {
            mStateToRestoreOnBringingToForeground.restore();
            mStateToRestoreOnBringingToForeground = null;
        }
        if (mPlayerCoordinator != null) {
            if (newState == ApplicationState.HAS_STOPPED_ACTIVITIES && !isScreenLocked) {
                mPlayerCoordinator.onScreenStatusChanged(/* isLocked= */ true);
            } else if (newState == ApplicationState.HAS_RUNNING_ACTIVITIES && isScreenLocked) {
                mPlayerCoordinator.onScreenStatusChanged(/* isLocked= */ false);
            }
        }
    }

    // Tests.
    public void setHighlighterForTests(Highlighter highighter) {
        mHighlighter = highighter;
    }

    public void setTimepointsSupportedForTest(String url, boolean supported) {
        mTimepointsSupportedMap.put(url, supported);
    }

    public TabModelTabObserver getTabModelTabObserverforTests() {
        return mTabObserver;
    }

    public TabModelTabObserver getIncognitoTabModelTabObserverforTests() {
        return mIncognitoTabObserver;
    }

    public TranslationObserver getTranslationObserverForTest() {
        return mTranslationObserver;
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
