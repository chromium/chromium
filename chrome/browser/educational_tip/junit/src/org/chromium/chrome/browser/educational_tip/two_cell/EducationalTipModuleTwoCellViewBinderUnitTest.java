// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.educational_tip.two_cell;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.verify;

import static org.chromium.chrome.browser.educational_tip.two_cell.EducationalTipModuleTwoCellProperties.ITEM_1_COMPLETED_ICON;
import static org.chromium.chrome.browser.educational_tip.two_cell.EducationalTipModuleTwoCellProperties.ITEM_1_ICON;
import static org.chromium.chrome.browser.educational_tip.two_cell.EducationalTipModuleTwoCellProperties.ITEM_1_MARK_COMPLETED;
import static org.chromium.chrome.browser.educational_tip.two_cell.EducationalTipModuleTwoCellProperties.ITEM_2_COMPLETED_ICON;
import static org.chromium.chrome.browser.educational_tip.two_cell.EducationalTipModuleTwoCellProperties.ITEM_2_ICON;
import static org.chromium.chrome.browser.educational_tip.two_cell.EducationalTipModuleTwoCellProperties.ITEM_2_MARK_COMPLETED;

import android.app.Activity;
import android.graphics.Paint;
import android.view.View;
import android.widget.ImageView;
import android.widget.TextView;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Robolectric;
import org.robolectric.Shadows;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.RobolectricUtil;
import org.chromium.chrome.browser.educational_tip.R;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModel.WritableObjectPropertyKey;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

/** Tests for {@link EducationalTipModuleTwoCellViewBinder}. */
@RunWith(BaseRobolectricTestRunner.class)
public final class EducationalTipModuleTwoCellViewBinderUnitTest {
    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    private Activity mActivity;
    private EducationalTipModuleTwoCellView mModuleView;
    private PropertyModel mModel;
    private PropertyModelChangeProcessor mPropertyModelChangeProcessor;

    @Mock private Runnable mItem1ClickHandler;
    @Mock private Runnable mItem2ClickHandler;

    @Before
    public void setUp() {
        mActivity = Robolectric.buildActivity(Activity.class).setup().get();
        mActivity.setTheme(R.style.Theme_BrowserUI_DayNight);
        mModuleView =
                (EducationalTipModuleTwoCellView)
                        mActivity
                                .getLayoutInflater()
                                .inflate(R.layout.educational_tip_module_two_cell_layout, null);
        mActivity.setContentView(mModuleView);
        mModel = new PropertyModel(EducationalTipModuleTwoCellProperties.ALL_KEYS);
    }

    @After
    public void tearDown() throws Exception {
        if (mPropertyModelChangeProcessor != null) {
            mPropertyModelChangeProcessor.destroy();
        }
    }

    @Test
    public void testSetModuleTitle() {
        mPropertyModelChangeProcessor =
                PropertyModelChangeProcessor.create(
                        mModel, mModuleView, EducationalTipModuleTwoCellViewBinder::bind);
        TextView moduleTitleView = mModuleView.findViewById(R.id.educational_tip_module_title);
        assertEquals(
                mActivity.getString(R.string.educational_tip_module_name),
                moduleTitleView.getText().toString());

        String expectedTitle = "Test Module Title";
        mModel.set(EducationalTipModuleTwoCellProperties.MODULE_TITLE, expectedTitle);
        assertEquals(expectedTitle, moduleTitleView.getText().toString());
    }

    @Test
    public void testSetItem1() {
        mPropertyModelChangeProcessor =
                PropertyModelChangeProcessor.create(
                        mModel, mModuleView, EducationalTipModuleTwoCellViewBinder::bind);
        TextView item1TitleView = mModuleView.findViewById(R.id.two_cell_item_1_title);
        TextView item1DescriptionView = mModuleView.findViewById(R.id.two_cell_item_1_description);
        ImageView item1IconView = mModuleView.findViewById(R.id.two_cell_item_1_icon);
        View item1Layout = mModuleView.findViewById(R.id.two_cell_item_1);

        assertEquals("", item1TitleView.getText().toString());
        assertEquals("", item1DescriptionView.getText().toString());
        Assert.assertNull(item1IconView.getDrawable());

        mModel.set(EducationalTipModuleTwoCellProperties.ITEM_1_TITLE, "Item 1 Title");
        mModel.set(EducationalTipModuleTwoCellProperties.ITEM_1_DESCRIPTION, "Item 1 Desc");
        int item1IconResId = R.drawable.default_browser_promo_logo;
        mModel.set(EducationalTipModuleTwoCellProperties.ITEM_1_ICON, item1IconResId);
        mModel.set(EducationalTipModuleTwoCellProperties.ITEM_1_CLICK_HANDLER, mItem1ClickHandler);

        assertEquals("Item 1 Title", item1TitleView.getText().toString());
        assertEquals("Item 1 Desc", item1DescriptionView.getText().toString());
        Assert.assertNotNull(item1IconView.getDrawable());
        Assert.assertEquals(
                item1IconResId,
                Shadows.shadowOf(item1IconView.getDrawable()).getCreatedFromResId());

        item1Layout.performClick();
        verify(mItem1ClickHandler).run();
    }

