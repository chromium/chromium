// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.permissions;

import android.Manifest;
import android.content.pm.PackageManager;
import android.os.Handler;
import android.view.View;
import android.widget.TextView;

import androidx.annotation.StringRes;
import androidx.test.filters.MediumTest;

import org.hamcrest.Matchers;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.app.ChromeActivity;
import org.chromium.chrome.browser.download.DownloadItem;
import org.chromium.chrome.browser.download.DownloadManagerService;
import org.chromium.chrome.browser.download.DownloadManagerService.DownloadObserver;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.permissions.PermissionTestRule.PermissionUpdateWaiter;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.util.browser.LocationSettingsTestUtil;
import org.chromium.components.offline_items_collection.ContentId;
import org.chromium.components.permissions.R;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.content_public.common.ContentSwitches;
import org.chromium.device.geolocation.LocationProviderOverrider;
import org.chromium.device.geolocation.MockLocationProvider;
import org.chromium.ui.base.AndroidPermissionDelegate;
import org.chromium.ui.base.PermissionCallback;
import org.chromium.ui.modaldialog.DialogDismissalCause;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogProperties;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.Arrays;
import java.util.List;
import java.util.Set;
import java.util.TreeSet;

/**
 * Testing the interaction with the runtime permission prompt (Android level prompt).
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class RuntimePermissionTest {
    enum RuntimePromptResponse {
        GRANT,
        DENY,
        NEVER_ASK_AGAIN,
        ASSERT_NEVER_ASKED,
        ALREADY_GRANTED, // Also implies "ASSERT_NEVER_ASKED"
    }

    private static class TestAndroidPermissionDelegate implements AndroidPermissionDelegate {
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
            Assert.assertNotSame("Runtime permission requested.", mResponse,
                    RuntimePromptResponse.ASSERT_NEVER_ASKED);
            // Call back needs to be made async.
            new Handler().post(() -> {
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

    @Rule
    public PermissionTestRule mPermissionTestRule = new PermissionTestRule();

    private static final String GEOLOCATION_TEST =
            "/chrome/test/data/geolocation/geolocation_on_load.html";
    private static final String MEDIA_TEST = "/content/test/data/media/getusermedia.html";
    private static final String DOWNLOAD_TEST = "/chrome/test/data/android/download/get.html";

    private TestAndroidPermissionDelegate mTestAndroidPermissionDelegate;

    @Before
    public void setUp() throws Exception {
        mPermissionTestRule.setUpActivity();
    }

    private void waitUntilDifferentDialogIsShowing(final PropertyModel currentDialog) {
        CriteriaHelper.pollUiThread(() -> {
            final ModalDialogManager manager =
                    mPermissionTestRule.getActivity().getModalDialogManager();
            Criteria.checkThat(manager.isShowing(), Matchers.is(true));
            Criteria.checkThat(manager.getCurrentDialogForTest(), Matchers.not(currentDialog));
        });
    }

    private void setupGeolocationSystemMock() {
        LocationSettingsTestUtil.setSystemLocationSettingEnabled(true);
        LocationProviderOverrider.setLocationProviderImpl(new MockLocationProvider());
    }

    /**
     * Run a test related to the runtime permission prompt, based on the specified parameters.
     * @param testUrl The URL of the test page to load in order to run the text.
     * @param expectPermissionAllowed Whether to expect that the permissions is granted by the end
     *         of the test.
     * @param permissionPromptAllow Whether to respond with "allow" on the Chrome permission prompt
     *         (`null` means skip waiting for a permission prompt at all).
     * @param runtimePromptResponse How to respond to the runtime prompt.
     * @param waitForMissingPermissionPrompt Whether to wait for a Chrome dialog informing the user
     *         that the Android permission is missing.
     * @param waitForUpdater Whether to wait for the test page to update the window title to confirm
     *         the test's success.
     * @param javascriptToExecute Some javascript to execute after the page loads (empty or null to
     *         skip).
     * @param missingPermissionPromptTextId The resource string id that matches the text of the
     *         missing permission prompt dialog (0 if not applicable).
     * @param requestablePermission The Android permission(s) that will be requested by this test.
     * @throws Exception
     */
    private void runTest(final String testUrl, final boolean expectPermissionAllowed,
            final Boolean permissionPromptAllow, final RuntimePromptResponse runtimePromptResponse,
            final boolean waitForMissingPermissionPrompt, final boolean waitForUpdater,
            final String javascriptToExecute, final @StringRes int missingPermissionPromptTextId,
            final String[] requestablePermission) throws Exception {
        final ChromeActivity activity = mPermissionTestRule.getActivity();

        if (mTestAndroidPermissionDelegate == null) {
            mTestAndroidPermissionDelegate =
                    new TestAndroidPermissionDelegate(requestablePermission, runtimePromptResponse);
            activity.getWindowAndroid().setAndroidPermissionDelegate(
                    mTestAndroidPermissionDelegate);
        }

        final Tab tab = activity.getActivityTab();
        final PermissionUpdateWaiter permissionUpdateWaiter = new PermissionUpdateWaiter(
                expectPermissionAllowed ? "Granted" : "Denied", activity);
        tab.addObserver(permissionUpdateWaiter);

        mPermissionTestRule.setUpUrl(testUrl);

        if (javascriptToExecute != null && !javascriptToExecute.isEmpty()) {
            mPermissionTestRule.runJavaScriptCodeInCurrentTab(javascriptToExecute);
        }

        if (permissionPromptAllow != null) {
            // Wait for chrome permission dialog and accept it.
            PermissionTestRule.waitForDialog(activity);
            PermissionTestRule.replyToDialog(permissionPromptAllow, activity);
        }

        if (waitForMissingPermissionPrompt) {
            // Wait for missing permission dialog and dismiss it.
            final ModalDialogManager manager = TestThreadUtils.runOnUiThreadBlockingNoException(
                    activity::getModalDialogManager);
            waitUntilDifferentDialogIsShowing(manager.getCurrentDialogForTest());

            final View dialogText = manager.getCurrentDialogForTest()
                                            .get(ModalDialogProperties.CUSTOM_VIEW)
                                            .findViewById(R.id.text);
            Assert.assertEquals(((TextView) dialogText).getText(),
                    activity.getResources().getString(missingPermissionPromptTextId));

            TestThreadUtils.runOnUiThreadBlocking(() -> {
                manager.getCurrentPresenterForTest().dismissCurrentDialog(
                        DialogDismissalCause.NAVIGATE_BACK_OR_TOUCH_OUTSIDE);
            });
        }

        if (waitForUpdater) {
            // PermissionUpdateWaiter should register a response.
            permissionUpdateWaiter.waitForNumUpdates(0);
        }

        tab.removeObserver(permissionUpdateWaiter);
    }

    @Test
    @MediumTest
    @Feature({"RuntimePermissions", "Location"})
    public void testAllowRuntimeLocation() throws Exception {
        setupGeolocationSystemMock();

        runTest(GEOLOCATION_TEST, true /* expectPermissionAllowed */,
                true /* permissionPromptAllow */, RuntimePromptResponse.GRANT,
                false /* waitForMissingPermissionPrompt */, true /* waitForUpdater */,
                null /* javascriptToExecute */, 0 /* missingPermissionPromptTextId */,
                new String[] {Manifest.permission.ACCESS_COARSE_LOCATION,
                        Manifest.permission.ACCESS_FINE_LOCATION});
    }

    @Test
    @MediumTest
    @Feature({"RuntimePermissions", "MediaPermissions"})
    @CommandLineFlags.Add(ContentSwitches.USE_FAKE_DEVICE_FOR_MEDIA_STREAM)
    public void testAllowRuntimeCamera() throws Exception {
        runTest(MEDIA_TEST, true /* expectPermissionAllowed */, true /* permissionPromptAllow */,
                RuntimePromptResponse.GRANT, false /* waitForMissingPermissionPrompt */,
                true /* waitForUpdater */, "getUserMediaAndStop({video: true, audio: false});",
                0 /* missingPermissionPromptTextId */, new String[] {Manifest.permission.CAMERA});
    }

    @Test
    @MediumTest
    @Feature({"RuntimePermissions", "MediaPermissions"})
    @CommandLineFlags.Add(ContentSwitches.USE_FAKE_DEVICE_FOR_MEDIA_STREAM)
    public void testAllowRuntimeMicrophone() throws Exception {
        runTest(MEDIA_TEST, true /* expectPermissionAllowed */, true /* permissionPromptAllow */,
                RuntimePromptResponse.GRANT, false /* waitForMissingPermissionPrompt */,
                true /* waitForUpdater */, "getUserMediaAndStop({video: false, audio: true});",
                0 /* missingPermissionPromptTextId */,
                new String[] {Manifest.permission.RECORD_AUDIO});
    }

    @Test
    @MediumTest
    @Feature({"RuntimePermissions", "Location"})
    public void testDenyRuntimeLocation() throws Exception {
        setupGeolocationSystemMock();

        runTest(GEOLOCATION_TEST, false /* expectPermissionAllowed */,
                true /* permissionPromptAllow */, RuntimePromptResponse.DENY,
                true /* waitForMissingPermissionPrompt */, true /* waitForUpdater */,
                null /* javascriptToExecute */, R.string.infobar_missing_location_permission_text,
                new String[] {Manifest.permission.ACCESS_COARSE_LOCATION,
                        Manifest.permission.ACCESS_FINE_LOCATION});
    }

    @Test
    @MediumTest
    @Feature({"RuntimePermissions", "MediaPermissions"})
    @CommandLineFlags.Add(ContentSwitches.USE_FAKE_DEVICE_FOR_MEDIA_STREAM)
    public void testDenyRuntimeCamera() throws Exception {
        runTest(MEDIA_TEST, false /* expectPermissionAllowed */, true /* permissionPromptAllow */,
                RuntimePromptResponse.DENY, true /* waitForMissingPermissionPrompt */,
                true /* waitForUpdater */, "getUserMediaAndStop({video: true, audio: false});",
                R.string.infobar_missing_camera_permission_text,
                new String[] {Manifest.permission.CAMERA});
    }

    @Test
    @MediumTest
    @Feature({"RuntimePermissions", "MediaPermissions"})
    @CommandLineFlags.Add(ContentSwitches.USE_FAKE_DEVICE_FOR_MEDIA_STREAM)
    public void testDenyRuntimeMicrophone() throws Exception {
        runTest(MEDIA_TEST, false /* expectPermissionAllowed */, true /* permissionPromptAllow */,
                RuntimePromptResponse.DENY, true /* waitForMissingPermissionPrompt */,
                true /* waitForUpdater */, "getUserMediaAndStop({video: false, audio: true});",
                R.string.infobar_missing_microphone_permission_text,
                new String[] {Manifest.permission.RECORD_AUDIO});
    }

    @Test
    @MediumTest
    @Feature({"RuntimePermissions", "Downloads"})
    public void testDenyRuntimeDownload() throws Exception {
        DownloadObserver observer = new DownloadObserver() {
            @Override
            public void onAllDownloadsRetrieved(
                    final List<DownloadItem> list, boolean isOffTheRecord) {}
            @Override
            public void onDownloadItemUpdated(DownloadItem item) {}
            @Override
            public void onDownloadItemRemoved(String guid, boolean isOffTheRecord) {}
            @Override
            public void onAddOrReplaceDownloadSharedPreferenceEntry(ContentId id) {}

            @Override
            public void onDownloadItemCreated(DownloadItem item) {
                Assert.assertFalse("Should not have started a download item", true);
            }
        };

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            DownloadManagerService.getDownloadManagerService().addDownloadObserver(observer);
        });

        runTest(DOWNLOAD_TEST, false /* expectPermissionAllowed */,
                null /* permissionPromptAllow */, RuntimePromptResponse.DENY,
                true /* waitForMissingPermissionPrompt */, false /* waitForUpdater */,
                "document.getElementsByTagName('a')[0].click();",
                org.chromium.chrome.R.string.missing_storage_permission_download_education_text,
                new String[] {Manifest.permission.WRITE_EXTERNAL_STORAGE});
    }

    @Test
    @MediumTest
    @Feature({"RuntimePermissions", "Location"})
    public void testDenyTriggersNoRuntime() throws Exception {
        setupGeolocationSystemMock();

        runTest(GEOLOCATION_TEST, false /* expectPermissionAllowed */,
                false /* permissionPromptAllow */, RuntimePromptResponse.ASSERT_NEVER_ASKED,
                false /* waitForMissingPermissionPrompt */, true /* waitForUpdater */,
                null /* javascriptToExecute */, R.string.infobar_missing_location_permission_text,
                new String[] {Manifest.permission.ACCESS_COARSE_LOCATION,
                        Manifest.permission.ACCESS_FINE_LOCATION});
    }

    @Test
    @MediumTest
    @Feature({"RuntimePermissions", "MediaPermissions"})
    @CommandLineFlags.Add(ContentSwitches.USE_FAKE_DEVICE_FOR_MEDIA_STREAM)
    public void testDenyAndNeverAskMicrophone() throws Exception {
        // First ask for mic and reply with "deny and never ask again";
        runTest(MEDIA_TEST, false /* expectPermissionAllowed */, true /* permissionPromptAllow */,
                RuntimePromptResponse.NEVER_ASK_AGAIN, false /* waitForMissingPermissionPrompt */,
                true /* waitForUpdater */, "getUserMediaAndStop({video: false, audio: true});",
                0 /* missingPermissionPromptTextId */,
                new String[] {Manifest.permission.RECORD_AUDIO});

        // Now set the expectation that the runtime prompt is not shown again.
        mTestAndroidPermissionDelegate.setResponse(RuntimePromptResponse.ASSERT_NEVER_ASKED);

        // Reload the page and ask again, this time no prompt at all should be shown.
        runTest(MEDIA_TEST, false /* expectPermissionAllowed */, null /* permissionPromptAllow */,
                null /* runtimePromptResponse */, false /* waitForMissingPermissionPrompt */,
                true /* waitForUpdater */, "getUserMediaAndStop({video: false, audio: true});",
                0 /* missingPermissionPromptTextId */, null /* requestablePermission */);
    }

    @Test
    @MediumTest
    @Feature({"RuntimePermissions", "MediaPermissions"})
    @CommandLineFlags.Add(ContentSwitches.USE_FAKE_DEVICE_FOR_MEDIA_STREAM)
    public void testDenyAndNeverAskCamera() throws Exception {
        // First ask for camera and reply with "deny and never ask again";
        runTest(MEDIA_TEST, false /* expectPermissionAllowed */, true /* permissionPromptAllow */,
                RuntimePromptResponse.NEVER_ASK_AGAIN, false /* waitForMissingPermissionPrompt */,
                true /* waitForUpdater */, "getUserMediaAndStop({video: true, audio: false});",
                0 /* missingPermissionPromptTextId */, new String[] {Manifest.permission.CAMERA});

        // Now set the expectation that the runtime prompt is not shown again.
        mTestAndroidPermissionDelegate.setResponse(RuntimePromptResponse.ASSERT_NEVER_ASKED);

        // Reload the page and ask again, this time no prompt at all should be shown.
        runTest(MEDIA_TEST, false /* expectPermissionAllowed */, null /* permissionPromptAllow */,
                null /* runtimePromptResponse */, false /* waitForMissingPermissionPrompt */,
                true /* waitForUpdater */, "getUserMediaAndStop({video: true, audio: false});",
                0 /* missingPermissionPromptTextId */, null /* requestablePermission */);
    }

    @Test
    @MediumTest
    @Feature({"RuntimePermissions", "Location"})
    public void testAlreadyGrantedRuntimeLocation() throws Exception {
        setupGeolocationSystemMock();

        runTest(GEOLOCATION_TEST, true /* expectPermissionAllowed */,
                true /* permissionPromptAllow */, RuntimePromptResponse.ALREADY_GRANTED,
                false /* waitForMissingPermissionPrompt */, true /* waitForUpdater */,
                null /* javascriptToExecute */, 0 /* missingPermissionPromptTextId */,
                new String[] {Manifest.permission.ACCESS_COARSE_LOCATION,
                        Manifest.permission.ACCESS_FINE_LOCATION});
    }

    @Test
    @MediumTest
    @DisabledTest(message = "https://crbug.com/1144670")
    @Feature({"RuntimePermissions", "Location"})
    public void testAllowRuntimeLocationIncognito() throws Exception {
        setupGeolocationSystemMock();
        mPermissionTestRule.newIncognitoTabFromMenu();

        runTest(GEOLOCATION_TEST, true /* expectPermissionAllowed */,
                true /* permissionPromptAllow */, RuntimePromptResponse.GRANT,
                false /* waitForMissingPermissionPrompt */, true /* waitForUpdater */,
                null /* javascriptToExecute */, 0 /* missingPermissionPromptTextId */,
                new String[] {Manifest.permission.ACCESS_COARSE_LOCATION,
                        Manifest.permission.ACCESS_FINE_LOCATION});
    }

    @Test
    @MediumTest
    @DisabledTest(message = "https://crbug.com/1144670")
    @Feature({"RuntimePermissions", "MediaPermissions"})
    @CommandLineFlags.Add(ContentSwitches.USE_FAKE_DEVICE_FOR_MEDIA_STREAM)
    public void testAllowRuntimeCameraIncognito() throws Exception {
        mPermissionTestRule.newIncognitoTabFromMenu();
        runTest(MEDIA_TEST, true /* expectPermissionAllowed */, true /* permissionPromptAllow */,
                RuntimePromptResponse.GRANT, false /* waitForMissingPermissionPrompt */,
                true /* waitForUpdater */, "getUserMediaAndStop({video: true, audio: false});",
                0 /* missingPermissionPromptTextId */, new String[] {Manifest.permission.CAMERA});
    }

    @Test
    @MediumTest
    @DisabledTest(message = "https://crbug.com/1144670")
    @Feature({"RuntimePermissions", "MediaPermissions"})
    @CommandLineFlags.Add(ContentSwitches.USE_FAKE_DEVICE_FOR_MEDIA_STREAM)
    public void testAllowRuntimeMicrophoneIncognito() throws Exception {
        mPermissionTestRule.newIncognitoTabFromMenu();
        runTest(MEDIA_TEST, true /* expectPermissionAllowed */, true /* permissionPromptAllow */,
                RuntimePromptResponse.GRANT, false /* waitForMissingPermissionPrompt */,
                true /* waitForUpdater */, "getUserMediaAndStop({video: false, audio: true});",
                0 /* missingPermissionPromptTextId */,
                new String[] {Manifest.permission.RECORD_AUDIO});
    }
}
