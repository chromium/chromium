// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.handoff;

import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.atLeastOnce;
import static org.mockito.Mockito.clearInvocations;
import static org.mockito.Mockito.inOrder;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoMoreInteractions;
import static org.mockito.Mockito.when;
import static org.robolectric.Shadows.shadowOf;

import static org.chromium.chrome.browser.app.ChromeActivity.HANDOFF_SDK_VERSION;

import android.app.Activity;
import android.os.Build;
import android.os.Process;
import android.os.UserManager;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.InOrder;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Robolectric;
import org.robolectric.annotation.Config;
import org.robolectric.shadows.ShadowLooper;
import org.robolectric.util.ReflectionHelpers;

import org.chromium.base.supplier.ObservableSuppliers;
import org.chromium.base.supplier.SettableNullableObservableSupplier;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.base.test.util.UserActionTester;
import org.chromium.blink.mojom.TextFragmentReceiver;
import org.chromium.cc.mojom.RootScrollOffsetUpdateFrequency;
import org.chromium.chrome.browser.ActivityTabProvider;
import org.chromium.chrome.browser.ExternalIntentUrlChecker;
import org.chromium.chrome.browser.ExternalIntentUrlCheckerJni;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabSelectionType;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.content.browser.GestureListenerManagerImpl;
import org.chromium.content.browser.RenderCoordinatesImpl;
import org.chromium.content_public.browser.RenderFrameHost;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.url.GURL;

import java.lang.reflect.Method;
import java.util.Collections;
import java.util.concurrent.TimeUnit;

