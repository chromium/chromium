// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.media.immersive_playback.components;

import android.util.SizeF;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.media.immersive_playback.ImmersiveVideoFormatRadioGroup;
import org.chromium.content_public.browser.ImmersiveProjectionType;
import org.chromium.content_public.browser.ImmersiveStereoMode;
import org.chromium.ui.modelutil.PropertyModel;

/** Mediator for the format selection panel in immersive video playback. */
@NullMarked
public class ImmersiveVideoFormatMediator {

    /** Listener for format selection events. */
    public interface FormatListener {
        /**
         * Called when a format is selected.
         *
         * @param stereoMode The selected stereo mode.
         * @param projectionType The selected projection type.
         */
        void onFormatSelected(int stereoMode, int projectionType);
    }

    private final FormatListener mFormatListener;
    private final PropertyModel mModel;

    /**
     * Creates a new {{@link ImmersiveVideoFormatMediator}}.
     *
     * @param formatListener The {{@link FormatListener}} for format selection events.
     * @param model The {{@link PropertyModel}} to update.
     */
    public ImmersiveVideoFormatMediator(FormatListener formatListener, PropertyModel model) {
        mFormatListener = formatListener;
        mModel = model;
    }

    /**
     * Called when a format is selected in the UI.
     *
     * @param option The {@link ImmersiveVideoFormatRadioGroup.FormatOption} that was selected.
     */
    public void onFormatSelected(ImmersiveVideoFormatRadioGroup.FormatOption option) {
        mModel.set(ImmersiveVideoFormatProperties.SELECTED_STEREO_MODE, option.stereoMode);
        mModel.set(ImmersiveVideoFormatProperties.SELECTED_PROJECTION_TYPE, option.projectionType);
        mFormatListener.onFormatSelected(option.stereoMode, option.projectionType);
    }

    /** Sets the size of the parent entity in the model. */
    public void setParentSize(SizeF parentSize) {
        mModel.set(ImmersiveVideoFormatProperties.PARENT_WIDTH, parentSize.getWidth());
        mModel.set(ImmersiveVideoFormatProperties.PARENT_HEIGHT, parentSize.getHeight());
    }

    /** Sets the selected format options in the model. */
    public void setSelectedFormat(
            @ImmersiveStereoMode int stereoMode, @ImmersiveProjectionType int projectionType) {
        mModel.set(ImmersiveVideoFormatProperties.SELECTED_STEREO_MODE, stereoMode);
        mModel.set(ImmersiveVideoFormatProperties.SELECTED_PROJECTION_TYPE, projectionType);
    }
}
