// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.permissions;

import static org.mockito.Mockito.any;
import static org.mockito.Mockito.doReturn;

import static org.chromium.components.permissions.PermissionUtil.getGeolocationType;

import android.Manifest;
import android.text.TextUtils;

import androidx.test.filters.MediumTest;

import org.hamcrest.Matchers;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.Features;
import org.chromium.base.test.util.Restriction;
import org.chromium.build.BuildConfig;
import org.chromium.chrome.browser.app.ChromeActivity;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.permissions.PermissionTestRule.PermissionUpdateWaiter;
import org.chromium.chrome.browser.permissions.RuntimePermissionTestUtils.RuntimePromptResponse;
import org.chromium.chrome.browser.permissions.RuntimePermissionTestUtils.TestAndroidPermissionDelegate;
import org.chromium.chrome.browser.profiles.ProfileManager;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.ui.hats.SurveyClient;
import org.chromium.chrome.browser.ui.hats.SurveyClientFactory;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.components.browser_ui.site_settings.WebsitePreferenceBridgeJni;
import org.chromium.components.content_settings.ContentSetting;
import org.chromium.components.content_settings.ContentSettingsType;
import org.chromium.components.permissions.EmbeddedPermissionDialogMediator;
import org.chromium.components.permissions.PermissionDialogController;
import org.chromium.components.permissions.PermissionsAndroidFeatureList;
import org.chromium.content_public.browser.test.util.DOMUtils;
import org.chromium.ui.modaldialog.DialogDismissalCause;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogManager.ModalDialogType;
import org.chromium.ui.modaldialog.ModalDialogProperties;
import org.chromium.ui.test.util.DeviceRestriction;

import java.util.concurrent.CountDownLatch;
import java.util.concurrent.TimeUnit;

@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@Features.EnableFeatures(PermissionsAndroidFeatureList.BYPASS_PEPC_SECURITY_FOR_TESTING)
@Batch(Batch.PER_CLASS)
@Restriction({DeviceRestriction.RESTRICTION_TYPE_NON_AUTO}) // crbug.com/394097674
public class EmbeddedPermissionPromptTest {
    public enum EmbeddedPermissiontResponse {
        NEGATIVE,
        POSITIVE,
        POSITIVE_EPHEMERAL,
        DISMISS_OUTSIDE,
    }

    private static final String TEST_PAGE = "/content/test/data/android/permission_element.html";
    private static final String LOOPBACK_ADDRESS = "http://127.0.0.1:12345";
    private static final int TEST_TIMEOUT = 10000;
    private static final int TEST_POLLING = 1000;

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock SurveyClient mSurveyClient;
    @Mock SurveyClientFactory mSurveyClientFactory;

    @Rule public PermissionTestRule mActivityTestRule = new PermissionTestRule();
    private TestAndroidPermissionDelegate mTestAndroidPermissionDelegate;

