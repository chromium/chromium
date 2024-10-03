// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.readaloud;

import static androidx.test.espresso.matcher.ViewMatchers.assertThat;

import static org.hamcrest.Matchers.hasItems;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.any;
import static org.mockito.Mockito.anyBoolean;
import static org.mockito.Mockito.anyLong;
import static org.mockito.Mockito.anyString;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.eq;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.reset;
import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoInteractions;
import static org.mockito.Mockito.when;
import static org.robolectric.Shadows.shadowOf;

import android.app.Activity;
import android.content.Intent;
import android.view.WindowManager;

import androidx.appcompat.app.AppCompatActivity;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.MockitoAnnotations;
import org.robolectric.Robolectric;
import org.robolectric.annotation.Config;
import org.robolectric.shadows.ShadowLooper;

import org.chromium.base.ActivityState;
import org.chromium.base.ApplicationState;
import org.chromium.base.Promise;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.supplier.OneshotSupplierImpl;
import org.chromium.base.task.TaskTraits;
import org.chromium.base.task.test.ShadowPostTask;
import org.chromium.base.task.test.ShadowPostTask.TestImpl;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.base.test.util.JniMocker;
import org.chromium.base.test.util.UserActionTester;
import org.chromium.chrome.browser.browser_controls.BottomControlsStacker;
import org.chromium.chrome.browser.device.DeviceConditions;
import org.chromium.chrome.browser.device.ShadowDeviceConditions;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.fullscreen.FullscreenManager;
import org.chromium.chrome.browser.fullscreen.FullscreenOptions;
import org.chromium.chrome.browser.layouts.LayoutManager;
import org.chromium.chrome.browser.layouts.LayoutStateProvider;
import org.chromium.chrome.browser.layouts.LayoutStateProvider.LayoutStateObserver;
import org.chromium.chrome.browser.layouts.LayoutType;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.price_tracking.PriceTrackingFeatures;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.readaloud.ReadAloudMetrics.IneligibilityReason;
import org.chromium.chrome.browser.readaloud.exceptions.ReadAloudUnsupportedException;
import org.chromium.chrome.browser.search_engines.SearchEngineType;
import org.chromium.chrome.browser.search_engines.TemplateUrlServiceFactory;
import org.chromium.chrome.browser.signin.services.UnifiedConsentServiceBridge;
import org.chromium.chrome.browser.tab.MockTab;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabSelectionType;
import org.chromium.chrome.browser.tab.TabTestUtils;
import org.chromium.chrome.browser.tabmodel.TabModelUtils;
import org.chromium.chrome.browser.translate.FakeTranslateBridgeJni;
import org.chromium.chrome.browser.translate.TranslateBridgeJni;
import org.chromium.chrome.browser.ui.native_page.NativePage;
import org.chromium.chrome.modules.readaloud.Playback;
import org.chromium.chrome.modules.readaloud.Playback.PlaybackTextPart;
import org.chromium.chrome.modules.readaloud.Playback.PlaybackTextType;
import org.chromium.chrome.modules.readaloud.PlaybackArgs;
import org.chromium.chrome.modules.readaloud.PlaybackArgs.PlaybackVoice;
import org.chromium.chrome.modules.readaloud.PlaybackListener;
import org.chromium.chrome.modules.readaloud.PlaybackListener.PlaybackData;
import org.chromium.chrome.modules.readaloud.Player;
import org.chromium.chrome.modules.readaloud.ReadAloudPlaybackHooks;
import org.chromium.chrome.modules.readaloud.contentjs.Extractor;
import org.chromium.chrome.modules.readaloud.contentjs.Highlighter;
import org.chromium.chrome.test.util.browser.tabmodel.MockTabModelSelector;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.prefs.PrefService;
import org.chromium.components.search_engines.TemplateUrl;
import org.chromium.components.search_engines.TemplateUrlService;
import org.chromium.components.user_prefs.UserPrefsJni;
import org.chromium.content_public.browser.GlobalRenderFrameHostId;
import org.chromium.content_public.browser.RenderFrameHost;
import org.chromium.content_public.browser.SelectionClient;
import org.chromium.content_public.browser.SelectionPopupController;
import org.chromium.content_public.browser.WebContents;
import org.chromium.net.ConnectionType;
import org.chromium.ui.base.ActivityWindowAndroid;
import org.chromium.url.GURL;
import org.chromium.url.JUnitTestGURLs;

import java.util.Arrays;
import java.util.HashSet;
import java.util.List;
import java.util.Locale;

