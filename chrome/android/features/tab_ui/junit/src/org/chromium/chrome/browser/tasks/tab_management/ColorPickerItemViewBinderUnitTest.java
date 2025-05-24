// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static androidx.test.espresso.matcher.ViewMatchers.assertThat;

import static org.hamcrest.Matchers.instanceOf;
import static org.junit.Assert.assertEquals;

import static org.chromium.chrome.browser.tasks.tab_management.ColorPickerItemProperties.COLOR_ID;
import static org.chromium.chrome.browser.tasks.tab_management.ColorPickerItemProperties.IS_SELECTED;
import static org.chromium.chrome.browser.tasks.tab_management.ColorPickerItemProperties.ON_CLICK_LISTENER;

import android.app.Activity;
import android.content.res.ColorStateList;
import android.graphics.drawable.GradientDrawable;
import android.graphics.drawable.LayerDrawable;
import android.view.View;
import android.widget.ImageView;

import com.google.android.material.button.MaterialButton;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.Robolectric;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.components.browser_ui.styles.SemanticColorUtils;
import org.chromium.components.tab_groups.TabGroupColorId;
import org.chromium.components.tab_groups.TabGroupColorPickerUtils;
import org.chromium.ui.base.ViewUtils;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

/** Tests for ColorPickerItemViewBinder. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
// TODO(crbug.com/419289558): Re-enable color surface feature flags
@Features.DisableFeatures({
    ChromeFeatureList.ANDROID_SURFACE_COLOR_UPDATE,
    ChromeFeatureList.GRID_TAB_SWITCHER_SURFACE_COLOR_UPDATE,
    ChromeFeatureList.GRID_TAB_SWITCHER_UPDATE,
    ChromeFeatureList.ANDROID_THEME_MODULE
})
public class ColorPickerItemViewBinderUnitTest {
    private Activity mActivity;
    private View mColorPickerItemView;
    private PropertyModel mModel;
    private PropertyModelChangeProcessor mPropertyModelChangeProcessor;

    @Before
    public void setUp() throws Exception {
        mActivity = Robolectric.buildActivity(Activity.class).setup().get();
        mActivity.setTheme(R.style.Theme_BrowserUI_DayNight);
        mColorPickerItemView = ColorPickerItemViewBinder.createItemView(mActivity);

        mModel =
                ColorPickerItemProperties.create(
                        TabGroupColorId.BLUE,
                        ColorPickerType.TAB_GROUP,
                        false,
                        () -> {
                            mModel.set(IS_SELECTED, !mModel.get(IS_SELECTED));
                        },
                        false);

        mPropertyModelChangeProcessor =
                PropertyModelChangeProcessor.create(
                        mModel, mColorPickerItemView, ColorPickerItemViewBinder::bind);
    }

    @Test
    public void testColorPickerItem_color() {
        int faviconOuterInset =
                ViewUtils.dpToPx(
                        mActivity,
                        mActivity
                                .getResources()
                                .getDimensionPixelSize(R.dimen.color_picker_selection_layer_inset));
        int faviconInnerInset =
                ViewUtils.dpToPx(
                        mActivity,
                        mActivity
                                .getResources()
                                .getDimensionPixelSize(R.dimen.color_picker_inner_layer_inset));
        mModel.get(COLOR_ID);

        assertThat(
                mColorPickerItemView.findViewById(R.id.color_picker_icon).getBackground(),
                instanceOf(LayerDrawable.class));
        LayerDrawable layerDrawable =
                (LayerDrawable)
                        mColorPickerItemView.findViewById(R.id.color_picker_icon).getBackground();
        assertEquals(3, layerDrawable.getNumberOfLayers());

        // Check outer drawable
        assertThat(layerDrawable.getDrawable(0), instanceOf(GradientDrawable.class));
        GradientDrawable drawable0 = (GradientDrawable) layerDrawable.getDrawable(0);
        assertEquals(GradientDrawable.OVAL, drawable0.getShape());
        assertEquals(
                ColorStateList.valueOf(
                        TabGroupColorPickerUtils.getTabGroupColorPickerItemColor(
                                mActivity, TabGroupColorId.BLUE, false)),
                drawable0.getColor());

        // Check selection drawable
        assertThat(layerDrawable.getDrawable(1), instanceOf(GradientDrawable.class));
        GradientDrawable drawable1 = (GradientDrawable) layerDrawable.getDrawable(1);
        assertEquals(GradientDrawable.OVAL, drawable1.getShape());
        assertEquals(
                ColorStateList.valueOf(SemanticColorUtils.getDialogBgColor(mActivity)),
                drawable1.getColor());

        // Check inner drawable
        assertThat(layerDrawable.getDrawable(2), instanceOf(GradientDrawable.class));
        GradientDrawable drawable2 = (GradientDrawable) layerDrawable.getDrawable(2);
        assertEquals(GradientDrawable.OVAL, drawable2.getShape());
        assertEquals(
                ColorStateList.valueOf(
                        TabGroupColorPickerUtils.getTabGroupColorPickerItemColor(
                                mActivity, TabGroupColorId.BLUE, false)),
                drawable2.getColor());

        // Check layer insets
        assertEquals(faviconOuterInset, layerDrawable.getLayerInsetLeft(1));
        assertEquals(faviconOuterInset, layerDrawable.getLayerInsetTop(1));
        assertEquals(faviconOuterInset, layerDrawable.getLayerInsetRight(1));
        assertEquals(faviconOuterInset, layerDrawable.getLayerInsetBottom(1));
        assertEquals(faviconInnerInset, layerDrawable.getLayerInsetLeft(2));
        assertEquals(faviconInnerInset, layerDrawable.getLayerInsetTop(2));
        assertEquals(faviconInnerInset, layerDrawable.getLayerInsetRight(2));
        assertEquals(faviconInnerInset, layerDrawable.getLayerInsetBottom(2));
    }

    @Test
    @EnableFeatures({ChromeFeatureList.ANDROID_THEME_MODULE})
    public void testColorPickerItem_color_withThemeModuleEnabled() {
        mModel.get(COLOR_ID);

        View colorButton = mColorPickerItemView.findViewById(R.id.color_picker_icon);
        assertThat(colorButton, instanceOf(MaterialButton.class));

        assertEquals(
                ColorStateList.valueOf(
                        TabGroupColorPickerUtils.getTabGroupColorPickerItemColor(
                                mActivity, TabGroupColorId.BLUE, false)),
                colorButton.getBackgroundTintList());
    }

    @Test
    public void testColorPickerItem_onClickListener() {
        mModel.get(ON_CLICK_LISTENER);

        View onClickListener = mColorPickerItemView.findViewById(R.id.color_picker_icon);
        Assert.assertNotNull(onClickListener);
        onClickListener.performClick();
    }

    @Test
    @EnableFeatures({ChromeFeatureList.ANDROID_THEME_MODULE})
    public void testColorPickerItem_onClickListener_withThemeModuleEnabled() {
        mModel.get(ON_CLICK_LISTENER);

        View onClickListener = mColorPickerItemView.findViewById(R.id.color_picker_icon);
        Assert.assertNotNull(onClickListener);
        onClickListener.performClick();
    }

    @Test
    public void testColorPickerItem_isSelected() {
        ImageView imageView = mColorPickerItemView.findViewById(R.id.color_picker_icon);
        LayerDrawable layerDrawable1 = (LayerDrawable) imageView.getBackground();
        String color =
                mActivity.getString(R.string.accessibility_tab_group_color_picker_color_item_blue);
        String notSelectedString =
                mActivity.getString(
                        R.string
                                .accessibility_tab_group_color_picker_color_item_not_selected_description,
                        color);
        String selectedString =
                mActivity.getString(
                        R.string
                                .accessibility_tab_group_color_picker_color_item_selected_description,
                        color);

        assertEquals(0, layerDrawable1.getDrawable(1).getAlpha());
        assertEquals(notSelectedString, imageView.getContentDescription());

        mModel.set(IS_SELECTED, true);

        LayerDrawable layerDrawable2 = (LayerDrawable) imageView.getBackground();
        assertEquals(0xFF, layerDrawable2.getDrawable(1).getAlpha());
        assertEquals(selectedString, imageView.getContentDescription());
    }

    @Test
    @EnableFeatures({ChromeFeatureList.ANDROID_THEME_MODULE})
    public void testColorPickerItem_isSelected_withThemeModuleEnabled() {
        MaterialButton view = mColorPickerItemView.findViewById(R.id.color_picker_icon);
        String color =
                mActivity.getString(R.string.accessibility_tab_group_color_picker_color_item_blue);
        int notSelectedStringId =
                R.string.accessibility_tab_group_color_picker_color_item_not_selected_description;
        String notSelectedString = mActivity.getString(notSelectedStringId, color);
        int selectedStringId =
                R.string.accessibility_tab_group_color_picker_color_item_selected_description;
        String selectedString = mActivity.getString(selectedStringId, color);

        assertEquals(notSelectedString, view.getContentDescription());

        mModel.set(IS_SELECTED, true);

        assertEquals(selectedString, view.getContentDescription());
    }
}
