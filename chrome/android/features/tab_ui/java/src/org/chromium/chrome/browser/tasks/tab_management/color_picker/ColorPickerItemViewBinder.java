// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management.color_picker;

import static org.chromium.chrome.browser.tasks.tab_management.TabUiThemeProvider.getColorPickerDialogBackgroundColor;
import static org.chromium.chrome.browser.tasks.tab_management.color_picker.ColorPickerItemProperties.COLOR_ID;
import static org.chromium.chrome.browser.tasks.tab_management.color_picker.ColorPickerItemProperties.COLOR_PICKER_TYPE;
import static org.chromium.chrome.browser.tasks.tab_management.color_picker.ColorPickerItemProperties.IS_INCOGNITO;
import static org.chromium.chrome.browser.tasks.tab_management.color_picker.ColorPickerItemProperties.IS_SELECTED;
import static org.chromium.chrome.browser.tasks.tab_management.color_picker.ColorPickerItemProperties.ITEM_INDEX;
import static org.chromium.chrome.browser.tasks.tab_management.color_picker.ColorPickerItemProperties.ON_CLICK_LISTENER;

import android.content.Context;
import android.content.res.ColorStateList;
import android.content.res.Resources;
import android.graphics.Color;
import android.graphics.RectF;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.view.ViewOverlay;

import androidx.annotation.ColorInt;
import androidx.annotation.StringRes;
import androidx.core.view.AccessibilityDelegateCompat;
import androidx.core.view.ViewCompat;
import androidx.core.view.accessibility.AccessibilityNodeInfoCompat;

import com.google.android.material.button.MaterialButton;
import com.google.android.material.shape.ShapeAppearance;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.tab_ui.R;
import org.chromium.components.browser_ui.styles.SemanticColorUtils;
import org.chromium.components.tab_groups.TabGroupColorPickerUtils;
import org.chromium.ui.drawable.BorderDrawable;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.util.AttrUtils;

/** A binder class for color items on the color picker view. */
@NullMarked
public class ColorPickerItemViewBinder {
    private static final int[] CHECKED_STATE_SET =
            new int[] {android.R.attr.state_checkable, android.R.attr.state_checked};

    // When a color picker item is not selected, a full circle of color is shown. But when an item
    // becomes selected, the desire is to show a UI element mimicking a radio button. This consists
    // of a full circle of color with a ring cut out inside that is transparent to the background
    // color. This is implemented by using a checkable MaterialButton, and when it is checked
    // (selected), a BorderDrawable is added to the button's overlay to draw a border matching the
    // background color, creating the "cut out" visual effect.
    static View createItemView(ViewGroup parent) {
        Context context = parent.getContext();
        View view = LayoutInflater.from(context).inflate(R.layout.color_picker_item, parent, false);
        MaterialButton button = (MaterialButton) view;
        button.setCheckable(true);

        // Save the original shape appearance (which includes state-list from style)
        // so we can restore it after the parent group overrides it.
        ShapeAppearance originalShape = button.getShapeAppearance();
        button.setTag(R.id.tag_original_shape_appearance, originalShape);

        // This is required because the MaterialButtonToggleGroup needs each child button to have a
        // distinct ID.
        button.setId(View.generateViewId());
        return view;
    }

    static void bind(PropertyModel model, View view, PropertyKey propertyKey) {
        if (propertyKey == COLOR_ID) {
            setColorOnColorIcon(model, view);
        } else if (propertyKey == ON_CLICK_LISTENER) {
            view.setOnClickListener((v) -> model.get(ON_CLICK_LISTENER).run());
        } else if (propertyKey == IS_SELECTED) {
            refreshColorIconOnSelection(model, view);
            setAccessibilityContent(
                    view, model.get(IS_SELECTED), model.get(COLOR_ID), model.get(ITEM_INDEX));
        }
    }

    private static void setColorOnColorIcon(PropertyModel model, View view) {
        Context context = view.getContext();
        @ColorPickerType int colorPickerType = model.get(COLOR_PICKER_TYPE);
        boolean isIncognito = model.get(IS_INCOGNITO);
        int colorId = model.get(COLOR_ID);

        final @ColorInt int color = getColor(context, colorPickerType, colorId, isIncognito);

        // Update the color icon with the indicated color id.
        MaterialButton colorIcon = (MaterialButton) view;
        colorIcon.setBackgroundTintList(ColorStateList.valueOf(color));
        colorIcon.setRippleColor(
                TabGroupColorPickerUtils.buildTabGroupColorPickerRippleColorStateList(
                        context, isIncognito));
        view.setOnFocusChangeListener((v, hasFocus) -> refreshColorIconOnSelection(model, v));
    }

    private static void refreshColorIconOnSelection(PropertyModel model, View view) {
        boolean isSelected = model.get(IS_SELECTED);
        boolean isFocused = view.isFocused();

        var button = (MaterialButton) view;
        button.setToggleCheckedStateOnClick(false);
        button.setChecked(isSelected);

        ViewOverlay overlay = view.getOverlay();
        overlay.clear();

        if (isSelected) {
            BorderDrawable borderDrawable = getSelectedBorderDrawable(model, button);
            overlay.add(borderDrawable);
        }
        if (isFocused) {
            BorderDrawable focusDrawable = getFocusBorderDrawable(button, isSelected);
            overlay.add(focusDrawable);
        }

        // Refresh the color item view.
        view.invalidate();
    }

