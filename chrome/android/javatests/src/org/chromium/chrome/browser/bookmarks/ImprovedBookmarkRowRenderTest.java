// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyLong;
import static org.mockito.Mockito.doAnswer;

import static org.chromium.components.browser_ui.widget.listmenu.BasicListMenu.buildMenuListItem;

import android.graphics.Bitmap;
import android.graphics.Color;
import android.graphics.drawable.BitmapDrawable;
import android.view.ViewGroup;
import android.widget.FrameLayout;
import android.widget.LinearLayout;

import androidx.test.filters.MediumTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.BaseActivityTestRule;
import org.chromium.base.test.params.ParameterAnnotations;
import org.chromium.base.test.params.ParameterAnnotations.ClassParameter;
import org.chromium.base.test.params.ParameterSet;
import org.chromium.base.test.params.ParameterizedRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.bookmarks.ImprovedBookmarkRowProperties.StartImageVisibility;
import org.chromium.chrome.test.ChromeJUnit4RunnerDelegate;
import org.chromium.chrome.test.util.ChromeRenderTestRule;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.components.browser_ui.widget.listmenu.BasicListMenu;
import org.chromium.components.browser_ui.widget.listmenu.ListMenu;
import org.chromium.components.payments.CurrencyFormatter;
import org.chromium.components.payments.CurrencyFormatterJni;
import org.chromium.components.power_bookmarks.ProductPrice;
import org.chromium.components.power_bookmarks.ShoppingSpecifics;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;
import org.chromium.ui.test.util.BlankUiTestActivity;
import org.chromium.ui.test.util.DisableAnimationsTestRule;
import org.chromium.ui.test.util.NightModeTestUtils;

import java.io.IOException;
import java.util.Arrays;
import java.util.List;

/**
 * Render tests for the improved bookmark row.
 */
@RunWith(ParameterizedRunner.class)
@ParameterAnnotations.UseRunnerDelegate(ChromeJUnit4RunnerDelegate.class)
@Batch(Batch.PER_CLASS)
public class ImprovedBookmarkRowRenderTest {
    private static final long MICRO_CURRENCY_QUOTIENT = 1000000;

    @ClassParameter
    private static List<ParameterSet> sClassParams =
            Arrays.asList(new ParameterSet().value(true, true).name("VisualRow_NightModeEnabled"),
                    new ParameterSet().value(true, false).name("VisualRow_NightModeDisabled"),
                    new ParameterSet().value(false, true).name("CompactRow_NightModeEnabled"),
                    new ParameterSet().value(false, false).name("CompactRow_NightModeDisabled"));

    @Rule
    public final DisableAnimationsTestRule mDisableAnimationsRule = new DisableAnimationsTestRule();
    @Rule
    public MockitoRule mMockitoRule = MockitoJUnit.rule();
    @Rule
    public BaseActivityTestRule<BlankUiTestActivity> mActivityTestRule =
            new BaseActivityTestRule<>(BlankUiTestActivity.class);
    @Rule
    public ChromeRenderTestRule mRenderTestRule =
            ChromeRenderTestRule.Builder.withPublicCorpus()
                    .setBugComponent(ChromeRenderTestRule.Component.UI_BROWSER_BOOKMARKS)
                    .build();
    @Rule
    public TestRule mProcessor = new Features.JUnitProcessor();
    @Rule
    public JniMocker mJniMocker = new JniMocker();

    @Mock
    private CurrencyFormatter.Natives mCurrencyFormatterJniMock;

    private final boolean mUseVisualRowLayout;

    private Bitmap mBitmap;
    private ImprovedBookmarkRow mImprovedBookmarkRow;
    private LinearLayout mContentView;
    private PropertyModel mModel;

    public ImprovedBookmarkRowRenderTest(boolean useVisualRowLayout, boolean nightModeEnabled) {
        mUseVisualRowLayout = useVisualRowLayout;
        mRenderTestRule.setVariantPrefix(mUseVisualRowLayout ? "visual_" : "compact_");

        // Sets a fake background color to make the screenshots easier to compare with bare eyes.
        NightModeTestUtils.setUpNightModeForBlankUiTestActivity(nightModeEnabled);
        mRenderTestRule.setNightModeEnabled(nightModeEnabled);
    }

