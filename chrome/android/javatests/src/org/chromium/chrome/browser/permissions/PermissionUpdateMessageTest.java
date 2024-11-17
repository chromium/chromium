// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.permissions;

import android.Manifest;

import androidx.test.core.app.ApplicationProvider;
import androidx.test.filters.MediumTest;
import androidx.test.platform.app.InstrumentationRegistry;

import org.hamcrest.Matchers;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.profiles.ProfileManager;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.util.ChromeTabUtils;
import org.chromium.chrome.test.util.browser.LocationSettingsTestUtil;
import org.chromium.components.browser_ui.site_settings.PermissionInfo;
import org.chromium.components.browser_ui.site_settings.WebsitePreferenceBridgeJni;
import org.chromium.components.content_settings.ContentSettingValues;
import org.chromium.components.content_settings.ContentSettingsType;
import org.chromium.components.content_settings.SessionModel;
import org.chromium.components.messages.MessageDispatcher;
import org.chromium.components.messages.MessageDispatcherProvider;
import org.chromium.components.messages.MessageIdentifier;
import org.chromium.components.messages.MessageStateHandler;
import org.chromium.components.messages.MessagesTestHelper;
import org.chromium.content_public.browser.WebContents;
import org.chromium.net.test.EmbeddedTestServer;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.permissions.AndroidPermissionDelegate;
import org.chromium.ui.permissions.PermissionCallback;
import org.chromium.ui.test.util.DeviceRestriction;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.HashSet;
import java.util.List;
import java.util.Set;
import java.util.concurrent.Callable;
import java.util.concurrent.ExecutionException;
import java.util.concurrent.TimeoutException;

