// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management.pinned_tabs_strip;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.ArgumentMatchers.isNull;
import static org.mockito.Mockito.verify;

import android.app.Activity;
import android.content.res.Resources;
import android.graphics.Bitmap;
import android.graphics.drawable.BitmapDrawable;
import android.graphics.drawable.ColorDrawable;
import android.graphics.drawable.Drawable;
import android.util.Size;
import android.view.LayoutInflater;
import android.widget.ImageView;
import android.widget.TextView;

import androidx.test.filters.SmallTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.ContextUtils;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.BaseActivityTestRule;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.tab_ui.TabListFaviconProvider;
import org.chromium.chrome.browser.tasks.tab_management.TabActionListener;
import org.chromium.chrome.tab_ui.R;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.ui.animation.AnimationHandler;
import org.chromium.ui.base.ViewUtils;
import org.chromium.ui.test.util.BlankUiTestActivity;
import org.chromium.ui.test.util.RenderTestRule;

/** Render tests for {@link PinnedTabStripItemView}. */
@RunWith(ChromeJUnit4ClassRunner.class)
@Batch(Batch.UNIT_TESTS)
public class PinnedTabStripItemViewTest {
    private static final int STRIP_ITEM_WIDTH = 500;
    private static final int STRIP_ITEM_HEIGHT = 80;
    private static final int TAB_ID = 129837;

    @Rule
    public BaseActivityTestRule<BlankUiTestActivity> mActivityTestRule =
            new BaseActivityTestRule<>(BlankUiTestActivity.class);

    @Rule
    public RenderTestRule mRenderTestRule =
            RenderTestRule.Builder.withPublicCorpus()
                    .setBugComponent(
                            RenderTestRule.Component
                                    .UI_BROWSER_MOBILE_TAB_SWITCHER_PINNED_TABS_STRIP)
                    .setRevision(2)
                    .build();

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private TabActionListener mMockTabActionListener;

    private Activity mActivity;
    private PinnedTabStripItemView mView;
    private TabListFaviconProvider.TabFavicon mTabFavicon;
    private TabListFaviconProvider.TabFaviconFetcher mFetcher;

