// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.contextmenu;

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
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.Callback;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.blink_public.common.ContextMenuDataMediaFlags;
import org.chromium.blink_public.common.ContextMenuDataMediaType;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.embedder_support.contextmenu.ContextMenuNativeDelegate;
import org.chromium.components.embedder_support.contextmenu.ContextMenuParams;
import org.chromium.components.favicon.LargeIconBridge;
import org.chromium.components.favicon.LargeIconBridgeJni;
import org.chromium.ui.base.TestActivity;
import org.chromium.ui.listmenu.ListMenuItemProperties;
import org.chromium.ui.listmenu.MenuModelBridge;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.url.GURL;
import org.chromium.url.JUnitTestGURLs;

/** Unit tests for the context menu header mediator. */
@RunWith(BaseRobolectricTestRunner.class)
public class ContextMenuHeaderMediatorTest {

    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Rule
    public ActivityScenarioRule<TestActivity> mActivityScenarioRule =
            new ActivityScenarioRule<>(TestActivity.class);

    @Mock LargeIconBridge.Natives mMockLargeIconBridgeJni;
    @Mock ContextMenuNativeDelegate mNativeDelegate;
    @Mock private MenuModelBridge mMenuModelBridge;

    private Activity mActivity;
    private final Profile mProfile = Mockito.mock(Profile.class);

    @Captor ArgumentCaptor<Callback<Bitmap>> mRetrieveImageCallbackCaptor;
    @Captor ArgumentCaptor<LargeIconBridge.LargeIconCallback> mLargeIconCallbackCaptor;

    @Before
    public void setUpTest() {
        mActivityScenarioRule.getScenario().onActivity((activity) -> mActivity = activity);
        LargeIconBridgeJni.setInstanceForTesting(mMockLargeIconBridgeJni);

        when(mMockLargeIconBridgeJni.init()).thenReturn(1L);
    }

    @Test
    public void testHeaderImage_Image() {
        PropertyModel model =
                new PropertyModel.Builder(ContextMenuHeaderProperties.ALL_KEYS).build();
        final GURL url = JUnitTestGURLs.EXAMPLE_URL;
        final ContextMenuParams params =
                new ContextMenuParams(
                        0,
                        mMenuModelBridge,
                        ContextMenuDataMediaType.IMAGE,
                        ContextMenuDataMediaFlags.MEDIA_NONE,
                        url,
                        GURL.emptyGURL(),
                        "",
                        GURL.emptyGURL(),
                        GURL.emptyGURL(),
                        "",
                        null,
                        false,
                        0,
                        0,
                        0,
                        false,
                        /* openedFromInterestFor= */ false,
                        /* interestForNodeID= */ 0,
                        /* additionalNavigationParams= */ null);
        final ContextMenuHeaderMediator mediator =
                new ContextMenuHeaderMediator(mActivity, model, params, mProfile, mNativeDelegate);

        verify(mNativeDelegate)
                .retrieveImageForContextMenu(
                        anyInt(), anyInt(), mRetrieveImageCallbackCaptor.capture());
        verify(mMockLargeIconBridgeJni, times(0))
                .getLargeIconForURL(anyLong(), any(), any(), anyInt(), anyInt(), any());

        Assert.assertNotNull(
                "Retrieve image callback is null.", mRetrieveImageCallbackCaptor.getValue());
        Assert.assertNull(
                "Header image should be null before thumbnail callback triggers.",
                model.get(ContextMenuHeaderProperties.IMAGE));

        Bitmap bitmap = Bitmap.createBitmap(1, 1, Bitmap.Config.ARGB_8888);
        mRetrieveImageCallbackCaptor.getValue().onResult(bitmap);
        Assert.assertNotNull(
                "Thumbnail should be set for model in retrieve image callback.",
                model.get(ContextMenuHeaderProperties.IMAGE));
        Assert.assertFalse(
                "Circle background should be invisible for thumbnail.",
                model.get(ContextMenuHeaderProperties.CIRCLE_BG_VISIBLE));
    }

