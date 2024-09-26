// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.privacy_guide;

import android.os.Bundle;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.RadioGroup;

import org.chromium.base.supplier.OneshotSupplier;
import org.chromium.chrome.browser.prefetch.settings.PreloadPagesSettingsBridge;
import org.chromium.chrome.browser.prefetch.settings.PreloadPagesState;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.widget.RadioButtonWithDescription;
import org.chromium.components.browser_ui.widget.RadioButtonWithDescriptionAndAuxButton;

/** Controls the behaviour of the Preload privacy guide page. */
public class PreloadFragment extends PrivacyGuideBasePage
        implements RadioButtonWithDescriptionAndAuxButton.OnAuxButtonClickedListener,
                RadioGroup.OnCheckedChangeListener {
    private RadioButtonWithDescription mDisabledPreloading;
    private RadioButtonWithDescriptionAndAuxButton mStandardPreloading;
    private BottomSheetController mBottomSheetController;
    private PrivacyGuideBottomSheetView mBottomSheetView;

    @Override
    public View onCreateView(
            LayoutInflater inflater, ViewGroup container, Bundle savedInstanceState) {
        return inflater.inflate(R.layout.privacy_guide_preload_step, container, false);
    }

    @Override
    public void onViewCreated(View view, Bundle savedInstanceState) {
        RadioGroup radioGroup = view.findViewById(R.id.preload_radio_button);
        radioGroup.setOnCheckedChangeListener(this);

        mStandardPreloading =
                (RadioButtonWithDescriptionAndAuxButton) view.findViewById(R.id.standard_option);
        mDisabledPreloading = (RadioButtonWithDescription) view.findViewById(R.id.disabled_option);

        mStandardPreloading.setAuxButtonClickedListener(this);

        initialRadioButtonConfig();
    }

    private void initialRadioButtonConfig() {
        @PreloadPagesState
        int preloadPagesState = PreloadPagesSettingsBridge.getState(getProfile());
        switch (preloadPagesState) {
            case PreloadPagesState.STANDARD_PRELOADING:
                mStandardPreloading.setChecked(true);
                break;
            case PreloadPagesState.NO_PRELOADING:
                mDisabledPreloading.setChecked(true);
                break;
            default:
                assert false : "Unexpected PreloadState " + preloadPagesState;
        }
    }

    @Override
    public void onAuxButtonClicked(int clickedButtonId) {
        LayoutInflater inflater = LayoutInflater.from(getView().getContext());
        if (clickedButtonId == mStandardPreloading.getId()) {
            displayBottomSheet(
                    inflater.inflate(R.layout.privacy_guide_preload_standard_explanation, null));
        } else {
            assert false : "Unknown Aux clickedButtonId " + clickedButtonId;
        }
    }

    @Override
    public void onCheckedChanged(RadioGroup radioGroup, int clickedButtonId) {
        if (clickedButtonId == R.id.standard_option) {
            PreloadPagesSettingsBridge.setState(
                    getProfile(), PreloadPagesState.STANDARD_PRELOADING);
        } else if (clickedButtonId == R.id.disabled_option) {
            PreloadPagesSettingsBridge.setState(getProfile(), PreloadPagesState.NO_PRELOADING);
        } else {
            assert false : "Unknown clickedButtonId " + clickedButtonId;
        }
    }

    private void displayBottomSheet(View sheetContent) {
        mBottomSheetView = new PrivacyGuideBottomSheetView(sheetContent, this::closeBottomSheet);
        // TODO(crbug.com/40211402): Re-enable animation once bug is fixed
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
