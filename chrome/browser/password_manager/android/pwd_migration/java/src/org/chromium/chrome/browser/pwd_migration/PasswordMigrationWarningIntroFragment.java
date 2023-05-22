// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.pwd_migration;

import android.content.Context;
import android.os.Bundle;
import android.view.View;
import android.widget.Button;

import androidx.annotation.Nullable;
import androidx.fragment.app.Fragment;

import org.chromium.ui.widget.TextViewWithLeading;

/** This fragment contains the UI for the first page of the password migration warning. */
public class PasswordMigrationWarningIntroFragment extends Fragment {
    private Context mContext;
    private Runnable mAcknowledgeCallback;
    private Runnable mMoreOptionsCallback;
    private String mChannelString;

    public PasswordMigrationWarningIntroFragment(Context context, Runnable acknowledgeCallback,
            Runnable moreOptionsCallback, String channelString) {
        super(R.layout.pwd_migration_warning_intro_fragment);
        mContext = context;
        mAcknowledgeCallback = acknowledgeCallback;
        mMoreOptionsCallback = moreOptionsCallback;
        mChannelString = channelString;
    }

    @Override
    public void onViewCreated(View view, @Nullable Bundle savedInstanceState) {
        TextViewWithLeading subtitleView = view.findViewById(R.id.migration_warning_sheet_subtitle);
        subtitleView.setText(mContext.getString(R.string.password_migration_warning_subtitle)
                                     .replace("%1$s", mChannelString));
        Button acknowledgeButton = view.findViewById(R.id.acknowledge_password_migration_button);
        Button moreOptionsButton = view.findViewById(R.id.password_migration_more_options_button);
        acknowledgeButton.setOnClickListener((unusedView) -> mAcknowledgeCallback.run());
        moreOptionsButton.setOnClickListener((unusedView) -> mMoreOptionsCallback.run());
    }
}