    @Test
    public void testHeaderImage_Video() {
        PropertyModel model =
                new PropertyModel.Builder(ContextMenuHeaderProperties.ALL_KEYS).build();
        final ContextMenuParams params =
                new ContextMenuParams(
                        0,
                        mMenuModelBridge,
                        ContextMenuDataMediaType.VIDEO,
                        ContextMenuDataMediaFlags.MEDIA_NONE,
                        GURL.emptyGURL(),
                        GURL.emptyGURL(),
                        "",
                        GURL.emptyGURL(),
                        GURL.emptyGURL(),
                        "",
                        null,
                        false,
                        0,
                        0,
                        0,
                        false,
                        /* openedFromInterestFor= */ false,
                        /* interestForNodeID= */ 0,
                        /* additionalNavigationParams= */ null);
        final ContextMenuHeaderMediator mediator =
                new ContextMenuHeaderMediator(mActivity, model, params, mProfile, mNativeDelegate);

        verify(mNativeDelegate, times(0)).retrieveImageForContextMenu(anyInt(), anyInt(), any());
        verify(mMockLargeIconBridgeJni, times(0))
                .getLargeIconForURL(anyLong(), any(), any(), anyInt(), anyInt(), any());

        Assert.assertNotNull(
                "Header image should be set for videos directly.",
                model.get(ContextMenuHeaderProperties.IMAGE));
        Assert.assertTrue(
                "Circle background should be visible for video.",
                model.get(ContextMenuHeaderProperties.CIRCLE_BG_VISIBLE));
    }

    @Test
    public void testHeaderImage_Link() {
        PropertyModel model =
                new PropertyModel.Builder(ContextMenuHeaderProperties.ALL_KEYS).build();
        // Bitmaps created need to have a size set to more than 0.
        model.set(ContextMenuHeaderProperties.MONOGRAM_SIZE_PIXEL, 1);
        final GURL linkUrl = JUnitTestGURLs.URL_1;
        final ContextMenuParams params =
                new ContextMenuParams(
                        0,
                        mMenuModelBridge,
                        ContextMenuDataMediaType.FILE,
                        ContextMenuDataMediaFlags.MEDIA_NONE,
                        GURL.emptyGURL(),
                        linkUrl,
                        JUnitTestGURLs.URL_1.getSpec(),
                        GURL.emptyGURL(),
                        GURL.emptyGURL(),
                        "",
                        null,
                        false,
                        0,
                        0,
                        0,
                        false,
                        /* openedFromInterestFor= */ false,
                        /* interestForNodeID= */ 0,
                        /* additionalNavigationParams= */ null);
        final ContextMenuHeaderMediator mediator =
                new ContextMenuHeaderMediator(mActivity, model, params, mProfile, mNativeDelegate);

        verify(mNativeDelegate, times(0)).retrieveImageForContextMenu(anyInt(), anyInt(), any());
        verify(mMockLargeIconBridgeJni)
                .getLargeIconForURL(
                        anyLong(),
                        any(),
                        any(),
                        anyInt(),
                        anyInt(),
                        mLargeIconCallbackCaptor.capture());

        Assert.assertNotNull("LargeIconCallback is null.", mLargeIconCallbackCaptor.getValue());
        Assert.assertNull(
                "Image should not be set for links before LarIconCallback triggers.",
                model.get(ContextMenuHeaderProperties.IMAGE));

        Bitmap bitmap = Bitmap.createBitmap(1, 2, Bitmap.Config.ARGB_8888);
        mLargeIconCallbackCaptor.getValue().onLargeIconAvailable(bitmap, 0, false, 0);
        Assert.assertNotNull(
                "Header image should be set after LargeIconCallback.",
                model.get(ContextMenuHeaderProperties.IMAGE));
        Assert.assertTrue(
                "Circle background should be visible for links.",
                model.get(ContextMenuHeaderProperties.CIRCLE_BG_VISIBLE));
    }

