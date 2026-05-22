// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.media.immersive_playback.components;

import android.view.View;
import android.widget.RadioButton;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.media.immersive_playback.ImmersiveVideoFormatRadioGroup;
import org.chromium.chrome.browser.modules.xr.R;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.xr.scenecore.XrPanelEntityHolder;
import org.chromium.ui.xr.scenecore.XrSpace;

/** View binder for the immersive playback format selection panel. */
@NullMarked
public class ImmersiveVideoFormatViewBinder {
    private static final float[] IDENTITY_ROTATION = new float[] {0f, 0f, 0f, 1f};

    /**
     * Binds the model to the view for a specific property key.
     *
     * @param model The property model.
     * @param view The spatial view wrapper.
     * @param propertyKey The property key to bind.
     */
    public static void bind(
            PropertyModel model, ImmersiveVideoFormatSpatialView view, PropertyKey propertyKey) {
        if (propertyKey == ImmersiveVideoFormatProperties.SELECTED_STEREO_MODE
                || propertyKey == ImmersiveVideoFormatProperties.SELECTED_PROJECTION_TYPE) {
            int stereoMode = model.get(ImmersiveVideoFormatProperties.SELECTED_STEREO_MODE);
            int projectionType = model.get(ImmersiveVideoFormatProperties.SELECTED_PROJECTION_TYPE);

            View foundView = view.androidView.findViewById(R.id.format_radio_group);
            assert foundView instanceof ImmersiveVideoFormatRadioGroup;
            checkFormatOption(
                    (ImmersiveVideoFormatRadioGroup) foundView, stereoMode, projectionType);
        } else if (propertyKey == ImmersiveVideoFormatProperties.DEFAULT_SPATIAL_WIDTH
                || propertyKey == ImmersiveVideoFormatProperties.DEFAULT_SPATIAL_HEIGHT) {
            Float width = model.get(ImmersiveVideoFormatProperties.DEFAULT_SPATIAL_WIDTH);
            Float height = model.get(ImmersiveVideoFormatProperties.DEFAULT_SPATIAL_HEIGHT);
            if (width != null && height != null && width > 0f && height > 0f) {
                view.spatialEntityHolder.setEntitySize(width, height);
                updatePose(model, view.spatialEntityHolder);
            }
        } else if (propertyKey == ImmersiveVideoFormatProperties.PARENT_WIDTH
                || propertyKey == ImmersiveVideoFormatProperties.PARENT_HEIGHT) {
            updatePose(model, view.spatialEntityHolder);
        } else if (propertyKey == ImmersiveVideoFormatProperties.DEFAULT_CORNER_RADIUS) {
            Float radius = model.get(ImmersiveVideoFormatProperties.DEFAULT_CORNER_RADIUS);
            if (radius != null) {
                view.spatialEntityHolder.setEntityCornerRadius(radius);
            }
        }
    }

    private static void updatePose(PropertyModel model, XrPanelEntityHolder<?> holder) {
        Float width = model.get(ImmersiveVideoFormatProperties.DEFAULT_SPATIAL_WIDTH);
        Float height = model.get(ImmersiveVideoFormatProperties.DEFAULT_SPATIAL_HEIGHT);
        Float parentWidth = model.get(ImmersiveVideoFormatProperties.PARENT_WIDTH);
        Float parentHeight = model.get(ImmersiveVideoFormatProperties.PARENT_HEIGHT);

        if (width != null
                && height != null
                && parentWidth != null
                && parentHeight != null
                && width > 0f
                && height > 0f
                && parentWidth > 0f
                && parentHeight > 0f) {
            float[] translation =
                    new float[] {parentWidth / 2 - width / 2, parentHeight / 2 + height / 2, 0f};
            holder.setEntityPose(translation, IDENTITY_ROTATION, XrSpace.PARENT);
        }
    }

    private static void checkFormatOption(
            ImmersiveVideoFormatRadioGroup radioGroup, int stereoMode, int projectionType) {
        for (int i = 0; i < radioGroup.getChildCount(); i++) {
            View child = radioGroup.getChildAt(i);
            if (child instanceof RadioButton) {
                Object tag = child.getTag();
                assert tag instanceof ImmersiveVideoFormatRadioGroup.FormatOption;
                ImmersiveVideoFormatRadioGroup.FormatOption option =
                        (ImmersiveVideoFormatRadioGroup.FormatOption) tag;
                if (option.stereoMode == stereoMode && option.projectionType == projectionType) {
                    if (radioGroup.getCheckedRadioButtonId() != child.getId()) {
                        radioGroup.check(child.getId());
                    }
                    break;
                }
            }
        }
    }
}
