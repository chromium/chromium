// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.share.send_tab_to_self;

import android.app.Activity;
import android.content.Context;
import android.content.res.Resources;
import android.text.TextUtils;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.JniType;
import org.jni_zero.NativeMethods;

import org.chromium.base.ApplicationStatus;
import org.chromium.base.ContextUtils;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.layouts.LayoutType;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.state.SendTabToSelfTabCardLabelData;
import org.chromium.components.messages.MessageBannerProperties;
import org.chromium.components.messages.MessageDispatcher;
import org.chromium.components.messages.MessageDispatcherProvider;
import org.chromium.components.messages.MessageIdentifier;
import org.chromium.components.messages.MessageScopeType;
import org.chromium.components.messages.PrimaryActionClickBehavior;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.widget.Toast;

import java.util.List;

/**
 * Bridge to interface with send_tab_to_self_android_bridge which interacts with the corresponding
 * sync service. This is used by SendTabToSelfShareActivity when a user taps to share a tab. The
 * bridge is created and destroyed within the same method call.
 */
@JNINamespace("send_tab_to_self")
@NullMarked
public class SendTabToSelfAndroidBridge {
    /** Interface for a callback to receive the result of a send tab to self operation. */
    @FunctionalInterface
    public interface CommitConfirmationCallback {
        @CalledByNative
        void onResult(@SendTabToSelfResult int result);
    }

    // TODO(crbug.com/40618597): Add logic back in to track whether model is loaded.
    // private boolean mIsNativeSendTabToSelfModelLoaded;

    // TODO(crbug.com/492072882): Eventually remove the commitConfirmation parameter once
    // confirmation feedback behavior is fully unified and centralized across all Android sites.
    /**
     * Handles the action when the user selects a device.
     *
     * @param profile The profile to use for sending.
     * @param webContents The web contents of the current tab, or null if not available. When null,
     *     page context such as scroll position, form fields and navigation history will not be
     *     captured.
     * @param targetDeviceSyncCacheGuid The GUID of the target device.
     * @param targetDeviceName The name of the target device.
     * @param url The URL being shared.
     * @param title The title of the page being shared.
     * @param commitConfirmation Callback to receive the commit result.
     */
    public static void sendTabToDevice(
            Profile profile,
            @Nullable WebContents webContents,
            String targetDeviceSyncCacheGuid,
            String targetDeviceName,
            String url,
            String title,
            @Nullable CommitConfirmationCallback commitConfirmation,
            @ShareEntryPoint int entryPoint) {
        SendTabToSelfAndroidBridgeJni.get()
                .sendTabToDevice(
                        profile,
                        webContents,
                        targetDeviceSyncCacheGuid,
                        url,
                        title,
                        result -> {
                            showPostSendToast(result, targetDeviceName);
                            if (commitConfirmation != null) {
                                commitConfirmation.onResult(result);
                            }
                        },
                        entryPoint);
    }

    private static void showPostSendToast(
            @SendTabToSelfResult int result, String targetDeviceName) {
        if (!ChromeFeatureList.isEnabled(ChromeFeatureList.SEND_TAB_TO_SELF_POST_SEND_TOAST)) {
            return;
        }
        Context appContext = ContextUtils.getApplicationContext();
        switch (result) {
            case SendTabToSelfResult.SUCCESS:
                String successMessage =
                        appContext.getString(
                                R.string.send_tab_to_self_post_send_success_toast_android,
                                targetDeviceName);
                Toast.makeText(appContext, successMessage, Toast.LENGTH_SHORT).show();
                break;
            case SendTabToSelfResult.SUCCESS_THROTTLED:
                String throttledMessage =
                        appContext.getString(
                                R.string.send_tab_to_self_post_send_throttled_toast_android,
                                targetDeviceName);
                Toast.makeText(appContext, throttledMessage, Toast.LENGTH_SHORT).show();
                break;
            case SendTabToSelfResult.FAILURE_NO_INTERNET_CONNECTION:
            case SendTabToSelfResult.FAILURE_COMMIT_TIMEOUT:
                String noInternetMessage =
                        appContext.getString(R.string.send_tab_to_self_post_send_no_internet_toast);
                Toast.makeText(appContext, noInternetMessage, Toast.LENGTH_SHORT).show();
                break;
            default:
                String failureMessage =
                        appContext.getString(R.string.send_tab_to_self_post_send_failure_toast);
                Toast.makeText(appContext, failureMessage, Toast.LENGTH_SHORT).show();
                break;
        }
    }

    /**
     * Marks the entry associated with the GUID as opened.
     *
     * @param profile Profile of the user to mark entry for.
     * @param guid The GUID to mark the entry for.
     */
    public static void markEntryOpened(Profile profile, String guid) {
        SendTabToSelfAndroidBridgeJni.get().markEntryOpened(profile, guid);
    }

    /**
     * Dismiss the entry associated with the GUID.
     *
     * @param profile Profile of the user to dismiss entry for.
     * @param guid The GUID to dismiss the entry for.
     */
    public static void dismissEntry(Profile profile, String guid) {
        SendTabToSelfAndroidBridgeJni.get().dismissEntry(profile, guid);
    }

    /**
     * Marks the entry associated with the GUID as activated.
     *
     * @param profile Profile of the user to mark entry for.
     * @param guid The GUID to mark the entry for.
     * @param entryPoint The entry point from which the tab was activated.
     */
    public static void markEntryActivated(
            Profile profile, String guid, @ShareActivatedEntryPoint int entryPoint) {
        SendTabToSelfAndroidBridgeJni.get().markEntryActivated(profile, guid, entryPoint);
    }

