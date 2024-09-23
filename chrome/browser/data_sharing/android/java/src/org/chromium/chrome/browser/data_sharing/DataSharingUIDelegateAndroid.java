// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.data_sharing;

import android.app.Activity;
import android.content.Context;
import android.content.Intent;
import android.view.ViewGroup;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import org.jni_zero.CalledByNative;

import org.chromium.base.Callback;
import org.chromium.base.ContextUtils;
import org.chromium.base.IntentUtils;
import org.chromium.base.ServiceLoaderUtil;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.data_sharing.DataSharingUIDelegate;
import org.chromium.components.data_sharing.configs.AvatarConfig;
import org.chromium.components.data_sharing.configs.GroupMemberConfig;
import org.chromium.components.data_sharing.configs.MemberPickerConfig;
import org.chromium.url.GURL;

import java.util.List;

/**
 * Implementation of {@link DataSharingUIDelegate} that implements some methods while delegating
 * some to the internal delegate.
 */
class DataSharingUIDelegateAndroid implements DataSharingUIDelegate {

    private final @Nullable DataSharingUIDelegate mInternalDelegate;

    DataSharingUIDelegateAndroid(Profile profile) {
        DataSharingImplFactory factory =
                ServiceLoaderUtil.maybeCreate(DataSharingImplFactory.class);
        if (factory != null) {
            mInternalDelegate = factory.createUiDelegate(profile);
        } else {
            mInternalDelegate = null;
        }
    }

    @CalledByNative
    private static DataSharingUIDelegateAndroid create(Profile profile) {
        return new DataSharingUIDelegateAndroid(profile);
    }

    @Override
    public void showMemberPicker(
            @NonNull Activity activity,
            @NonNull ViewGroup view,
            MemberPickerListener memberResult,
            MemberPickerConfig config) {
        if (mInternalDelegate != null) {
            mInternalDelegate.showMemberPicker(activity, view, memberResult, config);
        }
    }

    @Override
    public void showFullPicker(
            @NonNull Activity activity,
            @NonNull ViewGroup view,
            MemberPickerListener memberResult,
            MemberPickerConfig config) {
        if (mInternalDelegate != null) {
            mInternalDelegate.showFullPicker(activity, view, memberResult, config);
        }
    }

    @Override
    public void showAvatars(
            @NonNull Context context,
            List<ViewGroup> views,
            List<String> emails,
            Callback<Boolean> success,
            AvatarConfig config) {
        if (mInternalDelegate != null) {
            mInternalDelegate.showAvatars(context, views, emails, success, config);
        }
    }

    @Override
    public void createGroupMemberListView(
            @NonNull Activity activity,
            @NonNull ViewGroup view,
            String groupId,
            String tokenSecret,
            GroupMemberConfig config) {
        if (mInternalDelegate != null) {
            mInternalDelegate.createGroupMemberListView(
                    activity, view, groupId, tokenSecret, config);
        }
    }

    @Override
    @CalledByNative
    public void handleShareURLIntercepted(GURL url) {
        Context context = ContextUtils.getApplicationContext();
        Intent invitation_intent =
                DataSharingNotificationManager.createInvitationIntent(context, url);
        IntentUtils.safeStartActivity(context, invitation_intent);
    }
}
