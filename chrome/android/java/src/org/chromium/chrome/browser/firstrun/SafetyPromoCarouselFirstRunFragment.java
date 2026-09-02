// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.firstrun;

import static org.chromium.build.NullUtil.assumeNonNull;

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
import org.chromium.chrome.browser.safety_promo.SafetyPromoCarouselView;

/** A {@link Fragment} for the horizontal swipable Carousel page during the Safety FRE promo. */
@NullMarked
public class SafetyPromoCarouselFirstRunFragment extends Fragment implements FirstRunFragment {
    @Override
    public View onCreateView(
            LayoutInflater inflater,
            @Nullable ViewGroup container,
            @Nullable Bundle savedInstanceState) {
        FrameLayout rootView = new FrameLayout(getActivity());
        updateView(inflater, rootView);
        return rootView;
    }

    private void updateView(LayoutInflater inflater, ViewGroup container) {
        SafetyPromoCarouselView view =
                (SafetyPromoCarouselView)
                        inflater.inflate(
                                R.layout.safety_promo_fre_carousel_portrait_view, container, false);
        container.addView(view);

        var pageDelegate = assumeNonNull(getPageDelegate());
        new SafetyPromoCarouselCoordinator(
                getContext(),
                view,
                pageDelegate::advanceToNextPage,
                FirstRunUtils.getItemsForSafetyFrePromoArm(
                        ChromeFeatureList.sSafetyFrePromoArm.getValue()));
    }

    @Override
    public void setInitialA11yFocus() {
        if (getView() == null) return;

        getView()
                .findViewById(R.id.safety_promo_carousel_title)
                .sendAccessibilityEvent(AccessibilityEvent.TYPE_VIEW_FOCUSED);
    }
}
