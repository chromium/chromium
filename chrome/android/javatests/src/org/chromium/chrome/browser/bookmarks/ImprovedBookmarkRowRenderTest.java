// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks;

import static org.chromium.components.browser_ui.widget.listmenu.BasicListMenu.buildMenuListItem;

import android.graphics.Bitmap;
import android.graphics.Color;
import android.graphics.drawable.BitmapDrawable;
import android.view.View;
import android.view.ViewGroup;
import android.widget.FrameLayout;
import android.widget.LinearLayout;

import androidx.test.filters.MediumTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.BaseActivityTestRule;
import org.chromium.base.test.params.ParameterAnnotations;
import org.chromium.base.test.params.ParameterAnnotations.ClassParameter;
import org.chromium.base.test.params.ParameterSet;
import org.chromium.base.test.params.ParameterizedRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.R;
import org.chromium.chrome.test.ChromeJUnit4RunnerDelegate;
import org.chromium.chrome.test.util.ChromeRenderTestRule;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.components.browser_ui.widget.listmenu.BasicListMenu;
import org.chromium.components.browser_ui.widget.listmenu.ListMenu;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
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

    private boolean mUseVisualRowLayout;
    private Bitmap mBitmap;
    private ImprovedBookmarkRow mImprovedBookmarkRow;
    private LinearLayout mContentView;

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

        int bitmapSize = mActivityTestRule.getActivity().getResources().getDimensionPixelSize(
                mUseVisualRowLayout ? R.dimen.improved_bookmark_icon_visual_size
                                    : R.dimen.default_favicon_size);
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
            mImprovedBookmarkRow.setTitle("test title");
            mImprovedBookmarkRow.setDescription("test description");
            mImprovedBookmarkRow.setIcon(
                    new BitmapDrawable(mActivityTestRule.getActivity().getResources(), mBitmap));
            mImprovedBookmarkRow.setListMenu(buildListMenu());
            mImprovedBookmarkRow.setIsSelected(false);
        });
    }

    public static String getViewHierarchy(View v) {
        StringBuilder desc = new StringBuilder();
        getViewHierarchy(v, desc, 0);
        return desc.toString();
    }

    private static void getViewHierarchy(View v, StringBuilder desc, int margin) {
        desc.append(getViewMessage(v, margin));
        if (v instanceof ViewGroup) {
            margin++;
            ViewGroup vg = (ViewGroup) v;
            for (int i = 0; i < vg.getChildCount(); i++) {
                getViewHierarchy(vg.getChildAt(i), desc, margin);
            }
        }
    }

    private static String getViewMessage(View v, int marginOffset) {
        String repeated = new String(new char[marginOffset]).replace("\0", "  ");
        try {
            String resourceId = v.getResources() != null
                    ? (v.getId() > 0 ? v.getResources().getResourceName(v.getId()) : "no_id")
                    : "no_resources";
            return repeated + "[" + v.getClass().getSimpleName() + "] " + resourceId + " ("
                    + (v.getVisibility() == View.VISIBLE ? "Vis" : "Inv") + ")\n";
        } catch (Exception e) {
            return repeated + "[" + v.getClass().getSimpleName() + "] name_not_found\n";
        }
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
        TestThreadUtils.runOnUiThreadBlocking(() -> { mImprovedBookmarkRow.setIsSelected(true); });
        mRenderTestRule.render(mContentView, "selected");
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testNormal_withAccessoryView() throws IOException {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            ShoppingAccessoryView v =
                    ShoppingAccessoryView.buildView(mActivityTestRule.getActivity());
            v.setPriceTracked(true, true);
            v.setPriceInformation(100L, "$100", 50L, "$50");
            if (mUseVisualRowLayout) mImprovedBookmarkRow.setAccessoryView(v);
        });
        mRenderTestRule.render(mContentView, "normal_with_accessory");
    }
}
