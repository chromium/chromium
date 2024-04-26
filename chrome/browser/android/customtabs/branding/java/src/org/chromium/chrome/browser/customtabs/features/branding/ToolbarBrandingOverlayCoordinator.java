// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs.features.branding;

import static org.chromium.chrome.browser.customtabs.features.branding.ToolbarBrandingOverlayProperties.HIDING_PROGRESS;

import android.animation.Animator;
import android.animation.AnimatorListenerAdapter;
import android.animation.ValueAnimator;
import android.view.View;
import android.view.ViewGroup;
import android.view.ViewStub;

import org.chromium.chrome.R;
import org.chromium.ui.interpolators.Interpolators;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

/**
 * Coordinator that creates/shows and hides/destroys the toolbar branding overlay over the title and
 * url.
 */
public class ToolbarBrandingOverlayCoordinator {
    private static final int HIDING_DURATION_MS = 300;

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
            mView = null;
        }
    }

    /** Hides the toolbar branding overlay and performs necessary clean-up. */
    public void hideAndDestroy() {
        assert mView != null : "Toolbar branding overlay is already destroyed.";
        assert mHidingAnimator == null : "Toolbar branding overlay is already hiding.";

        mHidingAnimator = ValueAnimator.ofFloat(mModel.get(HIDING_PROGRESS), 1.f);
        mHidingAnimator.setInterpolator(Interpolators.STANDARD_INTERPOLATOR);
        mHidingAnimator.setDuration(HIDING_DURATION_MS);
        mHidingAnimator.addUpdateListener(
                anim -> mModel.set(HIDING_PROGRESS, (float) anim.getAnimatedValue()));
        mHidingAnimator.addListener(
                new AnimatorListenerAdapter() {
                    @Override
                    public void onAnimationEnd(Animator animation) {
                        ((ViewGroup) mView.getParent()).removeView(mView);
                        mView = null;
                        mHidingAnimator = null;
                    }
                });
        mHidingAnimator.start();
    }
}
