// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.privacy_guide;

import android.os.Bundle;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.RadioGroup;

import org.chromium.base.supplier.OneshotSupplier;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.safe_browsing.SafeBrowsingBridge;
import org.chromium.chrome.browser.safe_browsing.SafeBrowsingState;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.widget.RadioButtonWithDescription;
import org.chromium.components.browser_ui.widget.RadioButtonWithDescriptionAndAuxButton;

/** Controls the behaviour of the Safe Browsing privacy guide page. */
public class SafeBrowsingFragment extends PrivacyGuideBasePage
        implements RadioButtonWithDescriptionAndAuxButton.OnAuxButtonClickedListener,
                RadioGroup.OnCheckedChangeListener {
    private RadioButtonWithDescription mStandardProtectionFriendlier;
    private RadioButtonWithDescriptionAndAuxButton mStandardProtection;
    private RadioButtonWithDescriptionAndAuxButton mEnhancedProtection;
    private BottomSheetController mBottomSheetController;
    private PrivacyGuideBottomSheetView mBottomSheetView;

    @Override
    public View onCreateView(
            LayoutInflater inflater, ViewGroup container, Bundle savedInstanceState) {
        return inflater.inflate(R.layout.privacy_guide_sb_step, container, false);
    }

    @Override
    public void onViewCreated(View view, Bundle savedInstanceState) {
        RadioGroup radioGroup = view.findViewById(R.id.sb_radio_button);
        radioGroup.setOnCheckedChangeListener(this);

        mEnhancedProtection =
                (RadioButtonWithDescriptionAndAuxButton) view.findViewById(R.id.enhanced_option);
        mStandardProtection =
                (RadioButtonWithDescriptionAndAuxButton) view.findViewById(R.id.standard_option);
        mStandardProtectionFriendlier =
                (RadioButtonWithDescription) view.findViewById(R.id.standard_option_friendlier);

        // The enhanced protection description is being overwritten when the flag is enabled
        if (ChromeFeatureList.sFriendlierSafeBrowsingSettingsEnhancedProtection.isEnabled()) {
            mEnhancedProtection.setDescriptionText(
                    getContext()
                            .getString(R.string.safe_browsing_enhanced_protection_summary_updated));
        }

        if (ChromeFeatureList.sFriendlierSafeBrowsingSettingsStandardProtection.isEnabled()) {
            mStandardProtection.setVisibility(View.GONE);
            mStandardProtectionFriendlier.setVisibility(View.VISIBLE);
            if (SafeBrowsingBridge.isHashRealTimeLookupEligibleInSession()) {
                mStandardProtectionFriendlier.setDescriptionText(
                        getContext()
                                .getString(
                                        R.string
                                                .safe_browsing_standard_protection_summary_updated_proxy));
            }
        } else {
            mStandardProtection.setAuxButtonClickedListener(this);
            mStandardProtection.setVisibility(View.VISIBLE);
            mStandardProtectionFriendlier.setVisibility(View.GONE);
        }

        mEnhancedProtection.setAuxButtonClickedListener(this);

        initialRadioButtonConfig();
    }

    private void initialRadioButtonConfig() {
        @SafeBrowsingState int safeBrowsingState = PrivacyGuideUtils.getSafeBrowsingState();
        switch (safeBrowsingState) {
            case (SafeBrowsingState.ENHANCED_PROTECTION):
                mEnhancedProtection.setChecked(true);
                break;
            case (SafeBrowsingState.STANDARD_PROTECTION):
                if (ChromeFeatureList.sFriendlierSafeBrowsingSettingsStandardProtection
                        .isEnabled()) {
                    mStandardProtectionFriendlier.setChecked(true);
                } else {
                    mStandardProtection.setChecked(true);
                }
                break;
            default:
                assert false : "Unexpected SafeBrowsingState " + safeBrowsingState;
        }
    }

    @Override
    public void onAuxButtonClicked(int clickedButtonId) {
        LayoutInflater inflater = LayoutInflater.from(getView().getContext());
        if (clickedButtonId == mEnhancedProtection.getId()) {
            if (ChromeFeatureList.isEnabled(
                    ChromeFeatureList.FRIENDLIER_SAFE_BROWSING_SETTINGS_ENHANCED_PROTECTION)) {
                displayBottomSheet(
                        inflater.inflate(
                                R.layout.privacy_guide_sb_enhanced_explanation_updated, null));
            } else {
                displayBottomSheet(
                        inflater.inflate(R.layout.privacy_guide_sb_enhanced_explanation, null));
            }
        } else if (clickedButtonId == mStandardProtection.getId()) {
            View sheetContent =
                    inflater.inflate(R.layout.privacy_guide_sb_standard_explanation, null);
            if (SafeBrowsingBridge.isHashRealTimeLookupEligibleInSession()) {
                PrivacyGuideExplanationItem itemTwo =
                        sheetContent.findViewById(R.id.sb_standard_item_two);
                itemTwo.setSummaryText(
                        getContext().getString(R.string.privacy_guide_sb_standard_item_two_proxy));
                PrivacyGuideExplanationItem itemThree =
                        sheetContent.findViewById(R.id.sb_standard_item_three);
                itemThree.setSummaryText(
                        getContext()
                                .getString(R.string.privacy_guide_sb_standard_item_three_proxy));
            }
            displayBottomSheet(sheetContent);
        } else {
            assert false : "Unknown Aux clickedButtonId " + clickedButtonId;
        }
    }

    @Override
    public void onCheckedChanged(RadioGroup radioGroup, int clickedButtonId) {
        if (clickedButtonId == R.id.enhanced_option) {
            SafeBrowsingBridge.setSafeBrowsingState(SafeBrowsingState.ENHANCED_PROTECTION);
            PrivacyGuideMetricsDelegate.recordMetricsOnSafeBrowsingChange(
                    SafeBrowsingState.ENHANCED_PROTECTION);
        } else if (clickedButtonId == R.id.standard_option
                || clickedButtonId == R.id.standard_option_friendlier) {
            SafeBrowsingBridge.setSafeBrowsingState(SafeBrowsingState.STANDARD_PROTECTION);
            PrivacyGuideMetricsDelegate.recordMetricsOnSafeBrowsingChange(
                    SafeBrowsingState.STANDARD_PROTECTION);
        } else {
            assert false : "Unknown clickedButtonId " + clickedButtonId;
        }
    }

    private void displayBottomSheet(View sheetContent) {
        if (ChromeFeatureList.isEnabled(
                        ChromeFeatureList.FRIENDLIER_SAFE_BROWSING_SETTINGS_ENHANCED_PROTECTION)
                && ChromeFeatureList.isEnabled(
                        ChromeFeatureList.FRIENDLIER_SAFE_BROWSING_SETTINGS_STANDARD_PROTECTION)) {
            mBottomSheetView =
                    new PrivacyGuideBottomSheetView(
                            sheetContent, this::closeBottomSheet, 0.9f, 1.0f);
        } else {
            mBottomSheetView =
                    new PrivacyGuideBottomSheetView(sheetContent, this::closeBottomSheet);
        }
        // TODO(crbug.com/1287979): Re-enable animation once bug is fixed
        if (mBottomSheetController != null) {
            mBottomSheetController.requestShowContent(mBottomSheetView, false);
        }
    }

    private void closeBottomSheet() {
        if (mBottomSheetController != null && mBottomSheetView != null) {
            mBottomSheetController.hideContent(mBottomSheetView, true);
        }
    }

    void setBottomSheetControllerSupplier(
            OneshotSupplier<BottomSheetController> bottomSheetControllerSupplier) {
        bottomSheetControllerSupplier.onAvailable(
                (bottomSheetController) -> mBottomSheetController = bottomSheetController);
    }
}
