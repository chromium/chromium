// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.media.immersive_playback.components;

import android.app.Activity;
import android.util.SizeF;
import android.view.View;

import androidx.annotation.VisibleForTesting;

import org.chromium.build.annotations.EnsuresNonNull;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.build.annotations.RequiresNonNull;
import org.chromium.chrome.browser.media.immersive_playback.ImmersiveVideoFormatRadioGroup;
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
    private static final float PIXELS_PER_METER = 1000.0f;

    /** Delegate for receiving format selection and hover lifecycle events. */
    public interface Delegate extends ImmersiveVideoFormatMediator.FormatListener {
        /** Called when hover state of the format panel changes. */
        void onFormatPanelHoverChanged(boolean hovered);

        /** Called when accessibility focus state of the format panel changes. */
        void onFormatPanelAccessibilityFocusChanged(boolean focused);
    }

    private final PropertyModel mModel =
            new PropertyModel.Builder(ImmersiveVideoFormatProperties.ALL_KEYS)
                    .with(ImmersiveVideoFormatProperties.DEFAULT_SPATIAL_WIDTH, 0.25f)
                    .with(ImmersiveVideoFormatProperties.SPATIAL_HEIGHT, 0.25f)
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
    private @Nullable ImmersiveVideoFormatRadioGroup mRadioGroup;
    private boolean mReportFormatSelection = true;

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

    /**
     * Configures the video's native recommended format projection details.
     *
     * @param stereoMode The recommended stereo mode.
     * @param projectionType The recommended projection type.
     */
    public void setRecommendedFormat(
            @ImmersiveStereoMode int stereoMode, @ImmersiveProjectionType int projectionType) {
        mModel.set(ImmersiveVideoFormatProperties.RECOMMENDED_STEREO_MODE, stereoMode);
        mModel.set(ImmersiveVideoFormatProperties.RECOMMENDED_PROJECTION_TYPE, projectionType);
    }

    @EnsuresNonNull({"mMediator", "mHolder", "mView", "mRadioGroup"})
    private void ensureInitialized() {
        if (mHolder != null) {
            assert mMediator != null && mView != null && mRadioGroup != null;
            return;
        }

        mView = createView();
        mView.setAccessibilityFocusListener(
                mFormatControlDelegate::onFormatPanelAccessibilityFocusChanged);
        mRadioGroup = mView.getRadioGroup();
        mHolder = mSessionManager.createPanelEntity(mView, "FormatSelectionPanel");
        mMediator = new ImmersiveVideoFormatMediator(mFormatControlDelegate, mModel);

        mRadioGroup.setSelectionCallback(
                (selectedFormat) -> {
                    if (!mReportFormatSelection || mMediator == null || selectedFormat == null) {
                        return;
                    }
                    mMediator.onFormatSelected(selectedFormat);
                });

        PropertyModelChangeProcessor.create(
                mModel,
                new ImmersiveVideoFormatSpatialView(mView, mHolder),
                ImmersiveVideoFormatViewBinder::bind);

        updateSpatialHeight();
    }

    @RequiresNonNull({"mView", "mMediator"})
    private void updateSpatialHeight() {
        mView.measure(
                View.MeasureSpec.makeMeasureSpec(0, View.MeasureSpec.UNSPECIFIED),
                View.MeasureSpec.makeMeasureSpec(0, View.MeasureSpec.UNSPECIFIED));

        int heightPixels = mView.getMeasuredHeight();
        if (heightPixels > 0) {
            float density = mActivity.getResources().getDisplayMetrics().density;
            float panelHeight = (heightPixels / density) / PIXELS_PER_METER;
            mMediator.setSpatialHeight(panelHeight);
        }
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
     * @param currentStereoMode The current active stereo mode.
     * @param currentProjectionType The current active projection type.
     */
    public void show(
            XrEntityHolder<?> parent,
            SizeF parentSize,
            @ImmersiveStereoMode int currentStereoMode,
            @ImmersiveProjectionType int currentProjectionType) {
        ensureInitialized();

        mView.setVisibility(View.VISIBLE);
        mView.setHoverListener(mFormatControlDelegate::onFormatPanelHoverChanged);
        mMediator.setParentSize(parentSize);
        mReportFormatSelection = false;
        mMediator.setSelectedFormat(currentStereoMode, currentProjectionType);
        mReportFormatSelection = true;

        mHolder.setParent(parent);
        mHolder.setEntityEnabled(true);
    }

    /** Dismisses the format selection panel. */
    public void dismiss() {
        if (mView != null) {
            mView.setVisibility(View.GONE);
            mView.setHoverListener(null);
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

    /** Requests accessibility focus on the format panel. */
    public void requestFocusForAccessibility() {
        if (mView != null) {
            mView.requestFocusForAccessibility();
        }
    }

    /** Returns true if the panel is showing, false otherwise. */
    public boolean isShowing() {
        return mHolder != null && mHolder.getParent() != null;
    }

    public @Nullable @ImmersiveStereoMode Integer getRecommendedStereoModeForTesting() {
        return mModel.get(ImmersiveVideoFormatProperties.RECOMMENDED_STEREO_MODE);
    }

    public @Nullable @ImmersiveProjectionType Integer getRecommendedProjectionTypeForTesting() {
        return mModel.get(ImmersiveVideoFormatProperties.RECOMMENDED_PROJECTION_TYPE);
    }
}
