// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.contextmenu;

import static org.hamcrest.CoreMatchers.equalTo;
import static org.hamcrest.MatcherAssert.assertThat;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.anyLong;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.app.Activity;
import android.graphics.Bitmap;

import androidx.test.ext.junit.rules.ActivityScenarioRule;

import org.junit.Assert;
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

import org.chromium.base.Callback;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.JniMocker;
import org.chromium.blink_public.common.ContextMenuDataMediaType;
import org.chromium.chrome.browser.performance_hints.PerformanceHintsObserver;
import org.chromium.chrome.browser.performance_hints.PerformanceHintsObserver.PerformanceClass;
import org.chromium.chrome.browser.performance_hints.PerformanceHintsObserverJni;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.components.embedder_support.contextmenu.ContextMenuParams;
import org.chromium.components.favicon.LargeIconBridge;
import org.chromium.components.favicon.LargeIconBridgeJni;
import org.chromium.ui.base.TestActivity;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.url.GURL;
import org.chromium.url.JUnitTestGURLs;

/**
 * Unit tests for the context menu header mediator.
 */
@RunWith(BaseRobolectricTestRunner.class)
public class ContextMenuHeaderMediatorTest {
    @Rule
    public TestRule mProcessor = new Features.JUnitProcessor();
    @Rule
    public JniMocker mocker = new JniMocker();
    @Rule
    public ActivityScenarioRule<TestActivity> mActivityScenarioRule =
            new ActivityScenarioRule<>(TestActivity.class);

    @Mock
    PerformanceHintsObserver.Natives mMockPerformanceHintsObserverJni;
    @Mock
    LargeIconBridge.Natives mMockLargeIconBridgeJni;
    @Mock
    ContextMenuNativeDelegate mNativeDelegate;

    private Activity mActivity;
    private final Profile mProfile = Mockito.mock(Profile.class);

    @Captor
    ArgumentCaptor<Callback<Bitmap>> mRetrieveImageCallbackCaptor;
    @Captor
    ArgumentCaptor<LargeIconBridge.LargeIconCallback> mLargeIconCallbackCaptor;

    @Before
    public void setUpTest() {
        mActivityScenarioRule.getScenario().onActivity((activity) -> mActivity = activity);
        MockitoAnnotations.initMocks(this);
        mocker.mock(PerformanceHintsObserverJni.TEST_HOOKS, mMockPerformanceHintsObserverJni);
        mocker.mock(LargeIconBridgeJni.TEST_HOOKS, mMockLargeIconBridgeJni);

        when(mMockLargeIconBridgeJni.init()).thenReturn(1L);
    }

    @Test
    public void testPerformanceInfoEnabled() {
        final GURL url = JUnitTestGURLs.getGURL(JUnitTestGURLs.URL_1);
        when(mMockPerformanceHintsObserverJni.isContextMenuPerformanceInfoEnabled())
                .thenReturn(true);
        PropertyModel model = new PropertyModel.Builder(ContextMenuHeaderProperties.ALL_KEYS)
                                      .with(ContextMenuHeaderProperties.URL_PERFORMANCE_CLASS,
                                              PerformanceClass.PERFORMANCE_UNKNOWN)
                                      .build();
        final ContextMenuParams params = new ContextMenuParams(0, ContextMenuDataMediaType.IMAGE,
                url, JUnitTestGURLs.getGURL(JUnitTestGURLs.URL_1_WITH_PATH), "", GURL.emptyGURL(),
                GURL.emptyGURL(), "", null, false, 0, 0, 0, false);
        final ContextMenuHeaderMediator mediator = new ContextMenuHeaderMediator(mActivity, model,
                PerformanceClass.PERFORMANCE_FAST, params, mProfile, mNativeDelegate);
        assertThat(model.get(ContextMenuHeaderProperties.URL_PERFORMANCE_CLASS),
                equalTo(PerformanceClass.PERFORMANCE_FAST));
    }