    @Test
    public void testOnClick_ExpandNoSecondaryUrl() {
        PropertyModel model =
                new PropertyModel.Builder(ContextMenuHeaderProperties.ALL_KEYS)
                        .with(ContextMenuHeaderProperties.URL, JUnitTestGURLs.URL_1.getSpec())
                        .with(ListMenuItemProperties.TITLE, "Some Title")
                        .with(ContextMenuHeaderProperties.URL_MAX_LINES, 1)
                        .with(ContextMenuHeaderProperties.TITLE_MAX_LINES, 1)
                        .build();

        final ContextMenuHeaderMediator mediator = createMediatorWithDefaultParams(model);

        mediator.onClick(null);

        Assert.assertEquals(
                "URL max lines should be expanded to show the full text.",
                Integer.MAX_VALUE,
                model.get(ContextMenuHeaderProperties.URL_MAX_LINES));
        Assert.assertEquals(
                "Title max lines should be expanded to show the full text.",
                Integer.MAX_VALUE,
                model.get(ContextMenuHeaderProperties.TITLE_MAX_LINES));
    }

    @Test
    public void testOnClick_CollapseNoSecondaryUrl() {
        PropertyModel model =
                new PropertyModel.Builder(ContextMenuHeaderProperties.ALL_KEYS)
                        .with(ContextMenuHeaderProperties.URL, JUnitTestGURLs.URL_1.getSpec())
                        .with(ListMenuItemProperties.TITLE, "Some Title")
                        .with(ContextMenuHeaderProperties.URL_MAX_LINES, Integer.MAX_VALUE)
                        .with(ContextMenuHeaderProperties.TITLE_MAX_LINES, Integer.MAX_VALUE)
                        .build();

        final ContextMenuHeaderMediator mediator = createMediatorWithDefaultParams(model);

        mediator.onClick(null);

        Assert.assertEquals(
                "URL max lines should be collapsed back to a single line.",
                1,
                model.get(ContextMenuHeaderProperties.URL_MAX_LINES));
        Assert.assertEquals(
                "Title max lines should be collapsed back to a single line.",
                1,
                model.get(ContextMenuHeaderProperties.TITLE_MAX_LINES));
    }

    @Test
    public void testOnClick_CollapseNoSecondaryUrl_TitleOnly() {
        PropertyModel model =
                new PropertyModel.Builder(ContextMenuHeaderProperties.ALL_KEYS)
                        .with(ContextMenuHeaderProperties.URL, "")
                        .with(ListMenuItemProperties.TITLE, "Some Title")
                        .with(ContextMenuHeaderProperties.URL_MAX_LINES, Integer.MAX_VALUE)
                        .with(ContextMenuHeaderProperties.TITLE_MAX_LINES, Integer.MAX_VALUE)
                        .build();

        final ContextMenuHeaderMediator mediator = createMediatorWithDefaultParams(model);

        mediator.onClick(null);

        Assert.assertEquals(
                "URL is empty, so it should still be set to 1 line after collapse.",
                1,
                model.get(ContextMenuHeaderProperties.URL_MAX_LINES));
        Assert.assertEquals(
                "Title should collapse to 2 lines since the URL is empty.",
                2,
                model.get(ContextMenuHeaderProperties.TITLE_MAX_LINES));
    }

    @Test
    public void testOnClick_CollapseNoSecondaryUrl_UrlOnly() {
        PropertyModel model =
                new PropertyModel.Builder(ContextMenuHeaderProperties.ALL_KEYS)
                        .with(ContextMenuHeaderProperties.URL, JUnitTestGURLs.URL_1.getSpec())
                        .with(ListMenuItemProperties.TITLE, "")
                        .with(ContextMenuHeaderProperties.URL_MAX_LINES, Integer.MAX_VALUE)
                        .with(ContextMenuHeaderProperties.TITLE_MAX_LINES, Integer.MAX_VALUE)
                        .build();

        final ContextMenuHeaderMediator mediator = createMediatorWithDefaultParams(model);

        mediator.onClick(null);

        Assert.assertEquals(
                "URL should collapse to 2 lines since the title is empty.",
                2,
                model.get(ContextMenuHeaderProperties.URL_MAX_LINES));
        Assert.assertEquals(
                "Title is empty, so it should still be set to 1 line after collapse.",
                1,
                model.get(ContextMenuHeaderProperties.TITLE_MAX_LINES));
    }

