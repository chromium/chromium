// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.permissions;

import static org.chromium.build.NullUtil.assertNonNull;
import static org.chromium.build.NullUtil.assumeNonNull;

import android.app.Activity;
import android.content.Context;
import android.os.Bundle;
import android.text.SpannableStringBuilder;
import android.text.Spanned;
import android.text.method.LinkMovementMethod;
import android.view.ContextThemeWrapper;
import android.view.Gravity;
import android.widget.TextView;

import androidx.annotation.GravityInt;

import org.jni_zero.CalledByNative;
import org.jni_zero.JniType;
import org.jni_zero.NativeMethods;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider.ControlsPosition;
import org.chromium.chrome.browser.fullscreen.BrowserControlsManagerSupplier;
import org.chromium.chrome.browser.offlinepages.OfflinePageUtils;
import org.chromium.chrome.browser.page_info.ChromePageInfoControllerDelegate;
import org.chromium.chrome.browser.page_info.ChromePageInfoHighlight;
import org.chromium.chrome.browser.settings.SettingsNavigationFactory;
import org.chromium.chrome.browser.tabmodel.TabCreator;
import org.chromium.chrome.browser.tabmodel.TabCreatorManager;
import org.chromium.components.browser_ui.settings.SettingsNavigation;
import org.chromium.components.browser_ui.site_settings.SingleCategorySettings;
import org.chromium.components.browser_ui.site_settings.SiteSettingsCategory;
import org.chromium.components.content_settings.ContentSettingsType;
import org.chromium.components.page_info.PageInfoController;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modaldialog.DialogDismissalCause;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogManager.ModalDialogType;
import org.chromium.ui.modaldialog.ModalDialogProperties;
import org.chromium.ui.modaldialog.ModalDialogProperties.ButtonType;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.text.ChromeClickableSpan;

/**
 * Dialog triggered by the user clicking on the "manage" button in the Messages 2.0 flavor of quiet
 * permission prompt for notifications and geolocation. It is used for the loud Notifications
 * permission prompt for the Loud Clapper project.
 */
@NullMarked
public class PermissionBlockedDialog implements ModalDialogProperties.Controller {
    private final ModalDialogManager mModalDialogManager;
    private final Context mContext;
    private long mNativeDialogController;
    private @Nullable PropertyModel mPropertyModel;

    @CalledByNative
    private static PermissionBlockedDialog create(
            long nativeDialogController, WindowAndroid windowAndroid) {
        return new PermissionBlockedDialog(nativeDialogController, windowAndroid);
    }

    public PermissionBlockedDialog(long nativeDialogController, WindowAndroid windowAndroid) {
        mNativeDialogController = nativeDialogController;
        mContext = assertNonNull(windowAndroid.getActivity().get());

        mModalDialogManager = assertNonNull(windowAndroid.getModalDialogManager());
    }

    @CalledByNative
    void show(
            @JniType("std::u16string") String title,
            @JniType("std::u16string") String content,
            @JniType("std::u16string") String positiveButtonLabel,
            @JniType("std::u16string") String negativeButtonLabel,
            @JniType("std::u16string") String learnMoreText) {
        SpannableStringBuilder fullString = new SpannableStringBuilder();

        TextView message =
                new TextView(
                        new ContextThemeWrapper(
                                mContext, R.style.NotificationBlockedDialogContent));
        fullString.append(content);
        if (!learnMoreText.isEmpty()) {
            fullString.append(" ");
            int start = fullString.length();
            fullString.append(learnMoreText);
            fullString.setSpan(
                    new ChromeClickableSpan(
                            mContext,
                            (v) -> {
                                PermissionBlockedDialogJni.get()
                                        .onLearnMoreClicked(mNativeDialogController);
                            }),
                    start,
                    fullString.length(),
                    Spanned.SPAN_EXCLUSIVE_EXCLUSIVE);
        }

        message.setText(fullString);
        message.setMovementMethod(LinkMovementMethod.getInstance());

        mPropertyModel =
                new PropertyModel.Builder(ModalDialogProperties.ALL_KEYS)
                        .with(ModalDialogProperties.CONTROLLER, this)
                        .with(ModalDialogProperties.TITLE, title)
                        .with(ModalDialogProperties.CUSTOM_VIEW, message)
                        .with(ModalDialogProperties.POSITIVE_BUTTON_TEXT, positiveButtonLabel)
                        .with(ModalDialogProperties.NEGATIVE_BUTTON_TEXT, negativeButtonLabel)
                        .with(ModalDialogProperties.CANCEL_ON_TOUCH_OUTSIDE, true)
                        .build();
        mModalDialogManager.showDialog(mPropertyModel, ModalDialogType.APP);
    }

