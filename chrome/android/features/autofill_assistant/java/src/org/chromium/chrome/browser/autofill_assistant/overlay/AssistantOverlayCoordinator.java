// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant.overlay;

import android.graphics.Bitmap;
import android.graphics.RectF;
import android.text.TextUtils;
import android.util.DisplayMetrics;

import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.image_fetcher.ImageFetcher;
import org.chromium.chrome.browser.image_fetcher.ImageFetcherConfig;
import org.chromium.chrome.browser.image_fetcher.ImageFetcherFactory;
import org.chromium.chrome.browser.util.AccessibilityUtil;
import org.chromium.chrome.browser.widget.ScrimView;
import org.chromium.chrome.browser.widget.ScrimView.ScrimParams;

import java.util.List;

/**
 * Coordinator responsible for showing a full or partial overlay on top of the web page currently
 * displayed.
 */
public class AssistantOverlayCoordinator {
    private final ChromeActivity mActivity;
    private final AssistantOverlayModel mModel;
    private final AssistantOverlayEventFilter mEventFilter;
    private final AssistantOverlayDrawable mDrawable;
    private final ScrimView mScrim;
    private final ImageFetcher mImageFetcher;
    private boolean mScrimEnabled;

    public AssistantOverlayCoordinator(ChromeActivity activity, AssistantOverlayModel model) {
        this(activity, model,
                ImageFetcherFactory.createImageFetcher(ImageFetcherConfig.DISK_CACHE_ONLY));
    }

    public AssistantOverlayCoordinator(
            ChromeActivity activity, AssistantOverlayModel model, ImageFetcher imageFetcher) {
        mActivity = activity;
        mModel = model;
        mImageFetcher = imageFetcher;
        mScrim = mActivity.getScrim();
        mEventFilter = new AssistantOverlayEventFilter(
                mActivity, mActivity.getFullscreenManager(), mActivity.getCompositorViewHolder());
        mDrawable = new AssistantOverlayDrawable(mActivity, mActivity.getFullscreenManager());

        // Listen for changes in the state.
        // TODO(crbug.com/806868): Bind model to view through a ViewBinder instead.
        model.addObserver((source, propertyKey) -> {
            if (AssistantOverlayModel.STATE == propertyKey) {
                setState(model.get(AssistantOverlayModel.STATE));
            } else if (AssistantOverlayModel.VISUAL_VIEWPORT == propertyKey) {
                RectF rect = model.get(AssistantOverlayModel.VISUAL_VIEWPORT);
                mEventFilter.setVisualViewport(rect);
                mDrawable.setVisualViewport(rect);
            } else if (AssistantOverlayModel.TOUCHABLE_AREA == propertyKey) {
                List<RectF> area = model.get(AssistantOverlayModel.TOUCHABLE_AREA);
                mEventFilter.setTouchableArea(area);
                mDrawable.setTransparentArea(area);
            } else if (AssistantOverlayModel.RESTRICTED_AREA == propertyKey) {
                List<RectF> area = model.get(AssistantOverlayModel.RESTRICTED_AREA);
                mEventFilter.setRestrictedArea(area);
                mDrawable.setRestrictedArea(area);
            } else if (AssistantOverlayModel.DELEGATE == propertyKey) {
                AssistantOverlayDelegate delegate = model.get(AssistantOverlayModel.DELEGATE);
                mEventFilter.setDelegate(delegate);
                mDrawable.setDelegate(delegate);
            } else if (AssistantOverlayModel.BACKGROUND_COLOR == propertyKey) {
                mDrawable.setBackgroundColor(model.get(AssistantOverlayModel.BACKGROUND_COLOR));
            } else if (AssistantOverlayModel.HIGHLIGHT_BORDER_COLOR == propertyKey) {
                mDrawable.setHighlightBorderColor(
                        model.get(AssistantOverlayModel.HIGHLIGHT_BORDER_COLOR));
            } else if (AssistantOverlayModel.TAP_TRACKING_COUNT == propertyKey) {
                mEventFilter.setTapTrackingCount(
                        model.get(AssistantOverlayModel.TAP_TRACKING_COUNT));
            } else if (AssistantOverlayModel.TAP_TRACKING_DURATION_MS == propertyKey) {
                mEventFilter.setTapTrackingDurationMs(
                        model.get(AssistantOverlayModel.TAP_TRACKING_DURATION_MS));
            } else if (AssistantOverlayModel.OVERLAY_IMAGE == propertyKey) {
                AssistantOverlayImage image = model.get(AssistantOverlayModel.OVERLAY_IMAGE);
                if (image != null && !TextUtils.isEmpty(image.mImageUrl)
                        && image.mImageSize != null) {
                    DisplayMetrics displayMetrics = mActivity.getResources().getDisplayMetrics();
                    // TODO(b/143517837) Merge autofill assistant image fetcher UMA names.
                    mImageFetcher.fetchImage(image.mImageUrl,
                            ImageFetcher.ASSISTANT_DETAILS_UMA_CLIENT_NAME, result -> {
                                int imageSizePixels =
                                        image.mImageSize.getSizeInPixels(displayMetrics);
                                image.mImageBitmap = Bitmap.createScaledBitmap(
                                        result, imageSizePixels, imageSizePixels, true);
                                mDrawable.setFullOverlayImage(image);
                            });
                } else {
                    mDrawable.setFullOverlayImage(image);
                }
            }
        });
    }

    /** Return the model observed by this coordinator. */
    public AssistantOverlayModel getModel() {
        return mModel;
    }

    /**
     * Destroy this coordinator.
     */
    public void destroy() {
        setScrimEnabled(false);
        mEventFilter.destroy();
        mDrawable.destroy();
    }

    /**
     * Set the overlay state.
     */
    private void setState(@AssistantOverlayState int state) {
        if (state == AssistantOverlayState.PARTIAL && AccessibilityUtil.isAccessibilityEnabled()) {
            // Touch exploration is fully disabled if there's an overlay in front. In this case, the
            // overlay must be fully gone and filtering elements for touch exploration must happen
            // at another level.
            //
            // TODO(crbug.com/806868): filter elements available to touch exploration, when it
            // is enabled.
            state = AssistantOverlayState.HIDDEN;
        }

        if (state == AssistantOverlayState.HIDDEN) {
            setScrimEnabled(false);
            mEventFilter.reset();
        } else {
            setScrimEnabled(true);
            mDrawable.setPartial(state == AssistantOverlayState.PARTIAL);
            mEventFilter.setPartial(state == AssistantOverlayState.PARTIAL);
        }
    }

    private void setScrimEnabled(boolean enabled) {
        if (enabled == mScrimEnabled) return;

        if (enabled) {
            ScrimParams params = new ScrimParams(mActivity.getCompositorViewHolder(),
                    /* showInFrontOfAnchorView= */ true,
                    /* affectsStatusBar = */ false,
                    /* topMargin= */ 0,
                    /* observer= */ null);
            params.backgroundDrawable = mDrawable;
            params.eventFilter = mEventFilter;
            mScrim.showScrim(params);
        } else {
            mScrim.hideScrim(/* fadeOut= */ true);
        }
        mScrimEnabled = enabled;
    }
}