    /**
     * @param profile Profile of the user for whom to retrieve the targetDeviceInfos.
     * @return All {@link TargetDeviceInfo} for the user, or an empty list if the model isn't ready.
     */
    public static List<TargetDeviceInfo> getAllTargetDeviceInfos(Profile profile) {
        // TODO(crbug.com/40618597): Add this assertion back in once the
        // code to load is in place.
        // assert mIsNativeSendTabToSelfModelLoaded;
        return SendTabToSelfAndroidBridgeJni.get().getAllTargetDeviceInfos(profile);
    }

    public static @Nullable @EntryPointDisplayReason Integer getEntryPointDisplayReason(
            Profile profile, String url) {
        return SendTabToSelfAndroidBridgeJni.get().getEntryPointDisplayReason(profile, url);
    }

    /**
     * Records the target device count when the Send Tab to Self UI is invoked.
     *
     * @param profile The profile to use.
     * @param displayReason The reason the entry point is displayed.
     * @param deviceCount The number of target devices.
     */
    public static void recordTargetDeviceCount(
            @ShareEntryPoint int entryPoint,
            @EntryPointDisplayReason int displayReason,
            int deviceCount) {
        SendTabToSelfAndroidBridgeJni.get()
                .recordTargetDeviceCount(entryPoint, displayReason, deviceCount);
    }

    /**
     * Attaches SendTabToSelfTabCardLabelData to a Tab to indicate which device sent it.
     *
     * @param tab The Tab to attach the user data to.
     * @param senderDeviceName The name of the device that sent the tab.
     */
    @CalledByNative
    public static void attachTabLabel(Tab tab, String guid, String senderDeviceName) {
        if (tab == null || tab.getUserDataHost() == null || TextUtils.isEmpty(senderDeviceName))
            return;

        tab.getUserDataHost()
                .setUserData(
                        SendTabToSelfTabCardLabelData.class,
                        new SendTabToSelfTabCardLabelData(
                                tab, guid, senderDeviceName, System.currentTimeMillis()));
        // TODO(crbug.com/488072250): Inform SendTabToSelfTabLabeller to update the UI. This
        // specifically affects the case where the tab switcher is already opened and a tab gets
        // auto-opened.
    }

    @CalledByNative
    public static void showMessageBanner(@Nullable WebContents webContents, String deviceName) {
        // The tab or web page has been closed or destroyed.
        if (webContents == null) return;
        WindowAndroid windowAndroid = webContents.getTopLevelNativeWindow();
        // The tab is detached from the UI or the containing activity is being torn down.
        if (windowAndroid == null) return;
        MessageDispatcher messageDispatcher = MessageDispatcherProvider.from(windowAndroid);
        // The activity is being recreated, destroyed, or does not support messaging.
        if (messageDispatcher == null) return;

        Context context = ContextUtils.getApplicationContext();
        Resources res = context.getResources();

        PropertyModel message =
                new PropertyModel.Builder(MessageBannerProperties.ALL_KEYS)
                        .with(
                                MessageBannerProperties.MESSAGE_IDENTIFIER,
                                MessageIdentifier.SEND_TAB_TO_SELF)
                        .with(
                                MessageBannerProperties.TITLE,
                                res.getString(R.string.send_tab_to_self_message_banner_title))
                        .with(
                                MessageBannerProperties.DESCRIPTION,
                                res.getString(
                                        R.string.send_tab_to_self_message_banner_subtitle,
                                        deviceName))
                        .with(
                                MessageBannerProperties.PRIMARY_BUTTON_TEXT,
                                res.getString(R.string.send_tab_to_self_message_open))
                        .with(MessageBannerProperties.ICON_RESOURCE_ID, R.drawable.send_tab)
                        .with(
                                MessageBannerProperties.ON_PRIMARY_ACTION,
                                SendTabToSelfAndroidBridge::onMessageBannerPrimaryAction)
                        .build();

        messageDispatcher.enqueueMessage(
                message, webContents, MessageScopeType.WEB_CONTENTS, false);
    }

    /**
     * Handles the primary action click on the message banner by showing the tab switcher in the
     * currently focused activity, then dismissing the banner.
     *
     * @return The behavior to follow after the click (dismiss immediately).
     */
    private static @PrimaryActionClickBehavior int onMessageBannerPrimaryAction() {
        Activity activity = ApplicationStatus.getLastTrackedFocusedActivity();
        if (activity instanceof ChromeTabbedActivity) {
            ChromeTabbedActivity tabbedActivity = (ChromeTabbedActivity) activity;
            if (tabbedActivity.getLayoutManager() != null) {
                tabbedActivity.getLayoutManager().showLayout(LayoutType.HUB, true);
            }
        }
        return PrimaryActionClickBehavior.DISMISS_IMMEDIATELY;
    }

    @NativeMethods
    public interface Natives {
        void sendTabToDevice(
                @JniType("Profile*") Profile profile,
                @Nullable WebContents webContents,
                String targetDeviceSyncCacheGuid,
                String url,
                String title,
                CommitConfirmationCallback commitConfirmation,
                @ShareEntryPoint int entryPoint);

        void markEntryOpened(@JniType("Profile*") Profile profile, String guid);

        void dismissEntry(@JniType("Profile*") Profile profile, String guid);

        void markEntryActivated(
                @JniType("Profile*") Profile profile,
                String guid,
                @ShareActivatedEntryPoint int entryPoint);

        @JniType("std::vector")
        List<TargetDeviceInfo> getAllTargetDeviceInfos(@JniType("Profile*") Profile profile);

        @Nullable @EntryPointDisplayReason Integer getEntryPointDisplayReason(
                @JniType("Profile*") Profile profile, String url);

        void recordTargetDeviceCount(
                @ShareEntryPoint int entryPoint,
                @EntryPointDisplayReason int displayReason,
                int deviceCount);
    }
}
