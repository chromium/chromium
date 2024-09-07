// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.privacy_guide;

import android.os.Bundle;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;

import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.components.browser_ui.widget.MaterialSwitchWithText;
import org.chromium.components.user_prefs.UserPrefs;

/** Controls the behavior of the Topics privacy guide page. */
public class AdTopicsFragment extends PrivacyGuideBasePage {
    private MaterialSwitchWithText mAdTopicsSwitch;

    @Override
    public View onCreateView(
            LayoutInflater inflater, ViewGroup container, Bundle savedInstanceState) {
        return inflater.inflate(R.layout.privacy_guide_ad_topics_step, container, false);
    }

    @Override
    public void onViewCreated(View view, Bundle savedInstanceState) {
        mAdTopicsSwitch = view.findViewById(R.id.ad_topics_switch);

        mAdTopicsSwitch.setOnCheckedChangeListener(
                (button, isChecked) -> {
                    PrivacyGuideMetricsDelegate.recordMetricsOnAdTopicsChange(isChecked);
                    UserPrefs.get(getProfile())
                            .setBoolean(Pref.PRIVACY_SANDBOX_M1_TOPICS_ENABLED, isChecked);
                });
    }

    @Override
    public void onResume() {
        super.onResume();
        setAdTopicsSwitchState();
    }

    private void setAdTopicsSwitchState() {
        mAdTopicsSwitch.setChecked(PrivacyGuideUtils.isAdTopicsEnabled(getProfile()));
    }
}
