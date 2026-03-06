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
import org.chromium.chrome.browser.feature_engagement.TrackerFactory;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.ui.default_browser_promo.DefaultBrowserPromoUtils;
import org.chromium.chrome.browser.ui.default_browser_promo.DefaultBrowserPromoUtils.DefaultBrowserPromoEntryPoint;
import org.chromium.chrome.browser.ui.signin.SigninUtils;

@NullMarked
public class DefaultBrowserPromoFirstRunFragment extends Fragment implements FirstRunFragment {

    // If we are not triggering the RMD, use a small delay (500ms) that is greater than 450ms
    // (TRANSITION_DELAY_MS).
    private static final int ADVANCE_TO_NEXT_PAGE_DELAY_MS = 500;
    private static final String RMD_DIRECT_INVOCATION = "rmd_direct_invocation";
    private static final String PRIMER_NO_INSTRUCTIONS = "primer_no_instructions";

    // To avoid redundant triggers occurring via onResume.
    private boolean mHasTriggered;

    @Override
    public void onResume() {
        super.onResume();

        // onResume -> Trigger RMD -> Call onResume the 2nd time to advance to the next page.
        if (mHasTriggered) {
            assumeNonNull(getPageDelegate()).advanceToNextPage();
            return;
        }

        String arm = ChromeFeatureList.sDefaultBrowserPromoFreArm.getValue();
        // Arm 1: Directly call the RMD without primers.
        if (RMD_DIRECT_INVOCATION.equals(arm)) {
            triggerRoleManagerDialog();
        }
        // Arm 2: Waits for the user to tap the CPA in the primer.
    }

    private void triggerRoleManagerDialog() {
        assert !mHasTriggered : "triggerRoleManagerDialog should only be called once.";
        mHasTriggered = true;

        FirstRunPageDelegate delegate = assumeNonNull(getPageDelegate());
        // Get the standard, non-incognito user profile.
        var profileProvider = assumeNonNull(delegate.getProfileProviderSupplier().get());
        Profile profile = profileProvider.getOriginalProfile();

        // Trigger the RMD.
        boolean didTrigger =
                DefaultBrowserPromoUtils.getInstance()
                        .prepareLaunchPromoIfNeeded(
                                getActivity(),
                                delegate.getWindowAndroid(),
                                TrackerFactory.getTrackerForProfile(profile),
                                DefaultBrowserPromoEntryPoint.FRE);

        if (!didTrigger) {
            assert false : "Expected RMD to trigger because Gatekeeper conditions were met.";
            // Safeguard: The BooleanSupplier in FirstRunActivity should prevent reaching this
            // fragment if the promo shouldn't be shown. If we reach here but fail to trigger,
            // we advance after a short delay to ensure the previous page's transition is
            // complete to avoid a stuck pager.
            View root = getView();
            if (root != null) {
                root.postDelayed(delegate::advanceToNextPage, ADVANCE_TO_NEXT_PAGE_DELAY_MS);
            }
        }
    }

    @Override
    public View onCreateView(
            LayoutInflater inflater,
            @Nullable ViewGroup container,
            @Nullable Bundle savedInstanceState) {

        FrameLayout rootView = new FrameLayout(getActivity());

        String arm = ChromeFeatureList.sDefaultBrowserPromoFreArm.getValue();

        // Arm 1 (RMD_DIRECT_INVOCATION) : we return an empty FrameLayout and let onResume handle
        // the dialog.
        // Arm 2 (PRIMER_NO_INSTRUCTIONS): show the primer.
        if (PRIMER_NO_INSTRUCTIONS.equals(arm)) {
            updateView(inflater, rootView);
        }
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
            String arm = ChromeFeatureList.sDefaultBrowserPromoFreArm.getValue();
            // We don't call updateView for arm 1 since the fragment is just a blank page.
            if (PRIMER_NO_INSTRUCTIONS.equals(arm)) {
                updateView(getLayoutInflater(), rootView);
            }
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

        var pageDelegate = assumeNonNull(getPageDelegate());
        view.getContinueButtonView().setOnClickListener(v -> triggerRoleManagerDialog());
        view.getDismissButtonView().setOnClickListener(v -> pageDelegate.advanceToNextPage());
    }

    /** Implements {@link FirstRunFragment}. */
    @Override
    public void setInitialA11yFocus() {
        // Arm 1 doesn't have a dedicated primer, so it won't come with a R.id.title.
        String arm = ChromeFeatureList.sDefaultBrowserPromoFreArm.getValue();
        if (RMD_DIRECT_INVOCATION.equals(arm)) return;

        if (getView() == null) return;
        getView()
                .findViewById(R.id.title)
                .sendAccessibilityEvent(AccessibilityEvent.TYPE_VIEW_FOCUSED);
    }
}