/** Unit tests for {@link HandoffController}. */
@RunWith(BaseRobolectricTestRunner.class)
// TODO(crbug.com/503422619): Update to 37 once its available, and remove the delegate in
//  HandoffController.java.
@Config(sdk = 35)
public class HandoffControllerUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private TabModelSelector mTabModelSelector;
    @Mock private Tab mTab;
    @Mock private HandoffController.Delegate mDelegate;
    @Mock private ExternalIntentUrlChecker.Natives mExternalIntentUrlCheckerJni;
    @Mock private WebContents mWebContents;
    @Mock private RenderFrameHost mMainFrame;
    @Mock private WindowAndroid mWindowAndroid;
    @Mock private TextFragmentReceiver.Proxy mTextFragmentReceiver;
    @Mock private RenderCoordinatesImpl mRenderCoordinates;
    @Mock private GestureListenerManagerImpl mGestureListenerManager;

    private Activity mActivity;
    private SettableNullableObservableSupplier<Tab> mCurrentTabSupplier;
    private ActivityTabProvider mActivityTabProvider;
    private HandoffController mController;
    private UserActionTester mUserActionTester;

    @Before
    public void setUp() {
        mUserActionTester = new UserActionTester();
        // Force SDK_INT to HANDOFF_SDK_VERSION so the controller logic executes.
        ReflectionHelpers.setStaticField(Build.VERSION.class, "SDK_INT", HANDOFF_SDK_VERSION);

        ExternalIntentUrlCheckerJni.setInstanceForTesting(mExternalIntentUrlCheckerJni);
        RenderCoordinatesImpl.setInstanceForTesting(mRenderCoordinates);
        GestureListenerManagerImpl.setInstanceForTesting(mGestureListenerManager);

        mActivity = Robolectric.buildActivity(Activity.class).get();
        mCurrentTabSupplier = ObservableSuppliers.createNullable();
        when(mTabModelSelector.getCurrentTabSupplier()).thenReturn(mCurrentTabSupplier);

        mActivityTabProvider = new ActivityTabProvider();

        when(mTabModelSelector.isIncognitoBrandedModelSelected()).thenReturn(false);
        when(mTabModelSelector.getModels()).thenReturn(Collections.emptyList());
        when(mTab.isOffTheRecord()).thenReturn(false);
        when(mTab.isIncognitoBranded()).thenReturn(false);
        when(mTab.getUrl()).thenReturn(new GURL("https://example.com"));

        // By default, standard web URLs are safe.
        when(mExternalIntentUrlCheckerJni.validateUrl(any()))
                .thenAnswer(
                        invocation -> {
                            GURL url = invocation.getArgument(0);
                            return url.getScheme().equals("https")
                                    || url.getScheme().equals("http");
                        });

        mActivityTabProvider.setForTesting(mTab);
    }

    private void initializeController() {
        mController =
                new HandoffController(
                        mActivity, mTabModelSelector, mActivityTabProvider, mDelegate);
        ShadowLooper.idleMainLooper();
    }

    private void setUpExtractableWebContents() {
        when(mTab.isHidden()).thenReturn(false);
        when(mTab.getWebContents()).thenReturn(mWebContents);
        when(mWebContents.isDestroyed()).thenReturn(false);
        when(mWebContents.getTopLevelNativeWindow()).thenReturn(mWindowAndroid);
        when(mWebContents.getMainFrame()).thenReturn(mMainFrame);
        when(mRenderCoordinates.getScrollYPixInt()).thenReturn(50);
        when(mMainFrame.isRenderFrameLive()).thenReturn(true);
        when(mMainFrame.getInterfaceToRendererFrame(TextFragmentReceiver.MANAGER))
                .thenReturn(mTextFragmentReceiver);
    }

    private void simulateExtractedTextFragment(String selector) {
        ShadowLooper.idleMainLooper(
                HandoffController.FETCH_TEXT_DEBOUNCE_MS, TimeUnit.MILLISECONDS);
        ArgumentCaptor<TextFragmentReceiver.RequestSelectorForViewportCenter_Response> captor =
                ArgumentCaptor.forClass(
                        TextFragmentReceiver.RequestSelectorForViewportCenter_Response.class);
        verify(mTextFragmentReceiver, atLeastOnce())
                .requestSelectorForViewportCenter(captor.capture());
        captor.getValue().call(selector, /* error= */ 0, /* readyStatus= */ 0);
    }

    private void disallowHandoffByPolicy() {
        shadowOf(mActivity.getSystemService(UserManager.class))
                .setUserRestriction(Process.myUserHandle(), "disallow_handoff", true);
    }

    @Test
    public void testUpdateHandoffState_Initialization_Enabled() {
        HistogramWatcher watcher =
                HistogramWatcher.newSingleRecordWatcher("Android.Handoff.Enabled.TabSwitch", true);
        initializeController();

        verify(mDelegate, atLeastOnce()).setHandoffEnabled(eq(mActivity), eq(true));
        watcher.assertExpected();
    }

    @Test
    public void testUpdateHandoffState_StandardWebUrl_Enabled() {
        when(mTab.getUrl()).thenReturn(new GURL("https://google.com"));
        mController =
                new HandoffController(
                        mActivity, mTabModelSelector, mActivityTabProvider, mDelegate);
        ShadowLooper.idleMainLooper();

        verify(mDelegate, atLeastOnce()).setHandoffEnabled(eq(mActivity), eq(true));
    }

    @Test
    public void testUpdateHandoffState_Initialization_Incognito_Disabled() {
        when(mTabModelSelector.isIncognitoBrandedModelSelected()).thenReturn(true);
        when(mTab.isIncognitoBranded()).thenReturn(true);
        HistogramWatcher watcher =
                HistogramWatcher.newBuilder()
                        .expectNoRecords("Android.Handoff.Enabled.TabSwitch")
                        .build();
        initializeController();

        verify(mDelegate, atLeastOnce()).setHandoffEnabled(eq(mActivity), eq(false));
        watcher.assertExpected();
    }

    @Test
    public void testUpdateHandoffState_Initialization_Policy_Disabled() {
        disallowHandoffByPolicy();
        HistogramWatcher watcher =
                HistogramWatcher.newBuilder()
                        .expectNoRecords("Android.Handoff.Enabled.TabSwitch")
                        .build();
        initializeController();

        verify(mDelegate, atLeastOnce()).setHandoffEnabled(eq(mActivity), eq(false));
        watcher.assertExpected();
    }

    @Test
    public void testOnObservingDifferentTab_Enabled() {
        initializeController();
        when(mDelegate.isHandoffEnabled(mActivity)).thenReturn(false);
        clearInvocations(mDelegate);

        Tab tabB = mock(Tab.class);
        when(tabB.isIncognitoBranded()).thenReturn(false);
        when(tabB.getUrl()).thenReturn(new GURL("https://google.com"));

        HistogramWatcher watcher =
                HistogramWatcher.newSingleRecordWatcher("Android.Handoff.Enabled.TabSwitch", true);

        mActivityTabProvider.setForTesting(tabB);
        ShadowLooper.idleMainLooper();

        verify(mDelegate, atLeastOnce()).setHandoffEnabled(eq(mActivity), eq(true));
        watcher.assertExpected();
    }

    @Test
    public void testUpdateHandoffState_UnsafeUrl_Disabled() {
        when(mTab.getUrl()).thenReturn(new GURL("chrome://settings"));
        mController =
                new HandoffController(
                        mActivity, mTabModelSelector, mActivityTabProvider, mDelegate);
        ShadowLooper.idleMainLooper();

        verify(mDelegate, atLeastOnce()).setHandoffEnabled(eq(mActivity), eq(false));
    }

    @Test
    public void testUpdateHandoffState_SafeInternalUrl_Enabled() {
        when(mTab.getUrl()).thenReturn(new GURL("chrome://dino"));
        mController =
                new HandoffController(
                        mActivity, mTabModelSelector, mActivityTabProvider, mDelegate);
        ShadowLooper.idleMainLooper();

        verify(mDelegate, atLeastOnce()).setHandoffEnabled(eq(mActivity), eq(true));
    }

    @Test
    public void testUpdateHandoffState_FileUrl_Disabled() {
        when(mTab.getUrl()).thenReturn(new GURL("file:///sdcard/test.html"));
        mController =
                new HandoffController(
                        mActivity, mTabModelSelector, mActivityTabProvider, mDelegate);
        ShadowLooper.idleMainLooper();

        verify(mDelegate, atLeastOnce()).setHandoffEnabled(eq(mActivity), eq(false));
    }

    @Test
    public void testUpdateHandoffState_ContentUrl_Disabled() {
        when(mTab.getUrl()).thenReturn(new GURL("content://com.example.provider/file"));
        mController =
                new HandoffController(
                        mActivity, mTabModelSelector, mActivityTabProvider, mDelegate);
        ShadowLooper.idleMainLooper();

        verify(mDelegate, atLeastOnce()).setHandoffEnabled(eq(mActivity), eq(false));
    }

    @Test
    public void testUpdateHandoffState_PdfUrl_Disabled() {
        when(mTab.getUrl())
                .thenReturn(
                        new GURL(
                                "chrome-native://pdf/"
                                    + "link?url=content%3A%2F%2Fcom.example.provider%2Ffile.pdf"));
        mController =
                new HandoffController(
                        mActivity, mTabModelSelector, mActivityTabProvider, mDelegate);
        ShadowLooper.idleMainLooper();

        verify(mDelegate, atLeastOnce()).setHandoffEnabled(eq(mActivity), eq(false));
    }

    @Test
    public void testOnHandoffActivityDataRequested_UnsafeUrl_ReturnsNull() throws Exception {
        initializeController();
        when(mTab.getUrl()).thenReturn(new GURL("chrome://flags"));

        var data = callOnHandoffActivityDataRequested(null);

        assertNull(data);
    }

    @Test
    public void testOnHandoffActivityDataRequested_Success() throws Exception {
        initializeController();
        Object mockRequest = mock(Class.forName("android.app.HandoffActivityDataRequestInfo"));
        callOnHandoffActivityDataRequested(mockRequest);

        verify(mDelegate).buildHandoffActivityData(eq(mActivity), eq("https://example.com/"));
        assertTrue(mUserActionTester.getActions().contains("HandoffDataRequested"));
    }

    @Test
    public void testOnHandoffActivityDataRequested_WithTextFragment_AppendsFragment()
            throws Exception {
        // 1. Simulate a scrolled tab (scrollY = 50) where Blink extracts "hello%20world".
        setUpExtractableWebContents();
        initializeController();
        simulateExtractedTextFragment("hello%20world");

        // 2. Request Handoff payload and verify "#:~:text=hello%20world" is appended to the URL.
        Object mockRequest = mock(Class.forName("android.app.HandoffActivityDataRequestInfo"));
        callOnHandoffActivityDataRequested(mockRequest);

        verify(mDelegate)
                .buildHandoffActivityData(
                        eq(mActivity), eq("https://example.com/#:~:text=hello%20world"));
    }

    @Test
    public void testUnscrolledTab_SkipsMojoFetch() {
        // 1. Tab is at the top of the page (scrollY = 0).
        setUpExtractableWebContents();
        when(mRenderCoordinates.getScrollYPixInt()).thenReturn(0);

        // 2. Advance past the 300ms debounce window and verify no Mojo IPC was sent to Blink.
        initializeController();
        ShadowLooper.idleMainLooper(
                HandoffController.FETCH_TEXT_DEBOUNCE_MS, TimeUnit.MILLISECONDS);

        verifyNoMoreInteractions(mTextFragmentReceiver);
    }

    @Test
    public void testOnHandoffActivityDataRequested_PreservesExistingUrlHash() throws Exception {
        // 1. Start on a URL that already has an HTML "#section-1" anchor and extract a selector.
        when(mTab.getUrl()).thenReturn(new GURL("https://example.com/page#section-1"));
        setUpExtractableWebContents();
        initializeController();
        simulateExtractedTextFragment("hello%20world");

        // 2. Verify ":~:text=hello%20world" is appended after "#section-1" without overwriting it.
        Object mockRequest = mock(Class.forName("android.app.HandoffActivityDataRequestInfo"));
        callOnHandoffActivityDataRequested(mockRequest);

        verify(mDelegate)
                .buildHandoffActivityData(
                        eq(mActivity),
                        eq("https://example.com/page#section-1:~:text=hello%20world"));
    }

    @Test
    public void testOnScroll_RetainsPreviousTextFragmentUntilRefetchAndClearsAtTop()
            throws Exception {
        // 1. Extract an initial selector, then start a new scroll without finishing the 300ms
        // debounce; Handoff should fall back to the prior selector until the new fetch completes.
        setUpExtractableWebContents();
        initializeController();
        simulateExtractedTextFragment("prior%20fragment");
        mController.getGestureStateListenerForTesting().onScrollStarted(50, 100, false);
        mController.getGestureStateListenerForTesting().onScrollEnded(80, 100);

        Object mockRequest = mock(Class.forName("android.app.HandoffActivityDataRequestInfo"));
        callOnHandoffActivityDataRequested(mockRequest);

        verify(mDelegate)
                .buildHandoffActivityData(
                        eq(mActivity), eq("https://example.com/#:~:text=prior%20fragment"));

        // 2. Scrolling back to the top of the page (scrollY == 0) immediately clears the selector.
        mController.getGestureStateListenerForTesting().onScrollEnded(0, 100);
        callOnHandoffActivityDataRequested(mockRequest);
        verify(mDelegate).buildHandoffActivityData(eq(mActivity), eq("https://example.com/"));

        // 3. Navigating to a new URL clears the cached selector from the previous page.
        mController.getGestureStateListenerForTesting().onScrollEnded(50, 100);
        simulateExtractedTextFragment("restored%20fragment");
        when(mTab.getUrl()).thenReturn(new GURL("https://example.com/other"));
        mController.getActiveTabObserverForTesting().onUrlUpdated(mTab);
        callOnHandoffActivityDataRequested(mockRequest);
        verify(mDelegate).buildHandoffActivityData(eq(mActivity), eq("https://example.com/other"));
    }

    @Test
    public void testOnHandoffActivityDataRequested_EmptyFragment_StripsExistingTextDirective()
            throws Exception {
        // 1. Tab URL has both "#section-1" and a stale ":~:text=old%20quote", and is unscrolled.
        when(mTab.getUrl())
                .thenReturn(new GURL("https://example.com/page#section-1:~:text=old%20quote"));
        initializeController();

        // 2. Verify the stale ":~:text=" suffix is stripped while "#section-1" is preserved.
        Object mockRequest = mock(Class.forName("android.app.HandoffActivityDataRequestInfo"));
        callOnHandoffActivityDataRequested(mockRequest);

        verify(mDelegate)
                .buildHandoffActivityData(eq(mActivity), eq("https://example.com/page#section-1"));
    }

    @Test
    public void testOnHandoffActivityDataRequested_ReplacesExistingTextDirective()
            throws Exception {
        // 1. Tab URL has an old ":~:text=old%20quote", and the user scrolls to "new%20quote".
        when(mTab.getUrl())
                .thenReturn(new GURL("https://example.com/page#section-1:~:text=old%20quote"));
        setUpExtractableWebContents();
        initializeController();
        simulateExtractedTextFragment("new%20quote");

        // 2. Verify ":~:text=old%20quote" is replaced by ":~:text=new%20quote".
        Object mockRequest = mock(Class.forName("android.app.HandoffActivityDataRequestInfo"));
        callOnHandoffActivityDataRequested(mockRequest);

        verify(mDelegate)
                .buildHandoffActivityData(
                        eq(mActivity), eq("https://example.com/page#section-1:~:text=new%20quote"));
    }

    @Test
    public void testOnHandoffActivityDataRequested_EmptyFragment_StripsStandaloneTextDirective()
            throws Exception {
        // 1. Tab URL has a standalone "#:~:text=old%20quote" (no "#anchor" before it) at top.
        when(mTab.getUrl()).thenReturn(new GURL("https://example.com/page#:~:text=old%20quote"));
        initializeController();

        // 2. Verify the entire "#:~:text=" fragment is removed so no trailing "#" remains.
        Object mockRequest = mock(Class.forName("android.app.HandoffActivityDataRequestInfo"));
        callOnHandoffActivityDataRequested(mockRequest);

        verify(mDelegate).buildHandoffActivityData(eq(mActivity), eq("https://example.com/page"));
    }

    @Test
    public void testScrollAndFlingGestures_RefetchTextFragment() throws Exception {
        // 1. Extract an initial selector before flinging.
        setUpExtractableWebContents();
        initializeController();
        simulateExtractedTextFragment("initial");

        // 2. Fling start cancels the prior state, and fling end schedules a fresh extraction.
        mController.getGestureStateListenerForTesting().onFlingStartGesture(0, 100, false);
        mController.getGestureStateListenerForTesting().onFlingEndGesture(50, 100);
        simulateExtractedTextFragment("after%20fling");

        // 3. Verify the outgoing URL uses the post-fling selector.
        Object mockRequest = mock(Class.forName("android.app.HandoffActivityDataRequestInfo"));
        callOnHandoffActivityDataRequested(mockRequest);

        verify(mDelegate)
                .buildHandoffActivityData(
                        eq(mActivity), eq("https://example.com/#:~:text=after%20fling"));
    }

    @Test
    public void testTabHiddenAndShown_SkipsWhileHiddenAndRefetchesOnShow() throws Exception {
        // 1. Scrolling while the tab is hidden skips the Mojo IPC during the debounce callback.
        setUpExtractableWebContents();
        initializeController();
        clearInvocations(mTextFragmentReceiver);

        when(mTab.isHidden()).thenReturn(true);
        mController.getGestureStateListenerForTesting().onScrollEnded(50, 100);
        ShadowLooper.idleMainLooper(
                HandoffController.FETCH_TEXT_DEBOUNCE_MS, TimeUnit.MILLISECONDS);
        verifyNoMoreInteractions(mTextFragmentReceiver);

        // 2. Showing the tab again re-triggers the pending selector extraction.
        when(mTab.isHidden()).thenReturn(false);
        mController.getActiveTabObserverForTesting().onShown(mTab, TabSelectionType.FROM_USER);
        simulateExtractedTextFragment("shown%20fragment");

        Object mockRequest = mock(Class.forName("android.app.HandoffActivityDataRequestInfo"));
        callOnHandoffActivityDataRequested(mockRequest);
        verify(mDelegate)
                .buildHandoffActivityData(
                        eq(mActivity), eq("https://example.com/#:~:text=shown%20fragment"));
    }

    @Test
    public void testOnContentChanged_RebindsListenerAndClearsTextFragment() throws Exception {
        // 1. Extract an initial selector on the original WebContents.
        setUpExtractableWebContents();
        initializeController();
        simulateExtractedTextFragment("initial%20fragment");
        verify(mGestureListenerManager)
                .addListener(
                        eq(mController.getGestureStateListenerForTesting()),
                        eq(RootScrollOffsetUpdateFrequency.ON_SCROLL_END));
        clearInvocations(mGestureListenerManager, mTextFragmentReceiver);

        // 2. Swap WebContents on the active tab (unscrolled) and fire onContentChanged.
        WebContents newWebContents = mock(WebContents.class);
        when(mTab.getWebContents()).thenReturn(newWebContents);
        when(mRenderCoordinates.getScrollYPixInt()).thenReturn(0);
        mController.getActiveTabObserverForTesting().onContentChanged(mTab);

        // 3. Verify the gesture listener is detached from the old WebContents and attached to the
        // new one, and the cached text fragment is cleared.
        InOrder inOrder = inOrder(mGestureListenerManager);
        inOrder.verify(mGestureListenerManager)
                .removeListener(eq(mController.getGestureStateListenerForTesting()));
        inOrder.verify(mGestureListenerManager)
                .addListener(
                        eq(mController.getGestureStateListenerForTesting()),
                        eq(RootScrollOffsetUpdateFrequency.ON_SCROLL_END));

        Object mockRequest = mock(Class.forName("android.app.HandoffActivityDataRequestInfo"));
        callOnHandoffActivityDataRequested(mockRequest);
        verify(mDelegate).buildHandoffActivityData(eq(mActivity), eq("https://example.com/"));
    }

    @Test
    public void testOnPageLoadFinished_SchedulesFetchWhenScrolled() throws Exception {
        // 1. Initialize at scrollY == 0 so no extraction is scheduled before loading finishes.
        setUpExtractableWebContents();
        when(mRenderCoordinates.getScrollYPixInt()).thenReturn(0);
        initializeController();

        // 2. Restore scroll position (scrollY > 0) when page load finishes and verify extraction.
        when(mRenderCoordinates.getScrollYPixInt()).thenReturn(50);
        mController
                .getActiveTabObserverForTesting()
                .onPageLoadFinished(mTab, new GURL("https://example.com"));
        simulateExtractedTextFragment("loaded%20fragment");

        Object mockRequest = mock(Class.forName("android.app.HandoffActivityDataRequestInfo"));
        callOnHandoffActivityDataRequested(mockRequest);
        verify(mDelegate)
                .buildHandoffActivityData(
                        eq(mActivity), eq("https://example.com/#:~:text=loaded%20fragment"));
    }

    @Test
    public void testStaleMojoCallbackAfterScroll_IsIgnored() throws Exception {
        // 1. Capture the in-flight Mojo callback before the renderer replies.
        setUpExtractableWebContents();
        initializeController();
        ShadowLooper.idleMainLooper(
                HandoffController.FETCH_TEXT_DEBOUNCE_MS, TimeUnit.MILLISECONDS);

        ArgumentCaptor<TextFragmentReceiver.RequestSelectorForViewportCenter_Response> captor =
                ArgumentCaptor.forClass(
                        TextFragmentReceiver.RequestSelectorForViewportCenter_Response.class);
        verify(mTextFragmentReceiver).requestSelectorForViewportCenter(captor.capture());

        // 2. Starting a new scroll increments mFetchGeneration, so the late callback is ignored.
        mController.getGestureStateListenerForTesting().onScrollStarted(0, 100, false);
        captor.getValue().call("stale%20selector", /* error= */ 0, /* readyStatus= */ 0);

        Object mockRequest = mock(Class.forName("android.app.HandoffActivityDataRequestInfo"));
        callOnHandoffActivityDataRequested(mockRequest);

        verify(mDelegate).buildHandoffActivityData(eq(mActivity), eq("https://example.com/"));
    }

    @Test
    public void testOnHandoffActivityDataRequested_IncognitoTab_ReturnsNull() throws Exception {
        initializeController();
        when(mTab.isOffTheRecord()).thenReturn(true);
        when(mTab.isIncognitoBranded()).thenReturn(true);

        var data = callOnHandoffActivityDataRequested(null);

        assertNull(data);
    }

    @Test
    public void testOnHandoffActivityDataRequested_Policy_Disabled_ReturnsNull() throws Exception {
        initializeController();
        disallowHandoffByPolicy();

        var data = callOnHandoffActivityDataRequested(null);

        assertNull(data);
    }

    @Test
    public void testOnHandoffActivityDataRequested_NoTab_ReturnsNull() throws Exception {
        initializeController();
        mActivityTabProvider.setForTesting(null);

        var data = callOnHandoffActivityDataRequested(null);

        assertNull(data);
    }

    @Test
    public void testCurrentTabSupplier_TriggersUpdate() {
        initializeController();
        when(mDelegate.isHandoffEnabled(mActivity)).thenReturn(true);

        // Change state to incognito
        when(mTabModelSelector.isIncognitoBrandedModelSelected()).thenReturn(true);
        when(mTab.isIncognitoBranded()).thenReturn(true);

        // Trigger observer
        mCurrentTabSupplier.set(mTab);

        verify(mDelegate, atLeastOnce()).setHandoffEnabled(eq(mActivity), eq(false));
        assertTrue(mUserActionTester.getActions().contains("HandoffDisabled"));
    }

    @Test
    public void testOnUrlUpdated_MultipleTimes_SameUrl_TriggersOnlyOneUpdate() {
        initializeController();
        when(mDelegate.isHandoffEnabled(mActivity)).thenReturn(false);
        clearInvocations(mDelegate);

        GURL newUrl = new GURL("https://new.url");
        when(mTab.getUrl()).thenReturn(newUrl);

        HistogramWatcher watcher =
                HistogramWatcher.newSingleRecordWatcher(
                        "Android.Handoff.Enabled.UrlNavigation", true);
        mController.getActiveTabObserverForTesting().onUrlUpdated(mTab);
        watcher.assertExpected();

        mController.getActiveTabObserverForTesting().onUrlUpdated(mTab);

        // Now, multiple onUrlUpdated calls with same URL should only trigger one update.
        verify(mDelegate, times(1)).setHandoffEnabled(eq(mActivity), eq(true));
    }

    @Test
    public void testOnUrlUpdated_AfterTabSwitch_TriggersOnlyOneUpdate() {
        initializeController();
        when(mDelegate.isHandoffEnabled(mActivity)).thenReturn(true);
        clearInvocations(mDelegate);

        // 2. Switch to Tab B (URL: google.com)
        Tab tabB = mock(Tab.class);
        when(tabB.getUrl()).thenReturn(new GURL("https://google.com"));

        mActivityTabProvider.setForTesting(tabB);
        ShadowLooper.idleMainLooper();

        // Verify toggle - Force a state reset by toggling to false before re-enabling Handoff.
        // Handoff is already enabled, so it should reset.
        InOrder inOrder = inOrder(mDelegate);
        inOrder.verify(mDelegate).setHandoffEnabled(eq(mActivity), eq(false));
        inOrder.verify(mDelegate).setHandoffEnabled(eq(mActivity), eq(true));
        clearInvocations(mDelegate);

        // 3. onUrlUpdated fires for Tab B.
        // It should be deduplicated because onObservingDifferentTab already set mTabLastUrlSeen to
        // google.com.
        mController.getActiveTabObserverForTesting().onUrlUpdated(tabB);
        verifyNoMoreInteractions(mDelegate);
    }

    @Test
    public void testOnUrlUpdated_AfterIncognitoToggle_NotTriggered() {
        initializeController();
        when(mDelegate.isHandoffEnabled(mActivity)).thenReturn(true);
        clearInvocations(mDelegate);

        // 2. Toggle incognito (disables handoff)
        when(mTabModelSelector.isIncognitoBrandedModelSelected()).thenReturn(true);
        mCurrentTabSupplier.set(mTab);
        verify(mDelegate, atLeastOnce()).setHandoffEnabled(eq(mActivity), eq(false));
        clearInvocations(mDelegate);

        // 3. Toggle back (enables handoff)
        when(mTabModelSelector.isIncognitoBrandedModelSelected()).thenReturn(false);
        when(mDelegate.isHandoffEnabled(mActivity)).thenReturn(false);
        mCurrentTabSupplier.set(null);
        mCurrentTabSupplier.set(mTab);
        verify(mDelegate, atLeastOnce()).setHandoffEnabled(eq(mActivity), eq(true));
        clearInvocations(mDelegate);

        // 4. onUrlUpdated fires with same URL. Should be deduplicated.
        mController.getActiveTabObserverForTesting().onUrlUpdated(mTab);
        verifyNoMoreInteractions(mDelegate);
    }

    @Test
    public void testOnUrlUpdated_InIncognito_NotTriggered() {
        initializeController();
        // 1. Setup an incognito tab
        Tab incognitoTab = mock(Tab.class);
        when(incognitoTab.isIncognitoBranded()).thenReturn(true);
        when(incognitoTab.getUrl()).thenReturn(new GURL("https://incognito.com"));

        // 2. Switch to it (this will call onObservingDifferentTab and disable handoff)
        mActivityTabProvider.setForTesting(incognitoTab);
        ShadowLooper.idleMainLooper();
        clearInvocations(mDelegate);

        // 3. Trigger URL update on the incognito tab
        mController.getActiveTabObserverForTesting().onUrlUpdated(incognitoTab);

        // 4. Verify that updateHandoffState was NEVER called
        verifyNoMoreInteractions(mDelegate);
    }

    private Object callOnHandoffActivityDataRequested(Object requestInfo) throws Exception {
        Method method =
                HandoffController.class.getDeclaredMethod(
                        "onHandoffActivityDataRequested",
                        Class.forName("android.app.HandoffActivityDataRequestInfo"));
        return method.invoke(mController, requestInfo);
    }
}
