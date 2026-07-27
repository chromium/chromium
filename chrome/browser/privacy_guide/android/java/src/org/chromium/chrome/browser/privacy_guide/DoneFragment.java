// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.privacy_guide;

import static org.chromium.chrome.browser.privacy_guide.PrivacyGuideUtils.isUserSignedIn;

import android.os.Bundle;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.ui.widget.ChromeImageButton;

/** Last privacy guide page. */
@NullMarked
public class DoneFragment extends PrivacyGuideBasePage {

    @Override
    public @Nullable View onCreateView(
            LayoutInflater inflater,
            @Nullable ViewGroup container,
            @Nullable Bundle savedInstanceState) {
        return inflater.inflate(R.layout.privacy_guide_done, container, false);
    }

    @Override
    public void onViewCreated(View view, @Nullable Bundle savedInstanceState) {
        super.onViewCreated(view, savedInstanceState);

        if (isUserSignedIn(getProfile())) {
            ChromeImageButton waaButton = view.findViewById(R.id.waa_button);
            waaButton.setOnClickListener(this::onWaaButtonClick);
        } else {
            view.findViewById(R.id.waa_heading).setVisibility(View.GONE);
            view.findViewById(R.id.waa_explanation).setVisibility(View.GONE);
        }
    }

    private void onWaaButtonClick(View view) {
        PrivacyGuideMetricsDelegate.recordMetricsForWaaLink();
        getCustomTabLauncher()
                .openUrlInCct(
                        getContext(), UrlConstants.GOOGLE_ACCOUNT_ACTIVITY_CONTROLS_FROM_PG_URL);
    }
}
