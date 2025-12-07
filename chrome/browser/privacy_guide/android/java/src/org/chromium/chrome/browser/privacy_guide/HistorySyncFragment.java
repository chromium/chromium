// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.privacy_guide;

import static org.chromium.chrome.browser.privacy_guide.PrivacyGuideUtils.canUpdateHistorySyncValue;

import android.os.Bundle;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.CompoundButton;
import android.widget.TextView;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.ui.signin.history_sync.HistorySyncHelper;
import org.chromium.components.browser_ui.widget.MaterialSwitchWithText;

/** Controls the behavior of the History Sync privacy guide page. */
@NullMarked
public class HistorySyncFragment extends PrivacyGuideBasePage
        implements CompoundButton.OnCheckedChangeListener {
    private MaterialSwitchWithText mHistorySyncSwitch;
    private HistorySyncHelper mHistorySyncHelper;

    @Override
    public View onCreateView(
            LayoutInflater inflater,
            @Nullable ViewGroup container,
            @Nullable Bundle savedInstanceState) {
        return inflater.inflate(R.layout.privacy_guide_history_sync_step, container, false);
    }

    @Override
    public void onViewCreated(View view, @Nullable Bundle savedInstanceState) {
        mHistorySyncSwitch = view.findViewById(R.id.history_sync_switch);
        mHistorySyncHelper = HistorySyncHelper.getForProfile(getProfile());
        setHistorySyncSwitchState();

        mHistorySyncSwitch.setOnCheckedChangeListener(this);

        ((TextView) mHistorySyncSwitch.findViewById(R.id.switch_text))
                .setText(R.string.privacy_guide_history_and_tabs_sync_toggle);
        ((PrivacyGuideExplanationItem) view.findViewById(R.id.history_sync_item_one))
                .setSummaryText(
                        getContext()
                                .getString(R.string.privacy_guide_history_and_tabs_sync_item_one));
    }

    @Override
    public void onResume() {
        super.onResume();
        setHistorySyncSwitchState();
    }

    private void setHistorySyncSwitchState() {
        boolean newState = mHistorySyncHelper.isHistorySyncEnabled();
        boolean currentState = mHistorySyncSwitch.isChecked();
        if (newState != currentState) {
            mHistorySyncSwitch.setChecked(newState);
        }
    }

    @Override
    public void onCheckedChanged(CompoundButton buttonView, boolean isChecked) {
        if (!canUpdateHistorySyncValue(getProfile())) {
            return;
        }

        PrivacyGuideMetricsDelegate.recordMetricsOnHistorySyncChange(isChecked);
        mHistorySyncHelper.setHistoryAndTabsSync(isChecked);
    }
}
