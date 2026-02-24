// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.educational_tip.two_cell;

import static org.mockito.Mockito.verify;

import android.content.Context;
import android.graphics.drawable.Drawable;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.ImageView;
import android.widget.TextView;

import androidx.test.core.app.ApplicationProvider;
import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.RobolectricUtil;
import org.chromium.chrome.browser.educational_tip.R;

@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class EducationalTipModuleTwoCellViewUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private View.OnClickListener mSeeMoreClickListener;
    @Mock private View.OnClickListener mItem1ClickListener;
    @Mock private View.OnClickListener mItem2ClickListener;

    private EducationalTipModuleTwoCellView mModuleView;
    private Context mContext;

    @Before
    public void setUp() {
        mContext = ApplicationProvider.getApplicationContext();
        mContext.setTheme(R.style.Theme_BrowserUI_DayNight);
        mModuleView =
                (EducationalTipModuleTwoCellView)
                        LayoutInflater.from(mContext)
                                .inflate(R.layout.educational_tip_module_two_cell_layout, null);
    }

    @Test
    @SmallTest
    public void testSetModuleTitle() {
        TextView moduleTitleView = mModuleView.findViewById(R.id.educational_tip_module_title);
        String testTitle = "Test Module Title";
        mModuleView.setModuleTitle(testTitle);
        Assert.assertEquals(testTitle, moduleTitleView.getText().toString());
    }

    @Test
    @SmallTest
    public void testSetOnClickListenerSeeMore() {
        TextView seeMoreTextView = mModuleView.findViewById(R.id.see_more);
        mModuleView.setSeeMoreOnClickListener(mSeeMoreClickListener);

        seeMoreTextView.performClick();
        RobolectricUtil.runAllBackgroundAndUi();
        verify(mSeeMoreClickListener).onClick(seeMoreTextView);
    }

    @Test
    @SmallTest
    public void testSetItem1() {
        TextView item1TitleView = mModuleView.findViewById(R.id.two_cell_item_1_title);
        TextView item1DescriptionView = mModuleView.findViewById(R.id.two_cell_item_1_description);
        ImageView item1IconView = mModuleView.findViewById(R.id.two_cell_item_1_icon);
        View item1Layout = mModuleView.findViewById(R.id.two_cell_item_1);

        String testTitle = "Item 1 Title";
        String testDescription = "Item 1 Description";
        int testIconResId = R.drawable.default_browser_promo_logo;

        mModuleView.setItem1Title(testTitle);
        mModuleView.setItem1Description(testDescription);
        mModuleView.setItem1Icon(testIconResId);
        mModuleView.setItem1OnClickListener(mItem1ClickListener);

        Assert.assertEquals(testTitle, item1TitleView.getText().toString());
        Assert.assertEquals(testDescription, item1DescriptionView.getText().toString());

        Drawable drawable = item1IconView.getDrawable();
        Assert.assertNotNull(drawable);

        item1Layout.performClick();
        RobolectricUtil.runAllBackgroundAndUi();
        verify(mItem1ClickListener).onClick(item1Layout);
    }

    @Test
    @SmallTest
    public void testSetItem2() {
        TextView item2TitleView = mModuleView.findViewById(R.id.two_cell_item_2_title);
        TextView item2DescriptionView = mModuleView.findViewById(R.id.two_cell_item_2_description);
        ImageView item2IconView = mModuleView.findViewById(R.id.two_cell_item_2_icon);
        View item2Layout = mModuleView.findViewById(R.id.two_cell_item_2);

        String testTitle = "Item 2 Title";
        String testDescription = "Item 2 Description";
        int testIconResId = R.drawable.history_sync_promo_logo;

        mModuleView.setItem2Title(testTitle);
        mModuleView.setItem2Description(testDescription);
        mModuleView.setItem2Icon(testIconResId);
        mModuleView.setItem2OnClickListener(mItem2ClickListener);

        Assert.assertEquals(testTitle, item2TitleView.getText().toString());
        Assert.assertEquals(testDescription, item2DescriptionView.getText().toString());

        Drawable drawable = item2IconView.getDrawable();
        Assert.assertNotNull(drawable);

        item2Layout.performClick();
        RobolectricUtil.runAllBackgroundAndUi();
        verify(mItem2ClickListener).onClick(item2Layout);
    }

    @Test
    @SmallTest
    public void testSetItem1Completed_True() {
        TextView item1TitleView = mModuleView.findViewById(R.id.two_cell_item_1_title);
        TextView item1DescriptionView = mModuleView.findViewById(R.id.two_cell_item_1_description);
        View item1Layout = mModuleView.findViewById(R.id.two_cell_item_1);

        mModuleView.setItem1Completed(true);

        int disabledColor = mContext.getColor(R.color.default_text_color_disabled_list);
        Assert.assertEquals(disabledColor, item1TitleView.getCurrentTextColor());
        Assert.assertTrue(
                (item1TitleView.getPaintFlags() & android.graphics.Paint.STRIKE_THRU_TEXT_FLAG)
                        != 0);
        Assert.assertEquals(disabledColor, item1DescriptionView.getCurrentTextColor());
        Assert.assertTrue(
                (item1DescriptionView.getPaintFlags()
                                & android.graphics.Paint.STRIKE_THRU_TEXT_FLAG)
                        != 0);
        Assert.assertFalse(item1Layout.isClickable());
    }

    @Test
    @SmallTest
    public void testSetItem2Completed_True() {
        TextView item2TitleView = mModuleView.findViewById(R.id.two_cell_item_2_title);
        TextView item2DescriptionView = mModuleView.findViewById(R.id.two_cell_item_2_description);
        View item2Layout = mModuleView.findViewById(R.id.two_cell_item_2);

        mModuleView.setItem2Completed(true);

        int disabledColor = mContext.getColor(R.color.default_text_color_disabled_list);
        Assert.assertEquals(disabledColor, item2TitleView.getCurrentTextColor());
        Assert.assertTrue(
                (item2TitleView.getPaintFlags() & android.graphics.Paint.STRIKE_THRU_TEXT_FLAG)
                        != 0);
        Assert.assertEquals(disabledColor, item2DescriptionView.getCurrentTextColor());
        Assert.assertTrue(
                (item2DescriptionView.getPaintFlags()
                                & android.graphics.Paint.STRIKE_THRU_TEXT_FLAG)
                        != 0);
        Assert.assertFalse(item2Layout.isClickable());
    }

    @Test
    @SmallTest
    public void testSetItem1Completed_DoesNotAffectItem2() {
        TextView item2TitleView = mModuleView.findViewById(R.id.two_cell_item_2_title);
        TextView item2DescriptionView = mModuleView.findViewById(R.id.two_cell_item_2_description);
        View item2Layout = mModuleView.findViewById(R.id.two_cell_item_2);
        // Set an onlick listener to check for clickable state.
        item2Layout.setOnClickListener(mItem2ClickListener);

        // Complete Item 1
        mModuleView.setItem1Completed(true);

        // Assert Item 2 is NOT styled as completed
        Assert.assertNotEquals(
                mContext.getColor(R.color.default_text_color_disabled_list),
                item2TitleView.getCurrentTextColor());
        Assert.assertFalse(
                (item2TitleView.getPaintFlags() & android.graphics.Paint.STRIKE_THRU_TEXT_FLAG)
                        != 0);
        Assert.assertNotEquals(
                mContext.getColor(R.color.default_text_color_disabled_list),
                item2DescriptionView.getCurrentTextColor());
        Assert.assertFalse(
                (item2DescriptionView.getPaintFlags()
                                & android.graphics.Paint.STRIKE_THRU_TEXT_FLAG)
                        != 0);
        Assert.assertTrue(item2Layout.isClickable());
    }
}
