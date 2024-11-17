// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.permissions;

import android.content.pm.PackageManager;
import android.os.Handler;
import android.view.View;
import android.widget.TextView;

import androidx.annotation.StringRes;

import org.hamcrest.Matchers;
import org.junit.Assert;

import org.chromium.base.BuildInfo;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.chrome.browser.app.ChromeActivity;
import org.chromium.chrome.browser.permissions.PermissionTestRule.PermissionUpdateWaiter;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.test.R;
import org.chromium.chrome.test.util.browser.LocationSettingsTestUtil;
import org.chromium.device.geolocation.LocationProviderOverrider;
import org.chromium.device.geolocation.MockLocationProvider;
import org.chromium.ui.modaldialog.DialogDismissalCause;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogManager.ModalDialogType;
import org.chromium.ui.modaldialog.ModalDialogProperties;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.permissions.AndroidPermissionDelegate;
import org.chromium.ui.permissions.PermissionCallback;

import java.util.Arrays;
import java.util.Set;
import java.util.TreeSet;

/** Utility class to help test the runtime permission prompt (Android level prompt). */
public class RuntimePermissionTestUtils {
    public enum RuntimePromptResponse {
        GRANT,
        DENY,
        NEVER_ASK_AGAIN,
        ASSERT_NEVER_ASKED,
        ALREADY_GRANTED, // Also implies "ASSERT_NEVER_ASKED"
    }

    /** Utility delegate for to provide the permissions to be requested and the runtime response. */
    public static class TestAndroidPermissionDelegate implements AndroidPermissionDelegate {
        private RuntimePromptResponse mResponse;
        private final Set<String> mRequestablePermissions;
        private final Set<String> mGrantedPermissions;

        public TestAndroidPermissionDelegate(
                final String[] requestablePermissions, final RuntimePromptResponse response) {
            mRequestablePermissions = new TreeSet(Arrays.asList(requestablePermissions));
            mGrantedPermissions = new TreeSet();
            mResponse = response;
            if (mResponse == RuntimePromptResponse.ALREADY_GRANTED) {
                mGrantedPermissions.addAll(mRequestablePermissions);
                mResponse = RuntimePromptResponse.ASSERT_NEVER_ASKED;
            }
        }

        @Override
        public boolean hasPermission(final String permission) {
            return mGrantedPermissions.contains(permission);
        }

        @Override
        public boolean canRequestPermission(final String permission) {
            return mRequestablePermissions.contains(permission);
        }

        @Override
        public boolean isPermissionRevokedByPolicy(final String permission) {
            return false;
        }

        @Override
        public void requestPermissions(
                final String[] permissions, final PermissionCallback callback) {
            Assert.assertNotSame(
                    "Runtime permission requested.",
                    mResponse,
                    RuntimePromptResponse.ASSERT_NEVER_ASKED);
            // Call back needs to be made async.
            new Handler()
                    .post(
                            () -> {
                                int[] grantResults = new int[permissions.length];
                                for (int i = 0; i < permissions.length; ++i) {
                                    if (mRequestablePermissions.contains(permissions[i])
                                            && mResponse == RuntimePromptResponse.GRANT) {
                                        mGrantedPermissions.add(permissions[i]);
                                        grantResults[i] = PackageManager.PERMISSION_GRANTED;
                                    } else {
                                        grantResults[i] = PackageManager.PERMISSION_DENIED;
                                        if (mResponse == RuntimePromptResponse.NEVER_ASK_AGAIN) {
                                            mRequestablePermissions.remove(permissions[i]);
                                        }
                                    }
                                }
                                callback.onRequestPermissionsResult(permissions, grantResults);
                            });
        }

        @Override
        public boolean handlePermissionResult(
                final int requestCode, final String[] permissions, final int[] grantResults) {
            return false;
        }

        public void setResponse(RuntimePromptResponse response) {
            mResponse = response;
        }
    }

    public static void setupGeolocationSystemMock() {
        LocationSettingsTestUtil.setSystemLocationSettingEnabled(true);
        LocationProviderOverrider.setLocationProviderImpl(new MockLocationProvider());
    }

    private static void waitUntilDifferentDialogIsShowing(
            final PermissionTestRule permissionTestRule, final PropertyModel previousDialog) {
        CriteriaHelper.pollUiThread(
                () -> {
                    final ModalDialogManager manager =
                            permissionTestRule.getActivity().getModalDialogManager();
                    Criteria.checkThat(manager.isShowing(), Matchers.is(true));
                    Criteria.checkThat(
                            manager.getCurrentDialogForTest(), Matchers.not(previousDialog));
                });
    }

