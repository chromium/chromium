// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs.features.branding;

import static org.chromium.chrome.browser.customtabs.features.branding.ToolbarBrandingOverlayProperties.HIDING_PROGRESS;

import android.view.View;
import android.view.ViewGroup;
import android.view.ViewStub;

import androidx.annotation.VisibleForTesting;
import androidx.core.animation.Animator;
import androidx.core.animation.AnimatorListenerAdapter;
import androidx.core.animation.ValueAnimator;

import org.chromium.chrome.R;
import org.chromium.ui.interpolators.AndroidxInterpolators;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

/**
 * Coordinator that creates/shows and hides/destroys the toolbar branding overlay over the title and
 * url.
 */
public class ToolbarBrandingOverlayCoordinator {
    @VisibleForTesting static final int HIDING_DURATION_MS = 300;

    private View mView;
    private PropertyModel mModel;
    private ValueAnimator mHidingAnimator;

    /**
     * Constructs and shows the toolbar branding overlay.
     *
     * @param viewStub The {@link ViewStub} to inflate the branding overlay.
     * @param model The {@link PropertyModel} with the properties of the branding overlay.
     */
    public ToolbarBrandingOverlayCoordinator(ViewStub viewStub, PropertyModel model) {
        assert viewStub.getLayoutResource() == R.layout.custom_tabs_toolbar_branding_layout;
        mView = viewStub.inflate();
        mModel = model;
        PropertyModelChangeProcessor.create(mModel, mView, ToolbarBrandingOverlayViewBinder::bind);
    }

    public void destroy() {
        if (mHidingAnimator != null) {
            mHidingAnimator.cancel();
            mHidingAnimator = null;
        } else {
            destroyView();
        }
    }

    /** Hides the toolbar branding overlay and performs necessary clean-up. */
    public void hideAndDestroy() {
        assert mView != null : "Toolbar branding overlay is already destroyed.";
        assert mHidingAnimator == null : "Toolbar branding overlay is already hiding.";

        mHidingAnimator = ValueAnimator.ofFloat(mModel.get(HIDING_PROGRESS), 1.f);
        mHidingAnimator.setInterpolator(AndroidxInterpolators.STANDARD_INTERPOLATOR);
        mHidingAnimator.setDuration(HIDING_DURATION_MS);
        mHidingAnimator.addUpdateListener(
                anim ->
                        mModel.set(
                                HIDING_PROGRESS,
                                (float) ((ValueAnimator) anim).getAnimatedValue()));
        mHidingAnimator.addListener(
                new AnimatorListenerAdapter() {
                    @Override
                    public void onAnimationEnd(Animator animation) {
                        destroyView();
                        mHidingAnimator = null;
                    }
                });
        mHidingAnimator.start();
    }

    private void destroyView() {
        if (mView != null) {
            ((ViewGroup) mView.getParent()).removeView(mView);
            mView = null;
        }
    }
}
