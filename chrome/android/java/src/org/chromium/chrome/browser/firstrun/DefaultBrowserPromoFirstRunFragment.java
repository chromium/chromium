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
import android.view.ViewGroup.MarginLayoutParams;
import android.view.accessibility.AccessibilityEvent;
import android.widget.FrameLayout;
import android.widget.ImageView;
import android.widget.TextView;

import androidx.annotation.IntDef;
import androidx.annotation.VisibleForTesting;
import androidx.fragment.app.Fragment;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.feature_engagement.TrackerFactory;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.ui.default_browser_promo.DefaultBrowserPromoMetrics;
import org.chromium.chrome.browser.ui.default_browser_promo.DefaultBrowserPromoUtils;
import org.chromium.chrome.browser.ui.default_browser_promo.DefaultBrowserPromoUtils.DefaultBrowserPromoEntryPoint;
import org.chromium.chrome.browser.ui.signin.SigninUtils;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

@NullMarked
public class DefaultBrowserPromoFirstRunFragment extends Fragment implements FirstRunFragment {

    // If we are not triggering the RMD, use a small delay (500ms) that is greater than 450ms
    // (TRANSITION_DELAY_MS).
    private static final int ADVANCE_TO_NEXT_PAGE_DELAY_MS = 500;
    public static final String FRE_PROMO_ARM = "fre_promo_arm";

    static final String RMD_DIRECT_INVOCATION = "rmd_direct_invocation";
    static final String PRIMER_NO_INSTRUCTIONS = "primer_no_instructions";
    static final String PRIMER_PROMOTIONAL_TEXT = "primer_promotional_text";

    // Checks whether the Role Manager Dialog (RMD) has been shown. This local state is synchronized
    // with the {@link FirstRunPageDelegate} to survive Fragment recreation.
    @VisibleForTesting boolean mDialogHasTriggered;

    @IntDef({
        BehaviorType.RMD_DIRECT_INVOCATION,
        BehaviorType.PRIMER_NO_INSTRUCTIONS,
        BehaviorType.PRIMER_PROMOTIONAL_TEXT
    })
    @Retention(RetentionPolicy.SOURCE)
    public @interface BehaviorType {
        int RMD_DIRECT_INVOCATION = 0;
        int PRIMER_NO_INSTRUCTIONS = 1;
        int PRIMER_PROMOTIONAL_TEXT = 2;
    }

    // TODO(https://crbug.com/494974037): Move the BehaviorType, InDef, and the selection logic into
    // a dedicated helper class.
    private @BehaviorType int getBehaviorType() {
        String arm =
                ChromeFeatureList.getFieldTrialParamByFeature(
                        ChromeFeatureList.DEFAULT_BROWSER_PROMO_FRE, FRE_PROMO_ARM);
        if (PRIMER_NO_INSTRUCTIONS.equals(arm)) return BehaviorType.PRIMER_NO_INSTRUCTIONS;
        if (PRIMER_PROMOTIONAL_TEXT.equals(arm)) return BehaviorType.PRIMER_PROMOTIONAL_TEXT;
        // rmd_direct_invocation.
        return BehaviorType.RMD_DIRECT_INVOCATION;
    }

    @Override
    public void onResume() {
        super.onResume();

        mDialogHasTriggered = assumeNonNull(getPageDelegate()).getPromoRoleManagerDialogTriggered();

        // onResume -> Trigger RMD -> the user finishes the RMD and the OS returns focus to Chrome
        // -> onResume is triggered the 2nd time -> advance to the next page.
        if (mDialogHasTriggered) {
            assumeNonNull(getPageDelegate()).advanceToNextPage();
            return;
        }

        // Arm 1: Directly call the RMD without primers.
        if (getBehaviorType() == BehaviorType.RMD_DIRECT_INVOCATION) {
            triggerRoleManagerDialog();
        }
        // Arm 2: Waits for the user to tap the CPA in the primer.
    }

