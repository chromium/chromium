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
import org.chromium.chrome.R;
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
        int layoutId =
                useLandscape
                        ? R.layout.safety_promo_fre_illustration_landscape_view
                        : R.layout.safety_promo_fre_illustration_portrait_view;

        View inflatedView = inflater.inflate(layoutId, container, false);
        SafetyPromoFirstRunView view = (SafetyPromoFirstRunView) inflatedView;
        container.addView(view);

        // These are just placeholders for the arm 4 UI skeleton. Actual illustration and animation
        // logic will be added in follow-up patches.
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
