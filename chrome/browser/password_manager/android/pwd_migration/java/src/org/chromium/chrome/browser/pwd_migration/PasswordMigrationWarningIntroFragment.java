// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.pwd_migration;

import android.os.Bundle;
import android.view.View;
import android.widget.Button;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.fragment.app.Fragment;

import org.chromium.ui.widget.TextViewWithLeading;

/** This fragment contains the UI for the first page of the password migration warning. */
public class PasswordMigrationWarningIntroFragment extends Fragment {
    private static final String SUBTITLE_TEXT = "SUBTITLE_TEXT";
    private String mSubtitleText;
    private Runnable mAcknowledgeCallback;
    private Runnable mMoreOptionsCallback;

    public PasswordMigrationWarningIntroFragment(
            String introScreenSubtitle,
            Runnable acknowledgeCallback,
            Runnable moreOptionsCallback) {
        super(R.layout.pwd_migration_warning_intro_fragment);
        mSubtitleText = introScreenSubtitle;
        mAcknowledgeCallback = acknowledgeCallback;
        mMoreOptionsCallback = moreOptionsCallback;
    }

    public PasswordMigrationWarningIntroFragment() {}

    @Override
    public void onViewCreated(View view, @Nullable Bundle savedInstanceState) {
        TextViewWithLeading subtitleView = view.findViewById(R.id.migration_warning_sheet_subtitle);
        subtitleView.setText(mSubtitleText);
        Button acknowledgeButton = view.findViewById(R.id.acknowledge_password_migration_button);
        Button moreOptionsButton = view.findViewById(R.id.password_migration_more_options_button);
        acknowledgeButton.setOnClickListener((unusedView) -> mAcknowledgeCallback.run());
        moreOptionsButton.setOnClickListener((unusedView) -> mMoreOptionsCallback.run());
    }

    @Override
    public void onCreate(@Nullable Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        if (savedInstanceState != null) {
            mSubtitleText = savedInstanceState.getString(SUBTITLE_TEXT);
        }
    }

    @Override
    public void onSaveInstanceState(@NonNull Bundle outState) {
        super.onSaveInstanceState(outState);
        outState.putString(SUBTITLE_TEXT, mSubtitleText);
    }
}
