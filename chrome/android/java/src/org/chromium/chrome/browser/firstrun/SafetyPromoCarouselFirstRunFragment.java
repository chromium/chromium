// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.firstrun;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.content.res.Configuration;
import android.os.Bundle;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.view.accessibility.AccessibilityEvent;
import android.widget.FrameLayout;

import androidx.fragment.app.Fragment;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.safety_promo.R;
import org.chromium.chrome.browser.safety_promo.SafetyPromoCarouselCoordinator;
import org.chromium.chrome.browser.ui.signin.SigninUtils;

/** A {@link Fragment} for the horizontal swipable Carousel page during the Safety FRE promo. */
@NullMarked
public class SafetyPromoCarouselFirstRunFragment extends Fragment implements FirstRunFragment {
    private @Nullable FrameLayout mRootView;
    private @Nullable SafetyPromoCarouselCoordinator mCoordinator;
    private @Nullable Configuration mConfiguration;

    @Override
    public View onCreateView(
            LayoutInflater inflater,
            @Nullable ViewGroup container,
            @Nullable Bundle savedInstanceState) {
        mRootView = new FrameLayout(getActivity());
        mConfiguration = new Configuration(getResources().getConfiguration());
        var pageDelegate = assumeNonNull(getPageDelegate());
        mCoordinator =
                new SafetyPromoCarouselCoordinator(
                        getActivity(),
                        shouldUseLandscapeLayout(),
                        pageDelegate.getSafetyPromoFirstRunState().getSelectedItemSupplier(),
                        pageDelegate::advanceToNextPage,
                        FirstRunUtils.getItemsForSafetyFrePromoArm(
                                ChromeFeatureList.sSafetyFrePromoArm.getValue()));
        attachCarouselView();
        return mRootView;
    }

    @Override
    public void onConfigurationChanged(Configuration newConfig) {
        super.onConfigurationChanged(newConfig);
        if (mCoordinator == null || mConfiguration == null) return;

        boolean resourcesChanged = haveResourcesChanged(mConfiguration, newConfig);
        mConfiguration = new Configuration(newConfig);
        mCoordinator.onConfigurationChanged(shouldUseLandscapeLayout(), resourcesChanged);
        attachCarouselView();
    }

    @Override
    public void onDestroyView() {
        if (mCoordinator != null) {
            mCoordinator.destroy();
            mCoordinator = null;
        }
        if (mRootView != null) {
            mRootView.removeAllViews();
            mRootView = null;
        }
        mConfiguration = null;
        super.onDestroyView();
    }

    @Override
    public void setInitialA11yFocus() {
        if (getView() == null) return;

        getView()
                .findViewById(R.id.safety_promo_carousel_title)
                .sendAccessibilityEvent(AccessibilityEvent.TYPE_VIEW_FOCUSED);
    }

    /**
     * Attaches the carousel view of the current layout to the root view, replacing the previous
     * one.
     */
    private void attachCarouselView() {
        if (mRootView == null || mCoordinator == null) return;

        View carouselView = mCoordinator.getView();
        if (carouselView.getParent() == mRootView) return;

        mRootView.removeAllViews();
        mRootView.addView(carouselView);
    }

    private boolean shouldUseLandscapeLayout() {
        return SigninUtils.shouldShowDualPanesHorizontalLayout(getActivity());
    }

    /**
     * Returns whether the change outdates the resources of inflated views. Of the changes that
     * {@link FirstRunActivity} handles itself, only density and font scale affect the carousel's
     * resources.
     */
    private static boolean haveResourcesChanged(Configuration oldConfig, Configuration newConfig) {
        return oldConfig.densityDpi != newConfig.densityDpi
                || oldConfig.fontScale != newConfig.fontScale;
    }
}
