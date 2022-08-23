// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.privacy_guide;

import android.os.Bundle;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;

import androidx.fragment.app.Fragment;

import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.widget.RadioButtonWithDescriptionAndAuxButton;

/**
 * Controls the behaviour of the Safe Browsing privacy guide page.
 */
public class SafeBrowsingFragment extends Fragment
        implements RadioButtonWithDescriptionAndAuxButton.OnAuxButtonClickedListener {
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
        mEnhancedProtection =
                (RadioButtonWithDescriptionAndAuxButton) view.findViewById(R.id.enhanced_option);
        mStandardProtection =
                (RadioButtonWithDescriptionAndAuxButton) view.findViewById(R.id.standard_option);

        mEnhancedProtection.setAuxButtonClickedListener(this);
        mStandardProtection.setAuxButtonClickedListener(this);
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
            assert false : "Should not be reached.";
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
