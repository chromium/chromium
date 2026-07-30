// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.firstrun;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.content.res.Configuration;
import android.content.res.Resources;
import android.os.Bundle;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.view.accessibility.AccessibilityEvent;
import android.widget.FrameLayout;

import androidx.annotation.LayoutRes;
import androidx.fragment.app.Fragment;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.firstrun.FirstRunUtils.SafetyFrePromoArm;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.ui.signin.SigninUtils;

/** A {@link Fragment} for the Safety Promo during the First Run Experience (FRE). */
@NullMarked
public class SafetyPromoFirstRunFragment extends Fragment implements FirstRunFragment {
    @Override
    public View onCreateView(
            LayoutInflater inflater,
            @Nullable ViewGroup container,
            @Nullable Bundle savedInstanceState) {
        FrameLayout rootView = new FrameLayout(getActivity());
        updateView(inflater, rootView);
        return rootView;
    }

    @Override
    public void onConfigurationChanged(Configuration newConfig) {
        super.onConfigurationChanged(newConfig);
        ViewGroup rootView = (ViewGroup) getView();
        if (rootView != null) {
            rootView.removeAllViews();
            updateView(getLayoutInflater(), rootView);
        }
    }

    private void updateView(LayoutInflater inflater, ViewGroup container) {
        boolean useLandscape = SigninUtils.shouldShowDualPanesHorizontalLayout(getActivity());
        @SafetyFrePromoArm int arm = ChromeFeatureList.sSafetyFrePromoArm.getValue();

        @LayoutRes int layoutId = getLayoutId(arm, useLandscape);
        if (layoutId == Resources.ID_NULL) {
            return;
        }

        View inflatedView = inflater.inflate(layoutId, container, false);
        SafetyPromoFirstRunView view = (SafetyPromoFirstRunView) inflatedView;
        container.addView(view);

        setupView(view, arm);
    }

    private @LayoutRes int getLayoutId(@SafetyFrePromoArm int arm, boolean useLandscape) {
        if (arm == SafetyFrePromoArm.PASSWORD_MANAGER) {
            return R.layout.safety_promo_fre_cards_portrait_view;
        }

        if (arm == SafetyFrePromoArm.ANIMATED_ILLUSTRATION) {
            return useLandscape
                    ? R.layout.safety_promo_fre_illustration_landscape_view
                    : R.layout.safety_promo_fre_illustration_portrait_view;
        }

        assert false : "Unsupported Safety FRE Promo arm: " + arm;
        return Resources.ID_NULL;
    }

    private void setupView(SafetyPromoFirstRunView view, @SafetyFrePromoArm int arm) {
        if (arm == SafetyFrePromoArm.PASSWORD_MANAGER) {
            view.setCards(FirstRunUtils.getCardsForSafetyFrePromoArm(arm));
        }
        var pageDelegate = assumeNonNull(getPageDelegate());
        view.getContinueButtonView().setOnClickListener(v -> pageDelegate.advanceToNextPage());
    }

    @Override
    public void setInitialA11yFocus() {
        if (getView() == null) return;

        getView()
                .findViewById(R.id.title)
                .sendAccessibilityEvent(AccessibilityEvent.TYPE_VIEW_FOCUSED);
    }
}