/** Unit tests for {@link ReadAloudController}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(
        manifest = Config.NONE,
        shadows = {ShadowDeviceConditions.class, ShadowPostTask.class})
@EnableFeatures({ChromeFeatureList.READALOUD, ChromeFeatureList.READALOUD_PLAYBACK})
@DisableFeatures({
    ChromeFeatureList.READALOUD_IN_MULTI_WINDOW,
    ChromeFeatureList.READALOUD_BACKGROUND_PLAYBACK,
    ChromeFeatureList.READALOUD_TAP_TO_SEEK
})
public class ReadAloudControllerUnitTest {
    private static final GURL sTestGURL = JUnitTestGURLs.EXAMPLE_URL;
    private static final GURL sTestRedirectGURL = JUnitTestGURLs.URL_1_WITH_PATH;
    private static final long KNOWN_READABLE_TRIAL_PTR = 12345678L;
    private static final Locale EN_US = new Locale("en", "US");
    private static final Locale FR_FR = new Locale("fr", "FR");

    private MockTab mTab;
    private ReadAloudController mController;
    private ReadAloudController mController2;
    private Activity mActivity;
    private Locale mDefaultLocale;

    @Rule public JniMocker mJniMocker = new JniMocker();

    private FakeTranslateBridgeJni mFakeTranslateBridge;
    private ObservableSupplierImpl<Profile> mProfileSupplier;
    private ObservableSupplierImpl<LayoutManager> mLayoutManagerSupplier;
    @Mock private Profile mMockProfile;
    @Mock private Profile mMockIncognitoProfile;
    @Mock private ReadAloudReadabilityHooks mHooksImpl;
    @Mock private ReadAloudPlaybackHooks mPlaybackHooks;
    @Mock private Player mPlayerCoordinator;
    @Mock private BottomSheetController mBottomSheetController;
    @Mock private Extractor mExtractor;
    @Mock private Highlighter mHighlighter;
    @Mock private PlaybackListener.PhraseTiming mPhraseTiming;
    @Mock private BottomControlsStacker mBottomControlsStacker;
    @Mock private LayoutManager mLayoutManager;
    @Mock private ReadAloudPrefs.Natives mReadAloudPrefsNatives;
    @Mock private ReadAloudFeatures.Natives mReadAloudFeaturesNatives;
    @Mock private UserPrefsJni mUserPrefsNatives;
    @Mock private PrefService mPrefService;
    @Mock private TemplateUrlService mTemplateUrlService;
    @Mock private ActivityWindowAndroid mActivityWindowAndroid;
    @Mock private ActivityLifecycleDispatcher mActivityLifecycleDispatcher;
    MockTabModelSelector mTabModelSelector;

    @Captor ArgumentCaptor<ReadAloudReadabilityHooks.ReadabilityCallback> mCallbackCaptor;
    @Captor ArgumentCaptor<ReadAloudPlaybackHooks.CreatePlaybackCallback> mPlaybackCallbackCaptor;
    @Captor ArgumentCaptor<PlaybackArgs> mPlaybackArgsCaptor;
    @Captor ArgumentCaptor<PlaybackListener> mPlaybackListenerCaptor;
    @Captor ArgumentCaptor<LayoutStateObserver> mLayoutStateObserver;
    @Captor ArgumentCaptor<FullscreenManager.Observer> mFullscreenObserver;

    @Mock private Playback mPlayback;
    @Mock private Playback.Metadata mMetadata;
    @Mock private WebContents mWebContents;
    @Mock private RenderFrameHost mRenderFrameHost;
    @Mock private TemplateUrl mSearchEngine;
    @Mock private SelectionClient mSelectionClient;
    @Mock private SelectionPopupController mSelectionPopupController;
    @Mock private NativePage mNativePage;
    @Mock private LayoutStateProvider mLayoutStateProvider;
    @Mock private FullscreenManager mFullscreenManager;
    private GlobalRenderFrameHostId mGlobalRenderFrameHostId = new GlobalRenderFrameHostId(1, 1);
    public UserActionTester mUserActionTester;
    private HistogramWatcher mHighlightingEnabledOnStartupHistogram;
    private Promise<Long> mExtractorPromise;
    OneshotSupplierImpl<LayoutStateProvider> mLayoutStateProviderSupplier =
            new OneshotSupplierImpl<>();

    private FakeClock mClock;

    /** FakeClock for setting the time. */
    static class FakeClock implements ReadAloudController.Clock {
        private long mCurrentTimeMillis;

        FakeClock() {
            mCurrentTimeMillis = 0;
        }

        @Override
        public long currentTimeMillis() {
            return mCurrentTimeMillis;
        }

        void advanceCurrentTimeMillis(long millis) {
            mCurrentTimeMillis += millis;
        }
    }

    @Before
    public void setUp() {
        mDefaultLocale = Locale.getDefault();

        MockitoAnnotations.initMocks(this);
        ShadowPostTask.setTestImpl(
                new TestImpl() {
                    @Override
                    public void postDelayedTask(
                            @TaskTraits int taskTraits, Runnable task, long delay) {
                        task.run();
                    }
                });
        mLayoutStateProviderSupplier.set(mLayoutStateProvider);
        mProfileSupplier = new ObservableSupplierImpl<>();
        mProfileSupplier.set(mMockProfile);
        doReturn(true).when(mMockProfile).isNativeInitialized();

        mLayoutManagerSupplier = new ObservableSupplierImpl<>();
        mLayoutManagerSupplier.set(mLayoutManager);
        mActivity = Robolectric.buildActivity(AppCompatActivity.class).setup().get();
        mActivity.setTheme(R.style.Theme_BrowserUI_DayNight);

        when(mMockProfile.isOffTheRecord()).thenReturn(false);
        when(mMockIncognitoProfile.isOffTheRecord()).thenReturn(true);
        UnifiedConsentServiceBridge.setUrlKeyedAnonymizedDataCollectionEnabled(true);

        PriceTrackingFeatures.setPriceTrackingEnabledForTesting(false);

        mFakeTranslateBridge = new FakeTranslateBridgeJni();
        mJniMocker.mock(TranslateBridgeJni.TEST_HOOKS, mFakeTranslateBridge);
        mJniMocker.mock(ReadAloudPrefsJni.TEST_HOOKS, mReadAloudPrefsNatives);
        mJniMocker.mock(ReadAloudFeaturesJni.TEST_HOOKS, mReadAloudFeaturesNatives);
        mJniMocker.mock(UserPrefsJni.TEST_HOOKS, mUserPrefsNatives);
        doReturn(mPrefService).when(mUserPrefsNatives).get(any());
        when(mPrefService.getBoolean(Pref.LISTEN_TO_THIS_PAGE_ENABLED)).thenReturn(true);
        mTabModelSelector =
                new MockTabModelSelector(
                        mMockProfile,
                        mMockIncognitoProfile,
                        /* tabCount= */ 2,
                        /* incognitoTabCount= */ 1,
                        (id, incognito) -> {
                            Profile profile = incognito ? mMockIncognitoProfile : mMockProfile;
                            MockTab tab = spy(MockTab.createAndInitialize(id, profile));
                            return tab;
                        });
        when(mHooksImpl.isEnabled()).thenReturn(true);
        when(mHooksImpl.getCompatibleLanguages())
                .thenReturn(new HashSet<String>(Arrays.asList("en", "es", "fr", "ja")));
        initPlaybackHooks();
        ReadAloudController.setReadabilityHooks(mHooksImpl);
        ReadAloudController.setPlaybackHooks(mPlaybackHooks);

        TemplateUrlServiceFactory.setInstanceForTesting(mTemplateUrlService);
        doReturn(SearchEngineType.SEARCH_ENGINE_GOOGLE)
                .when(mTemplateUrlService)
                .getSearchEngineTypeFromTemplateUrl(anyString());
        doReturn("Google").when(mSearchEngine).getKeyword();
        doReturn(mSearchEngine).when(mTemplateUrlService).getDefaultSearchEngineTemplateUrl();
        doReturn(KNOWN_READABLE_TRIAL_PTR)
                .when(mReadAloudFeaturesNatives)
                .initSyntheticTrial(eq(ChromeFeatureList.READALOUD), eq("_KnownReadable"));

        mHighlightingEnabledOnStartupHistogram =
                HistogramWatcher.newSingleRecordWatcher(
                        "ReadAloud.HighlightingEnabled.OnStartup", true);

        mClock = new FakeClock();
        ReadAloudController.setClockForTesting(mClock);

        doReturn(false).when(mWebContents).isDestroyed();
        mTab = mTabModelSelector.getCurrentTab();
        mTab.setGurlOverrideForTesting(sTestGURL);
        mTab.setWebContentsOverrideForTesting(mWebContents);

        TapToSeekSelectionManager.setSmartSelectionClient(mSelectionClient);
        TapToSeekSelectionManager.setSelectionPopupController(mSelectionPopupController);

        mController = createController();
        verify(mLayoutStateProvider).addObserver(mLayoutStateObserver.capture());
        verify(mFullscreenManager).addObserver(mFullscreenObserver.capture());
        when(mMetadata.languageCode()).thenReturn("en");
        when(mPlayback.getMetadata()).thenReturn(mMetadata);
        when(mWebContents.getMainFrame()).thenReturn(mRenderFrameHost);
        when(mRenderFrameHost.getGlobalRenderFrameHostId()).thenReturn(mGlobalRenderFrameHostId);
        mController.setHighlighterForTests(mHighlighter);
        mUserActionTester = new UserActionTester();
        mExtractorPromise = new Promise<Long>();
        when(mExtractor.getDateModified(any())).thenReturn(mExtractorPromise);
        mExtractorPromise.fulfill(1234567123456L);
    }

    void initPlaybackHooks() {
        when(mPlaybackHooks.createPlayer(any())).thenReturn(mPlayerCoordinator);
        when(mPlaybackHooks.createExtractor()).thenReturn(mExtractor);
        doReturn(false).when(mPlaybackHooks).voicesInitialized();
        doReturn(List.of(new PlaybackVoice("en", "voiceA", "")))
                .when(mPlaybackHooks)
                .getVoicesFor(anyString());
    }

    private void resetPlaybackMocks() {
        reset(mPlayback);
        when(mPlayback.getMetadata()).thenReturn(mMetadata);
        reset(mPlaybackHooks);
        reset(mPlayerCoordinator);
        initPlaybackHooks();
    }

    private ReadAloudController createController() {
        var controller =
                new ReadAloudController(
                        mActivity,
                        mProfileSupplier,
                        mTabModelSelector.getModel(false),
                        mTabModelSelector.getModel(true),
                        mBottomSheetController,
                        mBottomControlsStacker,
                        mLayoutManagerSupplier,
                        mActivityWindowAndroid,
                        mActivityLifecycleDispatcher,
                        mLayoutStateProviderSupplier,
                        mFullscreenManager);
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();
        return controller;
    }

    @After
    public void tearDown() {
        Locale.setDefault(mDefaultLocale);
        mUserActionTester.tearDown();
        ReadAloudFeatures.shutdown();
        mController.destroy();
        if (mController2 != null) {
            mController2.destroy();
        }
        ReadAloudController.resetReadabilityCacheForTesting();
    }

    @Test
    public void testHideShowPlayer_tabSwitcher() {
        requestAndStartPlayback();
        mLayoutStateObserver.getValue().onStartedShowing(LayoutType.TAB_SWITCHER);
        verify(mPlayerCoordinator).hidePlayers();

        mLayoutStateObserver.getValue().onFinishedHiding(LayoutType.TAB_SWITCHER);
        verify(mPlayerCoordinator).restorePlayers();
    }

    @Test
    public void testDontHidePlayerWithNoPlayback_tabSwitcherUI() {
        mLayoutStateObserver.getValue().onStartedShowing(LayoutType.TAB_SWITCHER);
        verify(mPlayerCoordinator, never()).hidePlayers();

        mLayoutStateObserver.getValue().onFinishedHiding(LayoutType.TAB_SWITCHER);
        verify(mPlayerCoordinator, never()).restorePlayers();
    }

    @Test
    public void testDontHidePlayer_nonTabSwitcherUI() {
        requestAndStartPlayback();
        mLayoutStateObserver.getValue().onStartedShowing(LayoutType.START_SURFACE);
        verify(mPlayerCoordinator, never()).hidePlayers();

        mLayoutStateObserver.getValue().onFinishedHiding(LayoutType.START_SURFACE);
        verify(mPlayerCoordinator, never()).restorePlayers();
    }

    @Test
    public void testHidePlayer_FullScreen() {
        requestAndStartPlayback();
        mFullscreenObserver.getValue().onEnterFullscreen(mTab, new FullscreenOptions(true, true));
        verify(mPlayerCoordinator).hidePlayers();

        mFullscreenObserver.getValue().onExitFullscreen(mTab);
        verify(mPlayerCoordinator).restorePlayers();
    }

    @Test
    public void testIsAvailable() {
        // test set up: non incognito profile + MSBB Accepted + policy pref returns true
        assertTrue(mController.isAvailable());

        // test returns false when policy pref is false
        when(mPrefService.getBoolean("readaloud.listen_to_this_page_enabled")).thenReturn(false);
        assertFalse(mController.isAvailable());
    }

    @Test
    public void testIsAvailable_offTheRecord() {
        when(mMockProfile.isOffTheRecord()).thenReturn(true);
        assertFalse(mController.isAvailable());
    }

    @Test
    public void testIsAvailable_noMSBB() {
        UnifiedConsentServiceBridge.setUrlKeyedAnonymizedDataCollectionEnabled(false);
        assertFalse(mController.isAvailable());
    }

    @Test
    public void testIsAvailable_inMultiWindow() {
        shadowOf(mActivity).setInMultiWindowMode(true);
        assertFalse(mController.isAvailable());

        shadowOf(mActivity).setInMultiWindowMode(false);
        assertTrue(mController.isAvailable());
    }

    @Test
    @EnableFeatures({ChromeFeatureList.READALOUD_IN_MULTI_WINDOW})
    public void testIsAvailable_inMultiWindow_flag() {
        shadowOf(mActivity).setInMultiWindowMode(true);
        assertTrue(mController.isAvailable());

        shadowOf(mActivity).setInMultiWindowMode(false);
        assertTrue(mController.isAvailable());
    }

    @Test
    public void testOnLoadStarted_differentDocument() {
        // start a successful playback
        mController.playTab(mTab, ReadAloudController.Entrypoint.MAGIC_TOOLBAR);
        resolvePromises();
        verify(mPlaybackHooks).createPlayback(Mockito.any(), mPlaybackCallbackCaptor.capture());
        onPlaybackSuccess(mPlayback);
        resolvePromises();

        // Load new url
        when(mTab.getUrl()).thenReturn(new GURL("https://en.wikipedia.org/wiki/Alphabet_Inc."));
        mController.getTabModelTabObserverforTests().onLoadStarted(mTab, true);

        verify(mHighlighter).handleTabReloaded(eq(mTab));
        verify(mPlayerCoordinator).dismissPlayers();
    }

    @Test
    public void testOnLoadStarted_sameDocument() {
        // start a successful playback
        mController.playTab(mTab, ReadAloudController.Entrypoint.MAGIC_TOOLBAR);
        resolvePromises();
        verify(mPlaybackHooks).createPlayback(Mockito.any(), mPlaybackCallbackCaptor.capture());
        onPlaybackSuccess(mPlayback);
        resolvePromises();

        // Load the same document
        mController.getTabModelTabObserverforTests().onLoadStarted(mTab, false);

        // nothing should happen
        verify(mHighlighter, never()).handleTabReloaded(eq(mTab));
        verify(mPlayerCoordinator, never()).dismissPlayers();
    }

    @Test
    public void testReloadingPage() {
        // Reload tab before any playback starts - tests null checks
        mController.getTabModelTabObserverforTests().onPageLoadStarted(mTab, mTab.getUrl());

        verify(mPlayerCoordinator, never()).dismissPlayers();
        verify(mPlayback, never()).release();

        // now start playing a tab
        requestAndStartPlayback();

        // reload some other tab, playback should keep going
        MockTab newTab = mTabModelSelector.addMockTab();
        newTab.setGurlOverrideForTesting(new GURL("https://en.wikipedia.org/wiki/Alphabet_Inc."));
        mController.getTabModelTabObserverforTests().onUrlUpdated(newTab);

        verify(mPlayerCoordinator, never()).dismissPlayers();
        verify(mPlayback, never()).release();

        // now reload the playing tab, playback should still keep going
        mController.getTabModelTabObserverforTests().onUrlUpdated(mTab);

        verify(mPlayerCoordinator, never()).dismissPlayers();
        verify(mPlayback, never()).release();
    }

    @Test
    public void testOnActivityAttachmentChanged() {
        // change tab attachment before any playback starts - tests null checks
        mController
                .getTabModelTabObserverforTests()
                .onActivityAttachmentChanged(mTab, /* window= */ null);

        verify(mPlayerCoordinator, never()).dismissPlayers();
        verify(mPlayback, never()).release();

        // now start playing a tab
        requestAndStartPlayback();

        // change attachement of some other tab, playback should keep going
        MockTab newTab = mTabModelSelector.addMockTab();
        newTab.setGurlOverrideForTesting(new GURL("https://en.wikipedia.org/wiki/Alphabet_Inc."));
        mController
                .getTabModelTabObserverforTests()
                .onActivityAttachmentChanged(newTab, /* window= */ null);

        verify(mPlayerCoordinator, never()).dismissPlayers();
        verify(mPlayback, never()).release();

        // now detach the playing tab
        mController
                .getTabModelTabObserverforTests()
                .onActivityAttachmentChanged(mTab, /* window= */ null);

        verify(mPlayerCoordinator).dismissPlayers();
        verify(mPlayback).release();
    }

    @Test
    public void testOnActivityAttachmentChanged_saveAndRestoreState() {
        // start playing a tab
        requestAndStartPlayback();

        // now detach the playing tab
        mController
                .getTabModelTabObserverforTests()
                .onActivityAttachmentChanged(mTab, /* window= */ null);

        verify(mPlayerCoordinator).dismissPlayers();
        verify(mPlayback).release();

        // Load a different tab. Playback shouldn't be restored
        // Load the previously playing tab. Saved playback state should be restored.
        Tab tab = mTabModelSelector.addMockTab();
        TabModelUtils.selectTabById(mTabModelSelector, tab.getId(), TabSelectionType.FROM_NEW);

        verify(mPlaybackHooks, times(1)).createPlayback(any(), mPlaybackCallbackCaptor.capture());

        // Load the previously playing tab. Saved playback state should be restored.
        TabModelUtils.selectTabById(mTabModelSelector, mTab.getId(), TabSelectionType.FROM_NEW);
        verify(mPlaybackHooks, times(2)).createPlayback(any(), mPlaybackCallbackCaptor.capture());

        // Loading the same tab should not re-trigger playback
        TabModelUtils.selectTabById(mTabModelSelector, mTab.getId(), TabSelectionType.FROM_NEW);
        verify(mPlaybackHooks, times(2)).createPlayback(any(), mPlaybackCallbackCaptor.capture());
    }

    @Test
    public void testIsRestoringPlayer() {
        assertFalse(mController.isRestoringPlayer());

        // Start playing a tab, detach, restore
        requestAndStartPlayback();
        mController
                .getTabModelTabObserverforTests()
                .onActivityAttachmentChanged(mTab, /* window= */ null);
        verify(mPlayerCoordinator).dismissPlayers();
        verify(mPlayback).release();

        TabModelUtils.selectTabById(mTabModelSelector, mTab.getId(), TabSelectionType.FROM_NEW);
        verify(mPlaybackHooks, times(2)).createPlayback(any(), mPlaybackCallbackCaptor.capture());

        // Player is now being restored
        assertTrue(mController.isRestoringPlayer());

        // Mini player finishes showing, done restoring player
        mController.onMiniPlayerShown();
        assertFalse(mController.isRestoringPlayer());
    }

    @Test
    public void testClosingTab() {
        // Close a tab before any playback starts - tests null checks
        mController.getTabModelTabObserverforTests().willCloseTab(mTab);

        verify(mPlayerCoordinator, never()).dismissPlayers();
        verify(mPlayback, never()).release();

        // now start playing a tab
        requestAndStartPlayback();

        // close some other tab, playback should keep going
        MockTab newTab = mTabModelSelector.addMockTab();
        newTab.setGurlOverrideForTesting(new GURL("https://en.wikipedia.org/wiki/Alphabet_Inc."));
        mController.getTabModelTabObserverforTests().willCloseTab(newTab);

        verify(mPlayerCoordinator, never()).dismissPlayers();
        verify(mPlayback, never()).release();

        // now close the playing tab
        mController.getTabModelTabObserverforTests().willCloseTab(mTab);

        verify(mPlayerCoordinator).dismissPlayers();
        verify(mPlayback).release();
    }

    @Test
    public void testClosingTab_errorUiDismissed() {
        // start a playback with an error
        mController.playTab(mTab, ReadAloudController.Entrypoint.MAGIC_TOOLBAR);
        resolvePromises();
        verify(mPlaybackHooks, times(1))
                .createPlayback(Mockito.any(), mPlaybackCallbackCaptor.capture());
        mPlaybackCallbackCaptor.getValue().onFailure(new Exception("Very bad error"));
        resolvePromises();

        // Close this tab
        mController.getTabModelTabObserverforTests().willCloseTab(mTab);

        // No playback but error UI should get dismissed
        verify(mPlayerCoordinator).dismissPlayers();
    }

    // Helper function for checkReadabilityOnPageLoad_URLnotReadAloudSupported() to check
    // the provided url is recognized as unreadable
    private void checkURLNotReadAloudSupported(GURL url) {
        mTab.setGurlOverrideForTesting(url);

        mController.maybeCheckReadability(mTab);

        verify(mHooksImpl, never())
                .isPageReadable(
                        Mockito.anyString(),
                        Mockito.any(ReadAloudReadabilityHooks.ReadabilityCallback.class));
    }

    @Test
    public void checkReadabilityOnPageLoad_URLnotReadAloudSupported() {
        reset(mHooksImpl);
        checkURLNotReadAloudSupported(new GURL("invalid"));
        checkURLNotReadAloudSupported(GURL.emptyGURL());
        checkURLNotReadAloudSupported(new GURL("chrome://history/"));
        checkURLNotReadAloudSupported(new GURL("about:blank"));
        checkURLNotReadAloudSupported(new GURL("https://www.google.com/search?q=weather"));
        checkURLNotReadAloudSupported(new GURL("https://myaccount.google.com/"));
        checkURLNotReadAloudSupported(new GURL("https://myactivity.google.com/"));
    }

    @Test
    public void checkReadability_TabError() {
        TabTestUtils.setIsShowingErrorPage(mTab, true);
        assertFalse(mController.isReadable(mTab));
        verify(mHooksImpl, never())
                .isPageReadable(
                        Mockito.anyString(),
                        Mockito.any(ReadAloudReadabilityHooks.ReadabilityCallback.class));
    }

    @Test
    public void checkReadability_success() {
        mController.maybeCheckReadability(mTab);

        verify(mHooksImpl, times(1))
                .isPageReadable(eq(sTestGURL.getSpec()), mCallbackCaptor.capture());
        assertFalse(mController.isReadable(mTab));

        mCallbackCaptor.getValue().onSuccess(sTestGURL.getSpec(), true, false);
        assertTrue(mController.isReadable(mTab));
        assertFalse(mController.timepointsSupported(mTab));

        // now check that the second time the same url loads we don't resend a request
        mController.maybeCheckReadability(mTab);

        verify(mHooksImpl, times(1))
                .isPageReadable(
                        Mockito.anyString(),
                        Mockito.any(ReadAloudReadabilityHooks.ReadabilityCallback.class));
    }

    @Test
    public void checkReadability_noMSBB() {
        mController.maybeCheckReadability(mTab);

        verify(mHooksImpl, times(1))
                .isPageReadable(eq(sTestGURL.getSpec()), mCallbackCaptor.capture());
        assertFalse(mController.isReadable(mTab));

        mCallbackCaptor.getValue().onSuccess(sTestGURL.getSpec(), true, false);
        UnifiedConsentServiceBridge.setUrlKeyedAnonymizedDataCollectionEnabled(false);
        assertFalse(mController.isReadable(mTab));
    }

    @Test
    public void checkReadability_onlyOnePendingRequest() {
        mController.maybeCheckReadability(mTab);
        mController.maybeCheckReadability(mTab);
        mController.maybeCheckReadability(mTab);

        verify(mHooksImpl, times(1)).isPageReadable(Mockito.anyString(), mCallbackCaptor.capture());
    }

    @Test
    public void checkReadability_notReadable_resultExpired() {
        mController.maybeCheckReadability(mTab);
        verify(mHooksImpl, times(1))
                .isPageReadable(eq(sTestGURL.getSpec()), mCallbackCaptor.capture());
        assertFalse(mController.isReadable(mTab));

        mCallbackCaptor.getValue().onSuccess(sTestGURL.getSpec(), false, false);
        assertFalse(mController.isReadable(mTab));

        // check 1hr1s later for the same url, we should return false and request readability again
        mClock.advanceCurrentTimeMillis(1 * 60 * 60 * 1000 + 1000);

        mController.maybeCheckReadability(mTab);

        verify(mHooksImpl, times(2))
                .isPageReadable(
                        Mockito.anyString(),
                        Mockito.any(ReadAloudReadabilityHooks.ReadabilityCallback.class));
    }

    @Test
    public void checkReadability_readable_resultExpired() {
        mController.maybeCheckReadability(mTab);
        verify(mHooksImpl, times(1))
                .isPageReadable(eq(sTestGURL.getSpec()), mCallbackCaptor.capture());
        assertFalse(mController.isReadable(mTab));

        mCallbackCaptor.getValue().onSuccess(sTestGURL.getSpec(), true, false);
        assertTrue(mController.isReadable(mTab));

        // check 1hr1s later for the same url, we should remove the record, return false and request
        // readability again
        mClock.advanceCurrentTimeMillis(1 * 60 * 60 * 1000 + 1000);

        assertFalse(mController.isReadable(mTab));
        mController.maybeCheckReadability(mTab);

        verify(mHooksImpl, times(2))
                .isPageReadable(
                        Mockito.anyString(),
                        Mockito.any(ReadAloudReadabilityHooks.ReadabilityCallback.class));
    }

    @Test
    public void checkReadability_failure() {
        mController.maybeCheckReadability(mTab);

        verify(mHooksImpl, times(1))
                .isPageReadable(eq(sTestGURL.getSpec()), mCallbackCaptor.capture());
        assertFalse(mController.isReadable(mTab));

        mCallbackCaptor
                .getValue()
                .onFailure(sTestGURL.getSpec(), new Throwable("Something went wrong"));
        assertFalse(mController.isReadable(mTab));
        assertFalse(mController.timepointsSupported(mTab));

        // now check that the second time the same url loads we will resend a request
        mController.maybeCheckReadability(mTab);

        verify(mHooksImpl, times(2))
                .isPageReadable(
                        Mockito.anyString(),
                        Mockito.any(ReadAloudReadabilityHooks.ReadabilityCallback.class));
    }

    @Test
    public void checkReadability_emptyURL() {
        mController.maybeCheckReadability(mTab);

        verify(mHooksImpl, times(1))
                .isPageReadable(eq(sTestGURL.getSpec()), mCallbackCaptor.capture());
        boolean failed = false;
        try {
            mCallbackCaptor.getValue().onSuccess("", true, true);
        } catch (AssertionError e) {
            failed = true;
        }
        assertTrue(failed);
    }

    @Test
    public void checkReadability_offline() {
        DeviceConditions.sForceConnectionTypeForTesting = true;
        assertFalse(mController.isReadable(mTab));
    }

    @Test
    public void testNetworkConnectionTypeChangedNotifiesReadabilityChanged() {
        Runnable runnable = Mockito.mock(Runnable.class);
        mController.addReadabilityUpdateListener(runnable);

        mController.onConnectionTypeChanged(0);
        verify(runnable, times(1)).run();
    }

    @Test
    public void isReadable_cacheSharedBetweenInstances() {
        // Check readability
        mController.maybeCheckReadability(mTab);

        verify(mHooksImpl, times(1))
                .isPageReadable(eq(sTestGURL.getSpec()), mCallbackCaptor.capture());
        assertFalse(mController.isReadable(mTab));

        // The page is readable, result should be cached.
        mCallbackCaptor.getValue().onSuccess(sTestGURL.getSpec(), true, false);
        assertTrue(mController.isReadable(mTab));
        assertFalse(mController.timepointsSupported(mTab));

        // A second newly created controller should know that the page is readable.
        mController2 = createController();
        assertTrue(mController2.isReadable(mTab));
        assertFalse(mController2.timepointsSupported(mTab));

        // The second controller should not send requests to check the same URL's readability.
        mController2.maybeCheckReadability(mTab);
        // Still only one call.
        verify(mHooksImpl, times(1))
                .isPageReadable(
                        Mockito.anyString(),
                        Mockito.any(ReadAloudReadabilityHooks.ReadabilityCallback.class));
    }

    @Test
    public void isReadable_languageSupported() {
        mController.maybeCheckReadability(mTab);

        verify(mHooksImpl, times(1))
                .isPageReadable(eq(sTestGURL.getSpec()), mCallbackCaptor.capture());

        mCallbackCaptor.getValue().onSuccess(sTestGURL.getSpec(), true, false);
        assertTrue(mController.isReadable(mTab));

        // check that URL is supported when the language is set to a supported language
        mFakeTranslateBridge.setCurrentLanguage("en");
        assertTrue(mController.isReadable(mTab));
    }

    @Test
    public void isReadable_resultExpired() {
        mController.maybeCheckReadability(mTab);

        verify(mHooksImpl).isPageReadable(eq(sTestGURL.getSpec()), mCallbackCaptor.capture());

        mCallbackCaptor.getValue().onSuccess(sTestGURL.getSpec(), true, false);
        assertTrue(mController.isReadable(mTab));

        // advance by 1hr
        mClock.advanceCurrentTimeMillis(1 * 60 * 60 * 1000);
        assertTrue(mController.isReadable(mTab));

        // advance by 1s - we're past the 1h limit, the record should be deleted
        mClock.advanceCurrentTimeMillis(1000);
        assertFalse(mController.isReadable(mTab));
        // make sure readability isn't called again
        verify(mHooksImpl, times(1))
                .isPageReadable(eq(sTestGURL.getSpec()), mCallbackCaptor.capture());
    }

    @Test
    public void isReadable_languageUnsupported() {
        mController.maybeCheckReadability(mTab);

        verify(mHooksImpl, times(1))
                .isPageReadable(eq(sTestGURL.getSpec()), mCallbackCaptor.capture());

        mCallbackCaptor.getValue().onSuccess(sTestGURL.getSpec(), true, false);
        assertTrue(mController.isReadable(mTab));

        // check that URL isn't supported when the language is set to an unsupported language
        mFakeTranslateBridge.setCurrentLanguage("he");
        assertFalse(mController.isReadable(mTab));
    }

    @Test
    public void testIsReadable_errorCases() {
        assertFalse(mController.isReadable(null));

        when(mTab.getUrl()).thenReturn(null);
        assertFalse(mController.isReadable(mTab));

        when(mTab.getUrl()).thenReturn(sTestGURL);
        when(mTab.getWebContents()).thenReturn(mWebContents);
        doReturn(false).when(mMockProfile).isNativeInitialized();
        assertFalse(mController.isReadable(mTab));

        when(mTab.getUrl()).thenReturn(sTestGURL);
        when(mTab.getWebContents()).thenReturn(mWebContents);
        doReturn(true).when(mMockProfile).isNativeInitialized();
        doReturn(true).when(mTab).isNativePage();
        doReturn(mNativePage).when(mTab).getNativePage();
        doReturn(true).when(mNativePage).isPdf();
        assertFalse(mController.isReadable(mTab));
    }

    @Test
    public void testReactingtoMSBBChange() {
        mController.maybeCheckReadability(mTab);

        verify(mHooksImpl, times(1))
                .isPageReadable(eq(sTestGURL.getSpec()), mCallbackCaptor.capture());

        // Disable MSBB. Sending requests to Google servers no longer allowed but using
        // previous results is ok.
        UnifiedConsentServiceBridge.setUrlKeyedAnonymizedDataCollectionEnabled(false);
        mTab.setGurlOverrideForTesting(JUnitTestGURLs.GOOGLE_URL_CAT);
        mController.maybeCheckReadability(mTab);

        verify(mHooksImpl, times(1))
                .isPageReadable(
                        Mockito.anyString(),
                        Mockito.any(ReadAloudReadabilityHooks.ReadabilityCallback.class));
    }

    @Test
    public void testPlayTab() {
        requestAndStartPlayback();
        verify(mPlayerCoordinator).addObserver(mController);

        // test that previous playback is released when another playback is called
        MockTab newTab = mTabModelSelector.addMockTab();
        newTab.setGurlOverrideForTesting(new GURL("https://en.wikipedia.org/wiki/Alphabet_Inc."));
        newTab.setWebContentsOverrideForTesting(mWebContents);
        mController.playTab(newTab, ReadAloudController.Entrypoint.MAGIC_TOOLBAR);
        resolvePromises();
        verify(mPlayback, times(1)).release();
    }

    @Test
    public void testPlayTab_playerClosedDuringLoad() {
        // start a playback with an error
        mController.playTab(mTab, ReadAloudController.Entrypoint.MAGIC_TOOLBAR);
        resolvePromises();

        mController.onRequestClosePlayers();

        verify(mPlaybackHooks, times(1))
                .createPlayback(Mockito.any(), mPlaybackCallbackCaptor.capture());
        onPlaybackSuccess(mPlayback);
        resolvePromises();

        verify(mPlayerCoordinator, never())
                .playbackReady(eq(mPlayback), eq(PlaybackListener.State.PLAYING));
    }

    @Test
    public void testPlayTab_inMultiWindow() {
        mFakeTranslateBridge.setCurrentLanguage("en");
        mTab.setGurlOverrideForTesting(new GURL("https://en.wikipedia.org/wiki/Google"));
        mController.playTab(mTab, ReadAloudController.Entrypoint.MAGIC_TOOLBAR);
        resolvePromises();

        verify(mPlaybackHooks)
                .createPlayback(mPlaybackArgsCaptor.capture(), mPlaybackCallbackCaptor.capture());
        assertEquals(null, mPlaybackArgsCaptor.getValue().getLanguage());

        shadowOf(mActivity).setInMultiWindowMode(true);
        onPlaybackSuccess(mPlayback);

        verify(mPlayerCoordinator).playbackFailed();
    }

    @Test
    @EnableFeatures({ChromeFeatureList.READALOUD_IN_MULTI_WINDOW})
    public void testPlayTab_inMultiWindow_flag() {
        mFakeTranslateBridge.setCurrentLanguage("en");
        mTab.setGurlOverrideForTesting(new GURL("https://en.wikipedia.org/wiki/Google"));
        mController.playTab(mTab, ReadAloudController.Entrypoint.MAGIC_TOOLBAR);
        resolvePromises();

        verify(mPlaybackHooks)
                .createPlayback(mPlaybackArgsCaptor.capture(), mPlaybackCallbackCaptor.capture());
        assertEquals(null, mPlaybackArgsCaptor.getValue().getLanguage());

        shadowOf(mActivity).setInMultiWindowMode(true);
        onPlaybackSuccess(mPlayback);

        verify(mPlayerCoordinator, times(1))
                .playbackReady(eq(mPlayback), eq(PlaybackListener.State.PLAYING));
        verify(mPlayerCoordinator).addObserver(mController);
    }

    @Test
    public void testKeepScreenOnFlag() {
        // default - don't keep the screen on
        int flags = mActivity.getWindow().getAttributes().flags;
        assertTrue((flags & WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON) == 0);

        // play tab
        requestAndStartPlayback();
        verify(mPlayback).addListener(mPlaybackListenerCaptor.capture());
        // update playback data so it isn't null
        var data = Mockito.mock(PlaybackListener.PlaybackData.class);
        doReturn(PlaybackListener.State.PLAYING).when(data).state();
        mPlaybackListenerCaptor.getValue().onPlaybackDataChanged(data);

        // keep the screen on while something is playing
        flags = mActivity.getWindow().getAttributes().flags;
        assertTrue((flags & WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON) != 0);

        doReturn(PlaybackListener.State.BUFFERING).when(data).state();
        mPlaybackListenerCaptor.getValue().onPlaybackDataChanged(data);

        // don't keep the screen on if paused/stopped/buffering
        flags = mActivity.getWindow().getAttributes().flags;
        assertTrue((flags & WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON) == 0);

        doReturn(PlaybackListener.State.PLAYING).when(data).state();
        mPlaybackListenerCaptor.getValue().onPlaybackDataChanged(data);
        // playing again - keep the screen on
        flags = mActivity.getWindow().getAttributes().flags;
        assertTrue((flags & WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON) != 0);

        mController.maybeStopPlayback(
                mTab, ReadAloudMetrics.ReasonForStoppingPlayback.NEW_PLAYBACK_REQUEST);

        // playback stopped, clear the flag, don't keep the screen on
        flags = mActivity.getWindow().getAttributes().flags;
        assertTrue((flags & WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON) == 0);
    }

    @Test
    public void testPlayTab_sendsVoiceList() {
        mFakeTranslateBridge.setCurrentLanguage("en");
        doReturn(
                        List.of(
                                new PlaybackVoice("en", "voiceA"),
                                new PlaybackVoice("es", "voiceB"),
                                new PlaybackVoice("fr", "voiceC")))
                .when(mPlaybackHooks)
                .getPlaybackVoiceList(any());
        mTab.setGurlOverrideForTesting(new GURL("https://en.wikipedia.org/wiki/Google"));

        mController.playTab(mTab, ReadAloudController.Entrypoint.MAGIC_TOOLBAR);
        resolvePromises();

        verify(mPlaybackHooks, times(1)).initVoices();
        verify(mPlaybackHooks, times(1)).createPlayback(mPlaybackArgsCaptor.capture(), any());

        List<PlaybackVoice> voices = mPlaybackArgsCaptor.getValue().getVoices();
        assertNotNull(voices);
        assertEquals(3, voices.size());
        assertEquals("en", voices.get(0).getLanguage());
        assertEquals("voiceA", voices.get(0).getVoiceId());
        assertEquals("es", voices.get(1).getLanguage());
        assertEquals("voiceB", voices.get(1).getVoiceId());
        assertEquals("fr", voices.get(2).getLanguage());
        assertEquals("voiceC", voices.get(2).getVoiceId());
    }

    @Test
    public void testPlayTab_EmptyUrl() {
        mFakeTranslateBridge.setCurrentLanguage("en");
        mTab.setGurlOverrideForTesting(new GURL(""));
        boolean failed = false;
        try {
            mController.playTab(mTab, ReadAloudController.Entrypoint.MAGIC_TOOLBAR);
        } catch (AssertionError e) {
            failed = true;
        }
        assertTrue(failed);
    }

    @Test
    public void testPlayTranslatedTab_tabLanguageEmpty() {
        Locale.setDefault(FR_FR);

        mFakeTranslateBridge.setIsPageTranslated(true);
        mFakeTranslateBridge.setCurrentLanguage("");
        mTab.setGurlOverrideForTesting(new GURL("https://en.wikipedia.org/wiki/Google"));

        mController.playTab(mTab, ReadAloudController.Entrypoint.MAGIC_TOOLBAR);
        resolvePromises();

        // Without translate bridge reporting a language for the page, fall back to the system
        // language.
        verify(mPlaybackHooks).createPlayback(mPlaybackArgsCaptor.capture(), any());
        assertEquals("fr", mPlaybackArgsCaptor.getValue().getLanguage());
    }

    @Test
    public void testPlayTranslatedTab_unsupportedLanguage() {
        doReturn(List.of()).when(mPlaybackHooks).getVoicesFor(anyString());
        mFakeTranslateBridge.setCurrentLanguage("zz-ZZ");
        mTab.setGurlOverrideForTesting(new GURL("https://en.wikipedia.org/wiki/Google"));

        mController.playTab(mTab, ReadAloudController.Entrypoint.MAGIC_TOOLBAR);
        resolvePromises();

        verify(mPlaybackHooks, never()).createPlayback(mPlaybackArgsCaptor.capture(), any());
        verify(mPlayerCoordinator).playbackFailed();
    }

    @Test
    public void testPlayTranslatedTab_tabLanguageUnd() {
        Locale.setDefault(FR_FR);

        mFakeTranslateBridge.setIsPageTranslated(true);
        mFakeTranslateBridge.setCurrentLanguage("und");
        mTab.setGurlOverrideForTesting(new GURL("https://en.wikipedia.org/wiki/Google"));

        mController.playTab(mTab, ReadAloudController.Entrypoint.MAGIC_TOOLBAR);
        resolvePromises();

        // Without translate bridge reporting a language for the page, fall back to the system
        // language.
        verify(mPlaybackHooks).createPlayback(mPlaybackArgsCaptor.capture(), any());
        assertEquals("fr", mPlaybackArgsCaptor.getValue().getLanguage());
    }

    @Test
    public void testPlayUntranslatedTab() {
        mFakeTranslateBridge.setIsPageTranslated(false);
        mFakeTranslateBridge.setCurrentLanguage("fr");
        mTab.setGurlOverrideForTesting(new GURL("https://en.wikipedia.org/wiki/Google"));

        mController.playTab(mTab, ReadAloudController.Entrypoint.MAGIC_TOOLBAR);
        resolvePromises();

        // If page isn't translated, don't send a language and instead let the server decide the
        // language.
        verify(mPlaybackHooks).createPlayback(mPlaybackArgsCaptor.capture(), any());
        assertEquals(null, mPlaybackArgsCaptor.getValue().getLanguage());
    }

    @Test
    public void testVoicesMatchLanguage_pageTranslated() {
        // translated page should use chrome language
        var voiceEn = new PlaybackVoice("en", "asdf", "");
        var voiceFr = new PlaybackVoice("fr", "asdf", "");
        when(mMetadata.languageCode()).thenReturn("en");
        doReturn(List.of(voiceEn)).when(mPlaybackHooks).getVoicesFor(eq("en"));
        doReturn(List.of(voiceFr)).when(mPlaybackHooks).getVoicesFor(eq("fr"));
        doReturn(List.of(voiceEn, voiceFr)).when(mPlaybackHooks).getPlaybackVoiceList(any());
        mFakeTranslateBridge.setIsPageTranslated(true);
        mFakeTranslateBridge.setCurrentLanguage("fr");
        mTab.setGurlOverrideForTesting(new GURL("https://en.wikipedia.org/wiki/Google"));

        mController.playTab(mTab, ReadAloudController.Entrypoint.MAGIC_TOOLBAR);
        resolvePromises();

        verify(mPlaybackHooks)
                .createPlayback(mPlaybackArgsCaptor.capture(), mPlaybackCallbackCaptor.capture());
        assertEquals("fr", mPlaybackArgsCaptor.getValue().getLanguage());
        onPlaybackSuccess(mPlayback);
        // Page is in French, voice options should have voices for "fr"
        assertEquals(
                "fr", mController.getCurrentLanguageVoicesSupplier().get().get(0).getLanguage());
    }

    @Test
    public void testVoicesMatchLanguage_pageNotTranslated() {
        // non translated page should use server detected content language
        var voiceEn = new PlaybackVoice("en", "asdf", "");
        var voiceFr = new PlaybackVoice("fr", "asdf", "");
        doReturn(List.of(voiceEn)).when(mPlaybackHooks).getVoicesFor(eq("en"));
        doReturn(List.of(voiceFr)).when(mPlaybackHooks).getVoicesFor(eq("fr"));
        doReturn(List.of(voiceEn, voiceFr)).when(mPlaybackHooks).getPlaybackVoiceList(any());
        when(mMetadata.languageCode()).thenReturn("en");
        mFakeTranslateBridge.setIsPageTranslated(false);
        mFakeTranslateBridge.setCurrentLanguage("fr");
        mTab.setGurlOverrideForTesting(new GURL("https://en.wikipedia.org/wiki/Google"));

        mController.playTab(mTab, ReadAloudController.Entrypoint.MAGIC_TOOLBAR);
        resolvePromises();

        verify(mPlaybackHooks)
                .createPlayback(mPlaybackArgsCaptor.capture(), mPlaybackCallbackCaptor.capture());
        assertEquals(null, mPlaybackArgsCaptor.getValue().getLanguage());
        onPlaybackSuccess(mPlayback);

        assertEquals(
                "en", mController.getCurrentLanguageVoicesSupplier().get().get(0).getLanguage());
    }

    @Test
    public void testFailureIfServerLanguageUnsupported() {
        // non translated page should use server detected content language
        var voiceEn = new PlaybackVoice("en", "asdf", "");
        var voiceFr = new PlaybackVoice("fr", "asdf", "");
        doReturn(List.of(voiceEn)).when(mPlaybackHooks).getVoicesFor(eq("en"));
        doReturn(List.of(voiceFr)).when(mPlaybackHooks).getVoicesFor(eq("fr"));
        doReturn(List.of(voiceEn, voiceFr)).when(mPlaybackHooks).getPlaybackVoiceList(any());
        // unsupported
        when(mMetadata.languageCode()).thenReturn("pl");
        mFakeTranslateBridge.setIsPageTranslated(false);
        mFakeTranslateBridge.setCurrentLanguage("fr");
        mTab.setGurlOverrideForTesting(new GURL("https://en.wikipedia.org/wiki/Google"));

        mController.playTab(mTab, ReadAloudController.Entrypoint.MAGIC_TOOLBAR);
        resolvePromises();

        verify(mPlaybackHooks)
                .createPlayback(mPlaybackArgsCaptor.capture(), mPlaybackCallbackCaptor.capture());
        assertEquals(null, mPlaybackArgsCaptor.getValue().getLanguage());
        onPlaybackSuccess(mPlayback);

        verify(mPlayerCoordinator).playbackFailed();
    }

    @Test
    public void testPlayTab_onFailure() {
        mFakeTranslateBridge.setCurrentLanguage("en");
        GURL gurl = new GURL("https://en.wikipedia.org/wiki/Google");
        mTab.setGurlOverrideForTesting(gurl);
        mController.maybeCheckReadability(mTab);
        // also check that a generic error doesn't invalidate readability result
        verify(mHooksImpl).isPageReadable(eq(gurl.getSpec()), mCallbackCaptor.capture());
        mCallbackCaptor
                .getValue()
                .onSuccess(
                        gurl.getSpec(), /* isReadable= */ true, /* timepointsSupported= */ false);
        mController.playTab(mTab, ReadAloudController.Entrypoint.MAGIC_TOOLBAR);
        resolvePromises();

        assertTrue(mController.isReadable(mTab));
        verify(mPlaybackHooks, times(1))
                .createPlayback(Mockito.any(), mPlaybackCallbackCaptor.capture());
        mPlaybackCallbackCaptor.getValue().onFailure(new Throwable());
        resolvePromises();
        verify(mPlayerCoordinator, times(1)).playbackFailed();
        assertTrue(mController.isReadable(mTab));
    }

    @Test
    public void testPlayTab_onFailure_unsupportedLink() {
        mFakeTranslateBridge.setCurrentLanguage("en");
        GURL gurl = new GURL("https://en.wikipedia.org/wiki/Google");
        mTab.setGurlOverrideForTesting(gurl);
        mController.maybeCheckReadability(mTab);
        // also check that a readAloudUnsupported error does invalidate a false positive readability
        // result
        verify(mHooksImpl).isPageReadable(eq(gurl.getSpec()), mCallbackCaptor.capture());
        mCallbackCaptor
                .getValue()
                .onSuccess(
                        gurl.getSpec(), /* isReadable= */ true, /* timepointsSupported= */ false);

        assertTrue(mController.isReadable(mTab));
        mController.playTab(mTab, ReadAloudController.Entrypoint.MAGIC_TOOLBAR);
        resolvePromises();

        verify(mPlaybackHooks, times(1))
                .createPlayback(Mockito.any(), mPlaybackCallbackCaptor.capture());
        mPlaybackCallbackCaptor
                .getValue()
                .onFailure(
                        new ReadAloudUnsupportedException(
                                "message",
                                /* throwable= */ null,
                                ReadAloudUnsupportedException.RejectionReason
                                        .UNKNOWN_REJECTION_REASON));
        resolvePromises();
        verify(mPlayerCoordinator, times(1)).playbackFailed();
        assertFalse(mController.isReadable(mTab));
    }

    @Test
    public void testStopPlayback() {
        // Play tab
        requestAndStartPlayback();

        // Stop playback
        mController.maybeStopPlayback(
                mTab, ReadAloudMetrics.ReasonForStoppingPlayback.NEW_PLAYBACK_REQUEST);
        verify(mPlayback).release();

        resetPlaybackMocks();

        // Subsequent playTab() should play without trying to release anything.
        mController.playTab(mTab, ReadAloudController.Entrypoint.MAGIC_TOOLBAR);
        resolvePromises();
        verify(mPlaybackHooks).createPlayback(any(), any());
        verify(mPlayback, never()).release();
    }

    @Test
    public void testStopPlaybackWhenBackPressingToNewTab() {
        // Play tab
        requestAndStartPlayback();

        // now simulate back press to a new tab page (doesn't trigger new url load)
        when(mTab.getUrl()).thenReturn(new GURL("chrome-native://newtab/"));
        mController.getTabModelTabObserverforTests().onUrlUpdated(mTab);

        verify(mPlayback).release();
    }

    @Test
    public void testDontStopPlayback() {
        // Play tab
        requestAndStartPlayback();

        // now simulate a situation updateUrl was called with the same url as the one playing -
        // nothing should happen
        mController.getTabModelTabObserverforTests().onUrlUpdated(mTab);
        verify(mPlayback, never()).release();

        // now update url from a different, non playing tab. The active playback should be
        // unaffected.
        MockTab tab = mTabModelSelector.addMockTab();
        tab.setWebContentsOverrideForTesting(mWebContents);
        tab.setUrl(new GURL(""));
        mController.getTabModelTabObserverforTests().onUrlUpdated(tab);
        verify(mPlayback, never()).release();
    }

    @Test
    public void highlightsRequested() {
        // set up the highlighter
        mController.setTimepointsSupportedForTest(mTab.getUrl().getSpec(), true);
        mController.playTab(mTab, ReadAloudController.Entrypoint.MAGIC_TOOLBAR);
        resolvePromises();
        verify(mPlaybackHooks, times(1))
                .createPlayback(Mockito.any(), mPlaybackCallbackCaptor.capture());
        onPlaybackSuccess(mPlayback);
        verify(mHighlighter).initializeJs(eq(mTab), eq(mMetadata), any(Highlighter.Config.class));
        // Checks that the pref is read to set up highlighter state
        // hasPrefPath is called twice, once during ReadAloudPrefs.isHighlightingEnabled and during
        // ReadAloudPrefs.setHighlightingEnabled
        verify(mPrefService, times(2)).hasPrefPath(eq(ReadAloudPrefs.HIGHLIGHTING_ENABLED_PATH));

        // trigger highlights
        mController.onPhraseChanged(mPhraseTiming);

        verify(mHighlighter)
                .highlightText(eq(mGlobalRenderFrameHostId), eq(mTab), eq(mPhraseTiming));

        // now disable highlighting - we should not trigger highlights anymore
        mController.getHighlightingEnabledSupplier().set(false);
        // Pref is updated.
        verify(mPrefService).setBoolean(eq(ReadAloudPrefs.HIGHLIGHTING_ENABLED_PATH), eq(false));
        mController.onPhraseChanged(mPhraseTiming);
        verify(mHighlighter, times(1))
                .highlightText(eq(mGlobalRenderFrameHostId), eq(mTab), eq(mPhraseTiming));
    }

    @Test
    public void reloadingTab_highlightsNotCleared() {
        // set up the highlighter
        mController.setTimepointsSupportedForTest(mTab.getUrl().getSpec(), true);
        mController.playTab(mTab, ReadAloudController.Entrypoint.MAGIC_TOOLBAR);
        resolvePromises();
        verify(mPlaybackHooks, times(1))
                .createPlayback(Mockito.any(), mPlaybackCallbackCaptor.capture());
        onPlaybackSuccess(mPlayback);
        verify(mHighlighter).initializeJs(eq(mTab), eq(mMetadata), any(Highlighter.Config.class));

        // Reload tab to a different url.
        mController
                .getTabModelTabObserverforTests()
                .onPageLoadStarted(mTab, new GURL("http://wikipedia.org"));

        verify(mHighlighter, never()).handleTabReloaded(any());
    }

    @Test
    public void stoppingPlaybackClearsHighlighter() {
        // set up the highlighter
        mController.setTimepointsSupportedForTest(mTab.getUrl().getSpec(), true);
        mController.playTab(mTab, ReadAloudController.Entrypoint.MAGIC_TOOLBAR);
        resolvePromises();
        verify(mPlaybackHooks, times(1))
                .createPlayback(Mockito.any(), mPlaybackCallbackCaptor.capture());
        onPlaybackSuccess(mPlayback);
        verify(mHighlighter).initializeJs(eq(mTab), eq(mMetadata), any(Highlighter.Config.class));

        // stopping playback should clear highlighting.
        mController.maybeStopPlayback(
                mTab, ReadAloudMetrics.ReasonForStoppingPlayback.NEW_PLAYBACK_REQUEST);

        verify(mHighlighter).clearHighlights(eq(mGlobalRenderFrameHostId), eq(mTab));
    }

    @Test
    public void testUserDataStrippedFromReadabilityCheck() {
        GURL tabUrl = new GURL("http://user:pass@example.com");
        mTab.setGurlOverrideForTesting(tabUrl);

        mController.maybeCheckReadability(mTab);

        String sanitized = "http://example.com/";
        verify(mHooksImpl, times(1)).isPageReadable(eq(sanitized), mCallbackCaptor.capture());
        assertFalse(mController.isReadable(mTab));

        mCallbackCaptor.getValue().onSuccess(sanitized, true, true);
        assertTrue(mController.isReadable(mTab));
        assertTrue(mController.timepointsSupported(mTab));
    }

    @Test
    public void testSetHighlighterMode() {
        // highlighter can be null if page doesn't support highlighting,
        // this just test null checkss
        mController.setHighlighterMode(2);
        verify(mHighlighter, never()).handleTabReloaded(mTab);

        mController.setTimepointsSupportedForTest(mTab.getUrl().getSpec(), true);
        mController.playTab(mTab, ReadAloudController.Entrypoint.MAGIC_TOOLBAR);
        resolvePromises();
        verify(mPlaybackHooks, times(1))
                .createPlayback(Mockito.any(), mPlaybackCallbackCaptor.capture());
        onPlaybackSuccess(mPlayback);

        mController.setHighlighterMode(2);
        verify(mHighlighter, times(1)).handleTabReloaded(mTab);

        // only do something if new mode is different
        mController.setHighlighterMode(2);
        verify(mHighlighter, times(1)).handleTabReloaded(mTab);

        mController.setHighlighterMode(1);
        verify(mHighlighter, times(2)).handleTabReloaded(mTab);
    }

    @Test
    public void testSetVoiceAndRestartPlayback() {
        // Voices setup
        var oldVoice = new PlaybackVoice("lang", "OLD VOICE ID");
        doReturn(List.of(oldVoice)).when(mPlaybackHooks).getPlaybackVoiceList(any());

        // First play tab.
        mFakeTranslateBridge.setCurrentLanguage("en");
        mTab.setGurlOverrideForTesting(new GURL("https://en.wikipedia.org/wiki/Google"));
        mController.playTab(mTab, ReadAloudController.Entrypoint.MAGIC_TOOLBAR);
        resolvePromises();

        // Verify the original voice list.
        verify(mPlaybackHooks, times(1))
                .createPlayback(mPlaybackArgsCaptor.capture(), mPlaybackCallbackCaptor.capture());
        List<PlaybackVoice> gotVoices = mPlaybackArgsCaptor.getValue().getVoices();
        assertEquals(1, gotVoices.size());
        assertEquals("OLD VOICE ID", gotVoices.get(0).getVoiceId());
        onPlaybackSuccess(mPlayback);

        reset(mPlaybackHooks);

        // Set the new voice.
        var newVoice = new PlaybackVoice("lang", "NEW VOICE ID");
        doReturn(List.of(newVoice)).when(mPlaybackHooks).getPlaybackVoiceList(any());
        doReturn(List.of(newVoice)).when(mPlaybackHooks).getVoicesFor(anyString());
        var data = Mockito.mock(PlaybackData.class);
        doReturn(99).when(data).paragraphIndex();
        doReturn(PlaybackListener.State.PLAYING).when(data).state();
        mController.onPlaybackDataChanged(data);
        mController.setVoiceOverrideAndApplyToPlayback(newVoice);

        // Pref is updated.
        verify(mReadAloudPrefsNatives).setVoice(eq(mPrefService), eq("lang"), eq("NEW VOICE ID"));

        // Playback is stopped.
        verify(mPlayback).release();
        doReturn(List.of(newVoice)).when(mPlaybackHooks).getVoicesFor(anyString());

        // Playback starts again with new voice and original paragraph index.
        verify(mPlaybackHooks, times(1))
                .createPlayback(mPlaybackArgsCaptor.capture(), mPlaybackCallbackCaptor.capture());
        gotVoices = mPlaybackArgsCaptor.getValue().getVoices();
        assertEquals(1, gotVoices.size());
        assertEquals("NEW VOICE ID", gotVoices.get(0).getVoiceId());

        onPlaybackSuccess(mPlayback);
        verify(mPlayback, times(2)).play();
        verify(mPlayback).seekToParagraph(eq(99), eq(0L));
    }

    @Test
    public void testSetVoiceWhilePaused() {
        // Play tab.
        requestAndStartPlayback();
        verify(mPlayback).addListener(mPlaybackListenerCaptor.capture());

        resetPlaybackMocks();

        // Pause at paragraph 99.
        var data = Mockito.mock(PlaybackListener.PlaybackData.class);
        doReturn(PlaybackListener.State.PAUSED).when(data).state();
        doReturn(99).when(data).paragraphIndex();
        mPlaybackListenerCaptor.getValue().onPlaybackDataChanged(data);

        // Change voice setting.
        var newVoice = new PlaybackVoice("lang", "NEW VOICE ID", "description");
        doReturn(List.of(newVoice)).when(mPlaybackHooks).getVoicesFor(anyString());
        doReturn(List.of(newVoice)).when(mPlaybackHooks).getPlaybackVoiceList(any());
        mController.setVoiceOverrideAndApplyToPlayback(newVoice);

        // Tab audio should be loaded with the new voice but it should not be playing.
        verify(mPlaybackHooks)
                .createPlayback(mPlaybackArgsCaptor.capture(), mPlaybackCallbackCaptor.capture());
        var voices = mPlaybackArgsCaptor.getValue().getVoices();
        assertEquals(1, voices.size());
        assertEquals("NEW VOICE ID", voices.get(0).getVoiceId());

        onPlaybackSuccess(mPlayback);
        verify(mPlayback, never()).play();
        verify(mPlayback).seekToParagraph(eq(99), eq(0L));
    }

    @Test
    public void testPreviewVoice_whilePlaying_success() {
        // Play tab.
        requestAndStartPlayback();

        reset(mPlaybackHooks);

        // Preview a voice.
        var voice = new PlaybackVoice("en", "asdf", "");
        doReturn(List.of(voice)).when(mPlaybackHooks).getVoicesFor(anyString());
        doReturn(List.of(voice)).when(mPlaybackHooks).getPlaybackVoiceList(any());
        mController.previewVoice(voice);

        // Tab playback should stop.
        verify(mPlayback).release();
        reset(mPlayerCoordinator);
        reset(mPlayback);

        // Preview playback requested.
        verify(mPlaybackHooks)
                .createPlayback(mPlaybackArgsCaptor.capture(), mPlaybackCallbackCaptor.capture());
        reset(mPlaybackHooks);

        // Check preview playback args.
        PlaybackArgs args = mPlaybackArgsCaptor.getValue();
        assertNotNull(args);
        assertEquals("en", args.getLanguage());
        assertNotNull(args.getVoices());
        assertEquals(1, args.getVoices().size());
        assertEquals("en", args.getVoices().get(0).getLanguage());
        assertEquals("asdf", args.getVoices().get(0).getVoiceId());

        // Preview playback succeeds.
        Playback previewPlayback = Mockito.mock(Playback.class);
        onPlaybackSuccess(previewPlayback);
        verify(previewPlayback).play();
        verify(previewPlayback).addListener(mPlaybackListenerCaptor.capture());
        assertNotNull(mPlaybackListenerCaptor.getValue());

        // Preview finishes playing.
        var data = Mockito.mock(PlaybackListener.PlaybackData.class);
        doReturn(PlaybackListener.State.STOPPED).when(data).state();
        mPlaybackListenerCaptor.getValue().onPlaybackDataChanged(data);

        verify(previewPlayback).release();
    }

    @Test
    public void testPreviewVoice_whilePlaying_failure() {
        // Play tab.
        requestAndStartPlayback();

        reset(mPlaybackHooks);

        // Preview a voice.
        var voice = new PlaybackVoice("en", "asdf", "");
        doReturn(List.of(voice)).when(mPlaybackHooks).getVoicesFor(anyString());
        doReturn(List.of(voice)).when(mPlaybackHooks).getPlaybackVoiceList(any());
        mController.previewVoice(voice);

        verify(mPlaybackHooks)
                .createPlayback(mPlaybackArgsCaptor.capture(), mPlaybackCallbackCaptor.capture());
        reset(mPlaybackHooks);

        // Preview fails. Nothing to verify here yet.
        mPlaybackCallbackCaptor.getValue().onFailure(new Throwable());
        resolvePromises();
    }

    @Test
    public void testPreviewVoice_previewDuringPreview() {
        // Play tab.
        requestAndStartPlayback();

        reset(mPlaybackHooks);

        // Preview a voice.
        var voice = new PlaybackVoice("en", "asdf", "");
        doReturn(List.of(voice)).when(mPlaybackHooks).getPlaybackVoiceList(any());
        doReturn(List.of(voice)).when(mPlaybackHooks).getVoicesFor(anyString());
        mController.previewVoice(voice);

        verify(mPlaybackHooks).createPlayback(any(), mPlaybackCallbackCaptor.capture());
        Playback previewPlayback = Mockito.mock(Playback.class);
        onPlaybackSuccess(previewPlayback);
        reset(mPlaybackHooks);

        // Start another preview.
        doReturn(List.of(voice)).when(mPlaybackHooks).getVoicesFor(anyString());
        mController.previewVoice(new PlaybackVoice("en", "abcd", ""));
        // Preview playback should be stopped and cleaned up.
        verify(previewPlayback).release();
        reset(previewPlayback);
        verify(mPlaybackHooks).createPlayback(any(), mPlaybackCallbackCaptor.capture());
        reset(mPlaybackHooks);
        onPlaybackSuccess(previewPlayback);
        verify(previewPlayback).addListener(mPlaybackListenerCaptor.capture());

        // Preview finishes playing.
        var data = Mockito.mock(PlaybackListener.PlaybackData.class);
        doReturn(PlaybackListener.State.STOPPED).when(data).state();
        mPlaybackListenerCaptor.getValue().onPlaybackDataChanged(data);

        verify(previewPlayback).release();
    }

    @Test
    public void testPreviewVoice_closeVoiceMenu() {
        // Set up playback and restorable state.
        requestAndStartPlayback();
        verify(mPlayback).play();
        resetPlaybackMocks();

        var data = Mockito.mock(PlaybackListener.PlaybackData.class);
        doReturn(PlaybackListener.State.STOPPED).when(data).state();
        doReturn(99).when(data).paragraphIndex();
        mController.onPlaybackDataChanged(data);

        // Preview a voice.
        var voice = new PlaybackVoice("en", "asdf", "");
        doReturn(List.of(voice)).when(mPlaybackHooks).getPlaybackVoiceList(any());
        mController.previewVoice(voice);

        verify(mPlaybackHooks).createPlayback(any(), mPlaybackCallbackCaptor.capture());
        Playback previewPlayback = Mockito.mock(Playback.class);
        onPlaybackSuccess(previewPlayback);
        resetPlaybackMocks();

        // Closing the voice menu should stop the preview.
        mController.onVoiceMenuClosed();
        verify(previewPlayback).release();

        // Tab audio should be loaded and played. Position should be restored.
        verify(mPlaybackHooks)
                .createPlayback(mPlaybackArgsCaptor.capture(), mPlaybackCallbackCaptor.capture());
        assertEquals(1234567123456L, mPlaybackArgsCaptor.getValue().getDateModifiedMsSinceEpoch());
        onPlaybackSuccess(mPlayback);
        // Don't play, because original state was STOPPED.
        verify(mPlayback, never()).play();
        verify(mPlayback).seekToParagraph(eq(99), eq(0L));
    }

    @Test
    public void testRestorePlaybackState_whileLoading() {
        // Request playback but don't succeed yet.
        mController.playTab(mTab, ReadAloudController.Entrypoint.MAGIC_TOOLBAR);
        resolvePromises();
        verify(mPlaybackHooks).createPlayback(any(), mPlaybackCallbackCaptor.capture());
        resetPlaybackMocks();

        // User changes voices before the first playback is ready.
        mController.setVoiceOverrideAndApplyToPlayback(new PlaybackVoice("en", "1234", ""));
        // TODO(b/315028038): If changing voice during loading is possible, then we
        // should instead cancel the first request and request again.
        verify(mPlaybackHooks, never()).createPlayback(any(), any());
    }

    @Test
    public void testRestorePlaybackState_previewThenChangeVoice() {
        // When previewing a voice, tab playback should only be restored when closing
        // the menu. This test makes sure it doesn't start up early when a voice is
        // selected.

        // Set up playback and restorable state.
        requestAndStartPlayback();
        verify(mPlayback).play();

        resetPlaybackMocks();
        var data = Mockito.mock(PlaybackListener.PlaybackData.class);
        doReturn(PlaybackListener.State.PLAYING).when(data).state();
        doReturn(99).when(data).paragraphIndex();
        mController.onPlaybackDataChanged(data);

        // Preview voice.
        var voice = new PlaybackVoice("en", "asdf", "");
        doReturn(List.of(voice)).when(mPlaybackHooks).getPlaybackVoiceList(any());
        mController.previewVoice(voice);
        verify(mPlaybackHooks).createPlayback(any(), mPlaybackCallbackCaptor.capture());
        resetPlaybackMocks();
        Playback previewPlayback = Mockito.mock(Playback.class);
        onPlaybackSuccess(previewPlayback);

        // Select a voice. Tab shouldn't start playing.
        mController.setVoiceOverrideAndApplyToPlayback(new PlaybackVoice("en", "1234", ""));
        verify(mPlaybackHooks, never()).createPlayback(any(), any());

        // Close the menu. Now the tab should resume playback.
        mController.onVoiceMenuClosed();
        verify(mPlaybackHooks).createPlayback(any(), mPlaybackCallbackCaptor.capture());
        onPlaybackSuccess(mPlayback);
        verify(mPlayback).play();
        verify(mPlayback).seekToParagraph(eq(99), eq(0L));
    }

    @Test
    public void testTranslationListenerRegistration() {
        // Play tab.
        requestAndStartPlayback();

        // One observer should be registered on the playing tab to stop playback if translated, and
        // one is registered regardless of playback for refreshing the entrypoint.
        assertEquals(2, mFakeTranslateBridge.getObserverCount());

        // stopping playback should unregister the listener that stops playback
        mController.maybeStopPlayback(
                mTab, ReadAloudMetrics.ReasonForStoppingPlayback.NEW_PLAYBACK_REQUEST);
        assertEquals(1, mFakeTranslateBridge.getObserverCount());
    }

    @Test
    public void testTranslationListenerRegistration_nullWebContents() {
        assertEquals(1, mFakeTranslateBridge.getObserverCount());

        // Play tab.
        when(mTab.getWebContents()).thenReturn(null);
        requestAndStartPlayback();

        assertEquals(1, mFakeTranslateBridge.getObserverCount());

        mController.maybeStopPlayback(
                mTab, ReadAloudMetrics.ReasonForStoppingPlayback.NEW_PLAYBACK_REQUEST);
        assertEquals(1, mFakeTranslateBridge.getObserverCount());
    }

    @Test
    public void testTranslationListenersUnregisteredOnTabDestroyed() {
        // Play tab.
        requestAndStartPlayback();
        // One observer should be registered on the playing tab to stop playback if translated, and
        // one is registered regardless of playback for refreshing the entrypoint.
        assertEquals(2, mFakeTranslateBridge.getObserverCount());

        // Both should be removed if the tab is destroyed.
        mController.getTabModelTabObserverforTests().onDestroyed(mTab);
        assertEquals(0, mFakeTranslateBridge.getObserverCount());
    }

    @Test
    public void testTranslationListenersUnregisteredBeforeWebContentsSwap() {
        // Listener should be registered already because onTabSelected() is called when
        // TabModelTabObserver is created.
        assertEquals(1, mFakeTranslateBridge.getObserverCount());

        mController.getTabModelTabObserverforTests().webContentsWillSwap(mTab);
        assertEquals(0, mFakeTranslateBridge.getObserverCount());
    }

    @Test
    public void testTranslationListenerRegisteredOnPageLoad() {
        // Listener should be registered already because onTabSelected() is called when
        // TabModelTabObserver is created.
        assertEquals(1, mFakeTranslateBridge.getObserverCount());

        // Destroying tab should remove the observer.
        mController.getTabModelTabObserverforTests().onDestroyed(mTab);
        assertEquals(0, mFakeTranslateBridge.getObserverCount());

        // Do not register in onPageLoadStarted()!
        mController.getTabModelTabObserverforTests().onPageLoadStarted(mTab, sTestGURL);
        assertEquals(0, mFakeTranslateBridge.getObserverCount());

        // Instead register in onContentChanged().
        mController.getTabModelTabObserverforTests().onContentChanged(mTab);
        assertEquals(1, mFakeTranslateBridge.getObserverCount());
    }

    @Test
    public void testTranslationListenersUnregistered_nullWebContents() {
        assertEquals(1, mFakeTranslateBridge.getObserverCount());

        // If tab has null web contents, we should still remove the observer from whatever
        // WebContents it was added to.
        doReturn(null).when(mTab).getWebContents();
        mController.getTabModelTabObserverforTests().onDestroyed(mTab);
        assertEquals(0, mFakeTranslateBridge.getObserverCount());
    }

    @Test
    public void testTranslationListener_tabWebContentsChanged() {
        // An observer is added during ReadAloudController creation through onTabSelected().
        assertEquals(1, mFakeTranslateBridge.getObserverCount(mWebContents));

        // Simulate WebContents changing.
        WebContents otherWebContents = Mockito.mock(WebContents.class);
        mTab.setWebContentsOverrideForTesting(otherWebContents);
        mController.getTabModelTabObserverforTests().onContentChanged(mTab);

        // Observer should have been removed from old WebContents and added to the new one.
        assertEquals(0, mFakeTranslateBridge.getObserverCount(mWebContents));
        assertEquals(1, mFakeTranslateBridge.getObserverCount(otherWebContents));
    }

    @Test
    public void testTranslationListener_unsupportedURLTabSelected() {
        // An observer is added during ReadAloudController creation through onTabSelected().
        assertEquals(1, mFakeTranslateBridge.getObserverCount(mWebContents));

        // Select a different tab with an invalid URL.
        WebContents otherWebContents = Mockito.mock(WebContents.class);
        MockTab tab = mTabModelSelector.addMockTab();
        tab.setWebContentsOverrideForTesting(otherWebContents);
        tab.setUrl(new GURL(""));
        mController.getTabModelTabObserverforTests().onTabSelected(tab);

        // The observer should have been removed from the original WebContents. No need to observe
        // translation on the new tab since it's not readable: the observer will be added on
        // onContentChanged() if the user navigates to a readable page.
        assertEquals(0, mFakeTranslateBridge.getObserverCount(mWebContents));
        assertEquals(0, mFakeTranslateBridge.getObserverCount(otherWebContents));
    }

    @Test
    public void testTranslationListener_playingTabWebContentsChanged() {
        // An observer is added during ReadAloudController creation through onTabSelected().
        assertEquals(1, mFakeTranslateBridge.getObserverCount(mWebContents));

        // Play tab.
        requestAndStartPlayback();
        assertEquals(2, mFakeTranslateBridge.getObserverCount(mWebContents));

        // Switching WebContents of playing tab should remove the "playing tab" translation observer
        // and the "current tab" translation observer since mTab was also the currently selected
        // tab.
        WebContents otherWebContents = Mockito.mock(WebContents.class);
        mTab.setWebContentsOverrideForTesting(otherWebContents);
        mController.getTabModelTabObserverforTests().onContentChanged(mTab);
        assertEquals(0, mFakeTranslateBridge.getObserverCount(mWebContents));
    }

    @Test
    public void testTranslationListener_onTabSelected() {
        // An observer is added during ReadAloudController creation through onTabSelected().
        assertEquals(1, mFakeTranslateBridge.getObserverCount(mWebContents));

        // Select a different tab with a valid URL.
        WebContents otherWebContents = Mockito.mock(WebContents.class);
        MockTab tab = mTabModelSelector.addMockTab();
        tab.setWebContentsOverrideForTesting(otherWebContents);
        tab.setUrl(new GURL("https://some.cool.website/"));
        mController.getTabModelTabObserverforTests().onTabSelected(tab);

        // The observer should have been removed from the original WebContents and the new tab's
        // WebContents should be observed.
        assertEquals(0, mFakeTranslateBridge.getObserverCount(mWebContents));
        assertEquals(1, mFakeTranslateBridge.getObserverCount(otherWebContents));
    }

    @Test
    public void testTranslationListenersRemovedWhenControllerDestroyed() {
        // An observer is added during ReadAloudController creation through onTabSelected().
        assertEquals(1, mFakeTranslateBridge.getObserverCount(mWebContents));

        // Play tab.
        requestAndStartPlayback();
        assertEquals(2, mFakeTranslateBridge.getObserverCount(mWebContents));

        mController.destroy();
        assertEquals(0, mFakeTranslateBridge.getObserverCount(mWebContents));
    }

    @Test
    public void testIsPageTranslated_nullWebContent() {
        mFakeTranslateBridge.setIsPageTranslated(true);

        when(mTab.getWebContents()).thenReturn(null);
        assertFalse(mController.isTranslated(mTab));
    }

    @Test
    public void testIsPageTranslated() {

        mFakeTranslateBridge.setIsPageTranslated(true);
        assertTrue(mController.isTranslated(mTab));
    }

    @Test
    public void testIsTranslatedChangedStopsPlayback() {
        // Play tab.
        requestAndStartPlayback();

        // Trigger isTranslated state changed. Playback should stop.
        mController
                .getTranslationObserverForTest()
                .onIsPageTranslatedChanged(mTab.getWebContents());
        verify(mPlayback).release();
    }

    @Test
    public void testSuccessfulTranslationStopsPlayback() {
        // Play tab.
        requestAndStartPlayback();

        // Finish translating (status code 0 means "no error"). Playback should stop.
        mController.getTranslationObserverForTest().onPageTranslated("en", "es", 0);
        verify(mPlayback).release();
    }

    @Test
    public void testFailedTranslationDoesNotStopPlayback() {
        // Play tab.
        requestAndStartPlayback();

        // Fail to translate (status code 1). Playback should not stop.
        mController.getTranslationObserverForTest().onPageTranslated("en", "es", 1);
        verify(mPlayback, never()).release();
    }

    @Test
    public void testPageTranslatedNotifiesReadabilityChanged() {
        Runnable runnable = Mockito.mock(Runnable.class);
        mController.addReadabilityUpdateListener(runnable);

        var translationObserver = mController.getCurrentTabTranslationObserverForTest();
        translationObserver.onPageTranslated("en", "es", 1);
        verify(runnable, times(1)).run();

        translationObserver.onIsPageTranslatedChanged(null);
        verify(runnable, times(2)).run();
    }

    @Test
    public void testStoppingAnyPlayback() {
        // Play tab.
        requestAndStartPlayback();
        verify(mPlayback).play();

        // request to stop any playback
        mController.maybeStopPlayback(
                null, ReadAloudMetrics.ReasonForStoppingPlayback.NEW_PLAYBACK_REQUEST);
        verify(mPlayback).release();
        verify(mPlayerCoordinator).dismissPlayers();
    }

    @Test
    public void testIsHighlightingSupported_noPlayback() {
        mFakeTranslateBridge.setIsPageTranslated(false);

        assertFalse(mController.isHighlightingSupported());
    }

    @Test
    public void testIsHighlightingSupported_pageTranslated() {
        mFakeTranslateBridge.setIsPageTranslated(true);
        mController.setTimepointsSupportedForTest(mTab.getUrl().getSpec(), true);
        mController.playTab(mTab, ReadAloudController.Entrypoint.MAGIC_TOOLBAR);
        resolvePromises();

        assertFalse(mController.isHighlightingSupported());
    }

    @Test
    public void testIsHighlightingSupported_notSupported() {
        mFakeTranslateBridge.setIsPageTranslated(false);
        mController.setTimepointsSupportedForTest(mTab.getUrl().getSpec(), false);
        mController.playTab(mTab, ReadAloudController.Entrypoint.MAGIC_TOOLBAR);
        resolvePromises();

        assertFalse(mController.isHighlightingSupported());
    }

    @Test
    public void testIsHighlightingSupported_supported() {
        mFakeTranslateBridge.setIsPageTranslated(false);
        mController.setTimepointsSupportedForTest(mTab.getUrl().getSpec(), true);
        mController.playTab(mTab, ReadAloudController.Entrypoint.MAGIC_TOOLBAR);
        resolvePromises();

        assertTrue(mController.isHighlightingSupported());
    }

    @Test
    public void testReadabilitySupplier() {
        String testUrl = "https://en.wikipedia.org/wiki/Google";

        Runnable runnable = Mockito.mock(Runnable.class);
        mController.addReadabilityUpdateListener(runnable);
        mTab.setGurlOverrideForTesting(new GURL(testUrl));
        mController.maybeCheckReadability(mTab);

        verify(mHooksImpl, times(1)).isPageReadable(eq(testUrl), mCallbackCaptor.capture());

        mCallbackCaptor.getValue().onSuccess(testUrl, true, false);
        verify(runnable).run();
    }

    @Test
    public void testMetricRecorded_isReadable() {
        final String histogramName = ReadAloudMetrics.IS_READABLE;

        var histogram = HistogramWatcher.newSingleRecordWatcher(histogramName, true);
        mController.maybeCheckReadability(mTab);
        verify(mHooksImpl, times(1))
                .isPageReadable(eq(sTestGURL.getSpec()), mCallbackCaptor.capture());
        mCallbackCaptor.getValue().onSuccess(sTestGURL.getSpec(), true, false);
        histogram.assertExpected();

        histogram = HistogramWatcher.newSingleRecordWatcher(histogramName, false);
        mCallbackCaptor.getValue().onSuccess(sTestGURL.getSpec(), false, false);
        histogram.assertExpected();
    }

    @Test
    public void testMetricRecorded_readabilitySuccessful() {
        final String histogramName = ReadAloudMetrics.READABILITY_SUCCESS;

        var histogram = HistogramWatcher.newSingleRecordWatcher(histogramName, true);
        mController.maybeCheckReadability(mTab);
        verify(mHooksImpl, times(1))
                .isPageReadable(eq(sTestGURL.getSpec()), mCallbackCaptor.capture());
        mCallbackCaptor.getValue().onSuccess(sTestGURL.getSpec(), true, false);
        histogram.assertExpected();

        histogram = HistogramWatcher.newSingleRecordWatcher(histogramName, false);
        mController.maybeCheckReadability(mTab);
        verify(mHooksImpl, times(1))
                .isPageReadable(eq(sTestGURL.getSpec()), mCallbackCaptor.capture());
        mCallbackCaptor
                .getValue()
                .onFailure(sTestGURL.getSpec(), new Throwable("Something went wrong"));
        histogram.assertExpected();
    }

    @Test
    public void testMetricRecorded_serverReadability() {
        final String histogramName = ReadAloudMetrics.READABILITY_SERVER_SIDE;

        var histogram = HistogramWatcher.newSingleRecordWatcher(histogramName, true);
        mController.maybeCheckReadability(mTab);
        verify(mHooksImpl).isPageReadable(eq(sTestGURL.getSpec()), mCallbackCaptor.capture());
        mCallbackCaptor
                .getValue()
                .onSuccess(
                        sTestGURL.getSpec(),
                        /* isReadable= */ true,
                        /* timepointsSupported= */ false);
        histogram.assertExpected();

        histogram = HistogramWatcher.newSingleRecordWatcher(histogramName, false);
        mCallbackCaptor
                .getValue()
                .onSuccess(
                        sTestGURL.getSpec(),
                        /* isReadable= */ false,
                        /* timepointsSupported= */ false);
        histogram.assertExpected();

        // nothing should be emitted on error
        histogram = HistogramWatcher.newBuilder().expectNoRecords(histogramName).build();
        mController.maybeCheckReadability(mTab);
        verify(mHooksImpl, times(1))
                .isPageReadable(eq(sTestGURL.getSpec()), mCallbackCaptor.capture());
        mCallbackCaptor
                .getValue()
                .onFailure(sTestGURL.getSpec(), new Throwable("Something went wrong"));
        histogram.assertExpected();
    }

    @Test
    @DisableFeatures(ChromeFeatureList.READALOUD_PLAYBACK)
    public void testReadAloudPlaybackFlagCheckedAfterReadability() {
        mController.maybeCheckReadability(mTab);
        verify(mHooksImpl, times(1))
                .isPageReadable(eq(sTestGURL.getSpec()), mCallbackCaptor.capture());
        mCallbackCaptor.getValue().onSuccess(sTestGURL.getSpec(), true, false);

        assertFalse(mController.isReadable(mTab));
    }

    @Test
    @EnableFeatures(ChromeFeatureList.READALOUD_BACKGROUND_PLAYBACK)
    public void testBackgroundPlaybackContinuesWhenActivityPaused() {
        // Play tab.
        requestAndStartPlayback();
        // set progress
        var data = Mockito.mock(PlaybackData.class);

        doReturn(2).when(data).paragraphIndex();
        doReturn(1000000L).when(data).positionInParagraphNanos();
        mController.onPlaybackDataChanged(data);

        // App is backgrounded with the screen on. Playback should continue if the flag is on.
        setIsScreenOnAndUnlocked(true);
        mController.onApplicationStateChange(ApplicationState.HAS_PAUSED_ACTIVITIES);
        verify(mPlayback, never()).release();
        // also the screen is still on, don't notify about screen state change
        verify(mPlayerCoordinator, never()).onScreenStatusChanged(anyBoolean());

        // now turn the screen off.
        setIsScreenOnAndUnlocked(false);
        mController.onApplicationStateChange(ApplicationState.HAS_STOPPED_ACTIVITIES);
        verify(mPlayback, never()).release();
        // also the screen is still on, don't notify about screen state change
        verify(mPlayerCoordinator).onScreenStatusChanged(true);
    }

    @Test
    @EnableFeatures(ChromeFeatureList.READALOUD_BACKGROUND_PLAYBACK)
    public void testBackgroundPlayback_doesntCrashWhenNoPlayer() throws NullPointerException {
        setIsScreenOnAndUnlocked(false);
        mController.onApplicationStateChange(ApplicationState.HAS_STOPPED_ACTIVITIES);
    }

    @Test
    public void testPlaybackStopsAndStateSavedWhenAppBackgrounded_screenOn() {
        // Play tab.
        requestAndStartPlayback();
        // set progress
        var data = Mockito.mock(PlaybackData.class);

        doReturn(2).when(data).paragraphIndex();
        doReturn(1000000L).when(data).positionInParagraphNanos();
        mController.onPlaybackDataChanged(data);

        // App is backgrounded with the screen on. Make sure playback stops.
        setIsScreenOnAndUnlocked(true);
        mController.onApplicationStateChange(ApplicationState.HAS_STOPPED_ACTIVITIES);
        verify(mPlayback).release();
        reset(mPlayback);
        when(mPlayback.getMetadata()).thenReturn(mMetadata);

        // Activity goes back in foreground. Restore progress.
        mController.onActivityStateChange(mActivity, ActivityState.RESUMED);
        verify(mPlaybackHooks, times(2)).createPlayback(any(), mPlaybackCallbackCaptor.capture());
        onPlaybackSuccess(mPlayback);
        verify(mPlayback).seekToParagraph(2, 1000000L);
        verify(mPlayback, never()).play();

        // once saved state is restored, it's cleared and no further interactions with playback
        // should happen.
        resetPlaybackMocks();

        mController.onApplicationStateChange(ApplicationState.HAS_PAUSED_ACTIVITIES);
        mController.onApplicationStateChange(ApplicationState.HAS_RUNNING_ACTIVITIES);
        verifyNoInteractions(mPlaybackHooks);
        verifyNoInteractions(mPlayback);
    }

    @Test
    public void testPlaybackWhenAppStops_screenOff() {
        // Play tab.
        requestAndStartPlayback();
        // set progress
        var data = Mockito.mock(PlaybackData.class);

        doReturn(2).when(data).paragraphIndex();
        doReturn(1000000L).when(data).positionInParagraphNanos();
        mController.onPlaybackDataChanged(data);

        // App is backgrounded when the screen is off. Playback should keep playing.
        setIsScreenOnAndUnlocked(false);
        mController.onApplicationStateChange(ApplicationState.HAS_STOPPED_ACTIVITIES);
        verify(mPlayback, never()).release();
    }

    @Test
    public void testPlaybackWhenAppStops_userHint() {
        // Play tab.
        requestAndStartPlayback();
        // set progress
        var data = Mockito.mock(PlaybackData.class);

        doReturn(2).when(data).paragraphIndex();
        doReturn(1000000L).when(data).positionInParagraphNanos();
        mController.onPlaybackDataChanged(data);

        // App is backgrounded. Screen is off but there is user hint present - stop playback
        mController.onUserLeaveHint();
        setIsScreenOnAndUnlocked(false);
        mController.onApplicationStateChange(ApplicationState.HAS_STOPPED_ACTIVITIES);

        verify(mPlayback).release();
        resetPlaybackMocks();

        // App goes back in foreground. Restore progress.
        mController.onActivityStateChange(mActivity, ActivityState.RESUMED);
        verify(mPlaybackHooks).createPlayback(any(), mPlaybackCallbackCaptor.capture());
        onPlaybackSuccess(mPlayback);
        verify(mPlayback).seekToParagraph(2, 1000000L);
        verify(mPlayback, never()).play();
    }

    private void setIsScreenOnAndUnlocked(boolean isScreenOnAndUnlocked) {
        DeviceConditions deviceConditions =
                new DeviceConditions(
                        /* powerConnected= */ false,
                        /* batteryPercentage= */ 75,
                        ConnectionType.CONNECTION_UNKNOWN,
                        /* powerSaveOn= */ false,
                        /* activeNetworkMetered= */ false,
                        isScreenOnAndUnlocked);
        ShadowDeviceConditions.setCurrentConditions(deviceConditions);
    }

    @Test
    public void testPlaybackResumesWhenActivityResumes() {
        // Play tab.
        requestAndStartPlayback();
        // set progress
        var data = Mockito.mock(PlaybackData.class);

        doReturn(2).when(data).paragraphIndex();
        doReturn(1000000L).when(data).positionInParagraphNanos();
        mController.onPlaybackDataChanged(data);

        // App is backgrounded with the screen on. Make sure playback stops.
        setIsScreenOnAndUnlocked(true);
        mController.onApplicationStateChange(ApplicationState.HAS_STOPPED_ACTIVITIES);
        verify(mPlayback).release();
        resetPlaybackMocks();

        // App returns to foreground, but activity hasn't resumed yet.
        mController.onApplicationStateChange(ApplicationState.HAS_RUNNING_ACTIVITIES);
        verify(mPlaybackHooks, never()).createPlayback(any(), any());

        // Activity goes back in foreground. Restore progress.
        mController.onActivityStateChange(mActivity, ActivityState.RESUMED);
        verify(mPlaybackHooks).createPlayback(any(), mPlaybackCallbackCaptor.capture());
        onPlaybackSuccess(mPlayback);
        verify(mPlayback).seekToParagraph(2, 1000000L);
        verify(mPlayback, never()).play();
    }

    @Test
    @EnableFeatures(ChromeFeatureList.READALOUD_BACKGROUND_PLAYBACK)
    public void testPlaybackResumesWhenActivityResumes_backgroundPlaybackEnabled() {
        // Play tab.
        requestAndStartPlayback();
        // set progress
        var data = Mockito.mock(PlaybackData.class);

        doReturn(2).when(data).paragraphIndex();
        doReturn(1000000L).when(data).positionInParagraphNanos();
        mController.onPlaybackDataChanged(data);

        // App is backgrounded with the screen on. Playback should not stop.
        setIsScreenOnAndUnlocked(true);
        mController.onApplicationStateChange(ApplicationState.HAS_STOPPED_ACTIVITIES);
        resetPlaybackMocks();

        // Activity goes back in foreground. Nothing should be restored; playback was never stopped
        // in the first place.
        mController.onActivityStateChange(mActivity, ActivityState.RESUMED);
        verifyNoInteractions(mPlayback);
        verifyNoInteractions(mPlaybackHooks);
    }

    @Test
    public void testMetricRecorded_eligibility() {
        final String histogramName = ReadAloudMetrics.IS_USER_ELIGIBLE;

        var histogram = HistogramWatcher.newSingleRecordWatcher(histogramName, true);
        mController.onProfileAvailable(mMockProfile);
        histogram.assertExpected();

        histogram = HistogramWatcher.newSingleRecordWatcher(histogramName, false);
        when(mPrefService.getBoolean("readaloud.listen_to_this_page_enabled")).thenReturn(false);
        mController.onProfileAvailable(mMockProfile);
        histogram.assertExpected();
    }

    @Test
    public void testMetricRecorded_ineligibilityReason() {
        final String histogramName = ReadAloudMetrics.INELIGIBILITY_REASON;

        var histogram =
                HistogramWatcher.newSingleRecordWatcher(
                        histogramName, IneligibilityReason.POLICY_DISABLED);
        when(mPrefService.getBoolean("readaloud.listen_to_this_page_enabled")).thenReturn(false);
        mController.onProfileAvailable(mMockProfile);
        histogram.assertExpected();
        when(mPrefService.getBoolean("readaloud.listen_to_this_page_enabled")).thenReturn(true);

        histogram =
                HistogramWatcher.newSingleRecordWatcher(
                        histogramName, IneligibilityReason.DEFAULT_SEARCH_ENGINE_GOOGLE_FALSE);
        doReturn(SearchEngineType.SEARCH_ENGINE_OTHER)
                .when(mTemplateUrlService)
                .getSearchEngineTypeFromTemplateUrl(anyString());
        mController.onProfileAvailable(mMockProfile);
        histogram.assertExpected();
    }

    @Test
    public void testMetricRecorded_isPlaybackCreationSuccessful_True() {
        final String histogramName = ReadAloudMetrics.IS_TAB_PLAYBACK_CREATION_SUCCESSFUL;

        var histogram = HistogramWatcher.newSingleRecordWatcher(histogramName, true);
        requestAndStartPlayback();
        histogram.assertExpected();
    }

    @Test
    public void testMetricRecorded_isPlaybackCreationSuccessful_False() {
        final String histogramName = ReadAloudMetrics.IS_TAB_PLAYBACK_CREATION_SUCCESSFUL;

        var histogram = HistogramWatcher.newSingleRecordWatcher(histogramName, false);
        mController.playTab(mTab, ReadAloudController.Entrypoint.MAGIC_TOOLBAR);
        resolvePromises();
        verify(mPlaybackHooks).createPlayback(Mockito.any(), mPlaybackCallbackCaptor.capture());
        mPlaybackCallbackCaptor.getValue().onFailure(new Exception("Very bad error"));
        resolvePromises();
        histogram.assertExpected();
    }

    @Test
    public void testMetricRecorded_playbackWithoutReadabilityCheck() {
        final String histogramName = ReadAloudMetrics.TAB_PLAYBACK_WITHOUT_READABILITY_CHECK_ERROR;
        var histogram =
                HistogramWatcher.newSingleRecordWatcher(
                        histogramName, ReadAloudController.Entrypoint.OVERFLOW_MENU);

        mController.playTab(mTab, ReadAloudController.Entrypoint.OVERFLOW_MENU);
        histogram.assertExpected();
    }

    @Test
    public void testMetricRecorded_playbackSuccess() {
        final String histogramName = ReadAloudMetrics.TAB_PLAYBACK_CREATION_SUCCESS;
        var histogram =
                HistogramWatcher.newSingleRecordWatcher(
                        histogramName, ReadAloudController.Entrypoint.MAGIC_TOOLBAR);

        requestAndStartPlayback();

        histogram.assertExpected();
    }

    @Test
    public void testMetricRecorded_playbackFailure() {
        final String histogramName = ReadAloudMetrics.TAB_PLAYBACK_CREATION_FAILURE;
        var histogram =
                HistogramWatcher.newSingleRecordWatcher(
                        histogramName, ReadAloudController.Entrypoint.OVERFLOW_MENU);
        // Play tab to set up playbackhooks
        mController.playTab(mTab, ReadAloudController.Entrypoint.OVERFLOW_MENU);
        resolvePromises();
        verify(mPlaybackHooks, times(1))
                .createPlayback(Mockito.any(), mPlaybackCallbackCaptor.capture());
        mPlaybackCallbackCaptor.getValue().onFailure(new Exception("Very bad error"));
        resolvePromises();

        histogram.assertExpected();
    }

    @Test
    public void testMetricNotRecorded_isPlaybackCreationSuccessful() {
        final String histogramName = ReadAloudMetrics.IS_TAB_PLAYBACK_CREATION_SUCCESSFUL;
        var histogram = HistogramWatcher.newBuilder().expectNoRecords(histogramName).build();

        // Play tab to set up playbackhooks
        mController.playTab(mTab, ReadAloudController.Entrypoint.MAGIC_TOOLBAR);
        resolvePromises();

        // Preview a voice.
        var voice = new PlaybackVoice("en", "asdf", "");
        doReturn(List.of(voice)).when(mPlaybackHooks).getVoicesFor(anyString());
        doReturn(List.of(voice)).when(mPlaybackHooks).getPlaybackVoiceList(any());
        mController.previewVoice(voice);

        histogram.assertExpected();
    }

    @Test
    public void testMetricRecorded_playbackStarted() {
        final String actionName = "ReadAloud.PlaybackStarted";
        ReadAloudMetrics.recordPlaybackStarted();
        assertThat(mUserActionTester.getActions(), hasItems(actionName));
    }

    @Test
    public void testMetricRecorded_highlightingEnabledOnStartup() {
        mHighlightingEnabledOnStartupHistogram.assertExpected();
    }

    @Test
    public void testMetricRecorded_highlightingSupported_true() {
        final String histogramName = "ReadAloud.HighlightingSupported";
        var histogram = HistogramWatcher.newSingleRecordWatcher(histogramName, true);

        mController.playTab(mTab, ReadAloudController.Entrypoint.MAGIC_TOOLBAR);
        resolvePromises();

        verify(mPlaybackHooks, times(1))
                .createPlayback(Mockito.any(), mPlaybackCallbackCaptor.capture());

        mFakeTranslateBridge.setIsPageTranslated(false);
        mController.setTimepointsSupportedForTest(mTab.getUrl().getSpec(), true);
        onPlaybackSuccess(mPlayback);

        histogram.assertExpected();
    }

    @Test
    public void testMetricRecorded_highlightingSupported_false() {
        final String histogramName = "ReadAloud.HighlightingSupported";
        var histogram = HistogramWatcher.newSingleRecordWatcher(histogramName, false);

        mController.playTab(mTab, ReadAloudController.Entrypoint.MAGIC_TOOLBAR);
        resolvePromises();

        verify(mPlaybackHooks, times(1))
                .createPlayback(Mockito.any(), mPlaybackCallbackCaptor.capture());

        mFakeTranslateBridge.setIsPageTranslated(false);
        mController.setTimepointsSupportedForTest(mTab.getUrl().getSpec(), false);
        onPlaybackSuccess(mPlayback);

        histogram.assertExpected();
    }

    @Test
    public void testNavigateToPlayingTab() {
        // Play tab.
        requestAndStartPlayback();

        MockTab newTab = mTabModelSelector.addMockTab();
        mTabModelSelector
                .getModel(false)
                .setIndex(
                        mTabModelSelector.getModel(false).indexOf(newTab),
                        TabSelectionType.FROM_USER);
        // check that we switched to new tab
        assertEquals(mTabModelSelector.getCurrentTab(), newTab);

        // navigate
        mController.navigateToPlayingTab();

        // should switch back to original one
        assertEquals(mTabModelSelector.getCurrentTab(), mTab);

        // navigate
        mController.navigateToPlayingTab();

        // should still be on the playing tab
        assertEquals(mTabModelSelector.getCurrentTab(), mTab);
    }

    @Test
    public void testInitClearsStaleSyntheticTrialPrefs() {
        verify(mReadAloudFeaturesNatives, times(1)).clearStaleSyntheticTrialPrefs();
    }

    @Test
    public void testKnownReadableTrialInit() {
        // ReadAloudController creation should init the trial.
        verify(mReadAloudFeaturesNatives, times(1))
                .initSyntheticTrial(eq(ChromeFeatureList.READALOUD), eq("_KnownReadable"));
    }

    @Test
    public void testKnownReadableTrialActivate() {
        mController.maybeCheckReadability(mTab);
        verify(mHooksImpl, times(1))
                .isPageReadable(eq(sTestGURL.getSpec()), mCallbackCaptor.capture());
        // Page is readable so activate the trial.
        mCallbackCaptor.getValue().onSuccess(sTestGURL.getSpec(), true, false);
        verify(mReadAloudFeaturesNatives, times(1))
                .activateSyntheticTrial(eq(KNOWN_READABLE_TRIAL_PTR));

        // Subsequent readability checks may cause activateSyntheticTrial() to be called again
        // (though it has no effect after the first call).
        mCallbackCaptor.getValue().onSuccess(sTestGURL.getSpec(), true, false);
        verify(mReadAloudFeaturesNatives, times(2))
                .activateSyntheticTrial(eq(KNOWN_READABLE_TRIAL_PTR));
    }

    @Test
    public void testKnownReadableTrialDoesNotActivateIfNotReadable() {
        mController.maybeCheckReadability(mTab);
        verify(mHooksImpl, times(1))
                .isPageReadable(eq(sTestGURL.getSpec()), mCallbackCaptor.capture());
        // Page is not readable so do not activate the trial.
        mCallbackCaptor.getValue().onSuccess(sTestGURL.getSpec(), false, false);
        verify(mReadAloudFeaturesNatives, never()).activateSyntheticTrial(anyLong());
    }

    @Test
    @DisableFeatures(ChromeFeatureList.READALOUD_PLAYBACK)
    public void testKnownReadableTrialCanActivateWithoutPlaybackFlag() {
        mController.maybeCheckReadability(mTab);
        verify(mHooksImpl, times(1))
                .isPageReadable(eq(sTestGURL.getSpec()), mCallbackCaptor.capture());
        // Page is readable so activate the trial.
        mCallbackCaptor.getValue().onSuccess(sTestGURL.getSpec(), true, false);
        verify(mReadAloudFeaturesNatives, times(1))
                .activateSyntheticTrial(eq(KNOWN_READABLE_TRIAL_PTR));
    }

    @Test
    public void testDestroy() {
        // Play tab
        requestAndStartPlayback();

        // Destroy should clean up playback, UI, synthetic trials, and more
        mController.destroy();
        verify(mPlayback).release();
        verify(mPlayerCoordinator).destroy();
        verify(mReadAloudFeaturesNatives).destroySyntheticTrial(eq(KNOWN_READABLE_TRIAL_PTR));
    }

    @Test
    public void testMaybeShowPlayer() {
        // no playback, request is a no op
        mController.maybeShowPlayer();

        verify(mPlayerCoordinator, never()).restorePlayers();

        requestAndStartPlayback();
        mController.maybeShowPlayer();

        verify(mPlayerCoordinator).restorePlayers();
    }

    @Test
    public void testMaybeHideMiniPlayer() {
        // no playback, request is a no op
        mController.maybeHidePlayer();

        verify(mPlayerCoordinator, never()).hidePlayers();

        requestAndStartPlayback();
        mController.maybeHidePlayer();

        verify(mPlayerCoordinator).hidePlayers();
    }

    @Test
    public void testPauseAndHideOnIncognitoTabSelected() {
        requestAndStartPlayback();

        Tab tab = mTabModelSelector.addMockIncognitoTab();
        TabModelUtils.selectTabById(mTabModelSelector, tab.getId(), TabSelectionType.FROM_NEW);

        verify(mPlayback).pause();
        verify(mPlayerCoordinator).hidePlayers();
    }

    @Test
    public void testRestorePlayerOnReturnFromIncognitoTab() {
        requestAndStartPlayback();
        reset(mPlayback);

        Tab tab = mTabModelSelector.addMockIncognitoTab();
        TabModelUtils.selectTabById(mTabModelSelector, tab.getId(), TabSelectionType.FROM_NEW);

        verify(mPlayback).pause();
        verify(mPlayerCoordinator).hidePlayers();

        TabModelUtils.selectTabById(mTabModelSelector, mTab.getId(), TabSelectionType.FROM_USER);
        verify(mPlayback, never()).play();
        verify(mPlayerCoordinator).restorePlayers();
    }

    @Test
    @EnableFeatures(ChromeFeatureList.READALOUD_BACKGROUND_PLAYBACK)
    public void testCrossActivityPlayback_stopBackgroundPlayback() {
        // Play in Chrome, then play in CCT. Chrome playback should stop only when CCT plays.

        // Create a second instance of ReadAloudController to simulate other app's CCT.
        mController2 = createController();

        // Play in Chrome. requestAndStartPlayback() verifies playback started.
        requestAndStartPlayback();

        resetPlaybackMocks();

        // Simulate backgrounding the activity, and the entire app. Playback should not stop.
        mController.onActivityStateChange(mActivity, ActivityState.STOPPED);
        mController.onApplicationStateChange(ApplicationState.HAS_STOPPED_ACTIVITIES);
        verifyNoInteractions(mPlayback);

        // Play in CCT.
        var histogram =
                HistogramWatcher.newSingleRecordWatcher(
                        ReadAloudMetrics.REASON_FOR_STOPPING_PLAYBACK,
                        ReadAloudMetrics.ReasonForStoppingPlayback.EXTERNAL_PLAYBACK_REQUEST);
        mController2.playTab(mTab, ReadAloudController.Entrypoint.MAGIC_TOOLBAR);
        resolvePromises();

        // Chrome playback should stop. Reason should be recorded as EXTERNAL_PLAYBACK_REQUEST.
        verify(mPlayback).release();
        histogram.assertExpected();

        // CCT playback should start.
        verify(mPlaybackHooks, times(1))
                .createPlayback(Mockito.any(), mPlaybackCallbackCaptor.capture());
        onPlaybackSuccess(mPlayback);
        verify(mPlayerCoordinator, times(1))
                .playbackReady(eq(mPlayback), eq(PlaybackListener.State.PLAYING));
    }

    @Test
    @EnableFeatures(ChromeFeatureList.READALOUD_BACKGROUND_PLAYBACK)
    public void testCrossActivityPlayback_canRestoreIfSameTab() {
        // Play in Chrome, play in CCT, then request playback for original tab in Chrome. Playback
        // should be restored.

        // Create a second instance of ReadAloudController to simulate other app's CCT.
        mController2 = createController();

        // Play in Chrome. requestAndStartPlayback() verifies playback started.
        requestAndStartPlayback();

        // Simulate some progress.
        var data = Mockito.mock(PlaybackData.class);
        doReturn(2).when(data).paragraphIndex();
        doReturn(1000000L).when(data).positionInParagraphNanos();
        mController.onPlaybackDataChanged(data);

        resetPlaybackMocks();

        // Play in CCT.
        mController2.playTab(mTab, ReadAloudController.Entrypoint.MAGIC_TOOLBAR);
        resolvePromises();
        verify(mPlayerCoordinator).setPlayerRestorable(true);

        // Chrome playback should stop.
        verify(mPlayback).release();

        // CCT playback should start.
        verify(mPlaybackHooks, times(1))
                .createPlayback(Mockito.any(), mPlaybackCallbackCaptor.capture());
        onPlaybackSuccess(mPlayback);
        verify(mPlayerCoordinator, times(1))
                .playbackReady(eq(mPlayback), eq(PlaybackListener.State.PLAYING));

        resetPlaybackMocks();

        // Return to Chrome. CCT playback should not stop.
        mController.onActivityStateChange(mActivity, ActivityState.RESUMED);
        verifyNoInteractions(mPlayback);

        // Tap an entrypoint on the same tab in Chrome. CCT should stop playing and playback should
        // be restored (paused).
        mController.playTab(mTab, ReadAloudController.Entrypoint.MAGIC_TOOLBAR);
        // Release CCT playback
        verify(mPlayback).release();
        // Simulate successful playback creation.
        verify(mPlaybackHooks).createPlayback(any(), mPlaybackCallbackCaptor.capture());
        onPlaybackSuccess(mPlayback);
        // Progress should be restored and play should not have been called.
        verify(mPlayback).seekToParagraph(2, 1000000L);
        verify(mPlayback, never()).play();
    }

    @Test
    public void testRestorePlayback() {
        // Play in Chrome, play in CCT, then request to restore playback for original Chrome.
        // Playback
        // should be restored.

        // Create a second instance of ReadAloudController to simulate other app's CCT.
        mController2 = createController();

        // Play in Chrome. requestAndStartPlayback() verifies playback started.
        requestAndStartPlayback();

        // Simulate some progress.
        var data = Mockito.mock(PlaybackData.class);
        doReturn(2).when(data).paragraphIndex();
        doReturn(1000000L).when(data).positionInParagraphNanos();
        mController.onPlaybackDataChanged(data);

        resetPlaybackMocks();

        // Play in CCT.
        mController2.playTab(mTab, ReadAloudController.Entrypoint.MAGIC_TOOLBAR);
        resolvePromises();

        // Chrome playback should stop.
        verify(mPlayback).release();

        // CCT playback should start.
        verify(mPlaybackHooks, times(1))
                .createPlayback(Mockito.any(), mPlaybackCallbackCaptor.capture());
        onPlaybackSuccess(mPlayback);
        verify(mPlayerCoordinator, times(1))
                .playbackReady(eq(mPlayback), eq(PlaybackListener.State.PLAYING));

        resetPlaybackMocks();

        // Return to Chrome. CCT playback should not stop.
        mController.onActivityStateChange(mActivity, ActivityState.RESUMED);
        verifyNoInteractions(mPlayback);

        // Request to restore playback. Called by the PlayerMediator when the play pause button is
        // clicked on a UI without playback.
        mController.restorePlayback();
        // Simulate successful playback creation.
        verify(mPlaybackHooks).createPlayback(any(), mPlaybackCallbackCaptor.capture());
        onPlaybackSuccess(mPlayback);
        // Progress should be restored and play should not have been called.
        verify(mPlayback).seekToParagraph(2, 1000000L);
        verify(mPlayback, never()).play();
    }

    @Test
    @EnableFeatures(ChromeFeatureList.READALOUD_BACKGROUND_PLAYBACK)
    public void testCrossActivityPlayback_doNotRestoreIfDifferentTab() {
        // Play in Chrome, play in CCT, then request playback for a different tab in Chrome. A new
        // playback should start and the old one should not be restored.

        // Create a second instance of ReadAloudController to simulate other app's CCT.
        mController2 = createController();

        // Play in Chrome. requestAndStartPlayback() verifies playback started.
        requestAndStartPlayback();

        // Simulate some progress.
        var data = Mockito.mock(PlaybackData.class);
        doReturn(2).when(data).paragraphIndex();
        doReturn(1000000L).when(data).positionInParagraphNanos();
        mController.onPlaybackDataChanged(data);

        resetPlaybackMocks();

        // Play in CCT.
        mController2.playTab(mTab, ReadAloudController.Entrypoint.MAGIC_TOOLBAR);
        resolvePromises();

        // Chrome playback should stop.
        verify(mPlayback).release();

        // CCT playback should start.
        verify(mPlaybackHooks, times(1))
                .createPlayback(Mockito.any(), mPlaybackCallbackCaptor.capture());
        onPlaybackSuccess(mPlayback);
        verify(mPlayerCoordinator, times(1))
                .playbackReady(eq(mPlayback), eq(PlaybackListener.State.PLAYING));

        resetPlaybackMocks();

        // Return to Chrome. CCT playback should not stop.
        mController.onActivityStateChange(mActivity, ActivityState.RESUMED);
        verifyNoInteractions(mPlayback);

        // Tap an entrypoint on a different tab in Chrome.
        MockTab tab = mTabModelSelector.addMockTab();
        tab.setGurlOverrideForTesting(sTestGURL);
        tab.setWebContentsOverrideForTesting(mWebContents);
        mController.playTab(tab, ReadAloudController.Entrypoint.MAGIC_TOOLBAR);
        resolvePromises();
        // CCT playback was released
        verify(mPlayback).release();
        // Simulate successful playback creation and make sure playback starts.
        verify(mPlaybackHooks).createPlayback(any(), mPlaybackCallbackCaptor.capture());
        onPlaybackSuccess(mPlayback);
        verify(mPlayback).play();

        // Make sure saved state was not restored and was instead cleared.
        verify(mPlayback, never()).seekToParagraph(eq(2), eq(1000000L));
        resetPlaybackMocks();
        mController.onActivityStateChange(mActivity, ActivityState.RESUMED);
        verifyNoInteractions(mPlaybackHooks);
        verifyNoInteractions(mPlayback);
    }

    // TODO(b/322052505): This test won't be necessary if we keep track of profile changes.
    @Test
    public void testNoRequestsIfProfileDestroyed() {
        reset(mHooksImpl);
        doReturn(false).when(mMockProfile).isNativeInitialized();
        mController = createController();
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();

        // Check readability.
        mController.maybeCheckReadability(mTab);
        // No readability request should be made.
        verify(mHooksImpl, never()).isPageReadable(any(), any());

        // Try playing the tab.
        mFakeTranslateBridge.setCurrentLanguage("en");
        mTab.setGurlOverrideForTesting(new GURL("https://en.wikipedia.org/wiki/Google"));
        mController.playTab(mTab, ReadAloudController.Entrypoint.MAGIC_TOOLBAR);
        resolvePromises();
        // No playback request should be made.
        verify(mPlaybackHooks, never()).createPlayback(any(), any());
    }

    @Test
    public void testPause_notPlayingTab() {
        mController.pause();
        // Not currently playing, so nothing should happen.
        verify(mPlayback, never()).pause();
    }

    @Test
    public void testPause_alreadyStopped() {
        requestAndStartPlayback();
        var data = Mockito.mock(PlaybackListener.PlaybackData.class);
        doReturn(PlaybackListener.State.STOPPED).when(data).state();
        mController.onPlaybackDataChanged(data);

        mController.pause();
        // Not currently playing, so nothing should happen.
        verify(mPlayback, never()).pause();
    }

    @Test
    public void testPause() {
        requestAndStartPlayback();
        var data = Mockito.mock(PlaybackListener.PlaybackData.class);
        doReturn(PlaybackListener.State.PLAYING).when(data).state();
        mController.onPlaybackDataChanged(data);

        mController.pause();
        verify(mPlayback).pause();
    }

    @Test
    public void testMaybePauseForOutgoingIntent_pause() {
        // Play.
        requestAndStartPlayback();
        var data = Mockito.mock(PlaybackListener.PlaybackData.class);
        doReturn(PlaybackListener.State.PLAYING).when(data).state();
        mController.onPlaybackDataChanged(data);

        // Simulate select-to-speak context menu click. Playback should pause.
        Intent intent = new Intent();
        intent.setAction(Intent.ACTION_PROCESS_TEXT);
        mController.maybePauseForOutgoingIntent(intent);
        verify(mPlayback).pause();
    }

    @Test
    public void testMaybePauseForOutgoingIntent_noPause() {
        // Play.
        requestAndStartPlayback();
        var data = Mockito.mock(PlaybackListener.PlaybackData.class);
        doReturn(PlaybackListener.State.PLAYING).when(data).state();
        mController.onPlaybackDataChanged(data);

        // Simulate some unimportant context menu click. Playback should not pause.
        Intent intent = new Intent();
        intent.setAction(Intent.ACTION_DEFINE);
        mController.maybePauseForOutgoingIntent(intent);
        verify(mPlayback, never()).pause();
    }

    @Test
    public void testPlayTabWithDateExtraction() {
        requestAndStartPlayback();
        verify(mPlayerCoordinator).addObserver(mController);

        verify(mPlaybackHooks, times(1)).createPlayback(mPlaybackArgsCaptor.capture(), any());

        assertEquals(1234567123456L, mPlaybackArgsCaptor.getValue().getDateModifiedMsSinceEpoch());
    }

    @Test
    public void testLogDateExtraction_hasDateModified() {
        mFakeTranslateBridge.setCurrentLanguage("en");
        var histogram = HistogramWatcher.newSingleRecordWatcher("ReadAloud.HasDateModified", true);
        requestAndStartPlayback();
        histogram.assertExpected();
    }

    @Test
    public void testLogDateExtraction_noDateModified() {
        mFakeTranslateBridge.setCurrentLanguage("en");
        var failedPromise = new Promise<Long>();
        when(mExtractor.getDateModified(any())).thenReturn(failedPromise);
        failedPromise.reject(new Exception(""));

        var histogram = HistogramWatcher.newSingleRecordWatcher("ReadAloud.HasDateModified", false);
        requestAndStartPlayback();
        histogram.assertExpected();
    }

    @Test
    public void testIsPlayingCurrentTab() {
        // should be false at first since currentlyPlayingTab is null
        assertFalse(mController.isPlayingCurrentTab());
        // set to playing tab
        requestAndStartPlayback();
        assertTrue(mController.isPlayingCurrentTab());
        // should be false after switching to a non playing tab
        MockTab newTab = mTabModelSelector.addMockTab();
        mTabModelSelector
                .getModel(false)
                .setIndex(
                        mTabModelSelector.getModel(false).indexOf(newTab),
                        TabSelectionType.FROM_USER);
        assertFalse(mController.isPlayingCurrentTab());
        // switch back to current tab
        mTabModelSelector
                .getModel(false)
                .setIndex(
                        mTabModelSelector.getModel(false).indexOf(mTab),
                        TabSelectionType.FROM_USER);
        assertTrue(mController.isPlayingCurrentTab());
        // back to null after stopping playback
        mController.maybeStopPlayback(
                mTab, ReadAloudMetrics.ReasonForStoppingPlayback.NEW_PLAYBACK_REQUEST);
        assertFalse(mController.isPlayingCurrentTab());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.READALOUD_TAP_TO_SEEK)
    public void testTapToSeek() {
        // play tab
        requestAndStartPlayback();
        verify(mPlayback).addListener(mPlaybackListenerCaptor.capture());
        // update playback data so it isn't null
        var data = Mockito.mock(PlaybackListener.PlaybackData.class);
        doReturn(PlaybackListener.State.PLAYING).when(data).state();
        mPlaybackListenerCaptor.getValue().onPlaybackDataChanged(data);
        var histogram =
                HistogramWatcher.newSingleRecordWatcher(ReadAloudMetrics.TAP_TO_SEEK_TIME, 12);
        when(mMetadata.fullText())
                .thenAnswer(
                        invocation -> {
                            mClock.advanceCurrentTimeMillis(12);
                            return "the quick brown fox jumps over the lazy dog";
                        });
        PlaybackTextPart p =
                new PlaybackTextPart() {
                    @Override
                    public int getOffset() {
                        return 0;
                    }

                    @Override
                    public int getType() {
                        return PlaybackTextType.TEXT_TYPE_UNSPECIFIED;
                    }

                    @Override
                    public int getParagraphIndex() {
                        return -1;
                    }

                    @Override
                    public int getLength() {
                        return -1;
                    }
                };
        PlaybackTextPart[] paragraphs = new PlaybackTextPart[] {p};
        when(mMetadata.paragraphs()).thenReturn(paragraphs);
        mController.tapToSeek("the quick brown fox", 4, 9);
        verify(mPlayback, times(1)).seekToWord(0, 4);
        histogram.assertExpected();
    }

    @Test
    @EnableFeatures(ChromeFeatureList.READALOUD_TAP_TO_SEEK)
    public void testTapToSeek_differentTab() {
        // play tab
        requestAndStartPlayback();
        // switch tabs
        MockTab newTab = mTabModelSelector.addMockTab();
        mTabModelSelector
                .getModel(false)
                .setIndex(
                        mTabModelSelector.getModel(false).indexOf(newTab),
                        TabSelectionType.FROM_USER);
        // shouldn't seek
        mController.tapToSeek("the quick brown fox", 4, 9);
        verify(mPlayback, never()).seekToWord(0, 8);
    }

    @Test
    public void testDidFirstVisuallyNonEmptyPaint() {
        GURL gurl = new GURL("https://en.wikipedia.org/wiki/Alphabet_Inc.");
        when(mTab.getUrl()).thenReturn(gurl);
        mController.getTabModelTabObserverforTests().didFirstVisuallyNonEmptyPaint(mTab);
        verify(mHooksImpl).isPageReadable(eq(gurl.getPossiblyInvalidSpec()), any());
    }

    @Test
    public void testOnTabSelected() {
        MockTab tab = mTabModelSelector.addMockTab();

        // should do nothing on empty url
        tab.setUrl(new GURL(""));
        mController.getTabModelTabObserverforTests().onTabSelected(tab);
        verify(tab, never()).getUserDataHost();

        // should get user data for actual urls
        tab.setUrl(new GURL("https://en.wikipedia.org/wiki/Alphabet_Inc."));
        mController.getTabModelTabObserverforTests().onTabSelected(tab);
        verify(tab, times(1)).getUserDataHost();
    }

    @Test
    public void testTimepointsSupported_emptyUrl() {
        // if somehow an empty url sneaks into timepoints supported
        mController.setTimepointsSupportedForTest("", true);
        when(mTab.getUrl()).thenReturn(new GURL(""));
        // a tab with an empty url should not be supported
        assertFalse(mController.timepointsSupported(mTab));
    }

    @Test
    public void testEmptyUrlReadability() {
        // grab the callback
        mController.maybeCheckReadability(mTab);
        verify(mHooksImpl, times(1))
                .isPageReadable(eq(sTestGURL.getSpec()), mCallbackCaptor.capture());
        // if somehow an empty url sneaks into the readability maps
        boolean failed = false;
        try {
            mCallbackCaptor.getValue().onSuccess("", true, true);
        } catch (AssertionError e) {
            failed = true;
        }
        assertTrue(failed);
        when(mTab.getUrl()).thenReturn(new GURL(""));
        // empty urls should not be returned as readable
        assertFalse(mController.isReadable(mTab));
    }

    @Test
    public void testMetricRecorded_EmptyUrlPlayback() {
        final String histogramName = ReadAloudMetrics.EMPTY_URL_PLAYBACK;
        var histogram =
                HistogramWatcher.newSingleRecordWatcher(
                        histogramName, ReadAloudController.Entrypoint.MAGIC_TOOLBAR);

        mFakeTranslateBridge.setCurrentLanguage("en");
        mTab.setGurlOverrideForTesting(new GURL(""));

        boolean failed = false;
        try {
            mController.playTab(mTab, ReadAloudController.Entrypoint.MAGIC_TOOLBAR);
        } catch (AssertionError e) {
            failed = true;
        }
        assertTrue(failed);
        histogram.assertExpected();
    }

    @Test
    public void testMetricRecorded_EmptyUrlPlayback_RestoreState() {
        final String histogramName = ReadAloudMetrics.EMPTY_URL_PLAYBACK;
        var histogram =
                HistogramWatcher.newSingleRecordWatcher(
                        histogramName, ReadAloudController.Entrypoint.RESTORED_PLAYBACK);

        mTab.setGurlOverrideForTesting(new GURL("https://en.wikipedia.org/wiki/Google"));
        var data = Mockito.mock(PlaybackData.class);
        ReadAloudController.RestoreState restoreState =
                mController.new RestoreState(mTab, data, true, false, 0L);
        mController.setStateToRestoreOnBringingToForegroundForTests(restoreState);
        // for some reason the tab url goes null
        mTab.setGurlOverrideForTesting(new GURL(""));
        boolean failed = false;
        try {
            restoreState.restore();
        } catch (AssertionError e) {
            failed = true;
        }
        assertTrue(failed);
        histogram.assertExpected();
    }

    @Test
    public void testNoReadabilityUpdateAfterDestroy() {
        Runnable readabilityObserver = Mockito.mock(Runnable.class);
        mController.addReadabilityUpdateListener(readabilityObserver);

        // Check readability
        mController.maybeCheckReadability(mTab);
        verify(mHooksImpl, times(1))
                .isPageReadable(eq(sTestGURL.getSpec()), mCallbackCaptor.capture());
        assertFalse(mController.isReadable(mTab));

        // Simulate response coming back after ReadAloudController being destroyed.
        mController.destroy();
        mCallbackCaptor.getValue().onSuccess(sTestGURL.getSpec(), true, false);

        verify(readabilityObserver, never()).run();
    }

    @Test
    public void testReasonForStoppingPlaybackLogged() {
        final String histogramName = ReadAloudMetrics.REASON_FOR_STOPPING_PLAYBACK;
        var histogram =
                HistogramWatcher.newSingleRecordWatcher(
                        histogramName, ReadAloudMetrics.ReasonForStoppingPlayback.MANUAL_CLOSE);
        requestAndStartPlayback();

        mController.maybeStopPlayback(
                mTab, ReadAloudMetrics.ReasonForStoppingPlayback.MANUAL_CLOSE);

        histogram.assertExpected();
    }

    private void requestAndStartPlayback() {
        mFakeTranslateBridge.setCurrentLanguage("en");
        mTab.setGurlOverrideForTesting(new GURL("https://en.wikipedia.org/wiki/Google"));
        mController.playTab(mTab, ReadAloudController.Entrypoint.MAGIC_TOOLBAR);
        resolvePromises();

        verify(mPlaybackHooks, times(1))
                .createPlayback(Mockito.any(), mPlaybackCallbackCaptor.capture());

        onPlaybackSuccess(mPlayback);
        verify(mPlayerCoordinator, times(1))
                .playbackReady(eq(mPlayback), eq(PlaybackListener.State.PLAYING));
    }

    private void onPlaybackSuccess(Playback playback) {
        mPlaybackCallbackCaptor.getValue().onSuccess(playback);
        resolvePromises();
    }

    private static void resolvePromises() {
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();
    }
}
