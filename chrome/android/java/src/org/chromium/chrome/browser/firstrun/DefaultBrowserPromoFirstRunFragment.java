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

@NullMarked
public class DefaultBrowserPromoFirstRunFragment extends Fragment implements FirstRunFragment {

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
            // Remove the old view (e.g. the portrait/landscape version) that's still physically
            // inside the FrameLayout.
            rootView.removeAllViews();
            updateView(getLayoutInflater(), rootView);
        }
    }

    private void updateView(LayoutInflater inflater, ViewGroup container) {
        // TODO(https://crbug.com/483539670): Re-use this method and move it to a shared location.
        boolean useLandscape = SigninUtils.shouldShowDualPanesHorizontalLayout(getActivity());

        int layoutId =
                useLandscape
                        ? R.layout.default_browser_promo_fre_landscape_view
                        : R.layout.default_browser_promo_fre_portrait_view;

        // Inflate without attaching to root to avoid a ClassCastException, since we can't cast a
        // FrameLayout to a DefaultBrowserPromoFirstRunView.
        View inflatedView = inflater.inflate(layoutId, container, false);
        // Perform the cast here.
        DefaultBrowserPromoFirstRunView view = (DefaultBrowserPromoFirstRunView) inflatedView;
        // Manually add it to the FrameLayout wrapper.
        container.addView(view);

        // These are just placeholders for the primer. Will upload a follow-up CL for the actual
        // logic.
        var pageDelegate = assumeNonNull(getPageDelegate());
        view.getContinueButtonView().setOnClickListener(v -> pageDelegate.advanceToNextPage());
        view.getDismissButtonView().setOnClickListener(v -> pageDelegate.advanceToNextPage());
    }

    /** Implements {@link FirstRunFragment}. */
    @Override
    public void setInitialA11yFocus() {
        if (getView() == null) return;
        getView()
                .findViewById(R.id.title)
                .sendAccessibilityEvent(AccessibilityEvent.TYPE_VIEW_FOCUSED);
    }
}