    /**
     * Run a test related to the runtime permission prompt, based on the specified parameters.
     *
     * @param permissionTestRule The PermissionTestRule of the calling test.
     * @param testAndroidPermissionDelegate The TestAndroidPermissionDelegate to be used for this
     *     test.
     * @param testUrl The URL of the test page to load in order to run the text.
     * @param expectPermissionAllowed Whether to expect that the permissions is granted by the end
     *     of the test.
     * @param promptDecision What to respond on the Chrome permission promp. (`null` means skip
     *     waiting for a permission prompt at all).
     * @param waitForMissingPermissionPrompt Whether to wait for a Chrome dialog informing the user
     *     that the Android permission is missing.
     * @param waitForUpdater Whether to wait for the test page to update the window title to confirm
     *     the test's success.
     * @param javascriptToExecute Some javascript to execute after the page loads (empty or null to
     *     skip).
     * @param missingPermissionPromptTextId The resource string id that matches the text of the
     *     missing permission prompt dialog (0 if not applicable).
     */
    public static void runTest(
            final PermissionTestRule permissionTestRule,
            final TestAndroidPermissionDelegate testAndroidPermissionDelegate,
            final String testUrl,
            final boolean expectPermissionAllowed,
            final @PermissionTestRule.PromptDecision int promptDecision,
            final boolean waitForMissingPermissionPrompt,
            final boolean waitForUpdater,
            final String javascriptToExecute,
            final @StringRes int missingPermissionPromptTextId)
            throws Exception {
        final ChromeActivity activity = permissionTestRule.getActivity();
        activity.getWindowAndroid().setAndroidPermissionDelegate(testAndroidPermissionDelegate);

        final Tab tab = activity.getActivityTab();
        final PermissionUpdateWaiter permissionUpdateWaiter =
                new PermissionUpdateWaiter(
                        expectPermissionAllowed ? "Granted" : "Denied", activity);
        ThreadUtils.runOnUiThreadBlocking(() -> tab.addObserver(permissionUpdateWaiter));

        permissionTestRule.setUpUrl(testUrl);

        if (javascriptToExecute != null && !javascriptToExecute.isEmpty()) {
            permissionTestRule.runJavaScriptCodeInCurrentTabWithGesture(javascriptToExecute);
        }

        PropertyModel askPermissionDialogModel = null;
        if (promptDecision != PermissionTestRule.PromptDecision.NONE) {
            // A permission prompt dialog is expected. Wait for chrome to display and accept or
            // deny.
            PermissionTestRule.waitForDialog(activity);
            final ModalDialogManager manager =
                    ThreadUtils.runOnUiThreadBlocking(activity::getModalDialogManager);
            askPermissionDialogModel = manager.getCurrentDialogForTest();

            PermissionTestRule.replyToDialog(promptDecision, activity);

            if (waitForMissingPermissionPrompt) {
                // Wait for Chrome to inform user that a permission is missing --> different dialog
                waitUntilDifferentDialogIsShowing(permissionTestRule, askPermissionDialogModel);
            }
        }

        if (waitForMissingPermissionPrompt) {
            final ModalDialogManager manager =
                    ThreadUtils.runOnUiThreadBlocking(activity::getModalDialogManager);

            // Wait for the dialog that informs the user permissions are missing, when the initial
            // prompt is rejected or expected to not be shown.
            if (promptDecision != PermissionTestRule.PromptDecision.ALLOW) {
                waitUntilDifferentDialogIsShowing(permissionTestRule, askPermissionDialogModel);
            }

            // Verify the correct missing permission string resource is displayed.
            final View dialogText =
                    manager.getCurrentDialogForTest()
                            .get(ModalDialogProperties.CUSTOM_VIEW)
                            .findViewById(R.id.text);
            String appName = BuildInfo.getInstance().hostPackageLabel;
            Assert.assertEquals(
                    ((TextView) dialogText).getText(),
                    activity.getResources().getString(missingPermissionPromptTextId, appName));

            int dialogType = activity.getModalDialogManager().getCurrentType();
            ThreadUtils.runOnUiThreadBlocking(
                    () -> {
                        manager.getCurrentPresenterForTest()
                                .dismissCurrentDialog(
                                        dialogType == ModalDialogType.APP
                                                ? DialogDismissalCause
                                                        .NAVIGATE_BACK_OR_TOUCH_OUTSIDE
                                                : DialogDismissalCause.NAVIGATE_BACK);
                    });
        }

        if (waitForUpdater) {
            // PermissionUpdateWaiter should register a response.
            permissionUpdateWaiter.waitForNumUpdates(0);
        }

        ThreadUtils.runOnUiThreadBlocking(() -> tab.removeObserver(permissionUpdateWaiter));
    }
}