    private void triggerRoleManagerDialog() {
        mDialogHasTriggered = true;
        FirstRunPageDelegate delegate = assumeNonNull(getPageDelegate());

        assert !delegate.getPromoRoleManagerDialogTriggered()
                : "Role Manager Dialog should only be shown once.";
        delegate.setPromoRoleManagerDialogTriggered(true);

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

        // Arm 1 (RMD_DIRECT_INVOCATION) : we return an empty FrameLayout and let onResume handle
        // the dialog.
        // Arm 2 (PRIMER_NO_INSTRUCTIONS) or arm 3 (PRIMER_PROMOTIONAL_TEXT): show the primer.
        @BehaviorType int armType = getBehaviorType();
        if (armType == BehaviorType.PRIMER_NO_INSTRUCTIONS
                || armType == BehaviorType.PRIMER_PROMOTIONAL_TEXT) {
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
            // We don't call updateView for arm 1 since the fragment is just a blank page.
            @BehaviorType int armType = getBehaviorType();
            if (armType == BehaviorType.PRIMER_NO_INSTRUCTIONS
                    || armType == BehaviorType.PRIMER_PROMOTIONAL_TEXT) {
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

        // UI adjustments for arm 3.
        if (getBehaviorType() == BehaviorType.PRIMER_PROMOTIONAL_TEXT) {
            ((TextView) view.findViewById(R.id.title)).setText(R.string.get_the_most_out_of_chrome);
            view.findViewById(R.id.subtitle).setVisibility(View.GONE);
            view.getContinueButtonView().setText(R.string.set_chrome_as_your_default);

            int topMargin =
                    getResources()
                            .getDimensionPixelSize(
                                    useLandscape
                                            ? R.dimen.promo_large_padding_vertical
                                            : R.dimen.promo_large_padding_horizontal);
            int buttonBottomMargin =
                    getResources()
                            .getDimensionPixelSize(
                                    useLandscape
                                            ? R.dimen.promo_large_padding_vertical
                                            : R.dimen.promo_large_padding_horizontal);

            if (useLandscape) {
                // For landscape mode, remove the top and bottom padding of the LinearLayout to
                // reduce scrolling.
                View contentContainer = (View) view.findViewById(R.id.title).getParent();
                contentContainer.setPadding(0, 0, 0, 0);
            }

            int[] rowIds = new int[] {R.id.instruction_1, R.id.instruction_2, R.id.instruction_3};
            for (int i = 0; i < rowIds.length; i++) {
                View row = view.findViewById(rowIds[i]);
                var params = (MarginLayoutParams) row.getLayoutParams();

                // Adjust the top margin for the first row so it's not touching the title.
                if (i == 0) {
                    params.topMargin = topMargin;
                }

                // Align rows to the start (left) with the title.
                if (useLandscape) {
                    params.setMarginStart(0);
                    params.setMarginEnd(
                            getResources()
                                    .getDimensionPixelSize(R.dimen.promo_large_padding_horizontal));
                }
                row.setLayoutParams(params);
            }

            setUpPromotionalRows(
                    view,
                    R.id.instruction_1,
                    R.drawable.fre_promo_shield_illustration,
                    R.string.promo_text_security,
                    R.drawable.account_row_background_rounded_up);
            setUpPromotionalRows(
                    view,
                    R.id.instruction_2,
                    R.drawable.fre_promo_chrome_illustration,
                    R.string.promo_text_convenience,
                    R.drawable.account_row_background);
            setUpPromotionalRows(
                    view,
                    R.id.instruction_3,
                    R.drawable.fre_promo_sync_illustration,
                    R.string.promo_text_synchronicity,
                    R.drawable.account_row_background_rounded_down);

            // Decrease the button group bottom margin to create space for the promotional rows.
            View buttonGroup = view.findViewById(R.id.default_browser_fre_button_group);
            var buttonParams = (MarginLayoutParams) buttonGroup.getLayoutParams();
            buttonParams.bottomMargin = buttonBottomMargin;
            buttonGroup.setLayoutParams(buttonParams);
        }

        var pageDelegate = assumeNonNull(getPageDelegate());
        view.getContinueButtonView()
                .setOnClickListener(
                        v -> {
                            // Record that the user accepted the promo.
                            pageDelegate.recordFreProgressHistogram(
                                    MobileFreProgress.DEFAULT_BROWSER_PROMO_ACCEPTED);
                            triggerRoleManagerDialog();

                            DefaultBrowserPromoMetrics.recordPromoClick(
                                    DefaultBrowserPromoMetrics.DefaultBrowserPromoSourceType
                                            .FRE_PROMO);
                        });
        view.getDismissButtonView()
                .setOnClickListener(
                        v -> {
                            // Record that the user rejected the promo.
                            pageDelegate.recordFreProgressHistogram(
                                    MobileFreProgress.DEFAULT_BROWSER_PROMO_REJECTED);
                            pageDelegate.advanceToNextPage();
                        });
    }

    // These promotional rows are only visible in arm 3.
    private void setUpPromotionalRows(
            View rootView, int rowId, int iconRes, int stringRes, int bgRes) {
        View row = rootView.findViewById(rowId);
        row.setVisibility(View.VISIBLE);

        row.setBackgroundResource(bgRes);
        ((ImageView) row.findViewById(R.id.row_icon)).setImageResource(iconRes);
        ((TextView) row.findViewById(R.id.row_text)).setText(stringRes);
    }

    /** Implements {@link FirstRunFragment}. */
    @Override
    public void setInitialA11yFocus() {
        // Arm 1 doesn't have a dedicated primer, so it won't come with a R.id.title.
        if (getBehaviorType() == BehaviorType.RMD_DIRECT_INVOCATION) return;

        if (getView() == null) return;
        getView()
                .findViewById(R.id.title)
                .sendAccessibilityEvent(AccessibilityEvent.TYPE_VIEW_FOCUSED);
    }
}
