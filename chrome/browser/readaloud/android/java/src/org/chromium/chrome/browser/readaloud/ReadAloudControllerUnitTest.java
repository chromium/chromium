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
import static org.mockito.Mockito.anyLong;
import static org.mockito.Mockito.anyString;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.eq;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.reset;
import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;
import static org.robolectric.Shadows.shadowOf;

import android.app.Activity;
import android.content.Intent;

import androidx.appcompat.app.AppCompatActivity;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.MockitoAnnotations;
import org.robolectric.Robolectric;
import org.robolectric.annotation.Config;
import org.robolectric.shadows.ShadowLooper;

import org.chromium.base.ApplicationState;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.base.test.util.JniMocker;
import org.chromium.base.test.util.UserActionTester;
import org.chromium.chrome.browser.browser_controls.BrowserControlsSizer;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.language.AppLocaleUtils;
import org.chromium.chrome.browser.layouts.LayoutManager;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.price_tracking.PriceTrackingFeatures;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.readaloud.ReadAloudMetrics.IneligibilityReason;
import org.chromium.chrome.browser.search_engines.SearchEngineType;
import org.chromium.chrome.browser.search_engines.TemplateUrlServiceFactory;
import org.chromium.chrome.browser.signin.services.UnifiedConsentServiceBridge;
import org.chromium.chrome.browser.tab.MockTab;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabSelectionType;
import org.chromium.chrome.browser.tabmodel.TabModelUtils;
import org.chromium.chrome.browser.translate.FakeTranslateBridgeJni;
import org.chromium.chrome.browser.translate.TranslateBridgeJni;
import org.chromium.chrome.modules.readaloud.Playback;
import org.chromium.chrome.modules.readaloud.PlaybackArgs;
import org.chromium.chrome.modules.readaloud.PlaybackArgs.PlaybackVoice;
import org.chromium.chrome.modules.readaloud.PlaybackListener;
import org.chromium.chrome.modules.readaloud.PlaybackListener.PlaybackData;
import org.chromium.chrome.modules.readaloud.Player;
import org.chromium.chrome.modules.readaloud.ReadAloudPlaybackHooks;
import org.chromium.chrome.modules.readaloud.contentjs.Highlighter;
import org.chromium.chrome.test.util.browser.tabmodel.MockTabModelSelector;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.prefs.PrefService;
import org.chromium.components.search_engines.TemplateUrl;
import org.chromium.components.search_engines.TemplateUrlService;
import org.chromium.components.user_prefs.UserPrefsJni;
import org.chromium.content_public.browser.GlobalRenderFrameHostId;
import org.chromium.content_public.browser.RenderFrameHost;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.ActivityWindowAndroid;
import org.chromium.url.GURL;
import org.chromium.url.JUnitTestGURLs;

import java.util.Arrays;
import java.util.HashSet;
import java.util.List;