    @Test
    public void testPerformanceInfoDisabled() {
        final GURL url = JUnitTestGURLs.getGURL(JUnitTestGURLs.URL_1);
        when(mMockPerformanceHintsObserverJni.isContextMenuPerformanceInfoEnabled())
                .thenReturn(false);
        PropertyModel model = new PropertyModel.Builder(ContextMenuHeaderProperties.ALL_KEYS)
                                      .with(ContextMenuHeaderProperties.URL_PERFORMANCE_CLASS,
                                              PerformanceClass.PERFORMANCE_UNKNOWN)
                                      .build();
        final ContextMenuParams params = new ContextMenuParams(0, ContextMenuDataMediaType.IMAGE,
                url, JUnitTestGURLs.getGURL(JUnitTestGURLs.URL_1_WITH_PATH), "", GURL.emptyGURL(),
                GURL.emptyGURL(), "", null, false, 0, 0, 0, false);
        final ContextMenuHeaderMediator mediator = new ContextMenuHeaderMediator(mActivity, model,
                PerformanceClass.PERFORMANCE_FAST, params, mProfile, mNativeDelegate);
        assertThat(model.get(ContextMenuHeaderProperties.URL_PERFORMANCE_CLASS),
                equalTo(PerformanceClass.PERFORMANCE_UNKNOWN));
    }

    @Test
    public void testNoPerformanceInfoOnNonAnchor() {
        final GURL url = JUnitTestGURLs.getGURL(JUnitTestGURLs.EXAMPLE_URL);
        when(mMockPerformanceHintsObserverJni.isContextMenuPerformanceInfoEnabled())
                .thenReturn(true);
        PropertyModel model = new PropertyModel.Builder(ContextMenuHeaderProperties.ALL_KEYS)
                                      .with(ContextMenuHeaderProperties.URL_PERFORMANCE_CLASS,
                                              PerformanceClass.PERFORMANCE_UNKNOWN)
                                      .build();
        final ContextMenuParams params =
                new ContextMenuParams(0, ContextMenuDataMediaType.IMAGE, url, GURL.emptyGURL(), "",
                        GURL.emptyGURL(), GURL.emptyGURL(), "", null, false, 0, 0, 0, false);
        final ContextMenuHeaderMediator mediator = new ContextMenuHeaderMediator(mActivity, model,
                PerformanceClass.PERFORMANCE_FAST, params, mProfile, mNativeDelegate);
        assertThat(model.get(ContextMenuHeaderProperties.URL_PERFORMANCE_CLASS),
                equalTo(PerformanceClass.PERFORMANCE_UNKNOWN));
    }

    @Test
    public void testHeaderImage_Image() {
        PropertyModel model =
                new PropertyModel.Builder(ContextMenuHeaderProperties.ALL_KEYS).build();
        final GURL url = JUnitTestGURLs.getGURL(JUnitTestGURLs.EXAMPLE_URL);
        final ContextMenuParams params =
                new ContextMenuParams(0, ContextMenuDataMediaType.IMAGE, url, GURL.emptyGURL(), "",
                        GURL.emptyGURL(), GURL.emptyGURL(), "", null, false, 0, 0, 0, false);
        final ContextMenuHeaderMediator mediator = new ContextMenuHeaderMediator(mActivity, model,
                PerformanceClass.PERFORMANCE_FAST, params, mProfile, mNativeDelegate);

        verify(mNativeDelegate)
                .retrieveImageForContextMenu(
                        anyInt(), anyInt(), mRetrieveImageCallbackCaptor.capture());
        verify(mMockLargeIconBridgeJni, times(0))
                .getLargeIconForURL(anyLong(), any(), any(), anyInt(), any());

        Assert.assertNotNull(
                "Retrieve image callback is null.", mRetrieveImageCallbackCaptor.getValue());
        Assert.assertNull("Header image should be null before thumbnail callback triggers.",
                model.get(ContextMenuHeaderProperties.IMAGE));

        Bitmap bitmap = Bitmap.createBitmap(1, 1, Bitmap.Config.ARGB_8888);
        mRetrieveImageCallbackCaptor.getValue().onResult(bitmap);
        Assert.assertNotNull("Thumbnail should be set for model in retrieve image callback.",
                model.get(ContextMenuHeaderProperties.IMAGE));
        Assert.assertFalse("Circle background should be invisible for thumbnail.",
                model.get(ContextMenuHeaderProperties.CIRCLE_BG_VISIBLE));
    }