    @Test
    public void testOnClick_ExpandWithSecondaryUrl() {
        PropertyModel model =
                new PropertyModel.Builder(ContextMenuHeaderProperties.ALL_KEYS)
                        .with(ContextMenuHeaderProperties.URL, JUnitTestGURLs.URL_1.getSpec())
                        .with(ListMenuItemProperties.TITLE, "Some Title")
                        .with(
                                ContextMenuHeaderProperties.SECONDARY_URL,
                                JUnitTestGURLs.URL_2.getSpec())
                        .with(ContextMenuHeaderProperties.URL_MAX_LINES, 1)
                        .with(ContextMenuHeaderProperties.TITLE_MAX_LINES, 1)
                        .with(ContextMenuHeaderProperties.SECONDARY_URL_MAX_LINES, 1)
                        .build();

        final ContextMenuHeaderMediator mediator = createMediatorWithDefaultParams(model);

        mediator.onClick(null);

        Assert.assertEquals(
                "URL max lines should be expanded.",
                Integer.MAX_VALUE,
                model.get(ContextMenuHeaderProperties.URL_MAX_LINES));
        Assert.assertEquals(
                "Title max lines should be expanded.",
                Integer.MAX_VALUE,
                model.get(ContextMenuHeaderProperties.TITLE_MAX_LINES));
        Assert.assertEquals(
                "Secondary URL max lines should be expanded.",
                Integer.MAX_VALUE,
                model.get(ContextMenuHeaderProperties.SECONDARY_URL_MAX_LINES));
    }

    @Test
    public void testOnClick_CollapseWithSecondaryUrl() {
        PropertyModel model =
                new PropertyModel.Builder(ContextMenuHeaderProperties.ALL_KEYS)
                        .with(ContextMenuHeaderProperties.URL, JUnitTestGURLs.URL_1.getSpec())
                        .with(ListMenuItemProperties.TITLE, "Some Title")
                        .with(
                                ContextMenuHeaderProperties.SECONDARY_URL,
                                JUnitTestGURLs.URL_2.getSpec())
                        .with(ContextMenuHeaderProperties.URL_MAX_LINES, Integer.MAX_VALUE)
                        .with(ContextMenuHeaderProperties.TITLE_MAX_LINES, Integer.MAX_VALUE)
                        .with(
                                ContextMenuHeaderProperties.SECONDARY_URL_MAX_LINES,
                                Integer.MAX_VALUE)
                        .build();

        final ContextMenuHeaderMediator mediator = createMediatorWithDefaultParams(model);

        mediator.onClick(null);

        Assert.assertEquals(
                "URL max lines should be collapsed to 1.",
                1,
                model.get(ContextMenuHeaderProperties.URL_MAX_LINES));
        Assert.assertEquals(
                "Title max lines should be collapsed to 1.",
                1,
                model.get(ContextMenuHeaderProperties.TITLE_MAX_LINES));
        Assert.assertEquals(
                "Secondary URL max lines should be collapsed to 1.",
                1,
                model.get(ContextMenuHeaderProperties.SECONDARY_URL_MAX_LINES));
    }

