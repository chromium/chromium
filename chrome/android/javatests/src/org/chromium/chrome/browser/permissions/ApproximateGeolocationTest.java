// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.permissions;

import android.Manifest;
import android.view.View;

import androidx.test.filters.MediumTest;

import org.hamcrest.Matchers;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.params.ParameterAnnotations;
import org.chromium.base.test.params.ParameterProvider;
import org.chromium.base.test.params.ParameterSet;
import org.chromium.base.test.params.ParameterizedRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.Features;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.permissions.RuntimePermissionTestUtils.RuntimePromptResponse;
import org.chromium.chrome.browser.permissions.RuntimePermissionTestUtils.TestAndroidPermissionDelegate;
import org.chromium.chrome.browser.profiles.ProfileManager;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.test.ChromeJUnit4RunnerDelegate;
import org.chromium.chrome.test.transit.ChromeTransitTestRules;
import org.chromium.chrome.test.transit.FreshCtaTransitTestRule;
import org.chromium.components.browser_ui.site_settings.GeolocationSetting;
import org.chromium.components.browser_ui.site_settings.WebsitePreferenceBridge;
import org.chromium.components.browser_ui.site_settings.WebsitePreferenceBridgeJni;
import org.chromium.components.browser_ui.widget.RichRadioButtonList;
import org.chromium.components.content_settings.ContentSetting;
import org.chromium.components.content_settings.ContentSettingsType;
import org.chromium.components.permissions.PermissionDialogController;
import org.chromium.components.permissions.PermissionsAndroidFeatureList;
import org.chromium.content_public.browser.test.util.DOMUtils;
import org.chromium.content_public.browser.test.util.JavaScriptUtils;
import org.chromium.net.test.EmbeddedTestServer;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogProperties;

import java.util.Arrays;
import java.util.List;
import java.util.concurrent.CountDownLatch;
import java.util.concurrent.TimeUnit;

/** Test suite for Approximate Geolocation functionality. */
@RunWith(ParameterizedRunner.class)
@ParameterAnnotations.UseRunnerDelegate(ChromeJUnit4RunnerDelegate.class)
@CommandLineFlags.Add({
    ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE,
    "enable-blink-features=ApproximateGeolocationWebVisibleAPI"
})
@Features.EnableFeatures(PermissionsAndroidFeatureList.APPROXIMATE_GEOLOCATION_PERMISSION)
@Batch(Batch.PER_CLASS)
public class ApproximateGeolocationTest {
    @Rule
    public FreshCtaTransitTestRule mActivityTestRule =
            ChromeTransitTestRules.freshChromeTabbedActivityRule();

    private static final String TEST_FILE =
            "/content/test/data/android/approximate_geolocation.html";
    private static final int TEST_TIMEOUT = 10000;

    private TestAndroidPermissionDelegate mTestAndroidPermissionDelegate;
    private EmbeddedTestServer mTestServer;

    public static class LocationPrecisionParams implements ParameterProvider {
        @Override
        public List<ParameterSet> getParameters() {
            return Arrays.asList(
                    new ParameterSet().value(true).name("precise"),
                    new ParameterSet().value(false).name("approximate"));
        }
    }

    @Before
    public void setUp() throws Exception {
        mTestServer = mActivityTestRule.getTestServer();
        RuntimePermissionTestUtils.setupGeolocationSystemMock();
        mActivityTestRule.startOnBlankPage();
        setNativeContentSetting();
        setPermissionDelegate();
        mActivityTestRule.loadUrl(mTestServer.getURL(TEST_FILE));
    }

    /** Sets native ContentSetting value to ASK for geolocation. */
    private void setNativeContentSetting() {
        setGeolocationContentSetting(ContentSetting.ASK, ContentSetting.ASK);
    }

