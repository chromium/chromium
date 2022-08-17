// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.privacy_guide;

import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;

import androidx.annotation.IntDef;
import androidx.recyclerview.widget.RecyclerView;

import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.widget.RadioButtonWithDescriptionAndAuxButton;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * Controls the behavior of the ViewPager to navigate between privacy guide steps.
 */
public class PrivacyGuidePagerAdapter extends RecyclerView.Adapter<RecyclerView.ViewHolder> {
    /**
     * The types of views supported. Each view corresponds to a step in the privacy guide.
     */
    @Retention(RetentionPolicy.SOURCE)
    @IntDef({ViewType.COOKIES, ViewType.MSBB, ViewType.SAFE_BROWSING, ViewType.SYNC,
            ViewType.COUNT})
    private @interface ViewType {
        int COOKIES = 0;
        int MSBB = 1;
        int SAFE_BROWSING = 2;
        int SYNC = 3;
        int COUNT = 4;
    }

    class CookiesViewHolder extends RecyclerView.ViewHolder {
        private View mView;

        public CookiesViewHolder(View view) {
            super(view);
            mView = view;
        }
    }

    class SafeBrowsingViewHolder extends RecyclerView.ViewHolder
            implements RadioButtonWithDescriptionAndAuxButton.OnAuxButtonClickedListener {
        private View mView;
        private RadioButtonWithDescriptionAndAuxButton mStandardProtection;
        private RadioButtonWithDescriptionAndAuxButton mEnhancedProtection;
        private BottomSheetController mBottomSheetController;

        public SafeBrowsingViewHolder(View view, BottomSheetController controller) {
            super(view);
            mView = view;
            mBottomSheetController = controller;
            mEnhancedProtection = (RadioButtonWithDescriptionAndAuxButton) view.findViewById(
                    R.id.enhanced_option);
            mStandardProtection = (RadioButtonWithDescriptionAndAuxButton) view.findViewById(
                    R.id.standard_option);

            mEnhancedProtection.setAuxButtonClickedListener(this);
            mStandardProtection.setAuxButtonClickedListener(this);
        }

        @Override
        public void onAuxButtonClicked(int clickedButtonId) {
            LayoutInflater inflater = LayoutInflater.from(mView.getContext());
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
    }

    class SyncViewHolder extends RecyclerView.ViewHolder {
        private View mView;

        public SyncViewHolder(View view) {
            super(view);
            mView = view;
        }
    }

    private BottomSheetController mBottomSheetController;

    public PrivacyGuidePagerAdapter(BottomSheetController controller) {
        super();
        mBottomSheetController = controller;
    }

    @Override
    public int getItemViewType(int position) {
        // Each view is unique, so return the position directly, instead of 0 by default.
        if (position == 0) return ViewType.MSBB;
        if (position == 1) return ViewType.SYNC;
        if (position == 2) return ViewType.SAFE_BROWSING;
        return ViewType.COOKIES;
    }

    @Override
    public RecyclerView.ViewHolder onCreateViewHolder(ViewGroup parent, int viewType) {
        LayoutInflater inflater = LayoutInflater.from(parent.getContext());
        switch (viewType) {
            case ViewType.MSBB:
                return new MSBBViewHolder(
                        inflater.inflate(R.layout.privacy_guide_msbb_step, parent, false));
            case ViewType.SYNC:
                return new SyncViewHolder(
                        inflater.inflate(R.layout.privacy_guide_sync_step, parent, false));
            case ViewType.SAFE_BROWSING:
                return new SafeBrowsingViewHolder(
                        inflater.inflate(R.layout.privacy_guide_sb_step, parent, false),
                        mBottomSheetController);
            case ViewType.COOKIES:
                return new CookiesViewHolder(
                        inflater.inflate(R.layout.privacy_guide_cookies_step, parent, false));
        }
        return null;
    }

    @Override
    public int getItemCount() {
        return ViewType.COUNT;
    }

    @Override
    public void onBindViewHolder(RecyclerView.ViewHolder holder, int position) {
        return;
    }
}
