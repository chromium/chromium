// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.readaloud;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.any;
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
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.price_tracking.PriceTrackingFeatures;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.signin.services.UnifiedConsentServiceBridge;
import org.chromium.chrome.browser.tab.MockTab;
import org.chromium.chrome.browser.translate.FakeTranslateBridgeJni;
import org.chromium.chrome.browser.translate.TranslateBridgeJni;
import org.chromium.chrome.modules.readaloud.Playback;
import org.chromium.chrome.modules.readaloud.PlaybackListener;
import org.chromium.chrome.modules.readaloud.Player;
import org.chromium.chrome.modules.readaloud.ReadAloudPlaybackHooks;
import org.chromium.chrome.modules.readaloud.contentjs.Highlighter;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.chrome.test.util.browser.Features.EnableFeatures;
import org.chromium.chrome.test.util.browser.tabmodel.MockTabModelSelector;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.content_public.browser.GlobalRenderFrameHostId;
import org.chromium.content_public.browser.RenderFrameHost;
import org.chromium.content_public.browser.WebContents;
import org.chromium.url.GURL;
import org.chromium.url.JUnitTestGURLs;

/** Unit tests for {@link ReadAloudController}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
@EnableFeatures(ChromeFeatureList.READALOUD)
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

    MockTabModelSelector mTabModelSelector;

    @Captor ArgumentCaptor<ReadAloudReadabilityHooks.ReadabilityCallback> mCallbackCaptor;
    @Captor ArgumentCaptor<ReadAloudPlaybackHooks.CreatePlaybackCallback> mPlaybackCallbackCaptor;
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
                        mBottomSheetController);
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();

        mTab = mTabModelSelector.getCurrentTab();
        mTab.setGurlOverrideForTesting(sTestGURL);
        mTab.setWebContentsOverrideForTesting(mWebContents);

        when(mPlayback.getMetadata()).thenReturn(mMetadata);
        when(mWebContents.getMainFrame()).thenReturn(mRenderFrameHost);
        when(mRenderFrameHost.getGlobalRenderFrameHostId()).thenReturn(mGlobalRenderFrameHostId);
        mController.setHighlighterForTests(mHighlighter);
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

        mPlaybackCallbackCaptor.getValue().onSuccess(mPlayback);
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
    public void testPlayTab_onFailure() {
        mFakeTranslateBridge.setCurrentLanguage("en");
        mTab.setGurlOverrideForTesting(new GURL("https://en.wikipedia.org/wiki/Google"));
        mController.playTab(mTab);

        verify(mPlaybackHooks, times(1))
                .createPlayback(Mockito.any(), mPlaybackCallbackCaptor.capture());

        mPlaybackCallbackCaptor.getValue().onFailure(new Throwable());
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

        mPlaybackCallbackCaptor.getValue().onSuccess(mPlayback);
        verify(mPlayerCoordinator, times(1))
                .playbackReady(eq(mPlayback), eq(PlaybackListener.State.PLAYING));
        verify(mPlayerCoordinator).addObserver(mController);

        // Stop playback
        mController.stopPlayback();
        verify(mPlayerCoordinator).addObserver(eq(mController));
        verify(mPlayback).release();

        reset(mPlayerCoordinator);
        reset(mPlayback);
        reset(mPlaybackHooks);

        // Subsequent playTab() should play without trying to release anything.
        mController.playTab(mTab);
        verify(mPlaybackHooks).createPlayback(any(), any());
        verify(mPlayback, never()).release();
        verify(mPlayerCoordinator).addObserver(eq(mController));
        verify(mPlayerCoordinator, never()).removeObserver(eq(mController));
    }

    @Test
    public void highlightsRequested() {
        // set up the highlighter
        mController.setTimepointsSupportedForTest(mTab.getUrl().getSpec(), true);
        mController.playTab(mTab);
        verify(mPlaybackHooks, times(1))
                .createPlayback(Mockito.any(), mPlaybackCallbackCaptor.capture());
        mPlaybackCallbackCaptor.getValue().onSuccess(mPlayback);
        verify(mHighlighter).initializeJs(eq(mTab), eq(mMetadata), any(Highlighter.Config.class));

        // trigger highlights
        mController.onPhraseChanged(mPhraseTiming);

        verify(mHighlighter)
                .highlightText(eq(mGlobalRenderFrameHostId), eq(mTab), eq(mPhraseTiming));
    }

    @Test
    public void reloadingTab_highlightsCleared() {
        // set up the highlighter
        mController.setTimepointsSupportedForTest(mTab.getUrl().getSpec(), true);
        mController.playTab(mTab);
        verify(mPlaybackHooks, times(1))
                .createPlayback(Mockito.any(), mPlaybackCallbackCaptor.capture());
        mPlaybackCallbackCaptor.getValue().onSuccess(mPlayback);
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
        mPlaybackCallbackCaptor.getValue().onSuccess(mPlayback);
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
        mPlaybackCallbackCaptor.getValue().onSuccess(mPlayback);
        verify(mHighlighter).initializeJs(eq(mTab), eq(mMetadata), any(Highlighter.Config.class));

        // stopping playback should clear highlighting.
        mController.stopPlayback();

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
}
