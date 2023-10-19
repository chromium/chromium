// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.permissions;

import android.Manifest;
import android.os.Build;

import androidx.test.filters.MediumTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.MaxAndroidSdkLevel;
import org.chromium.chrome.browser.download.DownloadItem;
import org.chromium.chrome.browser.download.DownloadManagerService;
import org.chromium.chrome.browser.download.DownloadManagerService.DownloadObserver;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.permissions.RuntimePermissionTestUtils.RuntimePromptResponse;
import org.chromium.chrome.browser.permissions.RuntimePermissionTestUtils.TestAndroidPermissionDelegate;
import org.chromium.chrome.browser.profiles.ProfileKey;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.R;
import org.chromium.components.offline_items_collection.ContentId;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.content_public.common.ContentSwitches;

import java.util.List;

/** Testing the interaction with the runtime permission prompt (Android level prompt). */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@Batch(Batch.PER_CLASS)
public class RuntimePermissionTest {
    @Rule public PermissionTestRule mPermissionTestRule = new PermissionTestRule();

    private static final String GEOLOCATION_TEST =
            "/chrome/test/data/geolocation/geolocation_on_load.html";
    private static final String MEDIA_TEST = "/content/test/data/media/getusermedia.html";
    private static final String DOWNLOAD_TEST = "/chrome/test/data/android/download/get.html";

    private TestAndroidPermissionDelegate mTestAndroidPermissionDelegate;

    @Before
    public void setUp() throws Exception {
        mPermissionTestRule.setUpActivity();
    }

    @Test
    @MediumTest
    @Feature({"RuntimePermissions", "Location"})
    public void testAllowRuntimeLocation() throws Exception {
        RuntimePermissionTestUtils.setupGeolocationSystemMock();

        String[] requestablePermission =
                new String[] {
                    Manifest.permission.ACCESS_COARSE_LOCATION,
                    Manifest.permission.ACCESS_FINE_LOCATION
                };
        mTestAndroidPermissionDelegate =
                new TestAndroidPermissionDelegate(
                        requestablePermission, RuntimePromptResponse.GRANT);
        RuntimePermissionTestUtils.runTest(
                mPermissionTestRule,
                mTestAndroidPermissionDelegate,
                GEOLOCATION_TEST,
                /* expectPermissionAllowed= */ true,
                /* permissionPromptAllow= */ true,
                /* waitForMissingPermissionPrompt= */ false,
                /* waitForUpdater= */ true,
                /* javascriptToExecute= */ null,
                /* missingPermissionPromptTextId= */ 0);
    }

    @Test
    @MediumTest
    @Feature({"RuntimePermissions", "MediaPermissions"})
    @CommandLineFlags.Add(ContentSwitches.USE_FAKE_DEVICE_FOR_MEDIA_STREAM)
    public void testAllowRuntimeCamera() throws Exception {
        String[] requestablePermission = new String[] {Manifest.permission.CAMERA};
        mTestAndroidPermissionDelegate =
                new TestAndroidPermissionDelegate(
                        requestablePermission, RuntimePromptResponse.GRANT);
        RuntimePermissionTestUtils.runTest(
                mPermissionTestRule,
                mTestAndroidPermissionDelegate,
                MEDIA_TEST,
                /* expectPermissionAllowed= */ true,
                /* permissionPromptAllow= */ true,
                /* waitForMissingPermissionPrompt= */ false,
                /* waitForUpdater= */ true,
                "getUserMediaAndStopLegacy({video: true, audio: false});",
                /* missingPermissionPromptTextId= */ 0);
    }

    @Test
    @MediumTest
    @Feature({"RuntimePermissions", "MediaPermissions"})
    @CommandLineFlags.Add(ContentSwitches.USE_FAKE_DEVICE_FOR_MEDIA_STREAM)
    public void testAllowRuntimeMicrophone() throws Exception {
        String[] requestablePermission = new String[] {Manifest.permission.RECORD_AUDIO};
        mTestAndroidPermissionDelegate =
                new TestAndroidPermissionDelegate(
                        requestablePermission, RuntimePromptResponse.GRANT);
        RuntimePermissionTestUtils.runTest(
                mPermissionTestRule,
                mTestAndroidPermissionDelegate,
                MEDIA_TEST,
                /* expectPermissionAllowed= */ true,
                /* permissionPromptAllow= */ true,
                /* waitForMissingPermissionPrompt= */ false,
                /* waitForUpdater= */ true,
                "getUserMediaAndStopLegacy({video: false, audio: true});",
                /* missingPermissionPromptTextId= */ 0);
    }

