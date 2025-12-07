// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.privacy_guide;

import android.os.Bundle;
import android.text.method.LinkMovementMethod;
import android.text.style.ClickableSpan;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.TextView;

import org.chromium.base.metrics.RecordUserAction;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.components.browser_ui.widget.MaterialSwitchWithText;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.components.user_prefs.UserPrefs;
import org.chromium.ui.text.SpanApplier;

/** Controls the behavior of the Topics privacy guide page. */
@NullMarked
public class AdTopicsFragment extends PrivacyGuideBasePage {
    private MaterialSwitchWithText mAdTopicsSwitch;

    @Override
    public View onCreateView(
            LayoutInflater inflater,
            @Nullable ViewGroup container,
            @Nullable Bundle savedInstanceState) {
        return inflater.inflate(R.layout.privacy_guide_ad_topics_step, container, false);
    }

    @Override
    public void onViewCreated(View view, @Nullable Bundle savedInstanceState) {
        mAdTopicsSwitch = view.findViewById(R.id.ad_topics_switch);
        setAdTopicsSwitchState();

        mAdTopicsSwitch.setOnCheckedChangeListener(
                (button, isChecked) -> {
                    PrivacyGuideMetricsDelegate.recordMetricsOnAdTopicsChange(isChecked);
                    UserPrefs.get(getProfile())
                            .setBoolean(Pref.PRIVACY_SANDBOX_M1_TOPICS_ENABLED, isChecked);
                });
        PrivacyGuideExplanationItem thingsToConsiderBullet3 =
                view.findViewById(R.id.ad_topics_things_to_consider_bullet3_clank);
        thingsToConsiderBullet3.setSummaryText(
                SpanApplier.applySpans(
                        getResources()
                                .getString(
                                        R.string
                                                .settings_privacy_guide_ad_topics_things_to_consider_bullet3_clank),
                        new SpanApplier.SpanInfo(
                                "<link>",
                                "</link>",
                                new ClickableSpan() {
                                    @Override
                                    public void onClick(View view) {
                                        onPrivacyPolicyLinkClicked();
                                    }
                                })));
        ((TextView) thingsToConsiderBullet3.findViewById(R.id.summary))
                .setMovementMethod(LinkMovementMethod.getInstance());
    }

    @Override
    public void onResume() {
        super.onResume();
        setAdTopicsSwitchState();
    }

    private void setAdTopicsSwitchState() {
        mAdTopicsSwitch.setChecked(PrivacyGuideUtils.isAdTopicsEnabled(getProfile()));
    }

    private void onPrivacyPolicyLinkClicked() {
        RecordUserAction.record("Settings.PrivacyGuide.AdTopicsPrivacyPolicyLinkClicked");
        getCustomTabLauncher()
                .openUrlInCct(
                        getContext(),
                        getPrivacySandboxBridge().shouldUsePrivacyPolicyChinaDomain()
                                ? UrlConstants.GOOGLE_PRIVACY_POLICY_CHINA
                                : UrlConstants.GOOGLE_PRIVACY_POLICY);
    }
}
