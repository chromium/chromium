// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.readaloud;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.any;
import static org.mockito.Mockito.anyInt;
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

import android.app.Activity;

import androidx.appcompat.app.AppCompatActivity;

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

import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.browser.browser_controls.BrowserControlsSizer;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.language.AppLocaleUtils;
import org.chromium.chrome.browser.layouts.LayoutManager;
import org.chromium.chrome.browser.price_tracking.PriceTrackingFeatures;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.signin.services.UnifiedConsentServiceBridge;
import org.chromium.chrome.browser.tab.MockTab;
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
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.chrome.test.util.browser.Features.DisableFeatures;
import org.chromium.chrome.test.util.browser.Features.EnableFeatures;
import org.chromium.chrome.test.util.browser.tabmodel.MockTabModelSelector;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.prefs.PrefService;
import org.chromium.components.user_prefs.UserPrefsJni;
import org.chromium.content_public.browser.GlobalRenderFrameHostId;
import org.chromium.content_public.browser.RenderFrameHost;
import org.chromium.content_public.browser.WebContents;
import org.chromium.url.GURL;
import org.chromium.url.JUnitTestGURLs;

import java.util.List;

/** Unit tests for {@link ReadAloudController}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
@EnableFeatures({ChromeFeatureList.READALOUD, ChromeFeatureList.READALOUD_PLAYBACK})
public class ReadAloudControllerUnitTest {
    private static final GURL sTestGURL = JUnitTestGURLs.EXAMPLE_URL;

    private MockTab mTab;
    private ReadAloudController mController;
    private Activity mActivity;

    @Rule public JniMocker mJniMocker = new JniMocker();
    @Rule public TestRule mProcessor = new Features.JUnitProcessor();

    private FakeTranslateBridgeJni mFakeTranslateBridge;
    private ObservableSupplierImpl<Profile> mProfileSupplier;
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
    @Mock private UserPrefsJni mUserPrefsNatives;
    @Mock private PrefService mPrefService;

    MockTabModelSelector mTabModelSelector;

    @Captor ArgumentCaptor<ReadAloudReadabilityHooks.ReadabilityCallback> mCallbackCaptor;
    @Captor ArgumentCaptor<ReadAloudPlaybackHooks.CreatePlaybackCallback> mPlaybackCallbackCaptor;
    @Captor ArgumentCaptor<PlaybackArgs> mPlaybackArgsCaptor;
    @Captor ArgumentCaptor<PlaybackListener> mPlaybackListenerCaptor;
    @Mock private Playback mPlayback;
    @Mock private Playback.Metadata mMetadata;
    @Mock private WebContents mWebContents;
    @Mock private RenderFrameHost mRenderFrameHost;
    private GlobalRenderFrameHostId mGlobalRenderFrameHostId = new GlobalRenderFrameHostId(1, 1);

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        mProfileSupplier = new ObservableSupplierImpl<>();
        mProfileSupplier.set(mMockProfile);

        mActivity = Robolectric.buildActivity(AppCompatActivity.class).setup().get();
        mActivity.setTheme(R.style.Theme_BrowserUI_DayNight);

        when(mMockProfile.isOffTheRecord()).thenReturn(false);
        when(mMockIncognitoProfile.isOffTheRecord()).thenReturn(true);
        UnifiedConsentServiceBridge.setUrlKeyedAnonymizedDataCollectionEnabled(true);

        PriceTrackingFeatures.setPriceTrackingEnabledForTesting(false);

        mFakeTranslateBridge = new FakeTranslateBridgeJni();
        mJniMocker.mock(TranslateBridgeJni.TEST_HOOKS, mFakeTranslateBridge);
        mJniMocker.mock(ReadAloudPrefsJni.TEST_HOOKS, mReadAloudPrefsNatives);
        mJniMocker.mock(UserPrefsJni.TEST_HOOKS, mUserPrefsNatives);
        doReturn(mPrefService).when(mUserPrefsNatives).get(any());
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
        when(mPlaybackHooks.createPlayer(any())).thenReturn(mPlayerCoordinator);
        ReadAloudController.setReadabilityHooks(mHooksImpl);
        ReadAloudController.setPlaybackHooks(mPlaybackHooks);
        mController =
                new ReadAloudController(
                        mActivity,
                        mProfileSupplier,
                        mTabModelSelector.getModel(false),
                        mBottomSheetController,
                        mBrowserControlsSizer,
                        mLayoutManager);
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();

        mTab = mTabModelSelector.getCurrentTab();
        mTab.setGurlOverrideForTesting(sTestGURL);
        mTab.setWebContentsOverrideForTesting(mWebContents);

        when(mPlayback.getMetadata()).thenReturn(mMetadata);
        when(mWebContents.getMainFrame()).thenReturn(mRenderFrameHost);
        when(mRenderFrameHost.getGlobalRenderFrameHostId()).thenReturn(mGlobalRenderFrameHostId);
        mController.setHighlighterForTests(mHighlighter);

        doReturn(false).when(mPlaybackHooks).voicesInitialized();
        doReturn(List.of(new PlaybackVoice("en", "voiceA", "")))
                .when(mPlaybackHooks)
                .getVoicesFor(anyString());
    }

    @Test
    public void testIsAvailable() {
        // test set up: non incognito profile + MSBB Accepted
        assertTrue(mController.isAvailable());
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
    public void testPlayTab_sendsVoiceList() {
        mFakeTranslateBridge.setCurrentLanguage("en");
        doReturn(
                        List.of(
                                new PlaybackVoice("en", "voiceA", ""),
                                new PlaybackVoice("es", "voiceB", ""),
                                new PlaybackVoice("fr", "voiceC", "")))
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
    public void testPlayTab_tabLanguageEmpty() {
        AppLocaleUtils.setAppLanguagePref("fr-FR");

        mFakeTranslateBridge.setCurrentLanguage("");
        mTab.setGurlOverrideForTesting(new GURL("https://en.wikipedia.org/wiki/Google"));

        mController.playTab(mTab);

        verify(mPlaybackHooks).createPlayback(mPlaybackArgsCaptor.capture(), any());
        assertEquals("fr", mPlaybackArgsCaptor.getValue().getLanguage());
    }

    @Test
    public void testPlayTab_unsupportedLanguage() {
        doReturn(List.of()).when(mPlaybackHooks).getVoicesFor(anyString());
        mFakeTranslateBridge.setCurrentLanguage("pl-PL");
        mTab.setGurlOverrideForTesting(new GURL("https://en.wikipedia.org/wiki/Google"));

        mController.playTab(mTab);

        verify(mPlaybackHooks, never()).createPlayback(mPlaybackArgsCaptor.capture(), any());
    }

    @Test
    public void testPlayTab_tabLanguageUnd() {
        AppLocaleUtils.setAppLanguagePref("fr-FR");

        mFakeTranslateBridge.setCurrentLanguage("und");
        mTab.setGurlOverrideForTesting(new GURL("https://en.wikipedia.org/wiki/Google"));

        mController.playTab(mTab);

        verify(mPlaybackHooks).createPlayback(mPlaybackArgsCaptor.capture(), any());
        assertEquals("fr", mPlaybackArgsCaptor.getValue().getLanguage());
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
        verify(mPrefService).hasPrefPath(eq(ReadAloudPrefs.HIGHLIGHTING_ENABLED_PATH));

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
        var oldVoice = new PlaybackVoice("lang", "OLD VOICE ID", "description");
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
        var newVoice = new PlaybackVoice("lang", "NEW VOICE ID", "description");
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

        doReturn(Mockito.mock(Playback.Metadata.class)).when(mPlayback).getMetadata();
        onPlaybackSuccess(mPlayback);
        verify(mPlayback, never()).play();
        verify(mPlayback).seekToParagraph(eq(99), eq(0L));
    }

    @Test
    public void testPreviewVoice_whilePlaying_success() {
        // Play tab.
        mFakeTranslateBridge.setCurrentLanguage("en");
        mTab.setGurlOverrideForTesting(new GURL("https://en.wikipedia.org/wiki/Google"));
        mController.playTab(mTab);
        verify(mPlaybackHooks).createPlayback(any(), mPlaybackCallbackCaptor.capture());
        onPlaybackSuccess(mPlayback);
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
        mFakeTranslateBridge.setCurrentLanguage("en");
        mTab.setGurlOverrideForTesting(new GURL("https://en.wikipedia.org/wiki/Google"));
        mController.playTab(mTab);
        verify(mPlaybackHooks).createPlayback(any(), mPlaybackCallbackCaptor.capture());
        onPlaybackSuccess(mPlayback);
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
        mFakeTranslateBridge.setCurrentLanguage("en");
        mTab.setGurlOverrideForTesting(new GURL("https://en.wikipedia.org/wiki/Google"));
        mController.playTab(mTab);
        verify(mPlaybackHooks).createPlayback(any(), mPlaybackCallbackCaptor.capture());
        onPlaybackSuccess(mPlayback);
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
        verify(mPlaybackHooks).createPlayback(any(), mPlaybackCallbackCaptor.capture());

        // Playback succeeds just once. No seeking.
        onPlaybackSuccess(mPlayback);
        verify(mPlayback, times(1)).play();
        verify(mPlayback, never()).seekToParagraph(anyInt(), anyLong());
    }

    @Test
    public void testRestorePlaybackState_previewThenChangeVoice() {
        // When previewing a voice, tab playback should only be restored when closing
        // the menu. This test makes sure it doesn't start up early when a voice is
        // selected.

        // Set up playback and restorable state.
        mController.playTab(mTab);
        verify(mPlaybackHooks).createPlayback(any(), mPlaybackCallbackCaptor.capture());
        onPlaybackSuccess(mPlayback);
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
        mFakeTranslateBridge.setCurrentLanguage("en");
        mTab.setGurlOverrideForTesting(new GURL("https://en.wikipedia.org/wiki/Google"));
        mController.playTab(mTab);
        verify(mPlaybackHooks).createPlayback(any(), mPlaybackCallbackCaptor.capture());
        onPlaybackSuccess(mPlayback);
        assertEquals(1, mFakeTranslateBridge.getObserverCount());

        // stopping playback should unregister a listener
        mController.maybeStopPlayback(mTab);
        assertEquals(0, mFakeTranslateBridge.getObserverCount());
    }

    @Test
    public void testTranslationStopsPlayback() {
        // Play tab.
        mFakeTranslateBridge.setCurrentLanguage("en");
        mTab.setGurlOverrideForTesting(new GURL("https://en.wikipedia.org/wiki/Google"));
        mController.playTab(mTab);
        verify(mPlaybackHooks).createPlayback(any(), mPlaybackCallbackCaptor.capture());
        onPlaybackSuccess(mPlayback);

        // trigger translation. Playback should stop
        mController
                .getTranslationObserverForTest()
                .onIsPageTranslatedChanged(mTab.getWebContents());
        verify(mPlayback).release();
    }

    @Test
    public void testStoppingAnyPlayback() {
        // Play tab.
        mFakeTranslateBridge.setCurrentLanguage("en");
        mTab.setGurlOverrideForTesting(new GURL("https://en.wikipedia.org/wiki/Google"));
        mController.playTab(mTab);
        verify(mPlaybackHooks).createPlayback(any(), mPlaybackCallbackCaptor.capture());
        onPlaybackSuccess(mPlayback);
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
    @DisableFeatures(ChromeFeatureList.READALOUD_PLAYBACK)
    public void testReadAloudPlaybackFlagCheckedAfterReadability() {
        mController.maybeCheckReadability(sTestGURL);
        verify(mHooksImpl, times(1))
                .isPageReadable(eq(sTestGURL.getSpec()), mCallbackCaptor.capture());
        mCallbackCaptor.getValue().onSuccess(sTestGURL.getSpec(), true, false);

        assertFalse(mController.isReadable(mTab));
    }

    private void onPlaybackSuccess(Playback playback) {
        mPlaybackCallbackCaptor.getValue().onSuccess(playback);
        resolvePromises();
    }

    private static void resolvePromises() {
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();
    }
}