    @Before
    public void setUp() throws Exception {
        mActivityTestRule.launchActivity(null);
        mActivityTestRule.getActivity().setTheme(R.style.Theme_BrowserUI_DayNight);

        mJniMocker.mock(CurrencyFormatterJni.TEST_HOOKS, mCurrencyFormatterJniMock);
        doAnswer((invocation) -> { return "$" + invocation.getArgument(2); })
                .when(mCurrencyFormatterJniMock)
                .format(anyLong(), any(), any());

        int bitmapSize = mActivityTestRule.getActivity().getResources().getDimensionPixelSize(
                R.dimen.improved_bookmark_favicon_display_size);
        mBitmap = Bitmap.createBitmap(bitmapSize, bitmapSize, Bitmap.Config.ARGB_8888);
        mBitmap.eraseColor(Color.GREEN);

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mContentView = new LinearLayout(mActivityTestRule.getActivity());
            mContentView.setBackgroundColor(Color.WHITE);

            FrameLayout.LayoutParams params = new FrameLayout.LayoutParams(
                    ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT);
            mActivityTestRule.getActivity().setContentView(mContentView, params);

            mImprovedBookmarkRow = ImprovedBookmarkRow.buildView(
                    mActivityTestRule.getActivity(), mUseVisualRowLayout);
            mContentView.removeAllViews();
            mContentView.addView(mImprovedBookmarkRow);

            mModel = new PropertyModel.Builder(ImprovedBookmarkRowProperties.ALL_KEYS)
                             .with(ImprovedBookmarkRowProperties.TITLE, "test title")
                             .with(ImprovedBookmarkRowProperties.DESCRIPTION, "test description")
                             .with(ImprovedBookmarkRowProperties.START_ICON_DRAWABLE,
                                     new BitmapDrawable(
                                             mActivityTestRule.getActivity().getResources(),
                                             mBitmap))
                             .with(ImprovedBookmarkRowProperties.SELECTED, false)
                             .with(ImprovedBookmarkRowProperties.LIST_MENU_BUTTON_DELEGATE,
                                     () -> buildListMenu())
                             .with(ImprovedBookmarkRowProperties.START_IMAGE_VISIBILITY,
                                     StartImageVisibility.DRAWABLE)
                             .with(ImprovedBookmarkRowProperties.START_ICON_TINT, null)
                             .build();

            PropertyModelChangeProcessor.create(
                    mModel, mImprovedBookmarkRow, ImprovedBookmarkRowViewBinder::bind);
        });
    }

    ListMenu buildListMenu() {
        ModelList listItems = new ModelList();
        listItems.add(buildMenuListItem(R.string.bookmark_item_select, 0, 0));
        listItems.add(buildMenuListItem(R.string.bookmark_item_delete, 0, 0));
        listItems.add(buildMenuListItem(R.string.bookmark_item_edit, 0, 0));
        listItems.add(buildMenuListItem(R.string.bookmark_item_move, 0, 0));

        ListMenu.Delegate delegate = item -> {};
        return new BasicListMenu(mActivityTestRule.getActivity(), listItems, delegate);
    }
    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testNormal() throws IOException {
        mRenderTestRule.render(mContentView, "normal");
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testSelected() throws IOException {
        TestThreadUtils.runOnUiThreadBlocking(
                () -> { mModel.set(ImprovedBookmarkRowProperties.SELECTED, true); });
        mRenderTestRule.render(mContentView, "selected");
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testNormal_withAccessoryView() throws IOException {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            ShoppingSpecifics specifics =
                    ShoppingSpecifics.newBuilder()
                            .setCurrentPrice(ProductPrice.newBuilder()
                                                     .setCurrencyCode("USD")
                                                     .setAmountMicros(50 * MICRO_CURRENCY_QUOTIENT)
                                                     .build())
                            .setPreviousPrice(
                                    ProductPrice.newBuilder()
                                            .setCurrencyCode("USD")
                                            .setAmountMicros(100 * MICRO_CURRENCY_QUOTIENT)
                                            .build())
                            .setIsPriceTracked(true)
                            .build();
            ShoppingAccessoryCoordinator coordinator =
                    new ShoppingAccessoryCoordinator(mActivityTestRule.getActivity(), specifics);

            if (mUseVisualRowLayout) {
                mModel.set(ImprovedBookmarkRowProperties.ACCESSORY_VIEW, coordinator.getView());
            }
        });
        mRenderTestRule.render(mContentView, "normal_with_accessory");
    }
}
