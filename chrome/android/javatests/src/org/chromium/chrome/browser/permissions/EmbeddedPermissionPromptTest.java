// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.permissions;

import android.Manifest;

import androidx.test.filters.MediumTest;

import org.hamcrest.Matchers;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.Features;
import org.chromium.chrome.browser.app.ChromeActivity;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.permissions.PermissionTestRule.PermissionUpdateWaiter;
import org.chromium.chrome.browser.permissions.RuntimePermissionTestUtils.RuntimePromptResponse;
import org.chromium.chrome.browser.permissions.RuntimePermissionTestUtils.TestAndroidPermissionDelegate;
import org.chromium.chrome.browser.profiles.ProfileManager;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.components.browser_ui.site_settings.WebsitePreferenceBridgeJni;
import org.chromium.components.content_settings.ContentSettingValues;
import org.chromium.components.content_settings.ContentSettingsType;
import org.chromium.components.permissions.EmbeddedPermissionDialogMediator;
import org.chromium.components.permissions.PermissionDialogController;
import org.chromium.components.permissions.PermissionsAndroidFeatureList;
import org.chromium.content_public.browser.test.util.DOMUtils;
import org.chromium.ui.modaldialog.DialogDismissalCause;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogManager.ModalDialogType;
import org.chromium.ui.modaldialog.ModalDialogProperties;

import java.util.concurrent.CountDownLatch;
import java.util.concurrent.TimeUnit;

@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@Batch(Batch.PER_CLASS)
public class EmbeddedPermissionPromptTest {
    private static final String TEST_PAGE = "/content/test/data/android/permission_element.html";
    private static final String LOOPBACK_ADDRESS = "http://127.0.0.1:12345";
    private static final int TEST_TIMEOUT = 10000;
    private static final int TEST_POLLING = 1000;

    @Rule public PermissionTestRule mActivityTestRule = new PermissionTestRule();
    private TestAndroidPermissionDelegate mTestAndroidPermissionDelegate;

    @Before
    public void setUp() throws Exception {
        mActivityTestRule.getEmbeddedTestServerRule().setServerPort(12345);
        mActivityTestRule.setUpActivity();
    }

