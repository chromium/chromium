// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import android.content.Context;
import android.content.res.Resources;
import android.graphics.Bitmap;
import android.graphics.drawable.BitmapDrawable;
import android.graphics.drawable.Drawable;

import androidx.annotation.ColorInt;

import org.chromium.base.Callback;
import org.chromium.base.Token;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.components.browser_ui.styles.SemanticColorUtils;
import org.chromium.components.browser_ui.widget.async_image.AsyncImageView;
import org.chromium.components.collaboration.messaging.CollaborationEvent;
import org.chromium.components.collaboration.messaging.MessageUtils;
import org.chromium.components.collaboration.messaging.PersistentMessage;
import org.chromium.components.collaboration.messaging.PersistentNotificationType;
import org.chromium.components.data_sharing.DataSharingUIDelegate;
import org.chromium.components.data_sharing.GroupMember;
import org.chromium.components.data_sharing.configs.DataSharingAvatarBitmapConfig;
import org.chromium.components.data_sharing.configs.DataSharingAvatarBitmapConfig.DataSharingAvatarCallback;
import org.chromium.components.tab_group_sync.EitherId.EitherGroupId;
import org.chromium.components.tab_group_sync.LocalTabGroupId;

import java.util.Collections;
import java.util.List;
import java.util.Objects;

/** Pushes label updates to UI for tabs. */
@NullMarked
public class TabLabeller extends TabObjectLabeller {
    private final Context mContext;
    private final DataSharingUIDelegate mDataSharingUiDelegate;
    private final ObservableSupplier<@Nullable Token> mTabGroupIdSupplier;

    public TabLabeller(
            Profile profile,
            Context context,
            DataSharingUIDelegate dataSharingUiDelegate,
            TabListNotificationHandler tabListNotificationHandler,
            ObservableSupplier<@Nullable Token> tabGroupIdSupplier) {
        super(profile, tabListNotificationHandler);
        mContext = context;
        mDataSharingUiDelegate = dataSharingUiDelegate;
        mTabGroupIdSupplier = tabGroupIdSupplier;
        // Do not observe mTabGroupIdSupplier. We will be told to #showAll() is this changes.
    }

    @Override
    protected boolean shouldApply(PersistentMessage message) {
        return mTabGroupIdSupplier.get() != null
                && Objects.equals(
                        mTabGroupIdSupplier.get(), MessageUtils.extractTabGroupId(message))
                && message.type == PersistentNotificationType.DIRTY_TAB
                && getTabId(message) != Tab.INVALID_TAB_ID
                && getTextRes(message) != Resources.ID_NULL;
    }

    @Override
    protected int getTextRes(PersistentMessage message) {
        if (message.collaborationEvent == CollaborationEvent.TAB_ADDED) {
            return org.chromium.chrome.tab_ui.R.string.tab_added_label;
        } else if (message.collaborationEvent == CollaborationEvent.TAB_UPDATED) {
            return org.chromium.chrome.tab_ui.R.string.tab_changed_label;
        } else {
            return Resources.ID_NULL;
        }
    }

    @Override
    protected List<PersistentMessage> getAllMessages() {
        Token tabGroupId = mTabGroupIdSupplier.get();
        if (tabGroupId == null) return Collections.emptyList();
        LocalTabGroupId localTabGroupId = new LocalTabGroupId(tabGroupId);
        EitherGroupId eitherGroupId = EitherGroupId.createLocalId(localTabGroupId);
        return mMessagingBackendService.getMessagesForGroup(
                eitherGroupId, PersistentNotificationType.DIRTY_TAB);
    }

    @Override
    protected int getTabId(PersistentMessage message) {
        return MessageUtils.extractTabId(message);
    }

    @Override
    protected AsyncImageView.Factory getAsyncImageFactory(PersistentMessage message) {
        return new AsyncImageView.Factory() {
            boolean mCancellationFlag;

            @Override
            public Runnable get(Callback<Drawable> consumer, int widthPx, int heightPx) {
                assert widthPx == heightPx;
                // Even with a null member a valid bitmap will be produced.
                @Nullable GroupMember groupMember = MessageUtils.extractMember(message);
                @ColorInt
                int fallbackColor = SemanticColorUtils.getDefaultIconColorAccent1(mContext);
                DataSharingAvatarCallback avatarCallback =
                        (Bitmap bitmap) -> onAvatar(consumer, bitmap);
                DataSharingAvatarBitmapConfig config =
                        new DataSharingAvatarBitmapConfig.Builder()
                                .setContext(mContext)
                                .setGroupMember(groupMember)
                                .setAvatarSizeInPixels(widthPx)
                                .setAvatarFallbackColor(fallbackColor)
                                .setDataSharingAvatarCallback(avatarCallback)
                                .build();
                mDataSharingUiDelegate.getAvatarBitmap(config);
                return this::cancel;
            }

            private void onAvatar(Callback<Drawable> consumer, Bitmap bitmap) {
                if (mCancellationFlag) return;

                consumer.onResult(new BitmapDrawable(mContext.getResources(), bitmap));
            }

            private void cancel() {
                mCancellationFlag = true;
            }
        };
    }
}
