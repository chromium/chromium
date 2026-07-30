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
import org.chromium.base.ObserverList;
import org.chromium.base.ThreadUtils;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.app.ChromeActivity;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.layouts.LayoutType;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabSelectionType;
import org.chromium.chrome.browser.tab.state.SendTabToSelfTabCardLabelData;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.ui.messages.snackbar.Snackbar;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager.SnackbarManageable;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManagerProvider;
import org.chromium.components.messages.MessageBannerProperties;
import org.chromium.components.messages.MessageDispatcher;
import org.chromium.components.messages.MessageDispatcherProvider;
import org.chromium.components.messages.MessageIdentifier;
import org.chromium.components.messages.PrimaryActionClickBehavior;
import org.chromium.components.signin.base.AccountInfo;
import org.chromium.components.signin.identitymanager.IdentityManager;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modelutil.PropertyModel;

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

    /**
     * Interface to observe when a Send Tab to Self label is attached to a tab. This is useful to
     * handle the race condition where a tab is auto-opened immediately (e.g. in the tab switcher)
     * but the STTS metadata label is attached asynchronously via JNI later. Observers (like the tab
     * switcher labeller) can listen to this to refresh the UI when the label arrives.
     */
    public interface LabelObjectObserver {
        void onLabelAttached(Tab tab);
    }

    private static final ObserverList<LabelObjectObserver> sLabelObservers = new ObserverList<>();

    public static void addLabelObserver(LabelObjectObserver observer) {
        ThreadUtils.assertOnUiThread();
        sLabelObservers.addObserver(observer);
    }

    public static void removeLabelObserver(LabelObjectObserver observer) {
        ThreadUtils.assertOnUiThread();
        sLabelObservers.removeObserver(observer);
    }

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
     */
    public static void sendTabToDevice(
            Profile profile,
            @Nullable WebContents webContents,
            String targetDeviceSyncCacheGuid,
            String targetDeviceName,
            String url,
            String title,
            @ShareEntryPoint int entryPoint) {
        SendTabToSelfAndroidBridgeJni.get()
                .sendTabToDevice(
                        profile,
                        webContents,
                        targetDeviceSyncCacheGuid,
                        url,
                        title,
                        result -> showPostSendUi(profile, webContents, result, targetDeviceName),
                        entryPoint);
    }

    private static void showPostSendUi(
            Profile profile,
            @Nullable WebContents webContents,
            @SendTabToSelfResult int result,
            String targetDeviceName) {
        if (!ChromeFeatureList.isEnabled(ChromeFeatureList.SEND_TAB_TO_SELF_POST_SEND_TOAST)) {
            return;
        }

        String userEmail = getUserEmail(profile);

        if (!maybeShowPostSendSnackbar(webContents, result, targetDeviceName, userEmail)) {
            // Fallback to Toast if no SnackbarManager is available. This is the case if a URL
            // is shared from a different app via the system Share Sheet.
            showPostSendFallbackToast(result, targetDeviceName, userEmail);
        }
    }

    // Tries to show the post-send snackbar. Returns true if the snackbar was shown, or false if it
    // couldn't be shown, which can happen if there's no window and no focused Activity.
    private static boolean maybeShowPostSendSnackbar(
            @Nullable WebContents webContents,
            @SendTabToSelfResult int result,
            String targetDeviceName,
            @Nullable String userEmail) {
        SnackbarManager snackbarManager = null;
        Context context = null;

        // Try to get the window from the web contents if available. This is used when the tab was
        // shared from within Chrome directly.
        if (webContents != null) {
            WindowAndroid windowAndroid = webContents.getTopLevelNativeWindow();
            if (windowAndroid != null) {
                snackbarManager = SnackbarManagerProvider.from(windowAndroid);
                context = windowAndroid.getContext().get();
            }
        }

        // Fallback: Get the window from the last focused activity. This is used when the tab was
        // shared via a DirectSend action in the system Share Sheet triggered by Chrome.
        if (snackbarManager == null) {
            Activity currentActivity = ApplicationStatus.getLastTrackedFocusedActivity();
            if (currentActivity instanceof SnackbarManageable
                    && !currentActivity.isFinishing()
                    && !currentActivity.isDestroyed()) {
                snackbarManager = ((SnackbarManageable) currentActivity).getSnackbarManager();
                context = currentActivity;
            }
        }

        // If no SnackbarManager is available, no snackbar can be shown.
        if (snackbarManager == null || context == null) return false;

        String message = getSnackbarMessage(context, result, targetDeviceName, userEmail);
        Snackbar snackbar =
                Snackbar.make(
                        message, null, Snackbar.TYPE_NOTIFICATION, Snackbar.UMA_SEND_TAB_TO_SELF);
        snackbarManager.showSnackbar(snackbar);
        return true;
    }

    // Shows the post-send toast; to be used as a fallback if the snackbar can't be shown.
    private static void showPostSendFallbackToast(
            @SendTabToSelfResult int result,
            String targetDeviceName,
            @Nullable String userEmail) {
        Context context = ContextUtils.getApplicationContext();
        String message = getSnackbarMessage(context, result, targetDeviceName, userEmail);
        // Note: `org.chromium.ui.widget.Toast` does not work in this situation (where Chrome is not
        // in the foreground), since it uses a custom view, which Android does not allow from the
        // background. So here a standard Android Toast has to be used instead.
        android.widget.Toast.makeText(context, message, android.widget.Toast.LENGTH_SHORT).show();
    }

    private static String getSnackbarMessage(
            Context context,
            @SendTabToSelfResult int result,
            String targetDeviceName,
            @Nullable String userEmail) {
        switch (result) {
            case SendTabToSelfResult.SUCCESS:
                return getSuccessMessage(context, targetDeviceName, userEmail);
            case SendTabToSelfResult.SUCCESS_THROTTLED:
                return context.getString(
                        R.string.send_tab_to_self_post_send_throttled_toast_android,
                        targetDeviceName);
            case SendTabToSelfResult.FAILURE_NO_INTERNET_CONNECTION:
            case SendTabToSelfResult.FAILURE_COMMIT_TIMEOUT:
                return context.getString(R.string.send_tab_to_self_post_send_no_internet_toast);
            default:
                return context.getString(R.string.send_tab_to_self_post_send_failure_toast);
        }
    }

    private static String getSuccessMessage(
            Context context, String targetDeviceName, @Nullable String userEmail) {
        if (!TextUtils.isEmpty(userEmail)) {
            return context.getString(
                    R.string.send_tab_to_self_post_send_success_toast_android,
                    targetDeviceName,
                    userEmail);
        }
        return context.getString(
                R.string.send_tab_to_self_post_send_success_toast_no_email_android,
                targetDeviceName);
    }

    private static @Nullable String getUserEmail(Profile profile) {
        IdentityManager identityManager =
                IdentityServicesProvider.get().getIdentityManager(profile);
        if (identityManager == null) {
            return null;
        }

        AccountInfo accountInfo = identityManager.getPrimaryAccountInfo();
        if (accountInfo == null || !accountInfo.canHaveEmailAddressDisplayed()) {
            return null;
        }

        return AccountInfo.getEmailFrom(accountInfo);
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
        ThreadUtils.assertOnUiThread();

        if (tab == null || tab.getUserDataHost() == null || TextUtils.isEmpty(senderDeviceName)) {
            return;
        }

        tab.getUserDataHost()
                .setUserData(
                        SendTabToSelfTabCardLabelData.class,
                        new SendTabToSelfTabCardLabelData(
                                tab, guid, senderDeviceName, System.currentTimeMillis()));
        if (ChromeFeatureList.sSendTabToSelfSupportAutoOpenInTabGrid.isEnabled()) {
            // Notify observers (e.g., UI components like SendTabToSelfTabLabeller) that the label
            // has been attached asynchronously so they can update the UI immediately.
            for (LabelObjectObserver observer : sLabelObservers) {
                observer.onLabelAttached(tab);
            }
        }
    }

    @CalledByNative
    public static void showMessageBanner(
            @Nullable WebContents webContents, String deviceName, int openedTabCount) {
        assert openedTabCount > 0;

        // The tab or web page has been closed or destroyed.
        if (webContents == null || webContents.isDestroyed()) return;
        WindowAndroid windowAndroid = webContents.getTopLevelNativeWindow();
        // The tab is detached from the UI or the containing activity is being torn down.
        if (windowAndroid == null) return;

        // Do not show the banner if Chrome is in overview mode (tab switcher).
        if (windowAndroid.getActivity().get() instanceof ChromeActivity chromeActivity) {
            if (chromeActivity.isInOverviewMode()) {
                return;
            }
        }

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
                                res.getQuantityString(
                                        R.plurals.send_tab_to_self_message_banner_title,
                                        openedTabCount,
                                        openedTabCount))
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

        // Enqueue as a window-scoped message so the banner persists across tab switching and is
        // not prematurely suppressed by tab-level visibility transitions during background tab
        // creation.
        messageDispatcher.enqueueWindowScopedMessage(message, false);
    }

    /**
     * Handles the primary action click on the message banner. Selects and opens the newest received
     * tab.
     *
     * @return The behavior to follow after the click (dismiss immediately).
     */
    private static @PrimaryActionClickBehavior int onMessageBannerPrimaryAction() {
        Activity activity = ApplicationStatus.getLastTrackedFocusedActivity();
        if (!(activity instanceof ChromeTabbedActivity)) {
            return PrimaryActionClickBehavior.DISMISS_IMMEDIATELY;
        }

        ChromeTabbedActivity tabbedActivity = (ChromeTabbedActivity) activity;
        TabModelSelector selector = tabbedActivity.getTabModelSelector();
        if (selector == null) {
            // Fall back to opening the tab switcher directly if the tab model selector is
            // unavailable (e.g. during activity startup, recreation, or mock test execution).
            if (tabbedActivity.getLayoutManager() != null) {
                tabbedActivity.getLayoutManager().showLayout(LayoutType.HUB, true);
            }
            return PrimaryActionClickBehavior.DISMISS_IMMEDIATELY;
        }

        TabModel normalTabModel = selector.getModel(/* incognito= */ false);
        int newestNewTabIndex = TabModel.INVALID_TAB_INDEX;
        long maxTimestamp = -1;

        // Iterate through all tabs in the standard model to find all unread/new
        // Send-Tab-to-Self tabs and identify the most recently added one by checking addition
        // timestamps.
        for (int i = 0; i < normalTabModel.getCount(); i++) {
            Tab tab = normalTabModel.getTabAt(i);
            if (tab == null) continue;

            SendTabToSelfTabCardLabelData data = SendTabToSelfTabCardLabelData.get(tab);
            if (data == null || data.isNegativeCache() || !data.shouldShowLabel()) {
                continue;
            }

            long timestamp = data.getAdditionTimestampMs();
            if (timestamp > maxTimestamp) {
                maxTimestamp = timestamp;
                newestNewTabIndex = i;
            }
        }

        // Highlight and focus the newly received tab by setting it as the active tab.
        if (newestNewTabIndex != TabModel.INVALID_TAB_INDEX) {
            normalTabModel.setIndex(newestNewTabIndex, TabSelectionType.FROM_USER);
        }

        return PrimaryActionClickBehavior.DISMISS_IMMEDIATELY;
    }

    /** Interface to be notified when the list of target devices changes. */
    public interface DeviceInfoObserver {
        // Called when the list of target DeviceInfos is updated. This also happens when a foreign
        // session is updated, because that affects the corresponding DeviceInfo's timestamp.
        @CalledByNative
        void onDeviceInfoChanged();
    }

    /**
     * Adds an observer to be notified when the list of target devices changes.
     *
     * @param profile Profile of the user.
     * @param observer The observer to add.
     * @return A native pointer to the observer bridge.
     */
    public static long addDeviceInfoObserver(Profile profile, DeviceInfoObserver observer) {
        return SendTabToSelfAndroidBridgeJni.get().addDeviceInfoObserver(profile, observer);
    }

    /**
     * Removes a DeviceInfoObserver.
     *
     * @param observerPtr The native pointer returned by addDeviceInfoObserver.
     */
    public static void removeDeviceInfoObserver(long observerPtr) {
        SendTabToSelfAndroidBridgeJni.get().removeDeviceInfoObserver(observerPtr);
    }

    /** Interface to be notified when the SendTabToSelfModel becomes ready. */
    @FunctionalInterface
    public interface SendTabToSelfModelObserver {
        @CalledByNative
        void onModelReady();
    }

    /**
     * Adds an observer to be notified when the model becomes ready.
     *
     * @param profile Profile of the user.
     * @param observer The observer to add.
     * @return A native pointer to the observer bridge.
     */
    public static long addModelObserver(Profile profile, SendTabToSelfModelObserver observer) {
        return SendTabToSelfAndroidBridgeJni.get().addModelObserver(profile, observer);
    }

    /**
     * Removes a model observer.
     *
     * @param observerPtr The native pointer returned by addModelObserver.
     */
    public static void removeModelObserver(long observerPtr) {
        SendTabToSelfAndroidBridgeJni.get().removeModelObserver(observerPtr);
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

        @Nullable
        @EntryPointDisplayReason
        Integer getEntryPointDisplayReason(@JniType("Profile*") Profile profile, String url);

        void recordTargetDeviceCount(
                @ShareEntryPoint int entryPoint,
                @EntryPointDisplayReason int displayReason,
                int deviceCount);

        long addDeviceInfoObserver(
                @JniType("Profile*") Profile profile, DeviceInfoObserver observer);

        void removeDeviceInfoObserver(long observerPtr);

        long addModelObserver(
                @JniType("Profile*") Profile profile, SendTabToSelfModelObserver observer);

        void removeModelObserver(long observerPtr);
    }
}
