// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.educational_tip.two_cell;

import static org.junit.Assert.assertEquals;
import static org.mockito.Mockito.verify;

import static org.chromium.chrome.browser.educational_tip.two_cell.EducationalTipModuleTwoCellProperties.ITEM_1_COMPLETED_ICON;
import static org.chromium.chrome.browser.educational_tip.two_cell.EducationalTipModuleTwoCellProperties.ITEM_1_ICON;
import static org.chromium.chrome.browser.educational_tip.two_cell.EducationalTipModuleTwoCellProperties.ITEM_1_MARK_COMPLETED;
import static org.chromium.chrome.browser.educational_tip.two_cell.EducationalTipModuleTwoCellProperties.ITEM_2_COMPLETED_ICON;
import static org.chromium.chrome.browser.educational_tip.two_cell.EducationalTipModuleTwoCellProperties.ITEM_2_ICON;
import static org.chromium.chrome.browser.educational_tip.two_cell.EducationalTipModuleTwoCellProperties.ITEM_2_MARK_COMPLETED;

import android.app.Activity;
import android.view.View;
import android.widget.ImageView;
import android.widget.TextView;

import androidx.test.filters.SmallTest;

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
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.educational_tip.R;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

/** Tests for {@link EducationalTipModuleTwoCellViewBinder}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public final class EducationalTipModuleTwoCellViewBinderUnitTest {
    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    private Activity mActivity;
    private EducationalTipModuleTwoCellView mModuleView;
    @Mock private EducationalTipModuleTwoCellView mMockView;
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
    @SmallTest
    public void testSetModuleTitle() {
        mPropertyModelChangeProcessor =
                PropertyModelChangeProcessor.create(
                        mModel, mModuleView, EducationalTipModuleTwoCellViewBinder::bind);
        TextView moduleTitleView = mModuleView.findViewById(R.id.educational_tip_module_title);
        assertEquals(
                mActivity.getString(R.string.educational_tip_module_title),
                moduleTitleView.getText().toString());

        String expectedTitle = "Test Module Title";
        mModel.set(EducationalTipModuleTwoCellProperties.MODULE_TITLE, expectedTitle);
        assertEquals(expectedTitle, moduleTitleView.getText().toString());
    }

    @Test
    @SmallTest
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
    @SmallTest
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
    @SmallTest
    public void testSetItem1Icon() {
        mPropertyModelChangeProcessor =
                PropertyModelChangeProcessor.create(
                        mModel, mMockView, EducationalTipModuleTwoCellViewBinder::bind);
        int expectedRes = R.drawable.default_browser_promo_logo;
        mModel.set(ITEM_1_ICON, expectedRes);
        verify(mMockView).setItem1Icon(expectedRes);
    }

    @Test
    @SmallTest
    public void testSetItem1CompletedIcon() {
        mPropertyModelChangeProcessor =
                PropertyModelChangeProcessor.create(
                        mModel, mMockView, EducationalTipModuleTwoCellViewBinder::bind);
        int expectedRes = R.drawable.setup_list_completed_background_wavy_circle;
        mModel.set(ITEM_1_COMPLETED_ICON, expectedRes);
        verify(mMockView).setItem1IconWithAnimation(expectedRes);
    }

    @Test
    @SmallTest
    public void testSetItem1MarkCompleted() {
        mPropertyModelChangeProcessor =
                PropertyModelChangeProcessor.create(
                        mModel, mMockView, EducationalTipModuleTwoCellViewBinder::bind);
        mModel.set(ITEM_1_MARK_COMPLETED, true);
        verify(mMockView).setItem1Completed(true);
    }

    @Test
    @SmallTest
    public void testSetItem2Icon() {
        mPropertyModelChangeProcessor =
                PropertyModelChangeProcessor.create(
                        mModel, mMockView, EducationalTipModuleTwoCellViewBinder::bind);
        int expectedRes = R.drawable.history_sync_promo_logo;
        mModel.set(ITEM_2_ICON, expectedRes);
        verify(mMockView).setItem2Icon(expectedRes);
    }

    @Test
    @SmallTest
    public void testSetItem2CompletedIcon() {
        mPropertyModelChangeProcessor =
                PropertyModelChangeProcessor.create(
                        mModel, mMockView, EducationalTipModuleTwoCellViewBinder::bind);
        int expectedRes = R.drawable.setup_list_completed_background_wavy_circle;
        mModel.set(ITEM_2_COMPLETED_ICON, expectedRes);
        verify(mMockView).setItem2IconWithAnimation(expectedRes);
    }

    @Test
    @SmallTest
    public void testSetItem2MarkCompleted() {
        mPropertyModelChangeProcessor =
                PropertyModelChangeProcessor.create(
                        mModel, mMockView, EducationalTipModuleTwoCellViewBinder::bind);
        mModel.set(ITEM_2_MARK_COMPLETED, true);
        verify(mMockView).setItem2Completed(true);
    }
}