    @Override
    public void onClick(PropertyModel model, @ButtonType int buttonType) {
        if (buttonType == ButtonType.POSITIVE) {
            PermissionBlockedDialogJni.get().onPrimaryButtonClicked(mNativeDialogController);
            mModalDialogManager.dismissDialog(
                    mPropertyModel, DialogDismissalCause.POSITIVE_BUTTON_CLICKED);
        } else if (buttonType == ButtonType.NEGATIVE) {
            PermissionBlockedDialogJni.get().onNegativeButtonClicked(mNativeDialogController);
            mModalDialogManager.dismissDialog(
                    mPropertyModel, DialogDismissalCause.NEGATIVE_BUTTON_CLICKED);
        }
    }

    @Override
    public void onDismiss(PropertyModel model, int dismissalCause) {
        PermissionBlockedDialogJni.get().onDialogDismissed(mNativeDialogController);
        mNativeDialogController = 0;
    }

    @CalledByNative
    private void dismissDialog() {
        mModalDialogManager.dismissDialog(mPropertyModel, DialogDismissalCause.DISMISSED_BY_NATIVE);
    }

    /**
     * This method launches the site settings screen
     *
     * @param contentSettingsType The ContentSettingsType to which the site settings screen should
     *     navigate to.
     */
    @CalledByNative
    private void showSettings(int contentSettingsType) {
        int preferenceKey = 0;
        switch (contentSettingsType) {
            case ContentSettingsType.NOTIFICATIONS:
                preferenceKey = SiteSettingsCategory.Type.NOTIFICATIONS;
                break;
            case ContentSettingsType.GEOLOCATION:
            case ContentSettingsType.GEOLOCATION_WITH_OPTIONS:
                preferenceKey = SiteSettingsCategory.Type.DEVICE_LOCATION;
                break;
            default:
                assert false : "Should not be reached";
        }
        Bundle fragmentArguments = new Bundle();
        fragmentArguments.putString(
                SingleCategorySettings.EXTRA_CATEGORY,
                SiteSettingsCategory.preferenceKey(preferenceKey));
        SettingsNavigation settingsNavigation =
                SettingsNavigationFactory.createSettingsNavigation();
        settingsNavigation.startSettings(mContext, SingleCategorySettings.class, fragmentArguments);
    }

    @CalledByNative
    private static void showPageInfo(
            WindowAndroid windowAndroid, WebContents webContents, int contentSettingsType) {
        Activity activity = windowAndroid.getActivity().get();
        if (activity == null) return;

        BrowserControlsStateProvider stateProvider =
                BrowserControlsManagerSupplier.getValueOrNullFrom(windowAndroid);
        @GravityInt int dialogPosition = Gravity.TOP;
        if (stateProvider != null) {
            dialogPosition =
                    stateProvider.getControlsPosition() == ControlsPosition.BOTTOM
                            ? Gravity.BOTTOM
                            : Gravity.TOP;
        }

        TabCreator tabCreator = null;
        if (activity instanceof TabCreatorManager) {
            tabCreator = ((TabCreatorManager) activity).getTabCreator(/* incognito= */ false);
        }

        PageInfoController.show(
                activity,
                webContents,
                /* contentPublisher= */ null,
                PageInfoController.OpenedFromSource.PERMISSION_PROMPT,
                new ChromePageInfoControllerDelegate(
                        activity,
                        webContents,
                        () -> {
                            ModalDialogManager modalDialogManager =
                                    windowAndroid.getModalDialogManager();
                            assumeNonNull(modalDialogManager);
                            return modalDialogManager;
                        },
                        /* offlinePageLoadUrlDelegate= */ new OfflinePageUtils
                                .WebContentsOfflinePageLoadUrlDelegate(webContents),
                        /* storeInfoActionHandlerSupplier= */ null,
                        /* ephemeralTabCoordinatorSupplier= */ null,
                        ChromePageInfoHighlight.forPermission(contentSettingsType),
                        tabCreator,
                        /* packageName= */ null),
                ChromePageInfoHighlight.forPermission(contentSettingsType),
                dialogPosition,
                /* openPermissionsSubpage= */ true);
    }

    @NativeMethods
    interface Natives {
        void onPrimaryButtonClicked(long nativePermissionBlockedDialogController);

        void onNegativeButtonClicked(long nativePermissionBlockedDialogController);

        void onLearnMoreClicked(long nativePermissionBlockedDialogController);

        void onDialogDismissed(long nativePermissionBlockedDialogController);
    }
}