    private void setGeolocationContentSetting(int approximate, int precise) {
        final String origin = mTestServer.getURL(TEST_FILE);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    WebsitePreferenceBridgeJni.get()
                            .setGeolocationSettingForOrigin(
                                    ProfileManager.getLastUsedRegularProfile(),
                                    ContentSettingsType.GEOLOCATION_WITH_OPTIONS,
                                    origin,
                                    origin,
                                    approximate,
                                    precise);
                });
    }

    private void selectLocationPrecision(boolean precise) {
        CriteriaHelper.pollUiThread(
                () -> {
                    boolean isDialogShownForTest =
                            PermissionDialogController.getInstance().isDialogShownForTest();
                    Criteria.checkThat(isDialogShownForTest, Matchers.is(true));
                },
                TEST_TIMEOUT,
                CriteriaHelper.DEFAULT_POLLING_INTERVAL);
        ModalDialogManager manager =
                ThreadUtils.runOnUiThreadBlocking(
                        mActivityTestRule.getActivity()::getModalDialogManager);
        View dialogView = manager.getCurrentDialogForTest().get(ModalDialogProperties.CUSTOM_VIEW);
        if (dialogView == null) return;
        View recycler = dialogView.findViewById(R.id.rich_radio_button_list_recycler_view);
        if (recycler == null) return;
        RichRadioButtonList radioList = (RichRadioButtonList) recycler.getParent();

        final String approximateId = "approximate_location_option";
        final String preciseId = "precise_location_option";
        final String targetId = precise ? preciseId : approximateId;

        ThreadUtils.runOnUiThreadBlocking(() -> radioList.setSelectedItem(targetId));
    }

    private void checkPermission(boolean precise) throws Exception {
        final Tab tab = ThreadUtils.runOnUiThreadBlocking(() -> mActivityTestRule.getActivityTab());
        final String origin = mTestServer.getURL(TEST_FILE);
        GeolocationSetting actualSetting =
                ThreadUtils.runOnUiThreadBlocking(
                        () ->
                                WebsitePreferenceBridge.getGeolocationSettingForOrigin(
                                        ProfileManager.getLastUsedRegularProfile(),
                                        origin,
                                        origin));
        GeolocationSetting expectedSetting =
                precise
                        ? new GeolocationSetting(
                                /* approximate= */ ContentSetting.ALLOW,
                                /* precise= */ ContentSetting.ALLOW)
                        : new GeolocationSetting(
                                /* approximate= */ ContentSetting.ALLOW,
                                /* precise= */ ContentSetting.BLOCK);
        Assert.assertEquals("Geolocation setting does not match.", expectedSetting, actualSetting);
    }

    private void waitOnLatch(int seconds) throws Exception {
        CountDownLatch latch = new CountDownLatch(1);
        latch.await(seconds, TimeUnit.SECONDS);
    }

    private void clickGetLocationButton() throws Exception {
        DOMUtils.clickNode(mActivityTestRule.getWebContents(), "get-location-button");
    }

    private void clickGeolocationElement(boolean requestPrecise) throws Exception {
        String nodeId =
                requestPrecise ? "geolocation-element-precise" : "geolocation-element-approximate";
        DOMUtils.clickNode(mActivityTestRule.getWebContents(), nodeId);
    }

    private void setPermissionDelegate() {
        String[] requestablePermission =
                new String[] {
                    Manifest.permission.ACCESS_COARSE_LOCATION,
                    Manifest.permission.ACCESS_FINE_LOCATION
                };
        mTestAndroidPermissionDelegate =
                new TestAndroidPermissionDelegate(
                        requestablePermission, RuntimePromptResponse.GRANT);
        mActivityTestRule
                .getActivity()
                .getWindowAndroid()
                .setAndroidPermissionDelegate(mTestAndroidPermissionDelegate);
    }

    private void checkAccuracyMode(boolean precise) throws Exception {
        String result =
                JavaScriptUtils.runJavascriptWithAsyncResult(
                        mActivityTestRule.getWebContents(),
                        "getAccuracyModeResult().then(result =>"
                                + " domAutomationController.send(result)).catch(error =>"
                                + " domAutomationController.send('error: ' + error.message))");
        if (result.contains("error:")) {
            Assert.fail("Geolocation request failed: " + result);
        }
        String expected = precise ? "precise" : "approximate";
        Assert.assertEquals(expected, result.replace("\"", "").trim());
    }

    @Test
    @MediumTest
    @ParameterAnnotations.UseMethodParameter(LocationPrecisionParams.class)
    public void testAccuracyModeAPI(boolean precise) throws Exception {
        clickGetLocationButton();
        waitOnLatch(2);
        selectLocationPrecision(precise);
        PermissionTestRule.replyToDialog(
                PermissionTestRule.PromptDecision.ALLOW, mActivityTestRule.getActivity());
        checkPermission(precise);
        checkAccuracyMode(precise);
    }

    @Test
    @MediumTest
    @ParameterAnnotations.UseMethodParameter(LocationPrecisionParams.class)
    public void testAccuracyModeGeolocationElement(boolean precise) throws Exception {
        // It takes some time for the geolocation element to be initialized and ready to respond to
        // clicks.
        waitOnLatch(2);
        JavaScriptUtils.executeJavaScriptAndWaitForResult(
                mActivityTestRule.getWebContents(), "initResultPromise()");
        clickGeolocationElement(/* requestPrecise= */ true);
        waitOnLatch(2);
        selectLocationPrecision(precise);
        PermissionTestRule.replyToDialog(
                PermissionTestRule.PromptDecision.ALLOW, mActivityTestRule.getActivity());
        checkPermission(precise);
        checkAccuracyMode(precise);
    }

    @Test
    @MediumTest
    public void testUpgradeFlow_Strings() throws Exception {
        setGeolocationContentSetting(ContentSetting.ALLOW, ContentSetting.ASK);
        clickGetLocationButton();
        waitOnLatch(2);

        // Verify strings
        CriteriaHelper.pollInstrumentationThread(
                () -> {
                    try {
                        androidx.test.espresso.Espresso.onView(
                                        androidx.test.espresso.matcher.ViewMatchers.withText(
                                                "Change to precise"))
                                .check(
                                        androidx.test.espresso.assertion.ViewAssertions.matches(
                                                androidx.test.espresso.matcher.ViewMatchers
                                                        .isDisplayed()));
                        return true;
                    } catch (androidx.test.espresso.NoMatchingViewException e) {
                        return false;
                    }
                },
                "Dialog with 'Change to precise' not found");

        // Clean up dialog for subsequent tests
        PermissionTestRule.replyToDialog(
                PermissionTestRule.PromptDecision.DENY, mActivityTestRule.getActivity());
    }

    @Test
    @MediumTest
    public void testUpgradeFlow_ChangeToPrecise() throws Exception {
        setGeolocationContentSetting(ContentSetting.ALLOW, ContentSetting.ASK);
        clickGetLocationButton();
        waitOnLatch(2);

        PermissionTestRule.replyToDialog(
                PermissionTestRule.PromptDecision.ALLOW, mActivityTestRule.getActivity());

        checkPermission(/* precise= */ true);
        checkAccuracyMode(/* precise= */ true);
    }

    @Test
    @MediumTest
    public void testUpgradeFlow_AllowThisTime() throws Exception {
        setGeolocationContentSetting(ContentSetting.ALLOW, ContentSetting.ASK);
        clickGetLocationButton();
        waitOnLatch(2);

        PermissionTestRule.replyToDialog(
                PermissionTestRule.PromptDecision.ALLOW_ONCE, mActivityTestRule.getActivity());

        // After "Allow this time", both should be ALLOW in the current session.
        checkPermission(/* precise= */ true);
        checkAccuracyMode(/* precise= */ true);
    }

    @Test
    @MediumTest
    public void testUpgradeFlow_KeepApproximate() throws Exception {
        setGeolocationContentSetting(ContentSetting.ALLOW, ContentSetting.ASK);
        clickGetLocationButton();
        waitOnLatch(2);

        PermissionTestRule.replyToDialog(
                PermissionTestRule.PromptDecision.DENY, mActivityTestRule.getActivity());

        // Precise should be BLOCK, Approximate remains ALLOW.
        checkPermission(/* precise= */ false);

        // The initial request (for precise) should have succeeded with approximate accuracy.
        checkAccuracyMode(/* precise= */ false);
    }
}
