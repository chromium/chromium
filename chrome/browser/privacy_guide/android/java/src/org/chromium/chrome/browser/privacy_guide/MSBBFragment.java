// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.privacy_guide;

import android.os.Bundle;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;

import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.signin.services.UnifiedConsentServiceBridge;
import org.chromium.components.browser_ui.widget.MaterialSwitchWithText;

/** Controls the behaviour of the MSBB privacy guide page. */
public class MSBBFragment extends PrivacyGuideBasePage {
    @Override
    public View onCreateView(
            LayoutInflater inflater, ViewGroup container, Bundle savedInstanceState) {
        return ChromeFeatureList.sPrivacyGuideAndroid3.isEnabled()
                ? inflater.inflate(R.layout.privacy_guide_msbb_v3_step, container, false)
                : inflater.inflate(R.layout.privacy_guide_msbb_step, container, false);
    }

    @Override
    public void onViewCreated(View view, Bundle savedInstanceState) {
        MaterialSwitchWithText msbbSwitch = view.findViewById(R.id.msbb_switch);
        msbbSwitch.setChecked(PrivacyGuideUtils.isMsbbEnabled(getProfile()));

        msbbSwitch.setOnCheckedChangeListener(
                (button, isChecked) -> {
                    PrivacyGuideMetricsDelegate.recordMetricsOnMSBBChange(isChecked);
                    UnifiedConsentServiceBridge.setUrlKeyedAnonymizedDataCollectionEnabled(
                            getProfile(), isChecked);
                });
    }
}