    @Before
    public void setUp() throws Exception {
        SurveyClientFactory.setInstanceForTesting(mSurveyClientFactory);
        doReturn(mSurveyClient).when(mSurveyClientFactory).createClient(any(), any(), any(), any());
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
            @ContentSetting int value) {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    if (type == ContentSettingsType.GEOLOCATION_WITH_OPTIONS) {
                        WebsitePreferenceBridgeJni.get()
                                .setGeolocationSettingForOrigin(
                                        ProfileManager.getLastUsedRegularProfile(),
                                        type,
                                        origin,
                                        origin,
                                        value,
                                        value);
                    } else {
                        WebsitePreferenceBridgeJni.get()
                                .setPermissionSettingForOrigin(
                                        ProfileManager.getLastUsedRegularProfile(),
                                        type,
                                        origin,
                                        origin,
                                        value);
                    }
                });
    }

    private void checkPermission(
            @ContentSettingsType.EnumType int type, String title, ChromeActivity activity)
            throws Exception {
        final Tab tab = ThreadUtils.runOnUiThreadBlocking(() -> activity.getActivityTab());
        final PermissionUpdateWaiter permissionUpdateWaiter =
                new PermissionUpdateWaiter(title, activity);
        ThreadUtils.runOnUiThreadBlocking(() -> tab.addObserver(permissionUpdateWaiter));
        switch (type) {
            case ContentSettingsType.GEOLOCATION, ContentSettingsType.GEOLOCATION_WITH_OPTIONS -> {
                mActivityTestRule.runJavaScriptCodeInCurrentTab("checkGeolocation();");
            }
            default -> {
                throw new AssertionError("Unreached");
            }
        }
        permissionUpdateWaiter.waitForNumUpdates(0);
        ThreadUtils.runOnUiThreadBlocking(() -> tab.removeObserver(permissionUpdateWaiter));
    }

    private void waitForTitleUpdate(String title, ChromeActivity activity) throws Exception {
        final Tab tab = ThreadUtils.runOnUiThreadBlocking(() -> activity.getActivityTab());
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

    private void runTest(
            final TestAndroidPermissionDelegate testAndroidPermissionDelegate,
            final String page,
            final String nodeId,
            @ContentSettingsType.EnumType int type,
            @ContentSetting int value,
            final String expectedPromptText,
            final String expectedPositiveButtonText,
            final String expectedPositiveEphemeralButtonText,
            final String expectedNegativeButtonText)
            throws Exception {
        runTest(
                testAndroidPermissionDelegate,
                page,
                nodeId,
                type,
                value,
                EmbeddedPermissiontResponse.DISMISS_OUTSIDE,
                expectedPromptText,
                expectedPositiveButtonText,
                expectedPositiveEphemeralButtonText,
                expectedNegativeButtonText,
                /*expectedPermission*/ "",
                "promptdismiss");
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
     * @param response What to respond on the permission prompt.
     * @param expectedPromptText The string that matches the text of the embedded permission prompt
     *     dialog url.
     * @param expectedPositiveButtonText The string that matches the button text of the positive
     *     button on the dialog.
     * @param expectedPositiveEphemeralButtonText The string that matches the button text of the
     *     positive ephemeral button on the dialog url.
     * @param expectedNegativeButtonText The string that matches the button text of the negative
     *     button on the dialog url.
     * @param expectedPermission The string that matches the text title on the page when checking
     *     permission.
     * @param expectedTitle The string that matches the text title in the permission element test
     *     page.
     * @param response The string that matches the text title in the permission element test page.
     */
    private void runTest(
            final TestAndroidPermissionDelegate testAndroidPermissionDelegate,
            final String page,
            final String nodeId,
            @ContentSettingsType.EnumType int type,
            @ContentSetting int value,
            final EmbeddedPermissiontResponse response,
            final String expectedPromptText,
            final String expectedPositiveButtonText,
            final String expectedPositiveEphemeralButtonText,
            final String expectedNegativeButtonText,
            final String expectedPermission,
            final String expectedTitle)
            throws Exception {

        final String url = mActivityTestRule.getURL(TEST_PAGE);
        try {
            setNativeContentSetting(type, url, value);
            final ChromeActivity activity = mActivityTestRule.getActivity();
            activity.getWindowAndroid().setAndroidPermissionDelegate(testAndroidPermissionDelegate);

            mActivityTestRule.setUpUrl(TEST_PAGE);
            waitOnLatch(2);

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
            switch (response) {
                case DISMISS_OUTSIDE -> {
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
                case NEGATIVE -> {
                    PermissionTestRule.replyToDialog(
                            PermissionTestRule.PromptDecision.DENY, activity);
                }
                case POSITIVE -> {
                    PermissionTestRule.replyToDialog(
                            PermissionTestRule.PromptDecision.ALLOW, activity);
                }
                case POSITIVE_EPHEMERAL -> {
                    PermissionTestRule.replyToDialog(
                            PermissionTestRule.PromptDecision.ALLOW_ONCE, activity);
                }
                default -> {
                    throw new AssertionError("Unexpected response ");
                }
            }
            waitForTitleUpdate(expectedTitle, activity);
            if (!TextUtils.isEmpty(expectedPermission)) {
                checkPermission(type, expectedPermission, activity);
            }
        } finally {
            setNativeContentSetting(type, url, ContentSetting.DEFAULT);
        }
    }

    private void testAskPromptInteraction(final EmbeddedPermissiontResponse response)
            throws Exception {
        RuntimePermissionTestUtils.setupGeolocationSystemMock();
        String[] requestablePermission =
                new String[] {
                    Manifest.permission.ACCESS_COARSE_LOCATION,
                    Manifest.permission.ACCESS_FINE_LOCATION
                };
        mTestAndroidPermissionDelegate =
                new TestAndroidPermissionDelegate(
                        requestablePermission, RuntimePromptResponse.GRANT);
        final String expectedTitle =
                (response == EmbeddedPermissiontResponse.NEGATIVE)
                        ? "promptdismiss"
                        : "promptaction";
        final String expectedPermission =
                (response == EmbeddedPermissiontResponse.NEGATIVE) ? "prompt" : "granted";
        runTest(
                mTestAndroidPermissionDelegate,
                TEST_PAGE,
                "geolocation",
                stringToContentSettingsType("geolocation"),
                ContentSetting.ASK,
                response,
                LOOPBACK_ADDRESS + " wants to use your device's location",
                "Allow while visiting the site",
                "Allow this time",
                "Don't allow",
                expectedPermission,
                expectedTitle);
    }

    private void testPreviousDeniedInteraction(final EmbeddedPermissiontResponse response)
            throws Exception {
        RuntimePermissionTestUtils.setupGeolocationSystemMock();
        String[] requestablePermission =
                new String[] {
                    Manifest.permission.ACCESS_COARSE_LOCATION,
                    Manifest.permission.ACCESS_FINE_LOCATION
                };
        mTestAndroidPermissionDelegate =
                new TestAndroidPermissionDelegate(
                        requestablePermission, RuntimePromptResponse.GRANT);
        final String expectedTitle =
                (response == EmbeddedPermissiontResponse.NEGATIVE)
                        ? "promptaction"
                        : "promptdismiss";
        final String expectedPermission =
                (response == EmbeddedPermissiontResponse.NEGATIVE) ? "granted" : "denied";
        runTest(
                mTestAndroidPermissionDelegate,
                TEST_PAGE,
                "geolocation",
                stringToContentSettingsType("geolocation"),
                ContentSetting.BLOCK,
                response,
                "You previously didn't allow location for this site",
                "Continue not allowing",
                /* expectedPositiveEphemeralButtonText */ "",
                "Allow this time",
                expectedPermission,
                expectedTitle);
    }

    private void testPreviousGrantedInteraction(final EmbeddedPermissiontResponse response)
            throws Exception {
        RuntimePermissionTestUtils.setupGeolocationSystemMock();
        String[] requestablePermission =
                new String[] {
                    Manifest.permission.ACCESS_COARSE_LOCATION,
                    Manifest.permission.ACCESS_FINE_LOCATION
                };
        mTestAndroidPermissionDelegate =
                new TestAndroidPermissionDelegate(
                        requestablePermission, RuntimePromptResponse.GRANT);
        final String expectedTitle =
                (response == EmbeddedPermissiontResponse.NEGATIVE)
                        ? "promptaction"
                        : "promptdismiss";
        final String expectedPermission =
                (response == EmbeddedPermissiontResponse.NEGATIVE) ? "denied" : "granted";
        runTest(
                mTestAndroidPermissionDelegate,
                TEST_PAGE,
                "geolocation",
                stringToContentSettingsType("geolocation"),
                ContentSetting.ALLOW,
                response,
                "You have allowed location on " + LOOPBACK_ADDRESS,
                "Continue allowing",
                /* expectedPositiveEphemeralButtonText */ "",
                "Stop allowing",
                expectedPermission,
                expectedTitle);
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
                return getGeolocationType();
            default:
                throw new AssertionError("Unreached");
        }
    }

    @Test
    @MediumTest
    @Features.EnableFeatures({PermissionsAndroidFeatureList.PERMISSION_ELEMENT})
    @DisabledTest(message = "crbug.com/394097674")
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
                ContentSetting.ASK,
                LOOPBACK_ADDRESS + " wants to use your device's location",
                "Allow while visiting the site",
                "Allow this time",
                "Don't allow");
    }

    @Test
    @MediumTest
    @Features.EnableFeatures({PermissionsAndroidFeatureList.PERMISSION_ELEMENT})
    @DisabledTest(message = "crbug.com/394097674")
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
                ContentSetting.BLOCK,
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
                ContentSetting.ALLOW,
                "You have allowed location on " + LOOPBACK_ADDRESS,
                "Continue allowing",
                /* expectedPositiveEphemeralButtonText */ "",
                "Stop allowing");
    }

    @Test
    @MediumTest
    @Features.EnableFeatures({PermissionsAndroidFeatureList.PERMISSION_ELEMENT})
    public void testDisableLocationSettingsPromptText() throws Exception {
        String productName = "Chromium";
        if (BuildConfig.IS_CHROME_BRANDED) {
            productName = "Chrome";
        }
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
                ContentSetting.BLOCK,
                "To use your location on this site, give " + productName + " access",
                "Android settings",
                /* expectedPositiveEphemeralButtonText */ "",
                "Cancel");
    }

    @Test
    @MediumTest
    @Features.EnableFeatures({PermissionsAndroidFeatureList.PERMISSION_ELEMENT})
    public void testOsSettingsPromptText() throws Exception {
        String productName = "Chromium";
        if (BuildConfig.IS_CHROME_BRANDED) {
            productName = "Chrome";
        }
        mTestAndroidPermissionDelegate =
                new TestAndroidPermissionDelegate(new String[] {}, RuntimePromptResponse.DENY);
        runTest(
                mTestAndroidPermissionDelegate,
                TEST_PAGE,
                "geolocation",
                stringToContentSettingsType("geolocation"),
                ContentSetting.ALLOW,
                "To use your location on this site, give " + productName + " access",
                "Android settings",
                /* expectedPositiveEphemeralButtonText */ "",
                "Cancel");
    }

    @Test
    @MediumTest
    @Features.EnableFeatures({PermissionsAndroidFeatureList.PERMISSION_ELEMENT})
    public void testAskPromptInteractionAllow() throws Exception {
        testAskPromptInteraction(EmbeddedPermissiontResponse.POSITIVE);
    }

    @Test
    @MediumTest
    @Features.EnableFeatures({PermissionsAndroidFeatureList.PERMISSION_ELEMENT})
    public void testAskPromptInteractionAllowEphemeral() throws Exception {
        testAskPromptInteraction(EmbeddedPermissiontResponse.POSITIVE_EPHEMERAL);
    }

    @Test
    @MediumTest
    @Features.EnableFeatures({PermissionsAndroidFeatureList.PERMISSION_ELEMENT})
    public void testAskPromptInteractionDeny() throws Exception {
        testAskPromptInteraction(EmbeddedPermissiontResponse.NEGATIVE);
    }

    @Test
    @MediumTest
    @Features.EnableFeatures({PermissionsAndroidFeatureList.PERMISSION_ELEMENT})
    public void testPreviousDeniedInteractionContinue() throws Exception {
        testPreviousDeniedInteraction(EmbeddedPermissiontResponse.POSITIVE);
    }

    @Test
    @MediumTest
    @Features.EnableFeatures({PermissionsAndroidFeatureList.PERMISSION_ELEMENT})
    public void testPreviousDeniedInteractionAllow() throws Exception {
        testPreviousDeniedInteraction(EmbeddedPermissiontResponse.NEGATIVE);
    }

    @Test
    @MediumTest
    @Features.EnableFeatures({PermissionsAndroidFeatureList.PERMISSION_ELEMENT})
    @DisabledTest(message = "crbug.com/392083174")
    public void testPreviousGrantedInteractionContinue() throws Exception {
        testPreviousGrantedInteraction(EmbeddedPermissiontResponse.POSITIVE);
    }

    @Test
    @MediumTest
    @Features.EnableFeatures({PermissionsAndroidFeatureList.PERMISSION_ELEMENT})
    @DisabledTest(message = "crbug.com/392083174")
    public void testPreviousGrantedInteractionStop() throws Exception {
        testPreviousGrantedInteraction(EmbeddedPermissiontResponse.NEGATIVE);
    }
}
