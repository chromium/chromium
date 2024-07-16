// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.readaloud;

import static org.chromium.chrome.modules.readaloud.PlaybackListener.State.PAUSED;
import static org.chromium.chrome.modules.readaloud.PlaybackListener.State.PLAYING;
import static org.chromium.chrome.modules.readaloud.PlaybackListener.State.STOPPED;

import android.app.Activity;
import android.content.Intent;
import android.util.LruCache;

import androidx.annotation.IntDef;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import com.google.common.hash.Hashing;

import org.chromium.base.ActivityState;
import org.chromium.base.ApplicationState;
import org.chromium.base.ApplicationStatus;
import org.chromium.base.Log;
import org.chromium.base.ObserverList;
import org.chromium.base.Promise;
import org.chromium.base.ResettersForTesting;
import org.chromium.base.UserData;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.supplier.OneShotCallback;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.chrome.browser.browser_controls.BottomControlsStacker;
import org.chromium.chrome.browser.device.DeviceConditions;
import org.chromium.chrome.browser.language.AppLocaleUtils;
import org.chromium.chrome.browser.layouts.LayoutManager;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.lifecycle.OnUserLeaveHintObserver;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.readaloud.exceptions.ReadAloudUnsupportedException;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabSelectionType;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.translate.TranslateBridge;
import org.chromium.chrome.browser.translate.TranslationObserver;
import org.chromium.chrome.modules.readaloud.Playback;
import org.chromium.chrome.modules.readaloud.PlaybackArgs;
import org.chromium.chrome.modules.readaloud.PlaybackArgs.PlaybackVoice;
import org.chromium.chrome.modules.readaloud.PlaybackListener;
import org.chromium.chrome.modules.readaloud.Player;
import org.chromium.chrome.modules.readaloud.ReadAloudPlaybackHooks;
import org.chromium.chrome.modules.readaloud.ReadAloudPlaybackHooksProvider;
import org.chromium.chrome.modules.readaloud.contentjs.Extractor;
import org.chromium.chrome.modules.readaloud.contentjs.Highlighter;
import org.chromium.chrome.modules.readaloud.contentjs.Highlighter.Mode;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.components.prefs.PrefService;
import org.chromium.components.user_prefs.UserPrefs;
import org.chromium.content_public.browser.GlobalRenderFrameHostId;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.InsetObserver;
import org.chromium.ui.base.ActivityWindowAndroid;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.url.GURL;