/** Tests for the permission update message. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class PermissionUpdateMessageTest {
    private static final String GEOLOCATION_PAGE =
            "/chrome/test/data/geolocation/geolocation_on_load.html";
    private static final String MEDIASTREAM_PAGE = "/content/test/data/media/getusermedia.html";
    private EmbeddedTestServer mTestServer;

    @Rule public PermissionTestRule mActivityTestRule = new PermissionTestRule();

    /**
     * Utility delegate to provide the permissions to be requested for triggering a permission
     * update message.
     */
    private static class TestAndroidPermissionDelegate implements AndroidPermissionDelegate {
        private final Set<String> mHasPermissions;
        private final Set<String> mRequestablePermissions;
        private final Set<String> mPolicyRevokedPermissions;

        public TestAndroidPermissionDelegate(
                List<String> hasPermissions,
                List<String> requestablePermissions,
                List<String> policyRevokedPermissions) {
            mHasPermissions =
                    new HashSet<>(
                            hasPermissions == null ? new ArrayList<String>() : hasPermissions);
            mRequestablePermissions =
                    new HashSet<>(
                            requestablePermissions == null
                                    ? new ArrayList<String>()
                                    : requestablePermissions);
            mPolicyRevokedPermissions =
                    new HashSet<>(
                            policyRevokedPermissions == null
                                    ? new ArrayList<String>()
                                    : policyRevokedPermissions);
        }

        @Override
        public boolean hasPermission(String permission) {
            return mHasPermissions.contains(permission);
        }

        @Override
        public boolean canRequestPermission(String permission) {
            return mRequestablePermissions.contains(permission);
        }

        @Override
        public boolean isPermissionRevokedByPolicy(String permission) {
            return mPolicyRevokedPermissions.contains(permission);
        }

        @Override
        public void requestPermissions(String[] permissions, PermissionCallback callback) {}

        @Override
        public boolean handlePermissionResult(
                int requestCode, String[] permissions, int[] grantResults) {
            return false;
        }
    }

    @Before
    public void setUp() throws Exception {
        mActivityTestRule.startMainActivityOnBlankPage();
        mTestServer =
                EmbeddedTestServer.createAndStartServer(
                        ApplicationProvider.getApplicationContext());
    }

    /**
     * Determines if there is exact number of message presented in the given View hierarchy.
     *
     * @param windowAndroid The WindowAndroid to get the messages from.
     * @param count Number of messages should be presented.
     */
    private void expectMessagesCount(WindowAndroid windowAndroid, final int count) {
        CriteriaHelper.pollUiThread(
                () -> {
                    Criteria.checkThat(
                            "Message is not enqueued.",
                            MessagesTestHelper.getMessageCount(windowAndroid),
                            Matchers.is(count));
                });
    }

    /**
     * Returns the {@link PropertyModel} of an enqueued permission update message.
     *
     * @param windowAndroid The WindowAndroid to get the messages from.
     * @return The {@link PropertyModel} of an enqueued permission update message, null if the
     *     message is not present.
     */
    public static PropertyModel getPermissionUpdateMessage(WindowAndroid windowAndroid)
            throws ExecutionException {
        MessageDispatcher messageDispatcher =
                ThreadUtils.runOnUiThreadBlocking(
                        () -> MessageDispatcherProvider.from(windowAndroid));
        List<MessageStateHandler> messages =
                MessagesTestHelper.getEnqueuedMessages(
                        messageDispatcher, MessageIdentifier.PERMISSION_UPDATE);
        return messages == null || messages.isEmpty()
                ? null
                : MessagesTestHelper.getCurrentMessage(messages.get(0));
    }

    /**
     * Sets native ContentSetting value for the given type and origin.
     *
     * @param type defines ContentSetting type to call native permission setting.
     * @param origin defines origin to call native permission setting.
     * @param value expected value for the above ContentSetting type.
     */
    public void setNativeContentSetting(
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

    /**
     * Run a test related to the permission update message, based on the specified parameters.
     *
     * @param testPage The String of the test page to load in order to run the text.
     * @param androidPermission specify Android permission type will be required for the test.
     * @param javascriptToExecute Some javascript to execute after the page loads (empty or null to
     *     skip).
     * @param contentSettingsType specify content setting type will be notified of missing
     *     permission.
     * @param switchContent Whether to swap a web_content by switching to another tab back and
     *     forth.
     * @throws IllegalArgumentException,TimeoutException,ExecutionException
     */
    private void runTest(
            final String testPage,
            final String androidPermission,
            final String javascriptToExecute,
            final int contentSettingsType,
            final boolean switchContent)
            throws IllegalArgumentException, TimeoutException, ExecutionException {
        ChromeTabUtils.newTabFromMenu(
                InstrumentationRegistry.getInstrumentation(), mActivityTestRule.getActivity());

        WindowAndroid windowAndroid = mActivityTestRule.getActivity().getWindowAndroid();
        windowAndroid.setAndroidPermissionDelegate(
                new TestAndroidPermissionDelegate(null, Arrays.asList(androidPermission), null));
        final String url = mTestServer.getURL(testPage);
        try {
            setNativeContentSetting(contentSettingsType, url, ContentSettingValues.ALLOW);
            mActivityTestRule.loadUrl(mTestServer.getURL(testPage));

            if (javascriptToExecute != null && !javascriptToExecute.isEmpty()) {
                mActivityTestRule.runJavaScriptCodeInCurrentTabWithGesture(javascriptToExecute);
            }

            expectMessagesCount(windowAndroid, 1);
            final WebContents webContents =
                    ThreadUtils.runOnUiThreadBlocking(
                            () ->
                                    mActivityTestRule
                                            .getActivity()
                                            .getActivityTab()
                                            .getWebContents());
            Assert.assertFalse(webContents.isDestroyed());

            // TODO(tungnh): At the moment, strings are defined in native i18_n, not from android
            // context. We should find a way to add more UI verifications, such as description and
            // title here.
            PropertyModel message = getPermissionUpdateMessage(windowAndroid);
            Assert.assertNotNull("Permission update message should be presented.", message);

            if (switchContent) {
                // Switch to a new tab and switch back
                ChromeTabUtils.fullyLoadUrlInNewTab(
                        InstrumentationRegistry.getInstrumentation(),
                        mActivityTestRule.getActivity(),
                        "about:blank",
                        /* incognito= */ false);
                ChromeTabUtils.switchTabInCurrentTabModel(mActivityTestRule.getActivity(), 1);
                expectMessagesCount(windowAndroid, 1);
            }

            // Ensure destroying the permission update message UI does not crash
            // when handling permissions.
            ChromeTabUtils.closeCurrentTab(
                    InstrumentationRegistry.getInstrumentation(), mActivityTestRule.getActivity());
            CriteriaHelper.pollUiThread(() -> webContents.isDestroyed());

            final int countTabs = switchContent ? 2 : 1;
            CriteriaHelper.pollUiThread(
                    () -> {
                        Criteria.checkThat(
                                mActivityTestRule
                                        .getActivity()
                                        .getTabModelSelector()
                                        .getModel(false)
                                        .getCount(),
                                Matchers.is(countTabs));
                    });
        } finally {
            setNativeContentSetting(contentSettingsType, url, ContentSettingValues.DEFAULT);
        }
    }

    // Ensure the correct permission update message UI, and destroying the UI
    // does not crash when handling geolocation permissions.
    @Test
    @MediumTest
    public void testMessageForGeolocation()
            throws IllegalArgumentException, TimeoutException, ExecutionException {
        LocationSettingsTestUtil.setSystemLocationSettingEnabled(true);
        runTest(
                GEOLOCATION_PAGE,
                Manifest.permission.ACCESS_FINE_LOCATION,
                /* javascriptToExecute= */ null,
                ContentSettingsType.GEOLOCATION,
                /* switchContent= */ false);
    }

    // Ensure the correct permission update message UI, and destroying the UI does not crash when
    // handling camera permissions.
    @Test
    @MediumTest
    @Restriction({DeviceRestriction.RESTRICTION_TYPE_NON_AUTO}) // No camera device on auto.
    public void testMessageForMediaStreamCamera()
            throws IllegalArgumentException, TimeoutException, ExecutionException {
        runTest(
                MEDIASTREAM_PAGE,
                Manifest.permission.CAMERA,
                "getUserMediaAndStopLegacy({video: true, audio: false});",
                ContentSettingsType.MEDIASTREAM_CAMERA,
                /* switchContent= */ false);
    }

    // Ensure the correct permission update message UI, and destroying the UI does not crash when
    // handling microphone permissions.
    @Test
    @MediumTest
    public void testMessageForMediaStreamMicrophone()
            throws IllegalArgumentException, TimeoutException, ExecutionException {
        runTest(
                MEDIASTREAM_PAGE,
                Manifest.permission.RECORD_AUDIO,
                "getUserMediaAndStopLegacy({video: false, audio: true});",
                ContentSettingsType.MEDIASTREAM_MIC,
                /* switchContent= */ false);
    }

    // Make sure switching android web content will not trigger multiple prompts.
    @Test
    @MediumTest
    public void testswitchContentShouldNotReprompt()
            throws IllegalArgumentException, TimeoutException, ExecutionException {
        LocationSettingsTestUtil.setSystemLocationSettingEnabled(true);
        runTest(
                GEOLOCATION_PAGE,
                Manifest.permission.ACCESS_FINE_LOCATION,
                /* javascriptToExecute= */ null,
                ContentSettingsType.GEOLOCATION,
                /* switchContent= */ true);
    }

    // Ensure destroying the permission update message does not crash when handling geolocation
    // permissions.
    @Test
    @MediumTest
    public void testInfobarShutsDownCleanlyForGeolocation()
            throws IllegalArgumentException, TimeoutException {
        ChromeTabUtils.newTabFromMenu(
                InstrumentationRegistry.getInstrumentation(), mActivityTestRule.getActivity());

        // Register for animation notifications
        CriteriaHelper.pollInstrumentationThread(
                () -> mActivityTestRule.getInfoBarContainer() != null);

        final var windowAndroid = mActivityTestRule.getActivity().getWindowAndroid();
        final String locationUrl = mTestServer.getURL(GEOLOCATION_PAGE);
        final PermissionInfo geolocationSettings =
                ThreadUtils.runOnUiThreadBlocking(
                        new Callable<PermissionInfo>() {
                            @Override
                            public PermissionInfo call() {
                                return new PermissionInfo(
                                        ContentSettingsType.GEOLOCATION,
                                        locationUrl,
                                        null,
                                        /* isEmbargoed= */ false,
                                        SessionModel.DURABLE);
                            }
                        });

        mActivityTestRule
                .getActivity()
                .getWindowAndroid()
                .setAndroidPermissionDelegate(
                        new TestAndroidPermissionDelegate(
                                null,
                                Arrays.asList(Manifest.permission.ACCESS_FINE_LOCATION),
                                null));
        LocationSettingsTestUtil.setSystemLocationSettingEnabled(true);

        try {
            ThreadUtils.runOnUiThreadBlocking(
                    () ->
                            geolocationSettings.setContentSetting(
                                    ProfileManager.getLastUsedRegularProfile(),
                                    ContentSettingValues.ALLOW));

            mActivityTestRule.loadUrl(mTestServer.getURL(GEOLOCATION_PAGE));
            CriteriaHelper.pollUiThread(
                    () -> {
                        return MessagesTestHelper.getMessageIdentifier(windowAndroid, 0)
                                == MessageIdentifier.PERMISSION_UPDATE;
                    });
            ThreadUtils.runOnUiThreadBlocking(
                    () -> {
                        Assert.assertEquals(1, MessagesTestHelper.getMessageCount(windowAndroid));
                    });

            final WebContents webContents =
                    ThreadUtils.runOnUiThreadBlocking(
                            new Callable<WebContents>() {
                                @Override
                                public WebContents call() {
                                    return mActivityTestRule
                                            .getActivity()
                                            .getActivityTab()
                                            .getWebContents();
                                }
                            });
            Assert.assertFalse(webContents.isDestroyed());

            ChromeTabUtils.closeCurrentTab(
                    InstrumentationRegistry.getInstrumentation(), mActivityTestRule.getActivity());
            CriteriaHelper.pollUiThread(() -> webContents.isDestroyed());

            CriteriaHelper.pollUiThread(
                    () -> {
                        Criteria.checkThat(
                                mActivityTestRule
                                        .getActivity()
                                        .getTabModelSelector()
                                        .getModel(false)
                                        .getCount(),
                                Matchers.is(1));
                    });
        } finally {
            ThreadUtils.runOnUiThreadBlocking(
                    () ->
                            geolocationSettings.setContentSetting(
                                    ProfileManager.getLastUsedRegularProfile(),
                                    ContentSettingValues.DEFAULT));
        }
    }
}
