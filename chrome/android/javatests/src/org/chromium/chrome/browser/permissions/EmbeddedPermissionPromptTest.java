// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.permissions;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.any;
import static org.mockito.Mockito.doReturn;

import static org.chromium.components.permissions.PermissionUtil.getGeolocationType;

import android.Manifest;
import android.view.View;
import android.view.ViewGroup;

import androidx.test.filters.MediumTest;

import org.hamcrest.Matchers;
import org.junit.After;
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
import org.chromium.base.test.util.DisableIf;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.Features;
import org.chromium.base.test.util.Restriction;
import org.chromium.build.BuildConfig;
import org.chromium.chrome.R;
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
import org.chromium.components.permissions.PermissionDialogDelegate;
import org.chromium.components.permissions.PermissionsAndroidFeatureList;
import org.chromium.content_public.browser.test.util.DOMUtils;
import org.chromium.content_public.browser.test.util.JavaScriptUtils;
import org.chromium.ui.base.DeviceFormFactor;
import org.chromium.ui.modaldialog.DialogDismissalCause;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogManager.ModalDialogType;
import org.chromium.ui.modaldialog.ModalDialogProperties;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.test.util.DeviceRestriction;

import java.util.concurrent.CountDownLatch;
import java.util.concurrent.TimeUnit;

@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@Features.EnableFeatures(PermissionsAndroidFeatureList.BYPASS_PEPC_SECURITY_FOR_TESTING)
@Batch(Batch.PER_CLASS)
@Restriction({DeviceRestriction.RESTRICTION_TYPE_NON_AUTO}) // crbug.com/394097674
@DisableIf.Device(DeviceFormFactor.DESKTOP_FREEFORM) // crbug.com/511288462
public class EmbeddedPermissionPromptTest {

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