    /**
     * Sets native ContentSetting value for the given type and origin.
     *
     * @param type defines ContentSetting type to call native permission setting.
     * @param origin defines origin to call native permission setting.
     * @param value expected value for the above ContentSetting type.
     */
    private void setNativeContentSetting(
            @ContentSettingsType.EnumType int type,
            final String origin,
            @ContentSettingValues int value) {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    WebsitePreferenceBridgeJni.get()
                            .setPermissionSettingForOrigin(
                                    ProfileManager.getLastUsedRegularProfile(),
                                    type,
                                    origin,
                                    origin,
                                    value);
                });
    }

    private void waitForTitleUpdate(String title, ChromeActivity activity) throws Exception {
        final Tab tab = activity.getActivityTab();
        final PermissionUpdateWaiter permissionUpdateWaiter =
                new PermissionUpdateWaiter(title, activity);
        ThreadUtils.runOnUiThreadBlocking(() -> tab.addObserver(permissionUpdateWaiter));
        permissionUpdateWaiter.waitForNumUpdates(0);
        ThreadUtils.runOnUiThreadBlocking(() -> tab.removeObserver(permissionUpdateWaiter));
    }

    private void waitOnLatch(int seconds) throws Exception {
        CountDownLatch latch = new CountDownLatch(1);
        latch.await(seconds, TimeUnit.SECONDS);
    }

    /**
     * Run a test related to the embedded permission prompt, based on the specified parameters.
     *
     * @param testAndroidPermissionDelegate The TestAndroidPermissionDelegate to be used for this
     *     test.
     * @param page the test page to load in order to run the text.
     * @param nodeId ID of the permission element to click on during the test.
     * @param type content setting type for the permission element.
     * @param value value for the `type` content setting setup before running the test.
     * @param expectedPromptText The string that matches the text of the embedded permission prompt
     *     dialog url.
     * @param expectedPositiveButtonText The string that matches the button text of the positive
     *     button on the dialog.
     * @param expectedPositiveEphemeralButtonText The string that matches the button text of the
     *     positive ephemeral button on the dialog url.
     * @param expectedNegativeButtonText The string that matches the button text of the negative
     *     button on the dialog url.
     */
    private void runTest(
            final TestAndroidPermissionDelegate testAndroidPermissionDelegate,
            final String page,
            final String nodeId,
            @ContentSettingsType.EnumType int type,
            @ContentSettingValues int value,
            final String expectedPromptText,
            final String expectedPositiveButtonText,
            final String expectedPositiveEphemeralButtonText,
            final String expectedNegativeButtonText)
            throws Exception {

        final String url = mActivityTestRule.getURL(TEST_PAGE);
        try {
            setNativeContentSetting(type, url, value);
            final ChromeActivity activity = mActivityTestRule.getActivity();
            activity.getWindowAndroid().setAndroidPermissionDelegate(testAndroidPermissionDelegate);

            mActivityTestRule.setUpUrl(TEST_PAGE);
            waitOnLatch(3);

            // Click on a permission element with ID and wait for dialog
            clickNodeWithId(nodeId);
            CriteriaHelper.pollUiThread(
                    () -> {
                        boolean isDialogShownForTest =
                                PermissionDialogController.getInstance().isDialogShownForTest();
                        Criteria.checkThat(isDialogShownForTest, Matchers.is(true));
                    },
                    TEST_TIMEOUT,
                    TEST_POLLING);

            final ModalDialogManager manager =
                    ThreadUtils.runOnUiThreadBlocking(activity::getModalDialogManager);

            // Verify the correct string resources are displayed.
            EmbeddedPermissionDialogMediator dialogMediator =
                    (EmbeddedPermissionDialogMediator)
                            manager.getCurrentDialogForTest().get(ModalDialogProperties.CONTROLLER);
            Assert.assertEquals(
                    expectedPromptText, dialogMediator.getDelegateForTest().getMessageText());
            Assert.assertEquals(
                    expectedPositiveButtonText,
                    dialogMediator.getDelegateForTest().getPositiveButtonText());
            Assert.assertEquals(
                    expectedPositiveEphemeralButtonText,
                    dialogMediator.getDelegateForTest().getPositiveEphemeralButtonText());
            Assert.assertEquals(
                    expectedNegativeButtonText,
                    dialogMediator.getDelegateForTest().getNegativeButtonText());

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
            waitForTitleUpdate("dismiss", activity);
        } finally {
            setNativeContentSetting(type, url, ContentSettingValues.DEFAULT);
        }
    }

    private void clickNodeWithId(String id) throws Exception {
        DOMUtils.clickNode(mActivityTestRule.getWebContents(), id);
    }

    public @ContentSettingsType.EnumType int stringToContentSettingsType(
            String contentSettingsType) {
        switch (contentSettingsType) {
            case "camera":
                return ContentSettingsType.MEDIASTREAM_CAMERA;
            case "microphone":
                return ContentSettingsType.MEDIASTREAM_MIC;
            case "geolocation":
                return ContentSettingsType.GEOLOCATION;
            default:
                assert false : "Unreached";
        }
        return ContentSettingsType.DEFAULT;
    }

    @Test
    @MediumTest
    @Features.EnableFeatures({PermissionsAndroidFeatureList.PERMISSION_ELEMENT})
    @Features.DisableFeatures(PermissionsAndroidFeatureList.ONE_TIME_PERMISSION)
    public void testAskPromptTextWithoutOneTime() throws Exception {
        String[] requestablePermission =
                new String[] {
                    Manifest.permission.ACCESS_COARSE_LOCATION,
                    Manifest.permission.ACCESS_FINE_LOCATION
                };
        mTestAndroidPermissionDelegate =
                new TestAndroidPermissionDelegate(
                        requestablePermission, RuntimePromptResponse.GRANT);
        runTest(
                mTestAndroidPermissionDelegate,
                TEST_PAGE,
                "geolocation",
                stringToContentSettingsType("geolocation"),
                ContentSettingValues.ASK,
                LOOPBACK_ADDRESS + " wants to use your device's location",
                "Allow",
                "",
                "Don't allow");
    }

    @Test
    @MediumTest
    @Features.EnableFeatures({
        PermissionsAndroidFeatureList.ONE_TIME_PERMISSION,
        PermissionsAndroidFeatureList.PERMISSION_ELEMENT
    })
    public void testAskPromptTextWithOneTime() throws Exception {
        String[] requestablePermission =
                new String[] {
                    Manifest.permission.ACCESS_COARSE_LOCATION,
                    Manifest.permission.ACCESS_FINE_LOCATION
                };
        mTestAndroidPermissionDelegate =
                new TestAndroidPermissionDelegate(
                        requestablePermission, RuntimePromptResponse.GRANT);
        runTest(
                mTestAndroidPermissionDelegate,
                TEST_PAGE,
                "geolocation",
                stringToContentSettingsType("geolocation"),
                ContentSettingValues.ASK,
                LOOPBACK_ADDRESS + " wants to use your device's location",
                "Allow while visiting the site",
                "Allow this time",
                "Don't allow");
    }

    @Test
    @MediumTest
    @Features.EnableFeatures({
        PermissionsAndroidFeatureList.ONE_TIME_PERMISSION,
        PermissionsAndroidFeatureList.PERMISSION_ELEMENT
    })
    public void testPreviouslyDeniedPromptTextWithOneTime() throws Exception {
        String[] requestablePermission =
                new String[] {
                    Manifest.permission.ACCESS_COARSE_LOCATION,
                    Manifest.permission.ACCESS_FINE_LOCATION
                };
        mTestAndroidPermissionDelegate =
                new TestAndroidPermissionDelegate(
                        requestablePermission, RuntimePromptResponse.GRANT);
        runTest(
                mTestAndroidPermissionDelegate,
                TEST_PAGE,
                "geolocation",
                stringToContentSettingsType("geolocation"),
                ContentSettingValues.BLOCK,
                "You previously didn't allow location for this site",
                "Continue not allowing",
                /* expectedPositiveEphemeralButtonText */ "",
                "Allow this time");
    }

    @Test
    @MediumTest
    @Features.EnableFeatures(PermissionsAndroidFeatureList.PERMISSION_ELEMENT)
    @Features.DisableFeatures(PermissionsAndroidFeatureList.ONE_TIME_PERMISSION)
    public void testPreviouslyDeniedPromptTextWithoutOneTime() throws Exception {
        String[] requestablePermission =
                new String[] {
                    Manifest.permission.ACCESS_COARSE_LOCATION,
                    Manifest.permission.ACCESS_FINE_LOCATION
                };
        mTestAndroidPermissionDelegate =
                new TestAndroidPermissionDelegate(
                        requestablePermission, RuntimePromptResponse.GRANT);
        runTest(
                mTestAndroidPermissionDelegate,
                TEST_PAGE,
                "geolocation",
                stringToContentSettingsType("geolocation"),
                ContentSettingValues.BLOCK,
                "You previously didn't allow location for this site",
                "Continue not allowing",
                /* expectedPositiveEphemeralButtonText */ "",
                "Allow this time");
    }

    @Test
    @MediumTest
    @Features.EnableFeatures({PermissionsAndroidFeatureList.PERMISSION_ELEMENT})
    @DisabledTest(message = "crbug.com/392083174")
    public void testPreviouslyGrantedPromptText() throws Exception {
        String[] requestablePermission =
                new String[] {
                    Manifest.permission.ACCESS_COARSE_LOCATION,
                    Manifest.permission.ACCESS_FINE_LOCATION
                };
        mTestAndroidPermissionDelegate =
                new TestAndroidPermissionDelegate(
                        requestablePermission, RuntimePromptResponse.GRANT);
        runTest(
                mTestAndroidPermissionDelegate,
                TEST_PAGE,
                "geolocation",
                stringToContentSettingsType("geolocation"),
                ContentSettingValues.ALLOW,
                "You have allowed location on " + LOOPBACK_ADDRESS,
                "Continue allowing",
                /* expectedPositiveEphemeralButtonText */ "",
                "Stop allowing");
    }

    @Test
    @MediumTest
    @Features.EnableFeatures({PermissionsAndroidFeatureList.PERMISSION_ELEMENT})
    public void testDisableLocationSettingsPromptText() throws Exception {
        RuntimePermissionTestUtils.setupGeolocationSystemMock(false);
        String[] requestablePermission =
                new String[] {
                    Manifest.permission.ACCESS_COARSE_LOCATION,
                    Manifest.permission.ACCESS_FINE_LOCATION
                };
        mTestAndroidPermissionDelegate =
                new TestAndroidPermissionDelegate(
                        requestablePermission, RuntimePromptResponse.GRANT);
        runTest(
                mTestAndroidPermissionDelegate,
                TEST_PAGE,
                "geolocation",
                stringToContentSettingsType("geolocation"),
                ContentSettingValues.BLOCK,
                "To use your location on this site, give Chrome access",
                "Android settings",
                /* expectedPositiveEphemeralButtonText */ "",
                "Cancel");
    }

    @Test
    @MediumTest
    @Features.EnableFeatures({PermissionsAndroidFeatureList.PERMISSION_ELEMENT})
    public void testOsSettingsPromptText() throws Exception {
        mTestAndroidPermissionDelegate =
                new TestAndroidPermissionDelegate(new String[] {}, RuntimePromptResponse.DENY);
        runTest(
                mTestAndroidPermissionDelegate,
                TEST_PAGE,
                "geolocation",
                stringToContentSettingsType("geolocation"),
                ContentSettingValues.ALLOW,
                "To use your location on this site, give Chrome access",
                "Android settings",
                /* expectedPositiveEphemeralButtonText */ "",
                "Cancel");
    }
}
