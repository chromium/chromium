// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs.features.minimizedcustomtab;

import android.view.LayoutInflater;
import android.view.View;

import androidx.appcompat.app.AppCompatActivity;
import androidx.fragment.app.FragmentTransaction;

import org.chromium.chrome.R;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

/**
 * Coordinator that handles the creation of the minimized card UI and the Fragment that will host
 * this card.
 */
public class MinimizedCardCoordinator {
    private final AppCompatActivity mActivity;
    private final PropertyModelChangeProcessor mMcp;
    private final MinimizedCardDialogFragment mDialogFragment;
    private final View mCoordinatorView;
    private final int mPrevImportantForAccessibility;
    public MinimizedCardCoordinator(AppCompatActivity activity, PropertyModel model) {
        mActivity = activity;
        View minimizedCard =
                LayoutInflater.from(activity).inflate(R.layout.custom_tabs_minimized_card, null);
        mMcp = PropertyModelChangeProcessor.create(
                model, minimizedCard, MinimizedCardViewBinder::bind, true);
        mCoordinatorView = mActivity.findViewById(R.id.coordinator);
        mPrevImportantForAccessibility = mCoordinatorView.getImportantForAccessibility();
        mCoordinatorView.setImportantForAccessibility(
                View.IMPORTANT_FOR_ACCESSIBILITY_NO_HIDE_DESCENDANTS);
        mDialogFragment = new MinimizedCardDialogFragment(minimizedCard);
        FragmentTransaction transaction = mActivity.getSupportFragmentManager().beginTransaction();
        transaction.setTransition(FragmentTransaction.TRANSIT_NONE);
        transaction.add(android.R.id.content, mDialogFragment).commitNow();
    }

    public void destroy() {
        mCoordinatorView.setImportantForAccessibility(mPrevImportantForAccessibility);
        mDialogFragment.dismissAllowingStateLoss();
        mMcp.destroy();
    }
}
