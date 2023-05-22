// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.pwd_migration;

import android.content.Context;
import android.os.Bundle;
import android.view.View;

import androidx.annotation.Nullable;
import androidx.fragment.app.Fragment;

import org.chromium.components.version_info.VersionInfo;
import org.chromium.ui.widget.TextViewWithLeading;

/** This fragment contains the UI for the first page of the password migration warning. */
public class PasswordMigrationWarningIntroFragment extends Fragment {
    private Context mContext;

    public PasswordMigrationWarningIntroFragment(Context context) {
        super(R.layout.pwd_migration_warning_intro_fragment);
        mContext = context;
    }

    @Override
    public void onViewCreated(View view, @Nullable Bundle savedInstanceState) {
        TextViewWithLeading subtitleView = view.findViewById(R.id.migration_warning_sheet_subtitle);
        subtitleView.setText(mContext.getString(R.string.password_migration_warning_subtitle)
                                     .replace("%1$s", getChannelString()));
    }

    private String getChannelString() {
        if (VersionInfo.isCanaryBuild()) {
            return "Canary";
        }
        if (VersionInfo.isDevBuild()) {
            return "Dev";
        }
        if (VersionInfo.isBetaBuild()) {
            return "Beta";
        }
        assert !VersionInfo.isStableBuild();
        return "";
    }
}