        // Default Android permission delegate setup used by most tests
        String[] requestablePermission =
                new String[] {
                    Manifest.permission.ACCESS_COARSE_LOCATION,
                    Manifest.permission.ACCESS_FINE_LOCATION
                };
        mTestAndroidPermissionDelegate =
                new TestAndroidPermissionDelegate(
                        requestablePermission, RuntimePromptResponse.GRANT);
    }

    @After
    public void tearDown() throws Exception {
        setPermission(ContentSetting.DEFAULT);
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

    private void setPermission(@ContentSetting int value) {
        setNativeContentSetting(getGeolocationType(), mActivityTestRule.getURL(TEST_PAGE), value);
    }

    private String getGeolocationPermissionStateFromJS() throws Exception {
        return JavaScriptUtils.runJavascriptWithAsyncResult(
                mActivityTestRule.getWebContents(), "getGeolocationPermissionState();");
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

    private void clickNodeWithId(String id) throws Exception {
        DOMUtils.clickNode(mActivityTestRule.getWebContents(), id);
    }

    private PropertyModel getCurrentDialogModel(ChromeActivity activity) {
        return ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    return activity.getModalDialogManager().getCurrentDialogForTest();
                });
    }

    private PermissionDialogDelegate getPermissionDialogDelegate(PropertyModel dialogModel) {
        return ((EmbeddedPermissionDialogMediator)
                        dialogModel.get(ModalDialogProperties.CONTROLLER))
                .getDelegateForTest();
    }

    private void dismissDialog(ChromeActivity activity) {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    ModalDialogManager manager = activity.getModalDialogManager();
                    int dialogType = manager.getCurrentType();
                    manager.getCurrentPresenterForTest()
                            .dismissCurrentDialog(
                                    dialogType == ModalDialogType.APP
                                            ? DialogDismissalCause.NAVIGATE_BACK_OR_TOUCH_OUTSIDE
                                            : DialogDismissalCause.NAVIGATE_BACK);
                });
    }

    private View getChooserContainer(PropertyModel dialogModel) {
        View customView = dialogModel.get(ModalDialogProperties.CUSTOM_VIEW);
        return customView != null ? customView.findViewById(R.id.custom_view_container) : null;
    }

    private void assertLocationChooserVisible(PropertyModel dialogModel, boolean visible) {
        View chooserContainer = getChooserContainer(dialogModel);
        if (visible) {
            assertNotNull(chooserContainer);
            assertEquals(View.VISIBLE, chooserContainer.getVisibility());
            assertTrue(chooserContainer instanceof ViewGroup);
            assertTrue(((ViewGroup) chooserContainer).getChildCount() > 0);
        } else {
            if (chooserContainer != null) {
                assertEquals(View.GONE, chooserContainer.getVisibility());
            }
        }
    }

    private ChromeActivity prepareActivity() throws Exception {
        ChromeActivity activity = mActivityTestRule.getActivity();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    activity.getSnackbarManager().dismissAllSnackbars();
                });
        activity.getWindowAndroid().setAndroidPermissionDelegate(mTestAndroidPermissionDelegate);

        mActivityTestRule.setUpUrl(TEST_PAGE);
        waitOnLatch(2);

        return activity;
    }

    private void triggerPrompt(String nodeId) throws Exception {
        clickNodeWithId(nodeId);
        CriteriaHelper.pollUiThread(
                () -> {
                    boolean isDialogShownForTest =
                            PermissionDialogController.getInstance().isDialogShownForTest();
                    Criteria.checkThat(isDialogShownForTest, Matchers.is(true));
                },
                TEST_TIMEOUT,
                TEST_POLLING);
    }

    @Test
    @MediumTest
    @DisabledTest(message = "crbug.com/394097674")
    public void testAskPromptTextWithOneTime() throws Exception {
        setPermission(ContentSetting.ASK);
        final ChromeActivity activity = prepareActivity();

        triggerPrompt("geolocation");

        PropertyModel dialogModel = getCurrentDialogModel(activity);
        PermissionDialogDelegate delegate = getPermissionDialogDelegate(dialogModel);

        assertEquals(
                LOOPBACK_ADDRESS + " wants to use your device's location",
                delegate.getMessageText());
        assertEquals("Allow while visiting the site", delegate.getPositiveButtonText());
        assertEquals("Allow this time", delegate.getPositiveEphemeralButtonText());
        assertEquals("Don't allow", delegate.getNegativeButtonText());

        assertLocationChooserVisible(dialogModel, true);

        dismissDialog(activity);
        waitForTitleUpdate("promptdismiss", activity);
    }

    @Test
    @MediumTest
    @DisabledTest(message = "crbug.com/394097674")
    public void testPreviouslyDeniedPromptTextWithOneTime() throws Exception {
        setPermission(ContentSetting.BLOCK);
        final ChromeActivity activity = prepareActivity();

        triggerPrompt("geolocation");

        PropertyModel dialogModel = getCurrentDialogModel(activity);
        PermissionDialogDelegate delegate = getPermissionDialogDelegate(dialogModel);

        assertEquals(
                "You previously didn't allow location for this site", delegate.getMessageText());
        assertEquals("Continue not allowing", delegate.getPositiveButtonText());
        assertEquals("", delegate.getPositiveEphemeralButtonText());
        assertEquals("Allow this time", delegate.getNegativeButtonText());

        assertLocationChooserVisible(dialogModel, true);

        dismissDialog(activity);
        waitForTitleUpdate("promptdismiss", activity);
    }

    @Test
    @MediumTest
    @DisabledTest(message = "crbug.com/392083174")
    public void testPreviouslyGrantedPromptText() throws Exception {
        setPermission(ContentSetting.ALLOW);
        final ChromeActivity activity = prepareActivity();

        triggerPrompt("geolocation");

        PropertyModel dialogModel = getCurrentDialogModel(activity);
        PermissionDialogDelegate delegate = getPermissionDialogDelegate(dialogModel);

        assertEquals("You have allowed location on " + LOOPBACK_ADDRESS, delegate.getMessageText());
        assertEquals("Continue allowing", delegate.getPositiveButtonText());
        assertEquals("", delegate.getPositiveEphemeralButtonText());
        assertEquals("Stop allowing", delegate.getNegativeButtonText());

        assertLocationChooserVisible(dialogModel, true);

        dismissDialog(activity);
        waitForTitleUpdate("promptdismiss", activity);
    }

    @Test
    @MediumTest
    public void testDisableLocationSettingsPromptText() throws Exception {
        String productName = "Chromium";
        if (BuildConfig.IS_CHROME_BRANDED) {
            productName = "Chrome";
        }
        RuntimePermissionTestUtils.setupGeolocationSystemMock(false);

        setPermission(ContentSetting.BLOCK);
        final ChromeActivity activity = prepareActivity();

        triggerPrompt("geolocation");

        PropertyModel dialogModel = getCurrentDialogModel(activity);
        PermissionDialogDelegate delegate = getPermissionDialogDelegate(dialogModel);

        assertEquals(
                "To use your location on this site, give " + productName + " access",
                delegate.getMessageText());
        assertEquals("Android settings", delegate.getPositiveButtonText());
        assertEquals("", delegate.getPositiveEphemeralButtonText());
        assertEquals("Cancel", delegate.getNegativeButtonText());

        assertLocationChooserVisible(dialogModel, false);

        dismissDialog(activity);
        waitForTitleUpdate("promptdismiss", activity);
    }

    @Test
    @MediumTest
    @DisabledTest(message = "crbug.com/456384544")
    public void testOsSettingsPromptText() throws Exception {
        String productName = "Chromium";
        if (BuildConfig.IS_CHROME_BRANDED) {
            productName = "Chrome";
        }
        // Override the default delegate setup for this specific test
        mTestAndroidPermissionDelegate =
                new TestAndroidPermissionDelegate(new String[] {}, RuntimePromptResponse.DENY);

        setPermission(ContentSetting.ALLOW);
        final ChromeActivity activity = prepareActivity();

        triggerPrompt("geolocation");

        PropertyModel dialogModel = getCurrentDialogModel(activity);
        PermissionDialogDelegate delegate = getPermissionDialogDelegate(dialogModel);

        assertEquals(
                "To use your location on this site, give " + productName + " access",
                delegate.getMessageText());
        assertEquals("Android settings", delegate.getPositiveButtonText());
        assertEquals("", delegate.getPositiveEphemeralButtonText());
        assertEquals("Cancel", delegate.getNegativeButtonText());

        assertLocationChooserVisible(dialogModel, false);

        dismissDialog(activity);
        waitForTitleUpdate("promptdismiss", activity);
    }

    @Test
    @MediumTest
    public void testAskPromptInteractionAllow() throws Exception {
        RuntimePermissionTestUtils.setupGeolocationSystemMock();

        setPermission(ContentSetting.ASK);
        final ChromeActivity activity = prepareActivity();

        triggerPrompt("geolocation");

        PropertyModel dialogModel = getCurrentDialogModel(activity);
        PermissionDialogDelegate delegate = getPermissionDialogDelegate(dialogModel);

        assertEquals(
                LOOPBACK_ADDRESS + " wants to use your device's location",
                delegate.getMessageText());
        assertEquals("Allow while visiting the site", delegate.getPositiveButtonText());
        assertEquals("Allow this time", delegate.getPositiveEphemeralButtonText());
        assertEquals("Don't allow", delegate.getNegativeButtonText());

        assertLocationChooserVisible(dialogModel, true);

        PermissionTestRule.replyToDialog(PermissionTestRule.PromptDecision.ALLOW, activity);

        waitForTitleUpdate("promptaction", activity);
        assertEquals("\"granted\"", getGeolocationPermissionStateFromJS());
    }

    @Test
    @MediumTest
    public void testAskPromptInteractionAllowEphemeral() throws Exception {
        RuntimePermissionTestUtils.setupGeolocationSystemMock();

        setPermission(ContentSetting.ASK);
        final ChromeActivity activity = prepareActivity();

        triggerPrompt("geolocation");

        PropertyModel dialogModel = getCurrentDialogModel(activity);
        PermissionDialogDelegate delegate = getPermissionDialogDelegate(dialogModel);

        assertEquals(
                LOOPBACK_ADDRESS + " wants to use your device's location",
                delegate.getMessageText());
        assertEquals("Allow while visiting the site", delegate.getPositiveButtonText());
        assertEquals("Allow this time", delegate.getPositiveEphemeralButtonText());
        assertEquals("Don't allow", delegate.getNegativeButtonText());

        assertLocationChooserVisible(dialogModel, true);

        PermissionTestRule.replyToDialog(PermissionTestRule.PromptDecision.ALLOW_ONCE, activity);

        waitForTitleUpdate("promptaction", activity);
        assertEquals("\"granted\"", getGeolocationPermissionStateFromJS());
    }

    @Test
    @MediumTest
    public void testAskPromptInteractionDeny() throws Exception {
        RuntimePermissionTestUtils.setupGeolocationSystemMock();

        setPermission(ContentSetting.ASK);
        final ChromeActivity activity = prepareActivity();

        triggerPrompt("geolocation");

        PropertyModel dialogModel = getCurrentDialogModel(activity);
        PermissionDialogDelegate delegate = getPermissionDialogDelegate(dialogModel);

        assertEquals(
                LOOPBACK_ADDRESS + " wants to use your device's location",
                delegate.getMessageText());
        assertEquals("Allow while visiting the site", delegate.getPositiveButtonText());
        assertEquals("Allow this time", delegate.getPositiveEphemeralButtonText());
        assertEquals("Don't allow", delegate.getNegativeButtonText());

        assertLocationChooserVisible(dialogModel, true);

        PermissionTestRule.replyToDialog(PermissionTestRule.PromptDecision.DENY, activity);

        waitForTitleUpdate("promptdismiss", activity);
        assertEquals("\"prompt\"", getGeolocationPermissionStateFromJS());
    }

    @Test
    @MediumTest
    public void testPreviousDeniedInteractionContinue() throws Exception {
        RuntimePermissionTestUtils.setupGeolocationSystemMock();

        setPermission(ContentSetting.BLOCK);
        final ChromeActivity activity = prepareActivity();

        triggerPrompt("geolocation");

        PropertyModel dialogModel = getCurrentDialogModel(activity);
        PermissionDialogDelegate delegate = getPermissionDialogDelegate(dialogModel);

        assertEquals(
                "You previously didn't allow location for this site", delegate.getMessageText());
        assertEquals("Continue not allowing", delegate.getPositiveButtonText());
        assertEquals("", delegate.getPositiveEphemeralButtonText());
        assertEquals("Allow this time", delegate.getNegativeButtonText());

        assertLocationChooserVisible(dialogModel, true);

        PermissionTestRule.replyToDialog(PermissionTestRule.PromptDecision.ALLOW, activity);

        waitForTitleUpdate("promptdismiss", activity);
        assertEquals("\"denied\"", getGeolocationPermissionStateFromJS());
    }

    @Test
    @MediumTest
    public void testPreviousDeniedInteractionAllow() throws Exception {
        RuntimePermissionTestUtils.setupGeolocationSystemMock();

        setPermission(ContentSetting.BLOCK);
        final ChromeActivity activity = prepareActivity();

        triggerPrompt("geolocation");

        PropertyModel dialogModel = getCurrentDialogModel(activity);
        PermissionDialogDelegate delegate = getPermissionDialogDelegate(dialogModel);

        assertEquals(
                "You previously didn't allow location for this site", delegate.getMessageText());
        assertEquals("Continue not allowing", delegate.getPositiveButtonText());
        assertEquals("", delegate.getPositiveEphemeralButtonText());
        assertEquals("Allow this time", delegate.getNegativeButtonText());

        assertLocationChooserVisible(dialogModel, true);

        PermissionTestRule.replyToDialog(PermissionTestRule.PromptDecision.DENY, activity);

        waitForTitleUpdate("promptaction", activity);
        assertEquals("\"granted\"", getGeolocationPermissionStateFromJS());
    }

    @Test
    @MediumTest
    @DisabledTest(message = "crbug.com/392083174")
    public void testPreviousGrantedInteractionContinue() throws Exception {
        RuntimePermissionTestUtils.setupGeolocationSystemMock();

        setPermission(ContentSetting.ALLOW);
        final ChromeActivity activity = prepareActivity();

        triggerPrompt("geolocation");

        PropertyModel dialogModel = getCurrentDialogModel(activity);
        PermissionDialogDelegate delegate = getPermissionDialogDelegate(dialogModel);

        assertEquals("You have allowed location on " + LOOPBACK_ADDRESS, delegate.getMessageText());
        assertEquals("Continue allowing", delegate.getPositiveButtonText());
        assertEquals("", delegate.getPositiveEphemeralButtonText());
        assertEquals("Stop allowing", delegate.getNegativeButtonText());

        assertLocationChooserVisible(dialogModel, true);

        PermissionTestRule.replyToDialog(PermissionTestRule.PromptDecision.ALLOW, activity);

        waitForTitleUpdate("promptdismiss", activity);
        assertEquals("\"granted\"", getGeolocationPermissionStateFromJS());
    }

    @Test
    @MediumTest
    @DisabledTest(message = "crbug.com/392083174")
    public void testPreviousGrantedInteractionStop() throws Exception {
        RuntimePermissionTestUtils.setupGeolocationSystemMock();

        setPermission(ContentSetting.ALLOW);
        final ChromeActivity activity = prepareActivity();

        triggerPrompt("geolocation");

        PropertyModel dialogModel = getCurrentDialogModel(activity);
        PermissionDialogDelegate delegate = getPermissionDialogDelegate(dialogModel);

        assertEquals("You have allowed location on " + LOOPBACK_ADDRESS, delegate.getMessageText());
        assertEquals("Continue allowing", delegate.getPositiveButtonText());
        assertEquals("", delegate.getPositiveEphemeralButtonText());
        assertEquals("Stop allowing", delegate.getNegativeButtonText());

        assertLocationChooserVisible(dialogModel, true);

        PermissionTestRule.replyToDialog(PermissionTestRule.PromptDecision.DENY, activity);

        waitForTitleUpdate("promptaction", activity);
        assertEquals("\"denied\"", getGeolocationPermissionStateFromJS());
    }
}