    private static BorderDrawable getSelectedBorderDrawable(
            PropertyModel model, MaterialButton button) {
        Resources res = button.getResources();

        // Background drawable size.
        int sizePx =
                AttrUtils.getDimensionPixelSize(button.getContext(), R.attr.minInteractTargetSize);
        // Inset of the background from the button's bounds.
        int insetPx = button.getInsetTop();
        // ShapeAppearanceModel for the checked state.
        ShapeAppearance shapeAppearance =
                (ShapeAppearance) button.getTag(R.id.tag_original_shape_appearance);
        if (shapeAppearance == null) {
            assert false : "Expected shape appearance tag to be present";
            shapeAppearance = button.getShapeAppearance();
        }
        var shapeAppearanceModel = shapeAppearance.getShapeForState(CHECKED_STATE_SET);
        // Corner size of the checked (rounded rect) background. The reason we pass a RectF to
        // #getCornerSize is because the corner size is calculated based on the bounds of the
        // drawable, e.g. it could be a percentage.
        float cornerSize =
                shapeAppearanceModel
                        .getTopLeftCornerSize()
                        .getCornerSize(new RectF(0, 0, sizePx, sizePx));
        int borderWidthPx = res.getDimensionPixelSize(R.dimen.color_picker_button_stroke_width);
        int borderOuterWidthPx =
                res.getDimensionPixelSize(R.dimen.color_picker_button_stroke_outer_width);

        // The border's corner size needs to be smaller to align with the outer corner radius.
        int borderCornerSizePx = Math.round(cornerSize) - borderWidthPx - borderOuterWidthPx;
        // We want to leave an outline around the button.
        int borderInsetPx = insetPx + borderWidthPx + borderOuterWidthPx;

        BorderDrawable borderDrawable =
                new BorderDrawable(
                        borderWidthPx,
                        borderInsetPx,
                        getColorPickerDialogBackgroundColor(
                                button.getContext(), model.get(IS_INCOGNITO)),
                        borderCornerSizePx);

        // Set the bounds of the drawable to match the color button view.
        borderDrawable.setBounds(0, 0, sizePx, sizePx);
        return borderDrawable;
    }

    private static BorderDrawable getFocusBorderDrawable(
            MaterialButton button, boolean isSelected) {
        Resources res = button.getResources();
        Context context = button.getContext();

        int sizePx = AttrUtils.getDimensionPixelSize(context, R.attr.minInteractTargetSize);
        int insetPx = button.getInsetTop();

        int borderWidthPx = res.getDimensionPixelSize(R.dimen.color_picker_button_stroke_width);
        int offsetPx = res.getDimensionPixelSize(R.dimen.color_picker_button_focus_ring_offset);

        int focusInsetPx = insetPx - offsetPx - (borderWidthPx / 2);
        float focusRadiusPx;
        if (isSelected) {
            ShapeAppearance shapeAppearance =
                    (ShapeAppearance) button.getTag(R.id.tag_original_shape_appearance);
            if (shapeAppearance == null) {
                assert false : "Expected shape appearance tag to be present";
                shapeAppearance = button.getShapeAppearance();
            }
            var shapeAppearanceModel = shapeAppearance.getShapeForState(CHECKED_STATE_SET);
            float cornerSize =
                    shapeAppearanceModel
                            .getTopLeftCornerSize()
                            .getCornerSize(new RectF(0, 0, sizePx, sizePx));
            focusRadiusPx = cornerSize + offsetPx + (borderWidthPx / 2.0f);
        } else {
            focusRadiusPx = sizePx / 2.0f;
        }

        int focusColor = SemanticColorUtils.getColorPrimary(context);

        BorderDrawable borderDrawable =
                new BorderDrawable(borderWidthPx, focusInsetPx, focusColor, focusRadiusPx);
        borderDrawable.setBounds(0, 0, sizePx, sizePx);
        return borderDrawable;
    }

    private static @ColorInt int getColor(
            Context context,
            @ColorPickerType int colorPickerType,
            int colorListIndex,
            boolean isIncognito) {
        if (colorPickerType == ColorPickerType.TAB_GROUP) {
            return TabGroupColorPickerUtils.getTabGroupColorPickerItemColor(
                    context, colorListIndex, isIncognito);
        } else {
            return Color.TRANSPARENT;
        }
    }

    private static void setAccessibilityContent(
            View view, boolean isSelected, int colorId, int position) {
        Resources res = view.getContext().getResources();

        final @StringRes int colorDescRes =
                TabGroupColorPickerUtils.getTabGroupColorPickerItemColorAccessibilityString(
                        colorId);
        String colorDesc = res.getString(colorDescRes);

        // Since the buttons are "checkable", specifying their selected state in the content
        // description is redundant.
        view.setContentDescription(colorDesc);

        ViewCompat.setAccessibilityDelegate(
                view,
                new AccessibilityDelegateCompat() {
                    @Override
                    public void onInitializeAccessibilityNodeInfo(
                            View host, AccessibilityNodeInfoCompat info) {
                        super.onInitializeAccessibilityNodeInfo(host, info);
                        info.setCollectionItemInfo(
                                AccessibilityNodeInfoCompat.CollectionItemInfoCompat.obtain(
                                        /* rowIndex= */ 0,
                                        /* rowSpan= */ 1,
                                        /* columnIndex= */ position,
                                        /* columnSpan= */ 1,
                                        /* heading= */ false,
                                        /* selected= */ isSelected));
                    }
                });
    }
}
