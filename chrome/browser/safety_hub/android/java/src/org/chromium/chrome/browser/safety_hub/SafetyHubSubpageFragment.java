// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.safety_hub;

import android.os.Bundle;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.LinearLayout;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.preference.PreferenceCategory;

import org.chromium.components.browser_ui.settings.SettingsUtils;
import org.chromium.components.browser_ui.settings.TextMessagePreference;
import org.chromium.ui.widget.ButtonCompat;

/* Common ancestor for Safety Hub subpage fragments. */
public abstract class SafetyHubSubpageFragment extends SafetyHubBaseFragment {
    private static final String PREF_HEADER = "header";
    private static final String PREF_LIST = "preference_list";

    protected PreferenceCategory mPreferenceList;

    @Override
    public void onCreatePreferences(@Nullable Bundle bundle, @Nullable String s) {
        SettingsUtils.addPreferencesFromResource(this, R.xml.safety_hub_subpage_preferences);
        getActivity().setTitle(getTitleId());

        TextMessagePreference headPreference = (TextMessagePreference) findPreference(PREF_HEADER);
        headPreference.setSummary(getHeaderId());

        mPreferenceList = findPreference(PREF_LIST);
    }

    @NonNull
    @Override
    public View onCreateView(
            @NonNull LayoutInflater inflater,
            @Nullable ViewGroup container,
            @Nullable Bundle savedInstanceState) {
        LinearLayout view =
                (LinearLayout) super.onCreateView(inflater, container, savedInstanceState);
        LinearLayout bottomView =
                (LinearLayout) inflater.inflate(R.layout.safety_hub_bottom_elements, view, false);
        ButtonCompat bottomButton = bottomView.findViewById(R.id.safety_hub_permissions_button);
        bottomButton.setOnClickListener(
                new View.OnClickListener() {
                    @Override
                    public void onClick(View view) {
                        onBottomButtonClicked();
                    }
                });
        view.addView(bottomView);
        return view;
    }

    @Override
    public void onResume() {
        super.onResume();
        updatePreferenceList();
    }

    protected abstract int getTitleId();

    protected abstract int getHeaderId();

    protected abstract void onBottomButtonClicked();

    protected abstract void updatePreferenceList();
}
