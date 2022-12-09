// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.privacy_guide;

import android.os.Bundle;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.RadioGroup;

import androidx.fragment.app.Fragment;

import org.chromium.chrome.browser.safe_browsing.SafeBrowsingBridge;
import org.chromium.chrome.browser.safe_browsing.SafeBrowsingState;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.widget.RadioButtonWithDescriptionAndAuxButton;

/**
 * Controls the behaviour of the Safe Browsing privacy guide page.
 */
public class SafeBrowsingFragment extends Fragment
        implements RadioButtonWithDescriptionAndAuxButton.OnAuxButtonClickedListener,
                   RadioGroup.OnCheckedChangeListener {
    private RadioButtonWithDescriptionAndAuxButton mStandardProtection;
    private RadioButtonWithDescriptionAndAuxButton mEnhancedProtection;
    private BottomSheetController mBottomSheetController;

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

        mEnhancedProtection.setAuxButtonClickedListener(this);
        mStandardProtection.setAuxButtonClickedListener(this);

        initialRadioButtonConfig();
    }

    private void initialRadioButtonConfig() {
        @SafeBrowsingState
        int safeBrowsingState = SafeBrowsingBridge.getSafeBrowsingState();
        switch (safeBrowsingState) {
            case (SafeBrowsingState.ENHANCED_PROTECTION):
                mEnhancedProtection.setChecked(true);
                break;
            case (SafeBrowsingState.STANDARD_PROTECTION):
                mStandardProtection.setChecked(true);
                break;
            default:
                assert false : "Unexpected SafeBrowsingState " + safeBrowsingState;
        }
    }

    @Override
    public void onAuxButtonClicked(int clickedButtonId) {
        LayoutInflater inflater = LayoutInflater.from(getView().getContext());
        if (clickedButtonId == mEnhancedProtection.getId()) {
            displayBottomSheet(
                    inflater.inflate(R.layout.privacy_guide_sb_enhanced_explanation, null));
        } else if (clickedButtonId == mStandardProtection.getId()) {
            displayBottomSheet(
                    inflater.inflate(R.layout.privacy_guide_sb_standard_explanation, null));
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
        } else if (clickedButtonId == R.id.standard_option) {
            SafeBrowsingBridge.setSafeBrowsingState(SafeBrowsingState.STANDARD_PROTECTION);
            PrivacyGuideMetricsDelegate.recordMetricsOnSafeBrowsingChange(
                    SafeBrowsingState.STANDARD_PROTECTION);
        } else {
            assert false : "Unknown clickedButtonId " + clickedButtonId;
        }
    }

    private void displayBottomSheet(View sheetContent) {
        PrivacyGuideBottomSheetView bottomSheet = new PrivacyGuideBottomSheetView(sheetContent);
        mBottomSheetController.requestShowContent(bottomSheet, /* animate= */ true);
    }

    public void setBottomSheetController(BottomSheetController bottomSheetController) {
        mBottomSheetController = bottomSheetController;
    }
}