    @Test
    public void testSetItem2() {
        mPropertyModelChangeProcessor =
                PropertyModelChangeProcessor.create(
                        mModel, mModuleView, EducationalTipModuleTwoCellViewBinder::bind);
        TextView item2TitleView = mModuleView.findViewById(R.id.two_cell_item_2_title);
        TextView item2DescriptionView = mModuleView.findViewById(R.id.two_cell_item_2_description);
        ImageView item2IconView = mModuleView.findViewById(R.id.two_cell_item_2_icon);
        View item2Layout = mModuleView.findViewById(R.id.two_cell_item_2);

        assertEquals("", item2TitleView.getText().toString());
        assertEquals("", item2DescriptionView.getText().toString());
        Assert.assertNull(item2IconView.getDrawable());

        mModel.set(EducationalTipModuleTwoCellProperties.ITEM_2_TITLE, "Item 2 Title");
        mModel.set(EducationalTipModuleTwoCellProperties.ITEM_2_DESCRIPTION, "Item 2 Desc");
        int item2IconResId = R.drawable.history_sync_promo_logo;
        mModel.set(EducationalTipModuleTwoCellProperties.ITEM_2_ICON, item2IconResId);
        mModel.set(EducationalTipModuleTwoCellProperties.ITEM_2_CLICK_HANDLER, mItem2ClickHandler);

        assertEquals("Item 2 Title", item2TitleView.getText().toString());
        assertEquals("Item 2 Desc", item2DescriptionView.getText().toString());
        Assert.assertNotNull(item2IconView.getDrawable());
        Assert.assertEquals(
                item2IconResId,
                Shadows.shadowOf(item2IconView.getDrawable()).getCreatedFromResId());

        item2Layout.performClick();
        verify(mItem2ClickHandler).run();
    }

    @Test
    public void testSetItem1Icon() {
        verifySetIcon(
                ITEM_1_ICON, R.id.two_cell_item_1_icon, R.drawable.default_browser_promo_logo);
    }

    @Test
    public void testSetItem1CompletedIcon() {
        verifySetCompletedIcon(ITEM_1_COMPLETED_ICON, R.id.two_cell_item_1_icon);
    }

    @Test
    public void testSetItem1MarkCompleted() {
        verifyMarkCompleted(
                ITEM_1_MARK_COMPLETED, R.id.two_cell_item_1_title, R.id.two_cell_item_1);
    }

    @Test
    public void testSetItem2Icon() {
        verifySetIcon(ITEM_2_ICON, R.id.two_cell_item_2_icon, R.drawable.history_sync_promo_logo);
    }

    @Test
    public void testSetItem2CompletedIcon() {
        verifySetCompletedIcon(ITEM_2_COMPLETED_ICON, R.id.two_cell_item_2_icon);
    }

    @Test
    public void testSetItem2MarkCompleted() {
        verifyMarkCompleted(
                ITEM_2_MARK_COMPLETED, R.id.two_cell_item_2_title, R.id.two_cell_item_2);
    }

    private void verifySetIcon(
            WritableObjectPropertyKey<Integer> key, int iconViewId, int expectedRes) {
        mPropertyModelChangeProcessor =
                PropertyModelChangeProcessor.create(
                        mModel, mModuleView, EducationalTipModuleTwoCellViewBinder::bind);
        ImageView iconView = mModuleView.findViewById(iconViewId);
        iconView.setAlpha(0.5f);
        mModel.set(key, expectedRes);
        assertEquals(expectedRes, Shadows.shadowOf(iconView.getDrawable()).getCreatedFromResId());
        assertEquals(1f, iconView.getAlpha(), 0f);
    }

    private void verifySetCompletedIcon(WritableObjectPropertyKey<Integer> key, int iconViewId) {
        mPropertyModelChangeProcessor =
                PropertyModelChangeProcessor.create(
                        mModel, mModuleView, EducationalTipModuleTwoCellViewBinder::bind);
        ImageView iconView = mModuleView.findViewById(iconViewId);
        int expectedRes = R.drawable.setup_list_completed_background_wavy_circle;
        mModel.set(key, expectedRes);
        // The icon is swapped once the fade-out animation ends.
        RobolectricUtil.runAllBackgroundAndUiIncludingDelayed();
        assertEquals(expectedRes, Shadows.shadowOf(iconView.getDrawable()).getCreatedFromResId());
        assertEquals(1f, iconView.getAlpha(), 0f);
    }

    private void verifyMarkCompleted(
            WritableObjectPropertyKey<Boolean> key, int titleViewId, int itemLayoutId) {
        mPropertyModelChangeProcessor =
                PropertyModelChangeProcessor.create(
                        mModel, mModuleView, EducationalTipModuleTwoCellViewBinder::bind);
        TextView titleView = mModuleView.findViewById(titleViewId);
        View itemLayout = mModuleView.findViewById(itemLayoutId);
        itemLayout.setClickable(true);

        mModel.set(key, true);
        assertTrue((titleView.getPaintFlags() & Paint.STRIKE_THRU_TEXT_FLAG) != 0);
        assertFalse(itemLayout.isClickable());
    }
}