    @Test
    public void testOnClick_CollapseWithSecondaryUrl_SecondaryOnly() {
        PropertyModel model =
                new PropertyModel.Builder(ContextMenuHeaderProperties.ALL_KEYS)
                        .with(ContextMenuHeaderProperties.URL, "")
                        .with(ListMenuItemProperties.TITLE, "")
                        .with(
                                ContextMenuHeaderProperties.SECONDARY_URL,
                                JUnitTestGURLs.URL_2.getSpec())
                        .with(ContextMenuHeaderProperties.URL_MAX_LINES, Integer.MAX_VALUE)
                        .with(ContextMenuHeaderProperties.TITLE_MAX_LINES, Integer.MAX_VALUE)
                        .with(
                                ContextMenuHeaderProperties.SECONDARY_URL_MAX_LINES,
                                Integer.MAX_VALUE)
                        .build();

        final ContextMenuHeaderMediator mediator = createMediatorWithDefaultParams(model);

        mediator.onClick(null);

        Assert.assertEquals(
                "URL is empty, its line count should be 1.",
                1,
                model.get(ContextMenuHeaderProperties.URL_MAX_LINES));
        Assert.assertEquals(
                "Title is empty, its line count should be 1.",
                1,
                model.get(ContextMenuHeaderProperties.TITLE_MAX_LINES));
        Assert.assertEquals(
                "Secondary URL should expand to 3 lines since the others are empty.",
                3,
                model.get(ContextMenuHeaderProperties.SECONDARY_URL_MAX_LINES));
    }

    @Test
    public void testOnClick_ExpandWithTertiaryUrl() {
        PropertyModel model =
                new PropertyModel.Builder(ContextMenuHeaderProperties.ALL_KEYS)
                        .with(ContextMenuHeaderProperties.URL, JUnitTestGURLs.URL_1.getSpec())
                        .with(ListMenuItemProperties.TITLE, "Some Title")
                        .with(
                                ContextMenuHeaderProperties.SECONDARY_URL,
                                JUnitTestGURLs.URL_2.getSpec())
                        .with(
                                ContextMenuHeaderProperties.TERTIARY_URL,
                                JUnitTestGURLs.URL_3.getSpec())
                        .with(ContextMenuHeaderProperties.URL_MAX_LINES, 1)
                        .with(ContextMenuHeaderProperties.TITLE_MAX_LINES, 1)
                        .with(ContextMenuHeaderProperties.SECONDARY_URL_MAX_LINES, 1)
                        .with(ContextMenuHeaderProperties.TERTIARY_URL_MAX_LINES, 1)
                        .build();

        final ContextMenuHeaderMediator mediator = createMediatorWithDefaultParams(model);
        mediator.onClick(null);

        Assert.assertEquals(
                "URL max lines should be expanded.",
                Integer.MAX_VALUE,
                model.get(ContextMenuHeaderProperties.URL_MAX_LINES));
        Assert.assertEquals(
                "Title max lines should be expanded.",
                Integer.MAX_VALUE,
                model.get(ContextMenuHeaderProperties.TITLE_MAX_LINES));
        Assert.assertEquals(
                "Secondary URL max lines should be expanded.",
                Integer.MAX_VALUE,
                model.get(ContextMenuHeaderProperties.SECONDARY_URL_MAX_LINES));
        Assert.assertEquals(
                "Tertiary URL max lines should be expanded.",
                Integer.MAX_VALUE,
                model.get(ContextMenuHeaderProperties.TERTIARY_URL_MAX_LINES));
    }