import java.time.Duration;
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
                ApplicationStatus.ActivityStateListener,
                ApplicationStatus.ApplicationStateListener,
                InsetObserver.WindowInsetObserver,
                OnUserLeaveHintObserver {
    private static final String TAG = "ReadAloudController";
    private static final Class<RestoreState> USER_DATA_KEY = RestoreState.class;
    // There may be multiple ReadAloudController instances created by different Chrome activities
    // (like CCT). They are kept here so they can signal each other to release playback.
    private static final HashSet<ReadAloudController> sInstances = new HashSet<>();

    private final Activity mActivity;
    private final ObservableSupplier<Profile> mProfileSupplier;
    private final ObserverList<Runnable> mReadabilityUpdateObserverList = new ObserverList<>();
    // Delay added to readability check that should run it after largest contentful paint for >85%
    // of users http://uma/p/chrome/timeline_v2?sid=c975abf9022aac7b36bf28285f068dd6
    private static final int READABILITY_DELAY = 3000;
    private static final int MAX_URL_ENTRIES = 300;
    private static final LruCache<Integer, ReadabilityInfo> sReadabilityInfoMap =
            new LruCache<>(MAX_URL_ENTRIES);
    private final HashSet<Integer> mPendingRequests = new HashSet<>();
    private final TabModel mTabModel;
    private final TabModel mIncognitoTabModel;
    @Nullable private Player mPlayerCoordinator;
    private final ObservableSupplier<LayoutManager> mLayoutManagerSupplier;
    private final TapToSeekHandler mTapToSeekHandler;

    private TabModelTabObserver mTabObserver;
    private TabModelTabObserver mIncognitoTabObserver;

    private boolean mPausedForIncognito;

    private final BottomSheetController mBottomSheetController;
    private final BottomControlsStacker mBottomControlsStacker;
    private final ActivityLifecycleDispatcher mActivityLifecycleDispatcher;
    private ReadAloudReadabilityHooks mReadabilityHooks;

    @Nullable private static ReadAloudReadabilityHooks sReadabilityHooksForTesting;
    @Nullable private ReadAloudPlaybackHooks mPlaybackHooks;
    @Nullable private static ReadAloudPlaybackHooks sPlaybackHooksForTesting;
    @Nullable private Highlighter mHighlighter;
    @Nullable private Highlighter.Config mHighlighterConfig;
    @Nullable private Extractor mExtractor;

    // Information tied to a playback. When playback is reset it should be set to null together
    //  with the mCurrentlyPlayingTab and mGlobalRenderFrameId
    @Nullable private Playback mPlayback;
    @Nullable private Tab mCurrentlyPlayingTab;
    @Nullable private GlobalRenderFrameHostId mGlobalRenderFrameId;
    // Current tab playback data, or null if there is no playback.
    @Nullable private PlaybackData mCurrentPlaybackData;
    private long mDateModified;

    // Playback for voice previews
    @Nullable private Playback mVoicePreviewPlayback;

    private boolean mOnUserLeaveHint;
    private boolean mRestoringPlayer;
    private boolean mIsDestroyed;
    private boolean mIsScreenOnAndUnlocked = true;

    /**
     * ReadAloud entrypoint defined in readaloud/enums.xml.
     *
     * <p>Do not reorder or remove items, only add new items before NUM_ENTRIES.
     */
    @IntDef({Entrypoint.OVERFLOW_MENU, Entrypoint.MAGIC_TOOLBAR, Entrypoint.RESTORED_PLAYBACK})
    public @interface Entrypoint {
        int OVERFLOW_MENU = 0;
        int MAGIC_TOOLBAR = 1;
        int RESTORED_PLAYBACK = 2;

        // Be sure to also update enums.xml when updating these values.
        int NUM_ENTRIES = 3;
    }

    /** Clock to use so we can mock time in tests. */
    public interface Clock {
        long currentTimeMillis();
    }

    private static Clock sClock = System::currentTimeMillis;
    private static final long HOUR_TO_MS = Duration.ofHours(1).toMillis();

    static void setClockForTesting(Clock clock) {
        var oldValue = sClock;
        sClock = clock;
        ResettersForTesting.register(() -> sClock = oldValue);
    }

    private class ReadabilityInfo {
        private final boolean mIsReadable;
        private final long mResponseTimestamp;
        private final boolean mTimepointsSupported;

        /**
         * Constructor.
         *
         * @param isReadable Is page readable.
         * @param responseTimestamp Timestamp when readability request responded.
         * @param timepointsSupported Whether or not timepoints are supported (needed for
         *     highlighting).
         */
        ReadabilityInfo(boolean isReadable, long responseTimestamp, boolean timepointsSupported) {
            mIsReadable = isReadable;
            mResponseTimestamp = responseTimestamp;
            mTimepointsSupported = timepointsSupported;
        }

        boolean isReadable() {
            return mIsReadable;
        }

        long getResponseTime() {
            return mResponseTimestamp;
        }

        boolean getTimepointsSupported() {
            return mTimepointsSupported;
        }
    }

    // Information about a tab playback necessary for resuming later. Does not
    // include language or voice which should come from current tab state or
    // settings respectively.
    @VisibleForTesting(otherwise = VisibleForTesting.PACKAGE_PRIVATE)
    public class RestoreState implements UserData {
        // Tab to play.
        private final Tab mTab;
        // Paragraph index to resume from.
        private final int mParagraphIndex;
        // Optional - position within the paragraph to resume from.
        private final long mOffsetNanos;
        // True if audio should start playing immediately when this state is restored.
        private final boolean mPlaying;
        // Value of dateModified tag or 0
        private final long mDateModified;
        private final PlaybackData mData;

        /**
         * Constructor.
         *
         * @param tab Tab to play.
         * @param data Current PlaybackData which may be null if playback hasn't started yet.
         */
        RestoreState(Tab tab, @Nullable PlaybackData data, long dateModified) {
            this(
                    tab,
                    data,
                    /* useOffsetInParagraph= */ true,
                    /* shouldPlayOverride= */ null,
                    dateModified);
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
                @Nullable Boolean shouldPlayOverride,
                long dateModified) {
            assert !GURL.isEmptyOrInvalid(tab.getUrl());
            mTab = tab;
            mData = data;
            mDateModified = dateModified;
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

        Tab getTab() {
            return mTab;
        }

        long getDateModified() {
            return mDateModified;
        }

        @Nullable
        PlaybackData getPlaybackData() {
            return mData;
        }

        /** Apply the saved playback state. */
        void restore() {
            if (GURL.isEmptyOrInvalid(mTab.getUrl())) {
                ReadAloudMetrics.recordEmptyURLPlayback(
                        Entrypoint.RESTORED_PLAYBACK, Entrypoint.NUM_ENTRIES);
                assert false;
                return;
            }
            maybeInitializePlaybackHooks();
            createTabPlayback(mTab, mDateModified, Entrypoint.RESTORED_PLAYBACK)
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

    /**
     * Wrapper for TranslationObserver that keeps track of the tab it is observing and the pointer
     * to the underlying native observer so that callers don't need to manage them.
     */
    private static class TranslationObserverImpl implements TranslationObserver {
        private Tab mTab;
        private long mHandle;
        private WebContents mWebContents;

        void observeTab(Tab tab) {
            stopObservingTab(mTab);

            // A tab's WebContents can change and we'll only find out later, so keep track of the
            // WebContents we registered the observer on.
            WebContents webContents = tab.getWebContents();
            if (webContents == null || webContents.isDestroyed()) {
                return;
            }

            mWebContents = webContents;
            mHandle = TranslateBridge.addTranslationObserver(mWebContents, this);
            mTab = tab;
        }

        // If `tab` isn't null, only stop observing if it matches the tab being observed.
        void stopObservingTab(Tab tab) {
            if (tab != null && mTab != tab) {
                return;
            }

            if (mWebContents != null && !mWebContents.isDestroyed() && mHandle != 0L) {
                TranslateBridge.removeTranslationObserver(mWebContents, mHandle);
            }

            mTab = null;
            mHandle = 0L;
            mWebContents = null;
        }
    }

    private final TranslationObserverImpl mPlayingTabTranslationObserver =
            new TranslationObserverImpl() {
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

    private final TranslationObserverImpl mCurrentTabTranslationObserver =
            new TranslationObserverImpl() {
                @Override
                public void onIsPageTranslatedChanged(WebContents webContents) {
                    notifyReadabilityMayHaveChanged();
                }

                @Override
                public void onPageTranslated(
                        String sourceLanguage, String translatedLanguage, int errorCode) {
                    notifyReadabilityMayHaveChanged();
                }
            };

    /**
     * Kicks of readability check on a page load iff: the url is valid, no previous result is
     * available/pending and if a request has to be sent, the necessary conditions are satisfied.
     */
    private ReadAloudReadabilityHooks.ReadabilityCallback mReadabilityCallback =
            new ReadAloudReadabilityHooks.ReadabilityCallback() {
                @Override
                public void onSuccess(String url, boolean isReadable, boolean timepointsSupported) {
                    if (url.isEmpty() || url == null) {
                        assert false;
                        return;
                    }

                    Log.d(TAG, "onSuccess called for %s", url);
                    ReadAloudMetrics.recordIsPageReadable(isReadable);
                    ReadAloudMetrics.recordServerReadabilityResult(isReadable);
                    ReadAloudMetrics.recordIsPageReadabilitySuccessful(true);

                    // If destroy() was already called, stop now. Recording metrics should be okay.
                    if (mIsDestroyed) return;

                    // Register _KnownReadable trial before checking more playback conditions
                    if (isReadable) {
                        ReadAloudFeatures.activateKnownReadableTrial();
                    }

                    // isPlaybackEnabled() should only be checked if isReadable == true.
                    isReadable = isReadable && ReadAloudFeatures.isPlaybackEnabled();
                    int urlHash = urlToHash(url);
                    sReadabilityInfoMap.put(
                            urlHash,
                            new ReadabilityInfo(
                                    isReadable, sClock.currentTimeMillis(), timepointsSupported));
                    mPendingRequests.remove(urlHash);
                    notifyReadabilityMayHaveChanged();
                }

                @Override
                public void onFailure(String url, Throwable t) {
                    Log.d(TAG, "onFailure called for %s because %s", url, t);
                    ReadAloudMetrics.recordIsPageReadabilitySuccessful(false);
                    mPendingRequests.remove(urlToHash(url));
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
            BottomControlsStacker bottomControlsStacker,
            ObservableSupplier<LayoutManager> layoutManagerSupplier,
            ActivityWindowAndroid activityWindowAndroid,
            ActivityLifecycleDispatcher activityLifecycleDispatcher) {
        sInstances.add(this);
        ReadAloudFeatures.init();
        mActivity = activity;
        mProfileSupplier = profileSupplier;
        new OneShotCallback<Profile>(mProfileSupplier, this::onProfileAvailable);
        mTabModel = tabModel;
        mIncognitoTabModel = incognitoTabModel;
        mBottomSheetController = bottomSheetController;
        mCurrentLanguageVoices = new ObservableSupplierImpl<>();
        mSelectedVoiceId = new ObservableSupplierImpl<>();
        mBottomControlsStacker = bottomControlsStacker;
        mLayoutManagerSupplier = layoutManagerSupplier;
        mHighlightingEnabled = new ObservableSupplierImpl<>(false);
        ApplicationStatus.registerApplicationStateListener(this);
        ApplicationStatus.registerStateListenerForActivity(this, mActivity);
        mActivityWindowAndroid = activityWindowAndroid;
        mActivityLifecycleDispatcher = activityLifecycleDispatcher;
        mActivityLifecycleDispatcher.register(this);
        mTapToSeekHandler = new TapToSeekHandler(mTabModel.getCurrentTabSupplier());
    }

    @VisibleForTesting(otherwise = VisibleForTesting.PACKAGE_PRIVATE)
    public void onProfileAvailable(Profile profile) {
        mReadabilityHooks =
                sReadabilityHooksForTesting != null
                        ? sReadabilityHooksForTesting
                        : new ReadAloudReadabilityHooksImpl(mActivity, profile);
        if (mReadabilityHooks.isEnabled()) {
            boolean isAllowed = ReadAloudFeatures.isAllowed(profile);
            ReadAloudMetrics.recordIsUserEligible(isAllowed);
            if (!isAllowed) {
                ReadAloudMetrics.recordIneligibilityReason(
                        ReadAloudFeatures.getIneligibilityReason());
            }
            mHighlightingEnabled.addObserver(
                    ReadAloudController.this::onHighlightingEnabledChanged);
            mHighlightingEnabled.set(ReadAloudPrefs.isHighlightingEnabled(getPrefService()));
            ReadAloudMetrics.recordHighlightingEnabledOnStartup(mHighlightingEnabled.get());
            mTabObserver =
                    new TabModelTabObserver(mTabModel) {
                        @Override
                        public void onLoadStarted(Tab tab, boolean toDifferentDocument) {
                            if (tab != null && toDifferentDocument) {
                                maybeHandleTabReload(tab, tab.getUrl());
                                maybeStopPlayback(tab);
                            }
                        }

                        @Override
                        public void onActivityAttachmentChanged(
                                Tab tab, @Nullable WindowAndroid window) {
                            super.onActivityAttachmentChanged(tab, window);
                            if (mCurrentlyPlayingTab != null
                                    && mCurrentlyPlayingTab.getId() == tab.getId()) {
                                Log.d(TAG, "Saving state");
                                RestoreState state =
                                        new RestoreState(
                                                mCurrentlyPlayingTab,
                                                mCurrentPlaybackData,
                                                mDateModified);
                                tab.getUserDataHost().setUserData(USER_DATA_KEY, state);
                            }
                            maybeStopPlayback(tab);
                            mCurrentTabTranslationObserver.stopObservingTab(tab);
                        }

                        @Override
                        public void didFirstVisuallyNonEmptyPaint(Tab tab) {
                            if (tab != null && !GURL.isEmptyOrInvalid(tab.getUrl())) {
                                PostTask.postDelayedTask(
                                        TaskTraits.UI_DEFAULT,
                                        () -> maybeCheckReadability(tab.getUrl()),
                                        READABILITY_DELAY);
                            }
                        }

                        @Override
                        public void onTabSelected(Tab tab) {
                            mCurrentTabTranslationObserver.stopObservingTab(null);

                            // This method is called when a tab is manually selected by user or
                            // other reason, for example opening a new tab.
                            // For redirects, it will be called multiple times - for the original
                            // url and then the destination url. Because of that we should not use
                            // this method to trigger readability.
                            super.onTabSelected(tab);
                            if (tab != null
                                    && !tab.isDestroyed()
                                    && !GURL.isEmptyOrInvalid(tab.getUrl())) {
                                if (mPausedForIncognito) {
                                    mPausedForIncognito = false;
                                    if (mPlayback != null) {
                                        mPlayerCoordinator.restorePlayers();
                                    }
                                }
                                RestoreState restored =
                                        tab.getUserDataHost().getUserData(USER_DATA_KEY) != null
                                                ? tab.getUserDataHost().getUserData(USER_DATA_KEY)
                                                : null;
                                if (restored != null
                                        && restored.getTab().getUrl().equals(tab.getUrl())) {
                                    mRestoringPlayer = true;
                                    Log.d(
                                            TAG,
                                            "Restore state: swapping tab from the old activity with"
                                                    + " this one");
                                    RestoreState updatedRestored =
                                            new RestoreState(
                                                    tab,
                                                    restored.getPlaybackData(),
                                                    restored.getDateModified());
                                    updatedRestored.restore();
                                    tab.getUserDataHost().removeUserData(USER_DATA_KEY);
                                }
                                maybeAddTranslationObserver(tab);
                            }
                        }

                        @Override
                        public void willCloseTab(Tab tab) {
                            maybeStopPlayback(tab);
                            // Make sure our translation observers are removed before tab's
                            // WebContents is destroyed.
                            removeTranslationObservers(tab);
                        }

                        @Override
                        public void onDestroyed(Tab tab) {
                            // Make sure our translation observers are removed before tab's
                            // WebContents is destroyed.
                            removeTranslationObservers(tab);
                        }

                        @Override
                        public void onContentChanged(Tab tab) {
                            // Required to register the observer on navigation and reload, since it
                            // isn't safe to do in onPageLoadStarted().
                            mCurrentTabTranslationObserver.stopObservingTab(tab);
                            maybeAddTranslationObserver(tab);

                            if (tab == mCurrentlyPlayingTab) {
                                mPlayingTabTranslationObserver.stopObservingTab(tab);
                            }
                        }

                        @Override
                        public void webContentsWillSwap(Tab tab) {
                            // When restoring a tab from Recent Tabs, the tab's native WebContents
                            // is destroyed and replaced by a different one. We must remove the old
                            // WebContents' translation observers before it is destroyed.
                            removeTranslationObservers(tab);
                        }

                        private void maybeAddTranslationObserver(Tab tab) {
                            if (isURLReadAloudSupported(tab.getUrl())) {
                                mCurrentTabTranslationObserver.observeTab(tab);
                            }
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

            InsetObserver insetObserver = mActivityWindowAndroid.getInsetObserver();
            if (insetObserver != null) {
                insetObserver.addObserver(this);
            }
        }
    }

    @VisibleForTesting(otherwise = VisibleForTesting.PACKAGE_PRIVATE)
    public void maybeCheckReadability(GURL url) {
        if (!isAvailable()) {
            return;
        }
        if (mReadabilityHooks == null) {
            return;
        }
        if (mProfileSupplier.get() == null || !mProfileSupplier.get().isNativeInitialized()) {
            return;
        }
        if (!isURLReadAloudSupported(url)) {
            ReadAloudMetrics.recordIsPageReadable(false);
            return;
        }
        String urlSpec = stripUserData(url).getSpec();
        int urlSpecHash = urlToHash(urlSpec);
        if (mPendingRequests.contains(urlSpecHash)) {
            return;
        }
        ReadabilityInfo info = getReadabilityInfoIfUnexpired(urlSpecHash);
        if (info != null) {
            ReadAloudMetrics.recordIsPageReadable(info.isReadable());
            return;
        }
        mPendingRequests.add(urlSpecHash);
        mReadabilityHooks.isPageReadable(urlSpec, mReadabilityCallback);
    }

    private ReadabilityInfo getReadabilityInfoIfUnexpired(int sanitizedUrlHash) {
        ReadabilityInfo info = sReadabilityInfoMap.get(sanitizedUrlHash);
        if (info != null) {
            Long retrievalDate = info.getResponseTime();
            if (retrievalDate != null && sClock.currentTimeMillis() - retrievalDate <= HOUR_TO_MS) {
                return info;
            }
            sReadabilityInfoMap.remove(sanitizedUrlHash);
            notifyReadabilityMayHaveChanged();
        }
        return null;
    }

    /**
     * Checks if URL is supported by Read Aloud before sending a readability request. Read Aloud
     * won't be supported on the following URLs:
     *
     * <ul>
     *   <li>pages without an HTTP(S) scheme
     *   <li>myaccount.google.com and myactivity.google.com
     *   <li>www.google.com/...
     *   <li>Based on standards.google/processes/domains/domain-guidelines-by-use-case,
     *       www.google.com/... is reserved for Search features and content.
     * </ul>
     */
    public boolean isURLReadAloudSupported(GURL url) {
        return !GURL.isEmptyOrInvalid(url)
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
        // If we don't have a valid Profile, playback won't work.
        // TODO(crbug.com/41491180): Remove when valid profile is guaranteed.
        if (tab == null
                || GURL.isEmptyOrInvalid(tab.getUrl())
                || tab.getWebContents() == null
                || mProfileSupplier.get() == null
                || !mProfileSupplier.get().isNativeInitialized()) {
            return false;
        }

        if (isTabLanguageSupported(tab) && isAvailable()) {
            int sanitizedUrlHash = urlToHash(stripUserData(tab.getUrl()).getSpec());
            ReadabilityInfo info = getReadabilityInfoIfUnexpired(sanitizedUrlHash);
            if (info != null) {
                return info.isReadable();
            }
        }
        return false;
    }

    /**
     * Add a runnable to be called when new readability information is available for any page.
     * Listeners can then call isReadable() to check a tab's readability.
     *
     * @param runnable Runnable called when a readability check succeeds or when a page is
     *     translated.
     */
    public void addReadabilityUpdateListener(Runnable runnable) {
        mReadabilityUpdateObserverList.addObserver(runnable);
    }

    /**
     * Remove a runnable previously registered with addReadabilityUpdateListener. No effect if
     * runnable was not added.
     *
     * @param runnable Runnable to remove.
     */
    public void removeReadabilityUpdateListener(Runnable runnable) {
        mReadabilityUpdateObserverList.removeObserver(runnable);
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
     * Returns true if playback is being restored for a previously playing tab. True from
     * onTabSelected() until the mini player is fully shown.
     */
    public boolean isRestoringPlayer() {
        return mRestoringPlayer;
    }

    /**
     * Play the tab, creating and showing the player if it isn't already showing. No effect if tab's
     * URL is the same as the URL that is already playing.
     *
     * @param tab Tab to play.
     */
    public void playTab(Tab tab, @Entrypoint int entrypoint) {
        if (!isReadable(tab)) {
            ReadAloudMetrics.recordPlaybackWithoutReadabilityCheck(
                    entrypoint, Entrypoint.NUM_ENTRIES);
            if (GURL.isEmptyOrInvalid(tab.getUrl())) {
                ReadAloudMetrics.recordEmptyURLPlayback(entrypoint, Entrypoint.NUM_ENTRIES);
            }
        }
        // Should rarely ever happen since the profile has to be established for a readability check
        // to show the entrypoint.
        if (mProfileSupplier.get() == null) {
            return;
        }

        // There may be a saved playback state if another ReadAloudController instance interrupted
        // this one. Restore it if the entrypoint is showing on the same tab, otherwise clear state.
        if (mStateToRestoreOnBringingToForeground != null) {
            if (mStateToRestoreOnBringingToForeground.getTab() == tab) {
                restoreStateOnForeground();
                return;
            } else {
                mStateToRestoreOnBringingToForeground = null;
            }
        }

        extractDateModified(tab)
                .then(
                        timestamp -> {
                            ReadAloudMetrics.recordHasDateModified(true);
                            playTabWithDateModified(tab, timestamp, entrypoint);
                        },
                        exception -> {
                            ReadAloudMetrics.recordHasDateModified(false);
                            playTabWithDateModified(tab, 0L, entrypoint);
                        });
    }

    private void playTabWithDateModified(Tab tab, long dateModified, @Entrypoint int entrypoint) {
        createTabPlayback(tab, dateModified, entrypoint)
                .then(
                        playback -> {
                            mDateModified = dateModified;
                            mPlayerCoordinator.playbackReady(playback, PLAYING);
                            playback.play();
                            ReadAloudMetrics.recordPlaybackStarted();
                        },
                        exception -> {
                            Log.d(TAG, "playTab failed: %s", exception.getMessage());
                        });
    }

    private Promise<Long> extractDateModified(Tab tab) {
        assert !GURL.isEmptyOrInvalid(tab.getUrl());
        maybeInitializePlaybackHooks();
        if (mExtractor == null) {
            mExtractor = mPlaybackHooks.createExtractor();
        }
        return mExtractor.getDateModified(tab);
    }

    private void maybeInitializePlaybackHooks() {
        if (mPlaybackHooks == null) {
            mPlaybackHooks =
                    sPlaybackHooksForTesting != null
                            ? sPlaybackHooksForTesting
                            : ReadAloudPlaybackHooksProvider.getForProfile(mProfileSupplier.get());
            mPlayerCoordinator = mPlaybackHooks.createPlayer(/* delegate= */ this);
            mPlayerCoordinator.addObserver(this);
        }
    }

    private Promise<Playback> createTabPlayback(
            Tab tab, long dateModified, @Entrypoint int entrypoint) {
        assert !GURL.isEmptyOrInvalid(tab.getUrl());
        // only start a new playback if different URL or no active playback for that url
        if (mCurrentlyPlayingTab != null && tab.getUrl().equals(mCurrentlyPlayingTab.getUrl())) {
            var promise = new Promise<Playback>();
            promise.reject(new Exception("Tab already playing"));
            return promise;
        }


        // If there is a background playback from another instance, stop it.
        stopExternalBackgroundPlayback();
        // Stop ongoing playback in this activity.
        resetCurrentPlayback();

        mCurrentlyPlayingTab = tab;
        mPlayingTabTranslationObserver.observeTab(mCurrentlyPlayingTab);

        if (!mPlaybackHooks.voicesInitialized()) {
            mPlaybackHooks.initVoices();
        }

        // Notify player UI that playback is happening soon and show UI in case there's an error
        // coming.
        mPlayerCoordinator.playTabRequested();

        final String playbackLanguage = getLanguageForNewPlayback(tab);
        boolean isTranslated = isTranslated(tab);
        var voices = mPlaybackHooks.getVoicesFor(playbackLanguage);
        // TODO: Don't show entrypoints for unsupported languages
        if (voices == null || voices.isEmpty()) {
            onCreatePlaybackFailed(entrypoint);
            var promise = new Promise<Playback>();
            promise.reject(new Exception("Unsupported language"));
            return promise;
        }

        final String sanitizedUrl = stripUserData(tab.getUrl()).getSpec();
        final int sanitizedUrlHash = urlToHash(sanitizedUrl);
        PlaybackArgs args =
                new PlaybackArgs(
                        sanitizedUrl,
                        isTranslated ? playbackLanguage : null,
                        mPlaybackHooks.getPlaybackVoiceList(
                                ReadAloudPrefs.getVoices(getPrefService())),
                        /* dateModifiedMsSinceEpoch= */ dateModified);
        Log.d(TAG, "Creating playback with args: %s", args);

        Promise<Playback> promise = createPlayback(args);
        promise.then(
                playback -> {
                    ReadAloudMetrics.recordIsTabPlaybackCreationSuccessful(true);
                    ReadAloudMetrics.recordTabCreationSuccess(entrypoint, Entrypoint.NUM_ENTRIES);
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
                    if (exception instanceof ReadAloudUnsupportedException) {
                        Log.e(TAG, "Attempting to play a non readable website");
                        sReadabilityInfoMap.put(
                                sanitizedUrlHash,
                                new ReadabilityInfo(false, sClock.currentTimeMillis(), false));
                        notifyReadabilityMayHaveChanged();
                    }

                    onCreatePlaybackFailed(entrypoint);
                });
        return promise;
    }

    private void onCreatePlaybackFailed(@Entrypoint int entrypoint) {
        ReadAloudMetrics.recordIsTabPlaybackCreationSuccessful(false);
        ReadAloudMetrics.recordTabCreationFailure(entrypoint, Entrypoint.NUM_ENTRIES);
        mPlayerCoordinator.playbackFailed();
    }

    /**
     * Whether or not timepoints are supported for the tab's content. Timepoints are needed for word
     * highlighting.
     */
    public boolean timepointsSupported(Tab tab) {
        if (isAvailable() && !GURL.isEmptyOrInvalid(tab.getUrl())) {
            int urlHash = urlToHash(stripUserData(tab.getUrl()).getSpec());
            if (sReadabilityInfoMap.get(urlHash) == null) {
                return false;
            }
            return sReadabilityInfoMap.get(urlHash).getTimepointsSupported();
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
        mPlayingTabTranslationObserver.stopObservingTab(null);
        mCurrentlyPlayingTab = null;
        mGlobalRenderFrameId = null;
        mCurrentPlaybackData = null;
        mPausedForIncognito = false;
        mDateModified = 0L;
    }

    /** Cleanup: unregister listeners. */
    public void destroy() {
        sInstances.remove(this);
        mIsDestroyed = true;
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

        removeTranslationObservers(null);

        mHighlightingEnabled.removeObserver(ReadAloudController.this::onHighlightingEnabledChanged);
        ApplicationStatus.unregisterApplicationStateListener(this);
        ApplicationStatus.unregisterActivityStateListener(this);
        resetCurrentPlayback();
        mStateToRestoreOnBringingToForeground = null;
        ReadAloudFeatures.shutdown();
        InsetObserver insetObserver = mActivityWindowAndroid.getInsetObserver();
        if (insetObserver != null) {
            insetObserver.removeObserver(this);
        }
        mActivityLifecycleDispatcher.unregister(this);
        mRestoringPlayer = false;
        mReadabilityUpdateObserverList.clear();
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
        assert tab.getUrl().getSpec().equals(newUrl.getSpec());
        if (mHighlighter != null && !GURL.isEmptyOrInvalid(tab.getUrl())) {
            mHighlighter.handleTabReloaded(tab);
        }
    }

    private GURL stripUserData(GURL in) {
        if (GURL.isEmptyOrInvalid(in)
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
        WebContents webContents = tab.getWebContents();
        String language =
                webContents == null ? null : TranslateBridge.getCurrentLanguage(webContents);
        if (language == null || language.isEmpty() || language.equals("und")) {
            language = AppLocaleUtils.getAppLanguagePref();
        }

        if (language == null) {
            language = "en";
        }

        // If language string is a locale like "en-US", strip the "-US" part.
        return getLanguage(language);
    }

    /** A utility function doing null checks. */
    boolean isTranslated(Tab tab) {
        return tab.getWebContents() == null
                ? false
                : TranslateBridge.isPageTranslated(tab.getWebContents());
    }

    /** If language string includes locale, strip it */
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
        return timepointsSupported(mCurrentlyPlayingTab) && !isTranslated(mCurrentlyPlayingTab);
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
                && mode != mHighlighterConfig.getMode()
                && mPlayback != null) {
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
            assert !GURL.isEmptyOrInvalid(mCurrentlyPlayingTab.getUrl());
            RestoreState state =
                    new RestoreState(mCurrentlyPlayingTab, mCurrentPlaybackData, mDateModified);
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
                    new RestoreState(mCurrentlyPlayingTab, mCurrentPlaybackData, mDateModified);
            resetCurrentPlayback();
        }

        if (mVoicePreviewPlayback != null) {
            destroyVoicePreview();
        }

        // If there is a background playback from another instance, stop it. (The voice selection UI
        // should not be visible in this case, so it should not be possible to preview a voice, but
        // this ensures the preview still works anyway.)
        stopExternalBackgroundPlayback();

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
        if (mProfileSupplier.get() == null || !mProfileSupplier.get().isNativeInitialized()) {
            promise.reject(new Exception("missing profile"));
            return promise;
        }
        mPlaybackHooks.createPlayback(
                args,
                new ReadAloudPlaybackHooks.CreatePlaybackCallback() {
                    @Override
                    public void onSuccess(Playback playback) {
                        if (playback == null) {
                            promise.reject(new Exception("Playback is null"));
                        }
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
                        if (throwable instanceof Exception) {
                            promise.reject((Exception) throwable);
                        } else {
                            promise.reject(new Exception(throwable));
                        }
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
    public BottomControlsStacker getBottomControlsStacker() {
        return mBottomControlsStacker;
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

    @Override
    public void onMiniPlayerShown() {
        mRestoringPlayer = false;
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
        boolean isScreenOnAndUnlocked =
                DeviceConditions.isCurrentlyScreenOnAndUnlocked(mActivity.getApplicationContext());
        if (ReadAloudFeatures.isBackgroundPlaybackEnabled() && mPlayerCoordinator != null) {
            if (mIsScreenOnAndUnlocked != isScreenOnAndUnlocked) {
                mPlayerCoordinator.onScreenStatusChanged(/* isLocked= */ !isScreenOnAndUnlocked);
                mIsScreenOnAndUnlocked = isScreenOnAndUnlocked;
            }
            // Do nothing Chrome doesn't have to be in foreground to keep playback active.
            return;
        }

        // If background playback is disabled, stop any playback if user left Chrome while screen is
        // on and unlocked
        if (newState == ApplicationState.HAS_STOPPED_ACTIVITIES
                && (isScreenOnAndUnlocked || mOnUserLeaveHint)) {
            saveStateToRestoreOnForeground();
            resetCurrentPlayback();
            mOnUserLeaveHint = false;
        }

        if (mPlayerCoordinator != null) {
            if (newState == ApplicationState.HAS_STOPPED_ACTIVITIES && !isScreenOnAndUnlocked) {
                mPlayerCoordinator.onScreenStatusChanged(/* isLocked= */ true);
            } else if (newState == ApplicationState.HAS_RUNNING_ACTIVITIES
                    && isScreenOnAndUnlocked) {
                mPlayerCoordinator.onScreenStatusChanged(/* isLocked= */ false);
            }
        }
    }

    @Override
    public void onActivityStateChange(Activity activity, @ActivityState int newState) {
        // Restore saved playback if resuming after app was backgrounded.
        if (newState == ActivityState.RESUMED
                && mStateToRestoreOnBringingToForeground != null
                && mProfileSupplier.get() != null) {

            boolean otherInstanceHasPlayback = false;
            for (ReadAloudController controller : sInstances) {
                if (controller != this && controller.mPlayback != null) {
                    otherInstanceHasPlayback = true;
                    break;
                }
            }

            // Only resume if another instance doesn't already have a playback.
            if (!otherInstanceHasPlayback) {
                restoreStateOnForeground();
                mOnUserLeaveHint = false;
            }
            // TODO(b/352563278): If another instance is playing, show the player in the "paused"
            // state and then restore if the play button is clicked.
        }
    }

    // OnUserLeaveHintObserver
    @Override
    public void onUserLeaveHint() {
        Log.d(TAG, "on user leave hint");
        mOnUserLeaveHint = true;
    }

    /** if the current focused tab has an active playback */
    public boolean isPlayingCurrentTab() {
        return mPlayback != null
                && mCurrentlyPlayingTab != null
                && mCurrentlyPlayingTab == mTabModel.getCurrentTabSupplier().get();
    }

    /**
     * Triggered with ContextualSearch's onSelectionChange. Sends the selected webpage content and
     * playback to TapToSeekHandler to find the selected word in the playback and seek to it.
     *
     * @param content Selected word and surrounding content
     * @param beginOffset index of where the selected word starts within the content
     * @param endOffset index of where the selected word ends within the content
     */
    public void tapToSeek(String content, int beginOffset, int endOffset) {
        if (ReadAloudFeatures.isTapToSeekEnabled() && isPlayingCurrentTab()) {
            long timeWhenTapToSeekRequested = sClock.currentTimeMillis();
            mTapToSeekHandler.tapToSeek(
                    content, beginOffset, endOffset, mPlayback, mCurrentlyPlayingTab);
            ReadAloudMetrics.recordTapToSeekTime(
                    sClock.currentTimeMillis() - timeWhenTapToSeekRequested);
        }
    }

    private void notifyReadabilityMayHaveChanged() {
        for (Runnable observer : mReadabilityUpdateObserverList) {
            observer.run();
        }
    }

    private void removeTranslationObservers(Tab tab) {
        mPlayingTabTranslationObserver.stopObservingTab(tab);
        mCurrentTabTranslationObserver.stopObservingTab(tab);
    }

    private void saveStateToRestoreOnForeground() {
        if (mCurrentlyPlayingTab == null) return;

        mStateToRestoreOnBringingToForeground =
                new RestoreState(
                        mCurrentlyPlayingTab,
                        mCurrentPlaybackData,
                        /* useOffsetInParagraph= */ true,
                        /* shouldPlayOverride= */ false,
                        mDateModified);
    }

    private void restoreStateOnForeground() {
        mStateToRestoreOnBringingToForeground.restore();
        mStateToRestoreOnBringingToForeground = null;
    }

    private void stopExternalBackgroundPlayback() {
        for (ReadAloudController controller : sInstances) {
            if (controller != this && controller.mPlayback != null) {
                controller.saveStateToRestoreOnForeground();
                controller.maybeStopPlayback(null);
            }
        }
    }

    // Tests.
    public void setHighlighterForTests(Highlighter highighter) {
        mHighlighter = highighter;
    }

    public void setTimepointsSupportedForTest(String url, boolean supported) {
        sReadabilityInfoMap.put(urlToHash(url), new ReadabilityInfo(true, 0L, supported));
    }

    public void setStateToRestoreOnBringingToForegroundForTests(RestoreState restoreState) {
        mStateToRestoreOnBringingToForeground = restoreState;
    }

    public TabModelTabObserver getTabModelTabObserverforTests() {
        return mTabObserver;
    }

    public TabModelTabObserver getIncognitoTabModelTabObserverforTests() {
        return mIncognitoTabObserver;
    }

    public TranslationObserver getTranslationObserverForTest() {
        return mPlayingTabTranslationObserver;
    }

    public TranslationObserver getCurrentTabTranslationObserverForTest() {
        return mCurrentTabTranslationObserver;
    }

    private int urlToHash(String url) {
        return Hashing.murmur3_32_fixed().hashUnencodedChars(url).asInt();
    }

    static void resetReadabilityCacheForTesting() {
        sReadabilityInfoMap.evictAll();
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
