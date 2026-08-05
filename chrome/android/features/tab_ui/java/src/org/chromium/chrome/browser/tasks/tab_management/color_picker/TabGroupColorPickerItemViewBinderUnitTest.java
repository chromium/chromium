// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management.color_picker;

import static androidx.test.espresso.matcher.ViewMatchers.assertThat;

import static org.hamcrest.Matchers.instanceOf;
import static org.junit.Assert.assertEquals;

import static org.chromium.chrome.browser.tasks.tab_management.color_picker.TabGroupColorPickerItemProperties.COLOR_ID;
import static org.chromium.chrome.browser.tasks.tab_management.color_picker.TabGroupColorPickerItemProperties.IS_SELECTED;
import static org.chromium.chrome.browser.tasks.tab_management.color_picker.TabGroupColorPickerItemProperties.ON_CLICK_LISTENER;

import android.app.Activity;
import android.content.res.ColorStateList;
import android.view.View;
import android.widget.FrameLayout;

import androidx.core.view.accessibility.AccessibilityNodeInfoCompat;

import com.google.android.material.button.MaterialButton;
import com.google.android.material.shape.ShapeAppearance;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.Robolectric;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.tasks.tab_management.R;
import org.chromium.components.tab_groups.TabGroupColorId;
import org.chromium.components.tab_groups.TabGroupColorPickerUtils;
import org.chromium.components.tab_groups.TabGroupsFeatureMap;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

/** Tests for TabGroupColorPickerItemViewBinder. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
// TODO(crbug.com/419289558): Re-enable color surface feature flags
@Features.DisableFeatures({
    ChromeFeatureList.ANDROID_SURFACE_COLOR_UPDATE,
    ChromeFeatureList.GRID_TAB_SWITCHER_SURFACE_COLOR_UPDATE,
    TabGroupsFeatureMap.UPDATE_TAB_GROUP_COLORS
})
public class TabGroupColorPickerItemViewBinderUnitTest {
    private Activity mActivity;
    private View mTabGroupColorPickerItemView;
    private PropertyModel mModel;
    private PropertyModelChangeProcessor mPropertyModelChangeProcessor;

    @Before
    public void setUp() throws Exception {
        mActivity = Robolectric.buildActivity(Activity.class).setup().get();
        mActivity.setTheme(R.style.Theme_BrowserUI_DayNight);
        // A placeholder parent FrameLayout is created to provide a ViewGroup context for inflation.
        // This is necessary so that createItemView can resolve and preserve the view's
        // layout parameters (e.g. 48dp touch target size) from the layout XML.
        FrameLayout parent = new FrameLayout(mActivity);
        mTabGroupColorPickerItemView = TabGroupColorPickerItemViewBinder.createItemView(parent);

        mModel =
                TabGroupColorPickerItemProperties.create(
                        TabGroupColorId.BLUE,
                        TabGroupColorPickerType.TAB_GROUP,
                        false,
                        () -> {
                            mModel.set(IS_SELECTED, !mModel.get(IS_SELECTED));
                        },
                        false,
                        /* itemIndex= */ 0);

        mPropertyModelChangeProcessor =
                PropertyModelChangeProcessor.create(
                        mModel,
                        mTabGroupColorPickerItemView,
                        TabGroupColorPickerItemViewBinder::bind);
    }

    @Test
    public void testTabGroupColorPickerItem_color() {
        mModel.get(COLOR_ID);

        View colorButton = mTabGroupColorPickerItemView;
        assertThat(colorButton, instanceOf(MaterialButton.class));
        MaterialButton button = (MaterialButton) colorButton;

        int blueColor =
                TabGroupColorPickerUtils.getTabGroupColorPickerItemColor(
                        mActivity, TabGroupColorId.BLUE, false);
        ColorStateList expectedList =
                TabGroupColorPickerUtils.buildTabGroupColorPickerBackgroundTintList(
                        mActivity, blueColor, false);
        ColorStateList actualList = button.getBackgroundTintList();
        Assert.assertNotNull(actualList);
        assertEquals(expectedList.getDefaultColor(), actualList.getDefaultColor());
        assertEquals(
                expectedList.getColorForState(new int[] {android.R.attr.state_hovered}, 0),
                actualList.getColorForState(new int[] {android.R.attr.state_hovered}, 0));
    }

    @Test
    public void testTabGroupColorPickerItem_onClickListener() {
        mModel.get(ON_CLICK_LISTENER);

        View onClickListener = mTabGroupColorPickerItemView;
        Assert.assertNotNull(onClickListener);
        onClickListener.performClick();
    }

    @Test
    public void testTabGroupColorPickerItem_accessibilityDelegate() {
        AccessibilityNodeInfoCompat info = AccessibilityNodeInfoCompat.obtain();
        mTabGroupColorPickerItemView
                .getAccessibilityDelegate()
                .onInitializeAccessibilityNodeInfo(mTabGroupColorPickerItemView, info.unwrap());

        AccessibilityNodeInfoCompat.CollectionItemInfoCompat itemInfo =
                info.getCollectionItemInfo();
        Assert.assertNotNull(itemInfo);
        assertEquals(0, itemInfo.getRowIndex());
        assertEquals(1, itemInfo.getRowSpan());
        assertEquals(0, itemInfo.getColumnIndex());
        assertEquals(1, itemInfo.getColumnSpan());
        Assert.assertFalse(itemInfo.isSelected());

        mModel.set(IS_SELECTED, true);
        mTabGroupColorPickerItemView
                .getAccessibilityDelegate()
                .onInitializeAccessibilityNodeInfo(mTabGroupColorPickerItemView, info.unwrap());
        itemInfo = info.getCollectionItemInfo();
        Assert.assertTrue(itemInfo.isSelected());
    }

    @Test
    public void testTabGroupColorPickerItem_accessibilityDelegate_customPosition() {
        PropertyModel model =
                TabGroupColorPickerItemProperties.create(
                        TabGroupColorId.BLUE,
                        TabGroupColorPickerType.TAB_GROUP,
                        false,
                        () -> {},
                        false,
                        /* itemIndex= */ 5);
        PropertyModelChangeProcessor.create(
                model, mTabGroupColorPickerItemView, TabGroupColorPickerItemViewBinder::bind);

        AccessibilityNodeInfoCompat info = AccessibilityNodeInfoCompat.obtain();
        mTabGroupColorPickerItemView
                .getAccessibilityDelegate()
                .onInitializeAccessibilityNodeInfo(mTabGroupColorPickerItemView, info.unwrap());

        AccessibilityNodeInfoCompat.CollectionItemInfoCompat itemInfo =
                info.getCollectionItemInfo();
        Assert.assertNotNull(itemInfo);
        assertEquals(5, itemInfo.getColumnIndex());
    }

    @Test
    public void testTabGroupColorPickerItem_isSelected() {
        MaterialButton view = (MaterialButton) mTabGroupColorPickerItemView;
        String color = mActivity.getString(R.string.tab_group_color_blue);

        assertEquals(color, view.getContentDescription());

        ShapeAppearance originalShape =
                (ShapeAppearance) view.getTag(R.id.tag_original_shape_appearance);
        Assert.assertNotNull(originalShape);
        assertEquals(originalShape, view.getShapeAppearance());

        mModel.set(IS_SELECTED, true);

        assertEquals(color, view.getContentDescription());
        Assert.assertTrue(view.isChecked());
        assertEquals(originalShape, view.getShapeAppearance());

        mModel.set(IS_SELECTED, false);
        Assert.assertFalse(view.isChecked());
        assertEquals(originalShape, view.getShapeAppearance());
    }
}