    @Test
    @MediumTest
    @Feature({"RuntimePermissions", "Location"})
    public void testDenyRuntimeLocation() throws Exception {
        RuntimePermissionTestUtils.setupGeolocationSystemMock();

        String[] requestablePermission =
                new String[] {
                    Manifest.permission.ACCESS_COARSE_LOCATION,
                    Manifest.permission.ACCESS_FINE_LOCATION
                };
        mTestAndroidPermissionDelegate =
                new TestAndroidPermissionDelegate(
                        requestablePermission, RuntimePromptResponse.DENY);
        RuntimePermissionTestUtils.runTest(
                mPermissionTestRule,
                mTestAndroidPermissionDelegate,
                GEOLOCATION_TEST,
                /* expectPermissionAllowed= */ false,
                /* permissionPromptAllow= */ true,
                /* waitForMissingPermissionPrompt= */ true,
                /* waitForUpdater= */ true,
                /* javascriptToExecute= */ null,
                R.string.infobar_missing_location_permission_text);
    }

    @Test
    @MediumTest
    @Feature({"RuntimePermissions", "MediaPermissions"})
    @CommandLineFlags.Add(ContentSwitches.USE_FAKE_DEVICE_FOR_MEDIA_STREAM)
    public void testDenyRuntimeCamera() throws Exception {
        String[] requestablePermission = new String[] {Manifest.permission.CAMERA};
        mTestAndroidPermissionDelegate =
                new TestAndroidPermissionDelegate(
                        requestablePermission, RuntimePromptResponse.DENY);
        RuntimePermissionTestUtils.runTest(
                mPermissionTestRule,
                mTestAndroidPermissionDelegate,
                MEDIA_TEST,
                /* expectPermissionAllowed= */ false,
                /* permissionPromptAllow= */ true,
                /* waitForMissingPermissionPrompt= */ true,
                /* waitForUpdater= */ true,
                "getUserMediaAndStopLegacy({video: true, audio: false});",
                R.string.infobar_missing_camera_permission_text);
    }

    @Test
    @MediumTest
    @Feature({"RuntimePermissions", "MediaPermissions"})
    @CommandLineFlags.Add(ContentSwitches.USE_FAKE_DEVICE_FOR_MEDIA_STREAM)
    public void testDenyRuntimeMicrophone() throws Exception {
        String[] requestablePermission = new String[] {Manifest.permission.RECORD_AUDIO};
        mTestAndroidPermissionDelegate =
                new TestAndroidPermissionDelegate(
                        requestablePermission, RuntimePromptResponse.DENY);
        RuntimePermissionTestUtils.runTest(
                mPermissionTestRule,
                mTestAndroidPermissionDelegate,
                MEDIA_TEST,
                /* expectPermissionAllowed= */ false,
                /* permissionPromptAllow= */ true,
                /* waitForMissingPermissionPrompt= */ true,
                /* waitForUpdater= */ true,
                "getUserMediaAndStopLegacy({video: false, audio: true});",
                R.string.infobar_missing_microphone_permission_text);
    }

    @Test
    @MediumTest
    @Feature({"RuntimePermissions", "Downloads"})
    @MaxAndroidSdkLevel(
            value = Build.VERSION_CODES.Q,
            reason = "WRITE_EXTERNAL_STORAGE is not supported starting in Android R")
    public void testDenyRuntimeDownload() throws Exception {
        DownloadObserver observer =
                new DownloadObserver() {
                    @Override
                    public void onAllDownloadsRetrieved(
                            final List<DownloadItem> list, ProfileKey profileKey) {}

                    @Override
                    public void onDownloadItemUpdated(DownloadItem item) {}

                    @Override
                    public void onDownloadItemRemoved(String guid) {}

                    @Override
                    public void onAddOrReplaceDownloadSharedPreferenceEntry(ContentId id) {}

                    @Override
                    public void onDownloadItemCreated(DownloadItem item) {
                        Assert.assertFalse("Should not have started a download item", true);
                    }
                };

        TestThreadUtils.runOnUiThreadBlocking(
                () -> {
                    DownloadManagerService.getDownloadManagerService()
                            .addDownloadObserver(observer);
                });

        String[] requestablePermission = new String[] {Manifest.permission.WRITE_EXTERNAL_STORAGE};
        mTestAndroidPermissionDelegate =
                new TestAndroidPermissionDelegate(
                        requestablePermission, RuntimePromptResponse.DENY);
        RuntimePermissionTestUtils.runTest(
                mPermissionTestRule,
                mTestAndroidPermissionDelegate,
                DOWNLOAD_TEST,
                /* expectPermissionAllowed= */ false,
                /* permissionPromptAllow= */ null,
                /* waitForMissingPermissionPrompt= */ true,
                /* waitForUpdater= */ false,
                "document.getElementsByTagName('a')[0].click();",
                R.string.missing_storage_permission_download_education_text);
    }

