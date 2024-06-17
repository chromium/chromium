// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.privacy_guide;

import android.os.Bundle;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.CompoundButton;
import android.widget.TextView;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.sync.SyncServiceFactory;
import org.chromium.components.browser_ui.widget.MaterialSwitchWithText;
import org.chromium.components.sync.SyncService;
import org.chromium.components.sync.UserSelectableType;

import java.util.Set;

/** Controls the behaviour of the History Sync privacy guide page. */
public class HistorySyncFragment extends PrivacyGuideBasePage
        implements CompoundButton.OnCheckedChangeListener {
    private SyncService mSyncService;
    private boolean mInitialKeepEverythingSynced;

    @Override
    public View onCreateView(
            LayoutInflater inflater, ViewGroup container, Bundle savedInstanceState) {
        return inflater.inflate(R.layout.privacy_guide_history_sync_step, container, false);
    }

    @Override
    public void onViewCreated(@NonNull View view, @Nullable Bundle savedInstanceState) {
        mSyncService = SyncServiceFactory.getForProfile(getProfile());

        MaterialSwitchWithText historySyncSwitch = view.findViewById(R.id.history_sync_switch);
        historySyncSwitch.setChecked(PrivacyGuideUtils.isHistorySyncEnabled(getProfile()));

        historySyncSwitch.setOnCheckedChangeListener(this);

        if (!ChromeFeatureList.isEnabled(
                ChromeFeatureList.REPLACE_SYNC_PROMOS_WITH_SIGN_IN_PROMOS)) {
            mInitialKeepEverythingSynced = mSyncService.hasKeepEverythingSynced();
            return;
        }

        ((TextView) historySyncSwitch.findViewById(R.id.switch_text))
                .setText(R.string.privacy_guide_history_and_tabs_sync_toggle);
        ((PrivacyGuideExplanationItem) view.findViewById(R.id.history_sync_item_one))
                .setSummaryText(
                        getContext()
                                .getString(R.string.privacy_guide_history_and_tabs_sync_item_one));
    }

    @Override
    public void onCheckedChanged(CompoundButton buttonView, boolean isChecked) {
        PrivacyGuideMetricsDelegate.recordMetricsOnHistorySyncChange(isChecked);

        if (ChromeFeatureList.isEnabled(
                ChromeFeatureList.REPLACE_SYNC_PROMOS_WITH_SIGN_IN_PROMOS)) {
            mSyncService.setSelectedType(UserSelectableType.HISTORY, isChecked);
            mSyncService.setSelectedType(UserSelectableType.TABS, isChecked);
            return;
        }

        boolean keepEverythingSynced = isChecked && mInitialKeepEverythingSynced;

        Set<Integer> syncTypes = mSyncService.getSelectedTypes();
        if (isChecked) {
            syncTypes.add(UserSelectableType.HISTORY);
        } else {
            syncTypes.remove(UserSelectableType.HISTORY);
        }

        mSyncService.setSelectedTypes(keepEverythingSynced, syncTypes);
    }
}
