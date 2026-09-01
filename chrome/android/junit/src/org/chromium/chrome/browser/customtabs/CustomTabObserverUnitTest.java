// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.anyString;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.content.Intent;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.RuntimeEnvironment;
import org.robolectric.shadows.ShadowLooper;

import org.chromium.base.Callback;
import org.chromium.base.ContextUtils;
import org.chromium.base.FeatureOverrides;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.chrome.browser.browserservices.intents.SessionHolder;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.intents.BrowserIntentUtils;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.content_public.browser.RenderWidgetHostView;
import org.chromium.content_public.browser.WebContents;
import org.chromium.url.GURL;

import java.util.concurrent.TimeUnit;

/** Tests for screenshot capture inside {@link CustomTabObserver}. */
@RunWith(BaseRobolectricTestRunner.class)
@SuppressWarnings("unchecked")
public class CustomTabObserverUnitTest {
    private static final String TEST_URL = "https://example.com/test";
    private static final String TEST_TITLE = "Test Page Title";

    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private Tab mTab;
    @Mock private WebContents mWebContents;
    @Mock private RenderWidgetHostView mRenderWidgetHostView;
    @Mock private CustomTabsConnection mCustomTabsConnection;
    @Mock private SessionHolder<?> mSession;

    private CustomTabObserver mObserver;

    @Before
    public void setUp() {
        ContextUtils.initApplicationContextForTests(RuntimeEnvironment.application);
        CustomTabsConnection.setInstanceForTesting(mCustomTabsConnection);

        when(mTab.getWebContents()).thenReturn(mWebContents);
        when(mTab.getUrl()).thenReturn(new GURL(TEST_URL));
        when(mTab.getTitle()).thenReturn(TEST_TITLE);
        when(mWebContents.getRenderWidgetHostView()).thenReturn(mRenderWidgetHostView);
        when(mCustomTabsConnection.shouldSendNavigationInfoForSession(any())).thenReturn(true);
    }

    private void createObserver() {
        mObserver = new CustomTabObserver(/* openedByChrome= */ false, mSession);
    }

    @Test
    public void testCaptureNavigationInfo_ScreenshotDisabled() {
        FeatureOverrides.overrideFlag(ChromeFeatureList.CCT_NAVIGATION_INFO_SCREENSHOT, false);
        createObserver();

        // Trigger capture via hide
        mObserver.onHidden(mTab, 0);
        ShadowLooper.idleMainLooper(2, TimeUnit.SECONDS);

        // Verify screenshot is NOT captured and sends null screenshot Uri
        verify(mRenderWidgetHostView, never())
                .writeContentBitmapToDiskAsync(anyInt(), anyInt(), anyString(), any());
        verify(mCustomTabsConnection)
                .sendNavigationInfo(eq(mSession), eq(TEST_URL), eq(TEST_TITLE), eq(null));
    }

    @Test
    public void testCaptureNavigationInfo_ScreenshotEnabled() {
        FeatureOverrides.overrideFlag(ChromeFeatureList.CCT_NAVIGATION_INFO_SCREENSHOT, true);
        createObserver();

        // Trigger capture via hide
        mObserver.onHidden(mTab, 0);
        ShadowLooper.idleMainLooper(2, TimeUnit.SECONDS);

        // Verify screenshot capture is initiated on the RenderWidgetHostView
        ArgumentCaptor<Callback<String>> callbackCaptor = ArgumentCaptor.forClass(Callback.class);
        verify(mRenderWidgetHostView)
                .writeContentBitmapToDiskAsync(
                        anyInt(), anyInt(), anyString(), callbackCaptor.capture());

        // Invoke callback with an empty path to trigger synchronous result on the same thread
        callbackCaptor.getValue().onResult("");
        ShadowLooper.idleMainLooper();

        // Verify connection's sendNavigationInfo is invoked
        verify(mCustomTabsConnection)
                .sendNavigationInfo(eq(mSession), eq(TEST_URL), eq(TEST_TITLE), eq(null));
    }

    @Test
    public void testTrackNextPageLoadForHiddenTab_UsedSpeculation() {
        createObserver();
        Intent intent = new Intent();
        BrowserIntentUtils.addLauncherTimestampsToIntent(intent);

        var histogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        "CustomTabs.Startup.TimeToFirstCommitNavigation2.Speculated");

        mObserver.trackNextPageLoadForHiddenTab(
                mWebContents, /* usedSpeculation= */ true, /* hasCommitted= */ true, intent);

        histogramWatcher.assertExpected();
    }

    @Test
    public void testTrackNextPageLoadForHiddenTab_UnusedSpeculation() {
        createObserver();
        Intent intent = new Intent();
        BrowserIntentUtils.addLauncherTimestampsToIntent(intent);

        var histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectNoRecords(
                                "CustomTabs.Startup.TimeToFirstCommitNavigation2.Speculated")
                        .build();

        mObserver.trackNextPageLoadForHiddenTab(
                mWebContents, /* usedSpeculation= */ false, /* hasCommitted= */ false, intent);

        histogramWatcher.assertExpected();
    }
}