    @Test
    @MediumTest
    @Feature({"RuntimePermissions", "Location"})
    public void testDenyTriggersNoRuntime() throws Exception {
        RuntimePermissionTestUtils.setupGeolocationSystemMock();

        String[] requestablePermission =
                new String[] {
                    Manifest.permission.ACCESS_COARSE_LOCATION,
                    Manifest.permission.ACCESS_FINE_LOCATION
                };
        mTestAndroidPermissionDelegate =
                new TestAndroidPermissionDelegate(
                        requestablePermission, RuntimePromptResponse.ASSERT_NEVER_ASKED);
        RuntimePermissionTestUtils.runTest(
                mPermissionTestRule,
                mTestAndroidPermissionDelegate,
                GEOLOCATION_TEST,
                /* expectPermissionAllowed= */ false,
                /* permissionPromptAllow= */ false,
                /* waitForMissingPermissionPrompt= */ false,
                /* waitForUpdater= */ true,
                /* javascriptToExecute= */ null,
                R.string.infobar_missing_location_permission_text);
    }

    @Test
    @MediumTest
    @Feature({"RuntimePermissions", "MediaPermissions"})
    @CommandLineFlags.Add(ContentSwitches.USE_FAKE_DEVICE_FOR_MEDIA_STREAM)
    public void testDenyAndNeverAskMicrophone() throws Exception {
        // First ask for mic and reply with "deny and never ask again";
        String[] requestablePermission = new String[] {Manifest.permission.RECORD_AUDIO};
        mTestAndroidPermissionDelegate =
                new TestAndroidPermissionDelegate(
                        requestablePermission, RuntimePromptResponse.NEVER_ASK_AGAIN);
        RuntimePermissionTestUtils.runTest(
                mPermissionTestRule,
                mTestAndroidPermissionDelegate,
                MEDIA_TEST,
                /* expectPermissionAllowed= */ false,
                /* permissionPromptAllow= */ true,
                /* waitForMissingPermissionPrompt= */ false,
                /* waitForUpdater= */ true,
                "getUserMediaAndStopLegacy({video: false, audio: true});",
                /* missingPermissionPromptTextId= */ 0);

        // Now set the expectation that the runtime prompt is not shown again.
        mTestAndroidPermissionDelegate.setResponse(RuntimePromptResponse.ASSERT_NEVER_ASKED);

        // Reload the page and ask again, this time no prompt at all should be shown.
        RuntimePermissionTestUtils.runTest(
                mPermissionTestRule,
                mTestAndroidPermissionDelegate,
                MEDIA_TEST,
                /* expectPermissionAllowed= */ false,
                /* permissionPromptAllow= */ null,
                /* waitForMissingPermissionPrompt= */ false,
                /* waitForUpdater= */ true,
                "getUserMediaAndStopLegacy({video: false, audio: true});",
                /* missingPermissionPromptTextId= */ 0);
    }

    @Test
    @MediumTest
    @Feature({"RuntimePermissions", "MediaPermissions"})
    @CommandLineFlags.Add(ContentSwitches.USE_FAKE_DEVICE_FOR_MEDIA_STREAM)
    public void testDenyAndNeverAskCamera() throws Exception {
        // First ask for camera and reply with "deny and never ask again";
        String[] requestablePermission = new String[] {Manifest.permission.CAMERA};
        mTestAndroidPermissionDelegate =
                new TestAndroidPermissionDelegate(
                        requestablePermission, RuntimePromptResponse.NEVER_ASK_AGAIN);
        RuntimePermissionTestUtils.runTest(
                mPermissionTestRule,
                mTestAndroidPermissionDelegate,
                MEDIA_TEST,
                /* expectPermissionAllowed= */ false,
                /* permissionPromptAllow= */ true,
                /* waitForMissingPermissionPrompt= */ false,
                /* waitForUpdater= */ true,
                "getUserMediaAndStopLegacy({video: true, audio: false});",
                /* missingPermissionPromptTextId= */ 0);

        // Now set the expectation that the runtime prompt is not shown again.
        mTestAndroidPermissionDelegate.setResponse(RuntimePromptResponse.ASSERT_NEVER_ASKED);

        // Reload the page and ask again, this time no prompt at all should be shown.
        RuntimePermissionTestUtils.runTest(
                mPermissionTestRule,
                mTestAndroidPermissionDelegate,
                MEDIA_TEST,
                /* expectPermissionAllowed= */ false,
                /* permissionPromptAllow= */ null,
                /* waitForMissingPermissionPrompt= */ false,
                /* waitForUpdater= */ true,
                "getUserMediaAndStopLegacy({video: true, audio: false});",
                /* missingPermissionPromptTextId= */ 0);
    }

