// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.data_sharing;

import android.app.Activity;
import android.content.Context;
import android.view.ViewGroup;

import androidx.annotation.NonNull;

import org.chromium.base.Callback;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.data_sharing.DataSharingUIDelegate;
import org.chromium.components.data_sharing.configs.AvatarConfig;
import org.chromium.components.data_sharing.configs.GroupMemberConfig;
import org.chromium.components.data_sharing.configs.MemberPickerConfig;
import org.chromium.url.GURL;

import java.util.List;

/** Implementation of {@link DataSharingUIDelegate}. */
class DataSharingUIDelegateImpl implements DataSharingUIDelegate {

    private final Profile mProfile;

    DataSharingUIDelegateImpl(Profile profile) {
        mProfile = profile;
    }

    @Override
    public void showMemberPicker(
            @NonNull Activity activity,
            @NonNull ViewGroup view,
            MemberPickerListener memberResult,
            MemberPickerConfig config) {}

    @Override
    public void showFullPicker(
            @NonNull Activity activity,
            @NonNull ViewGroup view,
            MemberPickerListener memberResult,
            MemberPickerConfig config) {}

    @Override
    public void showAvatars(
            @NonNull Context context,
            List<ViewGroup> views,
            List<String> emails,
            Callback<Boolean> success,
            AvatarConfig config) {}

    @Override
    public void createGroupMemberListView(
            @NonNull Activity activity,
            @NonNull ViewGroup view,
            String groupId,
            String tokenSecret,
            GroupMemberConfig config) {}

    @Override
    public void handleShareURLIntercepted(GURL url) {}
}