    @Test
    public void testOnClick_CollapseWithTertiaryUrl() {
        PropertyModel model =
                new PropertyModel.Builder(ContextMenuHeaderProperties.ALL_KEYS)
                        .with(ContextMenuHeaderProperties.URL, JUnitTestGURLs.URL_1.getSpec())
                        .with(ListMenuItemProperties.TITLE, "Some Title")
                        .with(
                                ContextMenuHeaderProperties.SECONDARY_URL,
                                JUnitTestGURLs.URL_2.getSpec())
                        .with(
                                ContextMenuHeaderProperties.TERTIARY_URL,
                                JUnitTestGURLs.URL_3.getSpec())
                        .with(ContextMenuHeaderProperties.URL_MAX_LINES, Integer.MAX_VALUE)
                        .with(ContextMenuHeaderProperties.TITLE_MAX_LINES, Integer.MAX_VALUE)
                        .with(
                                ContextMenuHeaderProperties.SECONDARY_URL_MAX_LINES,
                                Integer.MAX_VALUE)
                        .with(ContextMenuHeaderProperties.TERTIARY_URL_MAX_LINES, Integer.MAX_VALUE)
                        .build();

        final ContextMenuHeaderMediator mediator = createMediatorWithDefaultParams(model);
        mediator.onClick(null);

        Assert.assertEquals(
                "URL max lines should be collapsed to 1.",
                1,
                model.get(ContextMenuHeaderProperties.URL_MAX_LINES));
        Assert.assertEquals(
                "Title max lines should be collapsed to 1.",
                1,
                model.get(ContextMenuHeaderProperties.TITLE_MAX_LINES));
        Assert.assertEquals(
                "Secondary URL max lines should be collapsed to 1.",
                1,
                model.get(ContextMenuHeaderProperties.SECONDARY_URL_MAX_LINES));
        Assert.assertEquals(
                "Tertiary URL max lines should be collapsed to 1.",
                1,
                model.get(ContextMenuHeaderProperties.TERTIARY_URL_MAX_LINES));
    }

    @Test
    public void testOnClick_CollapseWithTertiaryUrl_SecondaryAndTertiaryOnly() {
        PropertyModel model =
                new PropertyModel.Builder(ContextMenuHeaderProperties.ALL_KEYS)
                        .with(ContextMenuHeaderProperties.URL, "")
                        .with(ListMenuItemProperties.TITLE, "")
                        .with(
                                ContextMenuHeaderProperties.SECONDARY_URL,
                                JUnitTestGURLs.URL_2.getSpec())
                        .with(
                                ContextMenuHeaderProperties.TERTIARY_URL,
                                JUnitTestGURLs.URL_3.getSpec())
                        .with(ContextMenuHeaderProperties.URL_MAX_LINES, Integer.MAX_VALUE)
                        .with(ContextMenuHeaderProperties.TITLE_MAX_LINES, Integer.MAX_VALUE)
                        .with(
                                ContextMenuHeaderProperties.SECONDARY_URL_MAX_LINES,
                                Integer.MAX_VALUE)
                        .with(ContextMenuHeaderProperties.TERTIARY_URL_MAX_LINES, Integer.MAX_VALUE)
                        .build();

        final ContextMenuHeaderMediator mediator = createMediatorWithDefaultParams(model);
        mediator.onClick(null);

        Assert.assertEquals(
                "URL is empty, its line count should be 1.",
                1,
                model.get(ContextMenuHeaderProperties.URL_MAX_LINES));
        Assert.assertEquals(
                "Title is empty, its line count should be 1.",
                1,
                model.get(ContextMenuHeaderProperties.TITLE_MAX_LINES));
        Assert.assertEquals(
                "Secondary URL should get 2 lines since only one other URL is visible.",
                2,
                model.get(ContextMenuHeaderProperties.SECONDARY_URL_MAX_LINES));
        Assert.assertEquals(
                "Tertiary URL should get 1 line.",
                1,
                model.get(ContextMenuHeaderProperties.TERTIARY_URL_MAX_LINES));
    }

    private ContextMenuHeaderMediator createMediatorWithDefaultParams(PropertyModel model) {
        final ContextMenuParams params =
                new ContextMenuParams(
                        0,
                        mMenuModelBridge,
                        ContextMenuDataMediaType.IMAGE,
                        ContextMenuDataMediaFlags.MEDIA_NONE,
                        GURL.emptyGURL(),
                        GURL.emptyGURL(),
                        "",
                        GURL.emptyGURL(),
                        GURL.emptyGURL(),
                        "",
                        null,
                        false,
                        0,
                        0,
                        0,
                        false,
                        /* openedFromInterestFor= */ false,
                        /* interestForNodeID= */ 0,
                        /* additionalNavigationParams= */ null);
        return new ContextMenuHeaderMediator(mActivity, model, params, mProfile, mNativeDelegate);
    }
}