/** Unit tests for {@link ReadAloudController}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
@EnableFeatures({ChromeFeatureList.READALOUD, ChromeFeatureList.READALOUD_PLAYBACK})
@DisableFeatures({ChromeFeatureList.READALOUD_IN_MULTI_WINDOW})
public class ReadAloudControllerUnitTest {
    private static final GURL sTestGURL = JUnitTestGURLs.EXAMPLE_URL;
    private static final long KNOWN_READABLE_TRIAL_PTR = 12345678L;

    private MockTab mTab;
    private ReadAloudController mController;
    private Activity mActivity;

    @Rule public JniMocker mJniMocker = new JniMocker();
    @Rule public TestRule mProcessor = new Features.JUnitProcessor();

    private FakeTranslateBridgeJni mFakeTranslateBridge;
    private ObservableSupplierImpl<Profile> mProfileSupplier;
    private ObservableSupplierImpl<LayoutManager> mLayoutManagerSupplier;
    @Mock private Profile mMockProfile;
    @Mock private Profile mMockIncognitoProfile;
    @Mock private ReadAloudReadabilityHooksImpl mHooksImpl;
    @Mock private ReadAloudPlaybackHooks mPlaybackHooks;
    @Mock private Player mPlayerCoordinator;
    @Mock private BottomSheetController mBottomSheetController;
    @Mock private Highlighter mHighlighter;
    @Mock private PlaybackListener.PhraseTiming mPhraseTiming;
    @Mock private BrowserControlsSizer mBrowserControlsSizer;
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
    @Mock private Playback mPlayback;
    @Mock private Playback.Metadata mMetadata;
    @Mock private WebContents mWebContents;
    @Mock private RenderFrameHost mRenderFrameHost;
    @Mock private TemplateUrl mSearchEngine;
    private GlobalRenderFrameHostId mGlobalRenderFrameHostId = new GlobalRenderFrameHostId(1, 1);
    public UserActionTester mUserActionTester;
    private HistogramWatcher mHighlightingEnabledOnStartupHistogram;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        mProfileSupplier = new ObservableSupplierImpl<>();
        mProfileSupplier.set(mMockProfile);

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
        when(mPlaybackHooks.createPlayer(any())).thenReturn(mPlayerCoordinator);
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

        mController =
                new ReadAloudController(
                        mActivity,
                        mProfileSupplier,
                        mTabModelSelector.getModel(false),
                        mTabModelSelector.getModel(true),
                        mBottomSheetController,
                        mBrowserControlsSizer,
                        mLayoutManagerSupplier,
                        mActivityWindowAndroid,
                        mActivityLifecycleDispatcher);
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();

        mTab = mTabModelSelector.getCurrentTab();
        mTab.setGurlOverrideForTesting(sTestGURL);
        mTab.setWebContentsOverrideForTesting(mWebContents);

        when(mMetadata.languageCode()).thenReturn("en");
        when(mPlayback.getMetadata()).thenReturn(mMetadata);
        when(mWebContents.getMainFrame()).thenReturn(mRenderFrameHost);
        when(mRenderFrameHost.getGlobalRenderFrameHostId()).thenReturn(mGlobalRenderFrameHostId);
        mController.setHighlighterForTests(mHighlighter);

        doReturn(false).when(mPlaybackHooks).voicesInitialized();
        doReturn(List.of(new PlaybackVoice("en", "voiceA", "")))
                .when(mPlaybackHooks)
                .getVoicesFor(anyString());
        mUserActionTester = new UserActionTester();
    }

    @After
    public void tearDown() {
        mUserActionTester.tearDown();
        ReadAloudFeatures.shutdown();
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
    public void testReloadingPage() {
        // Reload tab before any playback starts - tests null checks
        mController.getTabModelTabObserverforTests().onPageLoadStarted(mTab, mTab.getUrl());

        verify(mPlayerCoordinator, never()).dismissPlayers();
        verify(mPlayback, never()).release();

        // now start playing a tab
        mController.playTab(mTab);
        verify(mPlaybackHooks, times(1))
                .createPlayback(Mockito.any(), mPlaybackCallbackCaptor.capture());
        onPlaybackSuccess(mPlayback);

        // reload some other tab, playback should keep going
        MockTab newTab = mTabModelSelector.addMockTab();
        newTab.setGurlOverrideForTesting(new GURL("https://en.wikipedia.org/wiki/Alphabet_Inc."));
        mController.getTabModelTabObserverforTests().onPageLoadStarted(newTab, newTab.getUrl());

        verify(mPlayerCoordinator, never()).dismissPlayers();
        verify(mPlayback, never()).release();

        // now reload the playing tab
        mController.getTabModelTabObserverforTests().onPageLoadStarted(mTab, mTab.getUrl());

        verify(mPlayerCoordinator).dismissPlayers();
        verify(mPlayback).release();
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
        mController.playTab(mTab);
        verify(mPlaybackHooks, times(1))
                .createPlayback(Mockito.any(), mPlaybackCallbackCaptor.capture());
        onPlaybackSuccess(mPlayback);

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
    public void testReloadPage_errorUiDismissed() {
        // start a playback with an error
        mController.playTab(mTab);
        verify(mPlaybackHooks, times(1))
                .createPlayback(Mockito.any(), mPlaybackCallbackCaptor.capture());
        mPlaybackCallbackCaptor.getValue().onFailure(new Exception("Very bad error"));
        resolvePromises();

        // Reload this url
        mController.getTabModelTabObserverforTests().onPageLoadStarted(mTab, mTab.getUrl());

        // No playback but error UI should get dismissed
        verify(mPlayerCoordinator).dismissPlayers();
    }

    @Test
    public void testClosingTab() {
        // Close a  tab before any playback starts - tests null checks
        mController.getTabModelTabObserverforTests().willCloseTab(mTab);

        verify(mPlayerCoordinator, never()).dismissPlayers();
        verify(mPlayback, never()).release();

        // now start playing a tab
        mController.playTab(mTab);
        verify(mPlaybackHooks, times(1))
                .createPlayback(Mockito.any(), mPlaybackCallbackCaptor.capture());
        onPlaybackSuccess(mPlayback);

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
        mController.playTab(mTab);
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

        mController.maybeCheckReadability(mTab.getUrl());

        verify(mHooksImpl, never())
                .isPageReadable(
                        Mockito.anyString(),
                        Mockito.any(ReadAloudReadabilityHooks.ReadabilityCallback.class));
    }

    @Test
    public void checkReadabilityOnPageLoad_URLnotReadAloudSupported() {
        checkURLNotReadAloudSupported(new GURL("invalid"));
        checkURLNotReadAloudSupported(GURL.emptyGURL());
        checkURLNotReadAloudSupported(new GURL("chrome://history/"));
        checkURLNotReadAloudSupported(new GURL("about:blank"));
        checkURLNotReadAloudSupported(new GURL("https://www.google.com/search?q=weather"));
        checkURLNotReadAloudSupported(new GURL("https://myaccount.google.com/"));
        checkURLNotReadAloudSupported(new GURL("https://myactivity.google.com/"));
    }

    @Test
    public void checkReadability_success() {
        mController.maybeCheckReadability(sTestGURL);

        verify(mHooksImpl, times(1))
                .isPageReadable(eq(sTestGURL.getSpec()), mCallbackCaptor.capture());
        assertFalse(mController.isReadable(mTab));

        mCallbackCaptor.getValue().onSuccess(sTestGURL.getSpec(), true, false);
        assertTrue(mController.isReadable(mTab));
        assertFalse(mController.timepointsSupported(mTab));

        // now check that the second time the same url loads we don't resend a request
        mController.maybeCheckReadability(sTestGURL);

        verify(mHooksImpl, times(1))
                .isPageReadable(
                        Mockito.anyString(),
                        Mockito.any(ReadAloudReadabilityHooks.ReadabilityCallback.class));
    }

    @Test
    public void checkReadability_noMSBB() {
        mController.maybeCheckReadability(sTestGURL);

        verify(mHooksImpl, times(1))
                .isPageReadable(eq(sTestGURL.getSpec()), mCallbackCaptor.capture());
        assertFalse(mController.isReadable(mTab));

        mCallbackCaptor.getValue().onSuccess(sTestGURL.getSpec(), true, false);
        UnifiedConsentServiceBridge.setUrlKeyedAnonymizedDataCollectionEnabled(false);
        assertFalse(mController.isReadable(mTab));
    }

    @Test
    public void checkReadability_onlyOnePendingRequest() {
        mController.maybeCheckReadability(sTestGURL);
        mController.maybeCheckReadability(sTestGURL);
        mController.maybeCheckReadability(sTestGURL);

        verify(mHooksImpl, times(1)).isPageReadable(Mockito.anyString(), mCallbackCaptor.capture());
    }

    @Test
    public void checkReadability_failure() {
        mController.maybeCheckReadability(sTestGURL);

        verify(mHooksImpl, times(1))
                .isPageReadable(eq(sTestGURL.getSpec()), mCallbackCaptor.capture());
        assertFalse(mController.isReadable(mTab));

        mCallbackCaptor
                .getValue()
                .onFailure(sTestGURL.getSpec(), new Throwable("Something went wrong"));
        assertFalse(mController.isReadable(mTab));
        assertFalse(mController.timepointsSupported(mTab));

        // now check that the second time the same url loads we will resend a request
        mController.maybeCheckReadability(sTestGURL);

        verify(mHooksImpl, times(2))
                .isPageReadable(
                        Mockito.anyString(),
                        Mockito.any(ReadAloudReadabilityHooks.ReadabilityCallback.class));
    }

    @Test
    public void isReadable_languageSupported() {
        mController.maybeCheckReadability(sTestGURL);

        verify(mHooksImpl, times(1))
                .isPageReadable(eq(sTestGURL.getSpec()), mCallbackCaptor.capture());

        mCallbackCaptor.getValue().onSuccess(sTestGURL.getSpec(), true, false);
        assertTrue(mController.isReadable(mTab));

        // check that URL is supported when the language is set to a supported language
        mFakeTranslateBridge.setCurrentLanguage("en");
        assertTrue(mController.isReadable(mTab));
    }

    @Test
    public void isReadable_languageUnsupported() {
        mController.maybeCheckReadability(sTestGURL);

        verify(mHooksImpl, times(1))
                .isPageReadable(eq(sTestGURL.getSpec()), mCallbackCaptor.capture());

        mCallbackCaptor.getValue().onSuccess(sTestGURL.getSpec(), true, false);
        assertTrue(mController.isReadable(mTab));

        // check that URL isn't supported when the language is set to an unsupported language
        mFakeTranslateBridge.setCurrentLanguage("he");
        assertFalse(mController.isReadable(mTab));
    }

    @Test
    public void testReactingtoMSBBChange() {
        mController.maybeCheckReadability(sTestGURL);

        verify(mHooksImpl, times(1))
                .isPageReadable(eq(sTestGURL.getSpec()), mCallbackCaptor.capture());

        // Disable MSBB. Sending requests to Google servers no longer allowed but using
        // previous results is ok.
        UnifiedConsentServiceBridge.setUrlKeyedAnonymizedDataCollectionEnabled(false);
        mController.maybeCheckReadability(JUnitTestGURLs.GOOGLE_URL_CAT);

        verify(mHooksImpl, times(1))
                .isPageReadable(
                        Mockito.anyString(),
                        Mockito.any(ReadAloudReadabilityHooks.ReadabilityCallback.class));
    }

    @Test
    public void testPlayTab() {
        mFakeTranslateBridge.setCurrentLanguage("en");
        mTab.setGurlOverrideForTesting(new GURL("https://en.wikipedia.org/wiki/Google"));
        mController.playTab(mTab);

        verify(mPlaybackHooks, times(1))
                .createPlayback(Mockito.any(), mPlaybackCallbackCaptor.capture());

        onPlaybackSuccess(mPlayback);
        verify(mPlayerCoordinator, times(1))
                .playbackReady(eq(mPlayback), eq(PlaybackListener.State.PLAYING));
        verify(mPlayerCoordinator).addObserver(mController);

        // test that previous playback is released when another playback is called
        MockTab newTab = mTabModelSelector.addMockTab();
        newTab.setGurlOverrideForTesting(new GURL("https://en.wikipedia.org/wiki/Alphabet_Inc."));
        mController.playTab(newTab);
        verify(mPlayback, times(1)).release();
    }

    @Test
    public void testPlayTab_inMultiWindow() {
        mFakeTranslateBridge.setCurrentLanguage("en");
        mTab.setGurlOverrideForTesting(new GURL("https://en.wikipedia.org/wiki/Google"));
        mController.playTab(mTab);

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
        mController.playTab(mTab);

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

        mController.playTab(mTab);

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
    public void testPlayTranslatedTab_tabLanguageEmpty() {
        AppLocaleUtils.setAppLanguagePref("fr-FR");

        mFakeTranslateBridge.setIsPageTranslated(true);
        mFakeTranslateBridge.setCurrentLanguage("");
        mTab.setGurlOverrideForTesting(new GURL("https://en.wikipedia.org/wiki/Google"));

        mController.playTab(mTab);

        verify(mPlaybackHooks).createPlayback(mPlaybackArgsCaptor.capture(), any());
        assertEquals("fr", mPlaybackArgsCaptor.getValue().getLanguage());
    }

    @Test
    public void testPlayTranslatedTab_unsupportedLanguage() {
        doReturn(List.of()).when(mPlaybackHooks).getVoicesFor(anyString());
        mFakeTranslateBridge.setCurrentLanguage("pl-PL");
        mTab.setGurlOverrideForTesting(new GURL("https://en.wikipedia.org/wiki/Google"));

        mController.playTab(mTab);

        verify(mPlaybackHooks, never()).createPlayback(mPlaybackArgsCaptor.capture(), any());
        verify(mPlayerCoordinator).playbackFailed();
    }

    @Test
    public void testPlayTranslatedTab_tabLanguageUnd() {
        AppLocaleUtils.setAppLanguagePref("fr-FR");

        mFakeTranslateBridge.setIsPageTranslated(true);
        mFakeTranslateBridge.setCurrentLanguage("und");
        mTab.setGurlOverrideForTesting(new GURL("https://en.wikipedia.org/wiki/Google"));

        mController.playTab(mTab);

        verify(mPlaybackHooks).createPlayback(mPlaybackArgsCaptor.capture(), any());
        assertEquals("fr", mPlaybackArgsCaptor.getValue().getLanguage());
    }

    @Test
    public void testPlayUntranslatedTab() {
        AppLocaleUtils.setAppLanguagePref("fr-FR");

        mFakeTranslateBridge.setIsPageTranslated(false);
        mFakeTranslateBridge.setCurrentLanguage("fr");
        mTab.setGurlOverrideForTesting(new GURL("https://en.wikipedia.org/wiki/Google"));

        mController.playTab(mTab);

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

        mController.playTab(mTab);

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

        mController.playTab(mTab);

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

        mController.playTab(mTab);

        verify(mPlaybackHooks)
                .createPlayback(mPlaybackArgsCaptor.capture(), mPlaybackCallbackCaptor.capture());
        assertEquals(null, mPlaybackArgsCaptor.getValue().getLanguage());
        onPlaybackSuccess(mPlayback);

        verify(mPlayerCoordinator).playbackFailed();
    }

    @Test
    public void testPlayTab_onFailure() {
        mFakeTranslateBridge.setCurrentLanguage("en");
        mTab.setGurlOverrideForTesting(new GURL("https://en.wikipedia.org/wiki/Google"));
        mController.playTab(mTab);

        verify(mPlaybackHooks, times(1))
                .createPlayback(Mockito.any(), mPlaybackCallbackCaptor.capture());

        mPlaybackCallbackCaptor.getValue().onFailure(new Throwable());
        resolvePromises();
        verify(mPlayerCoordinator, times(1)).playbackFailed();
    }

    @Test
    public void testStopPlayback() {
        // Play tab
        mFakeTranslateBridge.setCurrentLanguage("en");
        mTab.setGurlOverrideForTesting(new GURL("https://en.wikipedia.org/wiki/Google"));
        mController.playTab(mTab);

        verify(mPlaybackHooks, times(1))
                .createPlayback(Mockito.any(), mPlaybackCallbackCaptor.capture());

        onPlaybackSuccess(mPlayback);
        verify(mPlayerCoordinator, times(1))
                .playbackReady(eq(mPlayback), eq(PlaybackListener.State.PLAYING));

        // Stop playback
        mController.maybeStopPlayback(mTab);
        verify(mPlayback).release();

        reset(mPlayerCoordinator);
        reset(mPlayback);
        reset(mPlaybackHooks);

        doReturn(List.of(new PlaybackVoice("en", "voiceA", "")))
                .when(mPlaybackHooks)
                .getVoicesFor(anyString());
        // Subsequent playTab() should play without trying to release anything.
        mController.playTab(mTab);
        verify(mPlaybackHooks).createPlayback(any(), any());
        verify(mPlayback, never()).release();
    }

    @Test
    public void highlightsRequested() {
        // set up the highlighter
        mController.setTimepointsSupportedForTest(mTab.getUrl().getSpec(), true);
        mController.playTab(mTab);
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
    public void reloadingTab_highlightsCleared() {
        // set up the highlighter
        mController.setTimepointsSupportedForTest(mTab.getUrl().getSpec(), true);
        mController.playTab(mTab);
        verify(mPlaybackHooks, times(1))
                .createPlayback(Mockito.any(), mPlaybackCallbackCaptor.capture());
        onPlaybackSuccess(mPlayback);
        verify(mHighlighter).initializeJs(eq(mTab), eq(mMetadata), any(Highlighter.Config.class));

        // Reload this url
        mController.getTabModelTabObserverforTests().onPageLoadStarted(mTab, mTab.getUrl());

        verify(mHighlighter).handleTabReloaded(eq(mTab));
    }

    @Test
    public void reloadingTab_highlightsNotCleared() {
        // set up the highlighter
        mController.setTimepointsSupportedForTest(mTab.getUrl().getSpec(), true);
        mController.playTab(mTab);
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
        mController.playTab(mTab);
        verify(mPlaybackHooks, times(1))
                .createPlayback(Mockito.any(), mPlaybackCallbackCaptor.capture());
        onPlaybackSuccess(mPlayback);
        verify(mHighlighter).initializeJs(eq(mTab), eq(mMetadata), any(Highlighter.Config.class));

        // stopping playback should clear highlighting.
        mController.maybeStopPlayback(mTab);

        verify(mHighlighter).clearHighlights(eq(mGlobalRenderFrameHostId), eq(mTab));
    }

    @Test
    public void testUserDataStrippedFromReadabilityCheck() {
        GURL tabUrl = new GURL("http://user:pass@example.com");
        mTab.setGurlOverrideForTesting(tabUrl);

        mController.maybeCheckReadability(tabUrl);

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
        mController.playTab(mTab);
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
        mController.playTab(mTab);

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
        mController.playTab(mTab);
        verify(mPlaybackHooks).createPlayback(any(), mPlaybackCallbackCaptor.capture());
        onPlaybackSuccess(mPlayback);
        verify(mPlayback).addListener(mPlaybackListenerCaptor.capture());
        reset(mPlaybackHooks);
        reset(mPlayback);

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

        doReturn(mMetadata).when(mPlayback).getMetadata();
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
        mController.playTab(mTab);
        reset(mPlaybackHooks);
        doReturn(List.of(new PlaybackVoice("en", "voiceA", "")))
                .when(mPlaybackHooks)
                .getVoicesFor(anyString());

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
        reset(mPlaybackHooks);
        doReturn(List.of(new PlaybackVoice("en", "voiceA", "")))
                .when(mPlaybackHooks)
                .getVoicesFor(anyString());

        // Closing the voice menu should stop the preview.
        mController.onVoiceMenuClosed();
        verify(previewPlayback).release();

        // Tab audio should be loaded and played. Position should be restored.
        verify(mPlaybackHooks).createPlayback(any(), mPlaybackCallbackCaptor.capture());
        onPlaybackSuccess(mPlayback);
        // Don't play, because original state was STOPPED.
        verify(mPlayback, never()).play();
        verify(mPlayback).seekToParagraph(eq(99), eq(0L));
    }

    @Test
    public void testPreviewVoice_metric() {
        final String histogramName = ReadAloudMetrics.VOICE_PREVIEWED;

        var histogram = HistogramWatcher.newSingleRecordWatcher(histogramName + "abc", true);

        // Play tab.
        requestAndStartPlayback();

        reset(mPlaybackHooks);
        // Preview a voice.
        var voice = new PlaybackVoice("en", "abc", "");
        doReturn(List.of(voice)).when(mPlaybackHooks).getVoicesFor(anyString());
        doReturn(List.of(voice)).when(mPlaybackHooks).getPlaybackVoiceList(any());
        mController.previewVoice(voice);
        verify(mPlaybackHooks).createPlayback(any(), mPlaybackCallbackCaptor.capture());
        Playback previewPlayback = Mockito.mock(Playback.class);
        onPlaybackSuccess(previewPlayback);

        histogram.assertExpected();
    }

    @Test
    public void testRestorePlaybackState_whileLoading() {
        // Request playback but don't succeed yet.
        mController.playTab(mTab);
        verify(mPlaybackHooks).createPlayback(any(), mPlaybackCallbackCaptor.capture());
        reset(mPlaybackHooks);
        doReturn(List.of(new PlaybackVoice("en", "voiceA", "")))
                .when(mPlaybackHooks)
                .getVoicesFor(anyString());

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
        reset(mPlaybackHooks);
        doReturn(List.of(new PlaybackVoice("en", "voiceA", "")))
                .when(mPlaybackHooks)
                .getVoicesFor(anyString());
        var data = Mockito.mock(PlaybackListener.PlaybackData.class);
        doReturn(PlaybackListener.State.PLAYING).when(data).state();
        doReturn(99).when(data).paragraphIndex();
        mController.onPlaybackDataChanged(data);

        verify(mPlayback).play();

        // Preview voice.
        var voice = new PlaybackVoice("en", "asdf", "");
        doReturn(List.of(voice)).when(mPlaybackHooks).getPlaybackVoiceList(any());
        mController.previewVoice(voice);
        verify(mPlaybackHooks).createPlayback(any(), mPlaybackCallbackCaptor.capture());
        reset(mPlaybackHooks);
        doReturn(List.of(new PlaybackVoice("en", "voiceA", "")))
                .when(mPlaybackHooks)
                .getVoicesFor(anyString());
        Playback previewPlayback = Mockito.mock(Playback.class);
        onPlaybackSuccess(previewPlayback);

        // Select a voice. Tab shouldn't start playing.
        mController.setVoiceOverrideAndApplyToPlayback(new PlaybackVoice("en", "1234", ""));
        verify(mPlaybackHooks, never()).createPlayback(any(), any());

        // Close the menu. Now the tab should resume playback.
        mController.onVoiceMenuClosed();
        verify(mPlaybackHooks).createPlayback(any(), mPlaybackCallbackCaptor.capture());
        onPlaybackSuccess(mPlayback);
        verify(mPlayback, times(2)).play();
        verify(mPlayback).seekToParagraph(eq(99), eq(0L));
    }

    @Test
    public void testTranslationListenerRegistration() {
        // Play tab.
        requestAndStartPlayback();

        assertEquals(1, mFakeTranslateBridge.getObserverCount());

        // stopping playback should unregister a listener
        mController.maybeStopPlayback(mTab);
        assertEquals(0, mFakeTranslateBridge.getObserverCount());
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
    public void testStoppingAnyPlayback() {
        // Play tab.
        requestAndStartPlayback();
        verify(mPlayback).play();

        // request to stop any playback
        mController.maybeStopPlayback(null);
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
        mController.playTab(mTab);

        assertFalse(mController.isHighlightingSupported());
    }

    @Test
    public void testIsHighlightingSupported_notSupported() {
        mFakeTranslateBridge.setIsPageTranslated(false);
        mController.setTimepointsSupportedForTest(mTab.getUrl().getSpec(), false);
        mController.playTab(mTab);

        assertFalse(mController.isHighlightingSupported());
    }

    @Test
    public void testIsHighlightingSupported_supported() {
        mFakeTranslateBridge.setIsPageTranslated(false);
        mController.setTimepointsSupportedForTest(mTab.getUrl().getSpec(), true);
        mController.playTab(mTab);

        assertTrue(mController.isHighlightingSupported());
    }

    @Test
    public void testReadabilitySupplier() {
        String testUrl = "https://en.wikipedia.org/wiki/Google";

        mController.maybeCheckReadability(new GURL(testUrl));

        verify(mHooksImpl, times(1)).isPageReadable(eq(testUrl), mCallbackCaptor.capture());

        mCallbackCaptor.getValue().onSuccess(testUrl, true, false);

        assertEquals(mController.getReadabilitySupplier().get(), testUrl);
    }

    @Test
    public void testMetricRecorded_isReadable() {
        final String histogramName = ReadAloudMetrics.IS_READABLE;

        var histogram = HistogramWatcher.newSingleRecordWatcher(histogramName, true);
        mController.maybeCheckReadability(sTestGURL);
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
        mController.maybeCheckReadability(sTestGURL);
        verify(mHooksImpl, times(1))
                .isPageReadable(eq(sTestGURL.getSpec()), mCallbackCaptor.capture());
        mCallbackCaptor.getValue().onSuccess(sTestGURL.getSpec(), true, false);
        histogram.assertExpected();

        histogram = HistogramWatcher.newSingleRecordWatcher(histogramName, false);
        mController.maybeCheckReadability(sTestGURL);
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
        mController.maybeCheckReadability(sTestGURL);
        verify(mHooksImpl, times(1))
                .isPageReadable(eq(sTestGURL.getSpec()), mCallbackCaptor.capture());
        mCallbackCaptor.getValue().onSuccess(sTestGURL.getSpec(), true, false);

        assertFalse(mController.isReadable(mTab));
    }

    @Test
    public void testPlaybackStopsAndStateSavedWhenAppBackgrounded() {
        // Play tab.
        requestAndStartPlayback();
        // set progress
        var data = Mockito.mock(PlaybackData.class);

        doReturn(2).when(data).paragraphIndex();
        doReturn(1000000L).when(data).positionInParagraphNanos();
        mController.onPlaybackDataChanged(data);

        // App is backgrounded. Make sure playback stops.
        mController.onApplicationStateChange(ApplicationState.HAS_STOPPED_ACTIVITIES);
        verify(mPlayback).release();
        reset(mPlayback);
        when(mPlayback.getMetadata()).thenReturn(mMetadata);

        // App goes back in foreground. Restore progress.
        mController.onApplicationStateChange(ApplicationState.HAS_RUNNING_ACTIVITIES);
        verify(mPlaybackHooks, times(2)).createPlayback(any(), mPlaybackCallbackCaptor.capture());
        onPlaybackSuccess(mPlayback);
        verify(mPlayback).seekToParagraph(2, 1000000L);
        verify(mPlayback, never()).play();

        // once saved state is restored, it's cleared and no further interactions with playback
        // should happen.
        reset(mPlayback);
        reset(mPlaybackHooks);
        when(mPlayback.getMetadata()).thenReturn(mMetadata);

        mController.onApplicationStateChange(ApplicationState.HAS_PAUSED_ACTIVITIES);
        mController.onApplicationStateChange(ApplicationState.HAS_RUNNING_ACTIVITIES);
        verify(mPlaybackHooks, never()).createPlayback(any(), mPlaybackCallbackCaptor.capture());
        verify(mPlayback, never()).release();
    }

    @Test
    public void testMetricRecorded_eligibility() {
        final String histogramName = ReadAloudMetrics.IS_USER_ELIGIBLE;

        var histogram = HistogramWatcher.newSingleRecordWatcher(histogramName, true);
        mController.getTabModelTabObserverforTests().onPageLoadStarted(mTab, mTab.getUrl());
        histogram.assertExpected();

        histogram = HistogramWatcher.newSingleRecordWatcher(histogramName, false);
        when(mPrefService.getBoolean("readaloud.listen_to_this_page_enabled")).thenReturn(false);
        mController.getTabModelTabObserverforTests().onPageLoadStarted(mTab, mTab.getUrl());
        histogram.assertExpected();
    }

    @Test
    public void testMetricRecorded_ineligibilityReason() {
        final String histogramName = ReadAloudMetrics.INELIGIBILITY_REASON;

        var histogram =
                HistogramWatcher.newSingleRecordWatcher(
                        histogramName, IneligibilityReason.POLICY_DISABLED);
        when(mPrefService.getBoolean("readaloud.listen_to_this_page_enabled")).thenReturn(false);
        mController.getTabModelTabObserverforTests().onPageLoadStarted(mTab, mTab.getUrl());
        histogram.assertExpected();
        when(mPrefService.getBoolean("readaloud.listen_to_this_page_enabled")).thenReturn(true);

        histogram =
                HistogramWatcher.newSingleRecordWatcher(
                        histogramName, IneligibilityReason.DEFAULT_SEARCH_ENGINE_GOOGLE_FALSE);
        doReturn(SearchEngineType.SEARCH_ENGINE_OTHER)
                .when(mTemplateUrlService)
                .getSearchEngineTypeFromTemplateUrl(anyString());
        mController.getTabModelTabObserverforTests().onPageLoadStarted(mTab, mTab.getUrl());
        histogram.assertExpected();
    }

    @Test
    public void testMetricRecorded_isPlaybackCreationSuccessful_True() {
        final String histogramName = ReadAloudMetrics.IS_TAB_PLAYBACK_CREATION_SUCCESSFUL;

        var histogram = HistogramWatcher.newSingleRecordWatcher(histogramName, true);
        mController.playTab(mTab);
        verify(mPlaybackHooks, times(1))
                .createPlayback(Mockito.any(), mPlaybackCallbackCaptor.capture());
        onPlaybackSuccess(mPlayback);
        histogram.assertExpected();
    }

    @Test
    public void testMetricRecorded_isPlaybackCreationSuccessful_False() {
        final String histogramName = ReadAloudMetrics.IS_TAB_PLAYBACK_CREATION_SUCCESSFUL;

        var histogram = HistogramWatcher.newSingleRecordWatcher(histogramName, false);
        mController.playTab(mTab);
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
        mController.playTab(mTab);

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

        mController.playTab(mTab);

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

        mController.playTab(mTab);

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
        mFakeTranslateBridge.setCurrentLanguage("en");
        mTab.setGurlOverrideForTesting(new GURL("https://en.wikipedia.org/wiki/Google"));
        mController.playTab(mTab);
        verify(mPlaybackHooks).createPlayback(any(), mPlaybackCallbackCaptor.capture());
        onPlaybackSuccess(mPlayback);
        verify(mPlayback, times(1)).play();

        MockTab newTab = mTabModelSelector.addMockTab();
        mTabModelSelector
                .getModel(false)
                .setIndex(
                        mTabModelSelector.getModel(false).indexOf(newTab),
                        TabSelectionType.FROM_USER,
                        false);
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
        mController.maybeCheckReadability(sTestGURL);
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
        mController.maybeCheckReadability(sTestGURL);
        verify(mHooksImpl, times(1))
                .isPageReadable(eq(sTestGURL.getSpec()), mCallbackCaptor.capture());
        // Page is not readable so do not activate the trial.
        mCallbackCaptor.getValue().onSuccess(sTestGURL.getSpec(), false, false);
        verify(mReadAloudFeaturesNatives, never()).activateSyntheticTrial(anyLong());
    }

    @Test
    @DisableFeatures(ChromeFeatureList.READALOUD_PLAYBACK)
    public void testKnownReadableTrialCanActivateWithoutPlaybackFlag() {
        mController.maybeCheckReadability(sTestGURL);
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
        mFakeTranslateBridge.setCurrentLanguage("en");
        mTab.setGurlOverrideForTesting(new GURL("https://en.wikipedia.org/wiki/Google"));
        mController.playTab(mTab);
        verify(mPlaybackHooks).createPlayback(any(), mPlaybackCallbackCaptor.capture());
        onPlaybackSuccess(mPlayback);

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
        TabModelUtils.selectTabById(
                mTabModelSelector,
                tab.getId(),
                TabSelectionType.FROM_NEW,
                /* skipLoadingTab= */ true);

        verify(mPlayback).pause();
        verify(mPlayerCoordinator).hidePlayers();
    }

    @Test
    public void testRestorePlayerOnReturnFromIncognitoTab() {
        requestAndStartPlayback();
        reset(mPlayback);

        Tab tab = mTabModelSelector.addMockIncognitoTab();
        TabModelUtils.selectTabById(
                mTabModelSelector,
                tab.getId(),
                TabSelectionType.FROM_NEW,
                /* skipLoadingTab= */ true);

        verify(mPlayback).pause();
        verify(mPlayerCoordinator).hidePlayers();

        TabModelUtils.selectTabById(
                mTabModelSelector,
                mTab.getId(),
                TabSelectionType.FROM_USER,
                /* skipLoadingTab= */ true);
        verify(mPlayback, never()).play();
        verify(mPlayerCoordinator).restorePlayers();
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

    private void requestAndStartPlayback() {
        mFakeTranslateBridge.setCurrentLanguage("en");
        mTab.setGurlOverrideForTesting(new GURL("https://en.wikipedia.org/wiki/Google"));
        mController.playTab(mTab);

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
