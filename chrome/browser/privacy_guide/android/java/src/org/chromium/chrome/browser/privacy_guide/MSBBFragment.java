// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.privacy_guide;

import android.os.Bundle;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;

import org.chromium.chrome.browser.signin.services.UnifiedConsentServiceBridge;
import org.chromium.components.browser_ui.widget.MaterialSwitchWithText;

/** Controls the behavior of the MSBB privacy guide page. */
public class MSBBFragment extends PrivacyGuideBasePage {
    private MaterialSwitchWithText mMSBBSwitch;

    @Override
    public View onCreateView(
            LayoutInflater inflater, ViewGroup container, Bundle savedInstanceState) {
        return inflater.inflate(R.layout.privacy_guide_msbb_step, container, false);
    }

    @Override
    public void onViewCreated(View view, Bundle savedInstanceState) {
        mMSBBSwitch = view.findViewById(R.id.msbb_switch);
        setMSBBSwitchState();

        mMSBBSwitch.setOnCheckedChangeListener(
                (button, isChecked) -> {
                    PrivacyGuideMetricsDelegate.recordMetricsOnMSBBChange(isChecked);
                    UnifiedConsentServiceBridge.setUrlKeyedAnonymizedDataCollectionEnabled(
                            getProfile(), isChecked);
                });
    }

    @Override
    public void onResume() {
        super.onResume();
        setMSBBSwitchState();
    }

    private void setMSBBSwitchState() {
        mMSBBSwitch.setChecked(PrivacyGuideUtils.isMsbbEnabled(getProfile()));
    }
}
