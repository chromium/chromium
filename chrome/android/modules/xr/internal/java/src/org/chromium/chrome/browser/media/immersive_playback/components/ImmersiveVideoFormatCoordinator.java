// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.media.immersive_playback.components;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.app.Activity;
import android.util.SizeF;
import android.view.View;

import androidx.annotation.VisibleForTesting;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.media.immersive_playback.ImmersiveVideoFormatRadioGroup;
import org.chromium.chrome.browser.modules.xr.R;
import org.chromium.content_public.browser.ImmersiveProjectionType;
import org.chromium.content_public.browser.ImmersiveStereoMode;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;
import org.chromium.ui.xr.scenecore.XrEntityHolder;
import org.chromium.ui.xr.scenecore.XrPanelEntityHolder;
import org.chromium.ui.xr.scenecore.XrSceneCoreSessionManager;

/** Coordinator for the format selection panel. */
@NullMarked
public class ImmersiveVideoFormatCoordinator {
    /** Delegate for receiving format selection and hover lifecycle events. */
    public interface Delegate extends ImmersiveVideoFormatMediator.FormatListener {
        /** Called when hover state of the format panel changes. */
        void onFormatPanelHoverChanged(boolean hovered);
    }

    private final PropertyModel mModel =
            new PropertyModel.Builder(ImmersiveVideoFormatProperties.ALL_KEYS)
                    .with(ImmersiveVideoFormatProperties.DEFAULT_SPATIAL_WIDTH, 0.25f)
                    .with(ImmersiveVideoFormatProperties.DEFAULT_SPATIAL_HEIGHT, 0.25f)
                    .with(ImmersiveVideoFormatProperties.DEFAULT_CORNER_RADIUS, 0.024f)
                    .with(
                            ImmersiveVideoFormatProperties.SELECTED_STEREO_MODE,
                            ImmersiveStereoMode.MONO)
                    .with(
                            ImmersiveVideoFormatProperties.SELECTED_PROJECTION_TYPE,
                            ImmersiveProjectionType.QUAD)
                    .build();

    private final Activity mActivity;
    private final XrSceneCoreSessionManager mSessionManager;
    private final Delegate mFormatControlDelegate;
    private @Nullable ImmersiveVideoFormatMediator mMediator;
    private @Nullable XrPanelEntityHolder<?> mHolder;
    private @Nullable ImmersiveVideoFormatView mView;

    /**
     * Creates a new {@link ImmersiveVideoFormatCoordinator}.
     *
     * @param activity The {@link Activity} context.
     * @param sessionManager The {@link XrSceneCoreSessionManager}.
     * @param formatControlDelegate The {@link Delegate} for handling format selections and hover
     *     events.
     */
    public ImmersiveVideoFormatCoordinator(
            Activity activity,
            XrSceneCoreSessionManager sessionManager,
            Delegate formatControlDelegate) {
        mActivity = activity;
        mSessionManager = sessionManager;
        mFormatControlDelegate = formatControlDelegate;
    }

    private void ensureInitialized() {
        if (mHolder != null) return;

        mView = createView();
        mHolder = mSessionManager.createPanelEntity(mView, "FormatSelectionPanel");
        mMediator = new ImmersiveVideoFormatMediator(mFormatControlDelegate, mModel);

        ImmersiveVideoFormatRadioGroup radioView = mView.findViewById(R.id.format_radio_group);
        var mediator = assumeNonNull(mMediator);
        radioView.setOnCheckedChangeListener(
                (group, checkedId) -> mediator.onFormatSelected(radioView.getSelectedFormat()));

        PropertyModelChangeProcessor.create(
                mModel,
                new ImmersiveVideoFormatSpatialView(mView, mHolder),
                ImmersiveVideoFormatViewBinder::bind);
    }

    @VisibleForTesting
    ImmersiveVideoFormatView createView() {
        return new ImmersiveVideoFormatView(mActivity);
    }

    /**
     * Shows the format selection panel.
     *
     * @param parent The parent {@link XrEntityHolder} to attach to.
     * @param parentSize The size of the parent.
     * @param stereoMode The current stereo mode.
     * @param projectionType The current projection type.
     */
    public void show(
            XrEntityHolder<?> parent,
            SizeF parentSize,
            @ImmersiveStereoMode int stereoMode,
            @ImmersiveProjectionType int projectionType) {
        ensureInitialized();

        if (mHolder != null && mMediator != null) {
            mHolder.setParent(parent);
            mHolder.setEntityEnabled(true);
            mMediator.setParentSize(parentSize);
            mMediator.setSelectedFormat(stereoMode, projectionType);
        }
        if (mView != null) {
            mView.setVisibility(View.VISIBLE);
            mView.setHoverListener(mFormatControlDelegate::onFormatPanelHoverChanged);
        }
    }

    /** Dismisses the format selection panel. */
    public void dismiss() {
        if (mView != null) {
            mView.setHoverListener(null);
            mView.setVisibility(View.GONE);
        }
        if (mHolder != null) {
            mHolder.setEntityEnabled(false);
            mHolder.setParent(null);
        }
    }

    /** Disposes the format selection panel. */
    public void dispose() {
        dismiss();
        if (mHolder != null) {
            mHolder.dispose();
            mHolder = null;
        }
    }

    /** Returns true if the panel is showing, false otherwise. */
    public boolean isShowing() {
        return mHolder != null && mHolder.getParent() != null;
    }
}