    @Test
    public void testHeaderImage_Video() {
        PropertyModel model =
                new PropertyModel.Builder(ContextMenuHeaderProperties.ALL_KEYS).build();
        final ContextMenuParams params = new ContextMenuParams(0, ContextMenuDataMediaType.VIDEO,
                GURL.emptyGURL(), GURL.emptyGURL(), "", GURL.emptyGURL(), GURL.emptyGURL(), "",
                null, false, 0, 0, 0, false);
        final ContextMenuHeaderMediator mediator = new ContextMenuHeaderMediator(mActivity, model,
                PerformanceClass.PERFORMANCE_FAST, params, mProfile, mNativeDelegate);

        verify(mNativeDelegate, times(0)).retrieveImageForContextMenu(anyInt(), anyInt(), any());
        verify(mMockLargeIconBridgeJni, times(0))
                .getLargeIconForURL(anyLong(), any(), any(), anyInt(), any());

        Assert.assertNotNull("Header image should be set for videos directly.",
                model.get(ContextMenuHeaderProperties.IMAGE));
        Assert.assertTrue("Circle background should be visible for video.",
                model.get(ContextMenuHeaderProperties.CIRCLE_BG_VISIBLE));
    }

    @Test
    public void testHeaderImage_Link() {
        PropertyModel model =
                new PropertyModel.Builder(ContextMenuHeaderProperties.ALL_KEYS).build();
        // Bitmaps created need to have a size set to more than 0.
        model.set(ContextMenuHeaderProperties.MONOGRAM_SIZE_PIXEL, 1);
        final GURL linkUrl = JUnitTestGURLs.getGURL(JUnitTestGURLs.URL_1);
        final ContextMenuParams params = new ContextMenuParams(0, ContextMenuDataMediaType.FILE,
                GURL.emptyGURL(), linkUrl, JUnitTestGURLs.URL_1, GURL.emptyGURL(), GURL.emptyGURL(),
                "", null, false, 0, 0, 0, false);
        final ContextMenuHeaderMediator mediator = new ContextMenuHeaderMediator(mActivity, model,
                PerformanceClass.PERFORMANCE_FAST, params, mProfile, mNativeDelegate);

        verify(mNativeDelegate, times(0)).retrieveImageForContextMenu(anyInt(), anyInt(), any());
        verify(mMockLargeIconBridgeJni)
                .getLargeIconForURL(
                        anyLong(), any(), any(), anyInt(), mLargeIconCallbackCaptor.capture());

        Assert.assertNotNull("LargeIconCallback is null.", mLargeIconCallbackCaptor.getValue());
        Assert.assertNull("Image should not be set for links before LarIconCallback triggers.",
                model.get(ContextMenuHeaderProperties.IMAGE));

        Bitmap bitmap = Bitmap.createBitmap(1, 2, Bitmap.Config.ARGB_8888);
        mLargeIconCallbackCaptor.getValue().onLargeIconAvailable(bitmap, 0, false, 0);
        Assert.assertNotNull("Header image should be set after LargeIconCallback.",
                model.get(ContextMenuHeaderProperties.IMAGE));
        Assert.assertTrue("Circle background should be visible for links.",
                model.get(ContextMenuHeaderProperties.CIRCLE_BG_VISIBLE));
    }
}
