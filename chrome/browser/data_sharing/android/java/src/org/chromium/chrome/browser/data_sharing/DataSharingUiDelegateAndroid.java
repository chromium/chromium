// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.data_sharing;

import android.content.Context;
import android.content.Intent;

import org.jni_zero.CalledByNative;

import org.chromium.base.ContextUtils;
import org.chromium.base.IntentUtils;
import org.chromium.base.ServiceLoaderUtil;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.data_sharing.DataSharingUIDelegate;
import org.chromium.components.data_sharing.configs.DataSharingAvatarBitmapConfig;
import org.chromium.components.data_sharing.configs.DataSharingCreateUiConfig;
import org.chromium.components.data_sharing.configs.DataSharingJoinUiConfig;
import org.chromium.components.data_sharing.configs.DataSharingManageUiConfig;
import org.chromium.components.data_sharing.configs.DataSharingRuntimeDataConfig;
import org.chromium.url.GURL;

/**
 * Implementation of {@link DataSharingUIDelegate} that implements some methods while delegating
 * some to the internal delegate.
 */
@NullMarked
public class DataSharingUiDelegateAndroid implements DataSharingUIDelegate {

    private final @Nullable DataSharingUIDelegate mInternalDelegate;

    private static @Nullable DataSharingUIDelegate sDelegateForTesting;

    DataSharingUiDelegateAndroid(Profile profile) {
        DataSharingImplFactory factory =
                ServiceLoaderUtil.maybeCreate(DataSharingImplFactory.class);
        if (sDelegateForTesting != null) {
            mInternalDelegate = sDelegateForTesting;
            return;
        }
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
    public @Nullable String showCreateFlow(DataSharingCreateUiConfig createUiConfig) {
        if (mInternalDelegate != null) {
            return mInternalDelegate.showCreateFlow(createUiConfig);
        }
        return null;
    }

    @Override
    public @Nullable String showJoinFlow(DataSharingJoinUiConfig joinUiConfig) {
        if (mInternalDelegate != null) {
            return mInternalDelegate.showJoinFlow(joinUiConfig);
        }
        return null;
    }

    @Override
    public @Nullable String showManageFlow(DataSharingManageUiConfig manageUiConfig) {
        if (mInternalDelegate != null) {
            return mInternalDelegate.showManageFlow(manageUiConfig);
        }
        return null;
    }

    @Override
    public void updateRuntimeData(
            @Nullable String sessionId, DataSharingRuntimeDataConfig runtimeData) {
        if (mInternalDelegate != null) {
            mInternalDelegate.updateRuntimeData(sessionId, runtimeData);
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
        Intent invitation_intent = DataSharingIntentUtils.createInvitationIntent(context, url);
        IntentUtils.safeStartActivity(context, invitation_intent);
    }

    /* Sets UI delegate for testing, to be used when native needs a new delegate. */
    public static void setForTesting(DataSharingUIDelegate delegate) {
        sDelegateForTesting = delegate;
    }
}