    @Before
    public void setUp() {
        mActivityTestRule.launchActivity(null);
        mActivity = mActivityTestRule.getActivity();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mView =
                            (PinnedTabStripItemView)
                                    LayoutInflater.from(mActivity)
                                            .inflate(R.layout.pinned_tab_strip_item, mView, false);
                    mActivity.setContentView(mView);
                });

        mTabFavicon =
                new TabListFaviconProvider.ResourceTabFavicon(
                        newDrawable(), TabListFaviconProvider.StaticTabFaviconType.ROUNDED_GLOBE);
        mFetcher = callback -> callback.onResult(mTabFavicon);
    }

    @Test
    @SmallTest
    public void testSetTitle() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mView.setTitle("Test Title");
                    TextView titleView = mView.findViewById(R.id.tab_title);
                    assertEquals("Test Title", titleView.getText().toString());
                });
    }

    @Test
    @SmallTest
    public void testSetTrailingIcon() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    Drawable drawable = new ColorDrawable(android.graphics.Color.RED);
                    mView.setTrailingIcon(drawable);
                    ImageView trailingIcon = mView.findViewById(R.id.trailing_icon);
                    assertEquals(drawable, trailingIcon.getDrawable());
                });
    }

    @Test
    @SmallTest
    public void testSetFaviconIcon_NullFetcher() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mView.setFaviconIcon(null, false);
                    ImageView favicon = mView.findViewById(R.id.tab_favicon);
                    assertNull(favicon.getDrawable());
                });
    }

    @Test
    @SmallTest
    public void testSetGridCardSize_NoAnimationWhenWidthIsSame() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mView.getLayoutParams().width = 100;
                    ViewUtils.requestLayout(
                            mView, "PinnedTabStripItemViewTest.testNoAnimationWhenSameWidth");
                });

        CriteriaHelper.pollUiThread(() -> mView.getWidth() == 100);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    Size size = new Size(100, 200);
                    mView.setGridCardSize(size);
                    assertFalse(mView.getWidthAnimationHandlerForTesting().isAnimationPresent());
                });
    }

    @Test
    @SmallTest
    public void testSetGridCardSize_NoAnimationWhenInitialWidthIsZero() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mView.getLayoutParams().width = 0;
                    ViewUtils.requestLayout(
                            mView,
                            "PinnedTabStripItemViewTest.testNoAnimationWhenInitialWidthZero");
                });

        CriteriaHelper.pollUiThread(() -> mView.getWidth() == 0);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    Size size = new Size(100, 200);
                    mView.setGridCardSize(size);
                    assertNotNull(mView.getWidthAnimationHandlerForTesting());
                    assertFalse(mView.getWidthAnimationHandlerForTesting().isAnimationPresent());
                });
    }

    @Test
    @SmallTest
    public void testSetGridCardSize_AnimatesWidth() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mView.getLayoutParams().width = 100;
                    ViewUtils.requestLayout(
                            mView, "PinnedTabStripItemViewTest.testWidthChangeAnimation");
                });

        CriteriaHelper.pollUiThread(() -> mView.getWidth() == 100);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    Size size = new Size(346, 200);
                    mView.setGridCardSize(size);

                    AnimationHandler handler = mView.getWidthAnimationHandlerForTesting();
                    assertTrue(handler.isAnimationPresent());
                });

        CriteriaHelper.pollUiThread(() -> mView.getWidth() == 346);
    }

    @Test
    @SmallTest
    public void testSetFaviconIcon_FetcherReturnsNullFavicon() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mFetcher = callback -> callback.onResult(null);
                    mView.setFaviconIcon(mFetcher, false);
                    ImageView favicon = mView.findViewById(R.id.tab_favicon);
                    assertNull(favicon.getDrawable());
                });
    }

    @Test
    @SmallTest
    public void testSetFaviconIcon_FetcherReturnsValidFavicon_Selected() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mView.setFaviconIcon(mFetcher, true);
                    ImageView faviconView = mView.findViewById(R.id.tab_favicon);
                    assertEquals(mTabFavicon.getSelectedDrawable(), faviconView.getDrawable());
                });
    }

    @Test
    @SmallTest
    public void testSetFaviconIcon_FetcherReturnsValidFavicon_NotSelected() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mView.setFaviconIcon(mFetcher, false);
                    ImageView faviconView = mView.findViewById(R.id.tab_favicon);
                    assertEquals(mTabFavicon.getDefaultDrawable(), faviconView.getDrawable());
                });
    }

    @Test
    @SmallTest
    public void testSetContextClickListener() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mView.setNullableContextClickListener(mMockTabActionListener, mView, TAB_ID);
                    assertTrue(mView.isContextClickable());
                    mView.performContextClick();
                    verify(mMockTabActionListener).run(eq(mView), eq(TAB_ID), isNull());
                });
    }

    @Test
    @SmallTest
    @Feature({"RenderTest"})
    @DisabledTest(message = "https://crbug.com/456490811")
    public void testRenderView() throws Exception {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mView.setTitle("Test Title");
                    mView.setSelected(/* isSelected= */ true, /* isIncognito= */ false);
                    mView.setFaviconIcon(mFetcher, /* isSelected= */ true);
                    mView.setGridCardSize(new Size(STRIP_ITEM_WIDTH, STRIP_ITEM_HEIGHT));
                });
        mRenderTestRule.render(mView, "pinned_tab_strip_item_view_selected");
    }

    @Test
    @SmallTest
    @Feature({"RenderTest"})
    @DisabledTest(message = "https://crbug.com/456503160")
    public void testRenderView_NotSelected() throws Exception {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mView.setTitle("Test Title");
                    mView.setSelected(/* isSelected= */ false, /* isIncognito= */ false);
                    mView.setFaviconIcon(mFetcher, /* isSelected= */ true);
                    mView.setGridCardSize(new Size(STRIP_ITEM_WIDTH, STRIP_ITEM_HEIGHT));
                });
        mRenderTestRule.render(mView, "pinned_tab_strip_item_view_not_selected");
    }

    @Test
    @SmallTest
    @Feature({"RenderTest"})
    @DisabledTest(message = "https://crbug.com/456499455")
    public void testRenderView_Incognito() throws Exception {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mView.setTitle("Test Title");
                    mView.setSelected(/* isSelected= */ true, /* isIncognito= */ true);
                    mView.setFaviconIcon(mFetcher, /* isSelected= */ true);
                    mView.setGridCardSize(new Size(STRIP_ITEM_WIDTH, STRIP_ITEM_HEIGHT));
                });
        mRenderTestRule.render(mView, "pinned_tab_strip_item_view_incognito");
    }

    private Drawable newDrawable() {
        Bitmap image = Bitmap.createBitmap(1, 1, Bitmap.Config.ARGB_8888);
        Resources resources = ContextUtils.getApplicationContext().getResources();
        return new BitmapDrawable(resources, image);
    }
}
