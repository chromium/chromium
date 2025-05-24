// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.data_sharing;

import android.content.Context;
import android.graphics.drawable.BitmapDrawable;
import android.graphics.drawable.Drawable;

import androidx.annotation.ColorInt;

import org.chromium.base.Callback;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.data_sharing.ui.recent_activity.RecentActivityListCoordinator;
import org.chromium.components.browser_ui.styles.SemanticColorUtils;
import org.chromium.components.data_sharing.DataSharingUIDelegate;
import org.chromium.components.data_sharing.GroupMember;
import org.chromium.components.data_sharing.configs.DataSharingAvatarBitmapConfig;
import org.chromium.components.data_sharing.configs.DataSharingAvatarBitmapConfig.DataSharingAvatarCallback;

/** A provider that fetches avatar drawables for users from the data sharing backend. */
@NullMarked
public class DataSharingAvatarProvider implements RecentActivityListCoordinator.AvatarProvider {
    private final Context mContext;
    private final DataSharingUIDelegate mDataSharingUIDelegate;
    private final int mAvatarSizePx;

    /**
     * Constructor.
     *
     * @param context The application context.
     * @param dataSharingUIDelegate The UI delegate that fetches avatar from the backend.
     */
    public DataSharingAvatarProvider(Context context, DataSharingUIDelegate dataSharingUIDelegate) {
        mContext = context;
        mDataSharingUIDelegate = dataSharingUIDelegate;
        mAvatarSizePx =
                mContext.getResources().getDimensionPixelSize(R.dimen.recent_activity_avatar_size);
    }

    @Override
    public void getAvatarBitmap(
            @Nullable GroupMember member, Callback<Drawable> avatarDrawableCallback) {
        @ColorInt int fallbackColor = SemanticColorUtils.getDefaultIconColorAccent1(mContext);
        DataSharingAvatarCallback dataSharingAvatarCallback =
                bitmap -> {
                    avatarDrawableCallback.onResult(
                            new BitmapDrawable(mContext.getResources(), bitmap));
                };
        DataSharingAvatarBitmapConfig config =
                new DataSharingAvatarBitmapConfig.Builder()
                        .setContext(mContext)
                        .setGroupMember(member)
                        .setAvatarSizeInPixels(mAvatarSizePx)
                        .setAvatarFallbackColor(fallbackColor)
                        .setDataSharingAvatarCallback(dataSharingAvatarCallback)
                        .build();
        mDataSharingUIDelegate.getAvatarBitmap(config);
    }
}
