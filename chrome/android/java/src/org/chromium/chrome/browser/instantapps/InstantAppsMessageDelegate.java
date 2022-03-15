// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.instantapps;

import android.content.Context;
import android.graphics.drawable.BitmapDrawable;

import androidx.annotation.VisibleForTesting;
import androidx.appcompat.content.res.AppCompatResources;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.chrome.R;
import org.chromium.components.messages.DismissReason;
import org.chromium.components.messages.MessageBannerProperties;
import org.chromium.components.messages.MessageDispatcher;
import org.chromium.components.messages.MessageIdentifier;
import org.chromium.components.messages.MessageScopeType;
import org.chromium.components.messages.PrimaryActionClickBehavior;
import org.chromium.components.webapps.WebappsIconUtils;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * Delegate for the Instant Apps message. Use create() to create the delegate.
 */
public class InstantAppsMessageDelegate {
    private final Context mContext;
    private final WebContents mWebContents;
    private final InstantAppsBannerData mData;
    private final MessageDispatcher mMessageDispatcher;
    private PropertyModel mMessage;

    public static InstantAppsMessageDelegate create(Context context, WebContents webContents,
            MessageDispatcher messageDispatcher, InstantAppsBannerData data) {
        return new InstantAppsMessageDelegate(context, webContents, messageDispatcher, data);
    }

    private InstantAppsMessageDelegate(Context context, WebContents webContents,
            MessageDispatcher messageDispatcher, InstantAppsBannerData data) {
        mContext = context;
        mData = data;
        mWebContents = webContents;
        mMessageDispatcher = messageDispatcher;
        InstantAppsMessageDelegateJni.get().initializeNativeDelegate(
                this, mWebContents, data.getUrl());
    }

    /**
     * Construct and show the Instant Apps message.
     */
    public void showMessage() {
        mMessage =
                new PropertyModel.Builder(MessageBannerProperties.ALL_KEYS)
                        .with(MessageBannerProperties.MESSAGE_IDENTIFIER,
                                MessageIdentifier.INSTANT_APPS)
                        .with(MessageBannerProperties.TITLE,
                                String.format(mContext.getResources().getString(
                                                      R.string.instant_apps_message_title),
                                        mData.getAppName()))
                        .with(MessageBannerProperties.TITLE_CONTENT_DESCRIPTION,
                                String.format(
                                        mContext.getResources().getString(
                                                R.string.accessibility_instant_apps_message_title_content_description),
                                        mData.getAppName()))
                        .with(MessageBannerProperties.DESCRIPTION_ICON,
                                AppCompatResources.getDrawable(
                                        mContext, R.drawable.google_play_dark))
                        .with(MessageBannerProperties.RESIZE_DESCRIPTION_ICON, true)
                        .with(MessageBannerProperties.ICON, new BitmapDrawable(mData.getIcon()))
                        .with(MessageBannerProperties.ICON_TINT_COLOR,
                                MessageBannerProperties.TINT_NONE)
                        .with(MessageBannerProperties.LARGE_ICON, true)
                        .with(MessageBannerProperties.ICON_ROUNDED_CORNER_RADIUS_PX,
                                WebappsIconUtils.getIdealIconCornerRadiusPxForPromptUI())
                        .with(MessageBannerProperties.PRIMARY_BUTTON_TEXT,
                                mData.getPrimaryActionLabel())
                        .with(MessageBannerProperties.ON_PRIMARY_ACTION, this::handlePrimaryAction)
                        .with(MessageBannerProperties.ON_DISMISSED, this::handleMessageDismissed)
                        .build();

        if (mMessageDispatcher != null) {
            mMessageDispatcher.enqueueMessage(
                    mMessage, mWebContents, MessageScopeType.WEB_CONTENTS, false);
            InstantAppsMessageDelegateJni.get().onMessageShown(
                    mWebContents, mData.getUrl(), mData.isInstantAppDefault());
        }
    }

    /**
     * Open the Instant App when the message primary button is clicked.
     */
    private @PrimaryActionClickBehavior int handlePrimaryAction() {
        InstantAppsMessageDelegateJni.get().onPrimaryAction(mData.isInstantAppDefault());
        InstantAppsHandler.getInstance().launchFromBanner(mData);
        return PrimaryActionClickBehavior.DISMISS_IMMEDIATELY;
    }

    /**
     * Record dismissal metrics when the message is dismissed.
     * @param dismissReason The message dismissal reason.
     */
    private void handleMessageDismissed(@DismissReason int dismissReason) {
        InstantAppsMessageDelegateJni.get().onMessageDismissed(
                mWebContents, mData.getUrl(), mData.isInstantAppDefault());
    }

    @VisibleForTesting
    PropertyModel getMessageForTesting() {
        return mMessage;
    }

    @CalledByNative
    private void dismissMessage() {
        if (mMessageDispatcher != null) {
            mMessageDispatcher.dismissMessage(mMessage, DismissReason.DISMISSED_BY_FEATURE);
        }
    }

    @NativeMethods
    interface Natives {
        long initializeNativeDelegate(
                InstantAppsMessageDelegate delegate, WebContents webContents, String url);
        void onMessageShown(WebContents webContents, String url, boolean instantAppIsDefault);
        void onPrimaryAction(boolean instantAppIsDefault);
        void onMessageDismissed(WebContents webContents, String url, boolean instantAppIsDefault);
    }
}