    @Test
    @MediumTest
    @Feature({"RuntimePermissions", "Location"})
    public void testAlreadyGrantedRuntimeLocation() throws Exception {
        RuntimePermissionTestUtils.setupGeolocationSystemMock();

        String[] requestablePermission =
                new String[] {
                    Manifest.permission.ACCESS_COARSE_LOCATION,
                    Manifest.permission.ACCESS_FINE_LOCATION
                };
        mTestAndroidPermissionDelegate =
                new TestAndroidPermissionDelegate(
                        requestablePermission, RuntimePromptResponse.ALREADY_GRANTED);
        RuntimePermissionTestUtils.runTest(
                mPermissionTestRule,
                mTestAndroidPermissionDelegate,
                GEOLOCATION_TEST,
                /* expectPermissionAllowed= */ true,
                /* permissionPromptAllow= */ true,
                /* waitForMissingPermissionPrompt= */ false,
                /* waitForUpdater= */ true,
                /* javascriptToExecute= */ null,
                /* missingPermissionPromptTextId= */ 0);
    }

    @Test
    @MediumTest
    @Feature({"RuntimePermissions", "Location"})
    public void testAllowRuntimeLocationIncognito() throws Exception {
        RuntimePermissionTestUtils.setupGeolocationSystemMock();
        mPermissionTestRule.newIncognitoTabFromMenu();

        String[] requestablePermission =
                new String[] {
                    Manifest.permission.ACCESS_COARSE_LOCATION,
                    Manifest.permission.ACCESS_FINE_LOCATION
                };
        mTestAndroidPermissionDelegate =
                new TestAndroidPermissionDelegate(
                        requestablePermission, RuntimePromptResponse.GRANT);
        RuntimePermissionTestUtils.runTest(
                mPermissionTestRule,
                mTestAndroidPermissionDelegate,
                GEOLOCATION_TEST,
                /* expectPermissionAllowed= */ true,
                /* permissionPromptAllow= */ true,
                /* waitForMissingPermissionPrompt= */ false,
                /* waitForUpdater= */ true,
                /* javascriptToExecute= */ null,
                /* missingPermissionPromptTextId= */ 0);
    }

    @Test
    @MediumTest
    @Feature({"RuntimePermissions", "MediaPermissions"})
    @CommandLineFlags.Add(ContentSwitches.USE_FAKE_DEVICE_FOR_MEDIA_STREAM)
    public void testAllowRuntimeCameraIncognito() throws Exception {
        mPermissionTestRule.newIncognitoTabFromMenu();

        String[] requestablePermission = new String[] {Manifest.permission.CAMERA};
        mTestAndroidPermissionDelegate =
                new TestAndroidPermissionDelegate(
                        requestablePermission, RuntimePromptResponse.GRANT);
        RuntimePermissionTestUtils.runTest(
                mPermissionTestRule,
                mTestAndroidPermissionDelegate,
                MEDIA_TEST,
                /* expectPermissionAllowed= */ true,
                /* permissionPromptAllow= */ true,
                /* waitForMissingPermissionPrompt= */ false,
                /* waitForUpdater= */ true,
                "getUserMediaAndStopLegacy({video: true, audio: false});",
                /* missingPermissionPromptTextId= */ 0);
    }

    @Test
    @MediumTest
    @Feature({"RuntimePermissions", "MediaPermissions"})
    @CommandLineFlags.Add(ContentSwitches.USE_FAKE_DEVICE_FOR_MEDIA_STREAM)
    public void testAllowRuntimeMicrophoneIncognito() throws Exception {
        mPermissionTestRule.newIncognitoTabFromMenu();
        String[] requestablePermission = new String[] {Manifest.permission.RECORD_AUDIO};
        mTestAndroidPermissionDelegate =
                new TestAndroidPermissionDelegate(
                        requestablePermission, RuntimePromptResponse.GRANT);
        RuntimePermissionTestUtils.runTest(
                mPermissionTestRule,
                mTestAndroidPermissionDelegate,
                MEDIA_TEST,
                /* expectPermissionAllowed= */ true,
                /* permissionPromptAllow= */ true,
                /* waitForMissingPermissionPrompt= */ false,
                /* waitForUpdater= */ true,
                "getUserMediaAndStopLegacy({video: false, audio: true});",
                /* missingPermissionPromptTextId= */ 0);
    }
}
