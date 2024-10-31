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
import org.chromium.components.data_sharing.configs.DataSharingAvatarBitmapConfig;
import org.chromium.components.data_sharing.configs.DataSharingCreateUiConfig;
import org.chromium.components.data_sharing.configs.DataSharingJoinUiConfig;
import org.chromium.components.data_sharing.configs.DataSharingManageUiConfig;
import org.chromium.components.data_sharing.configs.MemberPickerConfig;
import org.chromium.url.GURL;

import java.util.List;

/**
 * Implementation of {@link DataSharingUIDelegate} that implements some methods while delegating
 * some to the internal delegate.
 */
class DataSharingUiDelegateAndroid implements DataSharingUIDelegate {

    private final @Nullable DataSharingUIDelegate mInternalDelegate;

    DataSharingUiDelegateAndroid(Profile profile) {
        DataSharingImplFactory factory =
                ServiceLoaderUtil.maybeCreate(DataSharingImplFactory.class);
        if (factory != null) {
            mInternalDelegate = factory.createUiDelegate(profile);
        } else {
            mInternalDelegate = null;
        }
    }

    @CalledByNative
    private static DataSharingUiDelegateAndroid create(Profile profile) {
        return new DataSharingUiDelegateAndroid(profile);
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
    public String showCreateFlow(DataSharingCreateUiConfig createUiConfig) {
        if (mInternalDelegate != null) {
            return mInternalDelegate.showCreateFlow(createUiConfig);
        }
        return null;
    }

    @Override
    public String showJoinFlow(DataSharingJoinUiConfig joinUiConfig) {
        if (mInternalDelegate != null) {
            return mInternalDelegate.showJoinFlow(joinUiConfig);
        }
        return null;
    }

    @Override
    public String showManageFlow(DataSharingManageUiConfig manageUiConfig) {
        if (mInternalDelegate != null) {
            return mInternalDelegate.showManageFlow(manageUiConfig);
        }
        return null;
    }

    @Override
    public void showAvatarsInTile(AvatarConfig avatarConfig) {
        if (mInternalDelegate != null) {
            mInternalDelegate.showAvatarsInTile(avatarConfig);
        }
    }

    @Override
    public void destroyFlow(String sessionId) {
        if (mInternalDelegate != null) {
            mInternalDelegate.destroyFlow(sessionId);
        }
    }

    @Override
    public void getAvatarBitmap(DataSharingAvatarBitmapConfig avatarBitmapConfig) {
        if (mInternalDelegate != null) {
            mInternalDelegate.getAvatarBitmap(avatarBitmapConfig);
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
