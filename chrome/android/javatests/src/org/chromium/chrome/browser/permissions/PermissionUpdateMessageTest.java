// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.permissions;

import android.Manifest;
import android.support.test.InstrumentationRegistry;

import androidx.test.filters.MediumTest;

import org.hamcrest.Matchers;
import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.util.ChromeTabUtils;
import org.chromium.chrome.test.util.browser.Features.EnableFeatures;
import org.chromium.chrome.test.util.browser.LocationSettingsTestUtil;
import org.chromium.components.browser_ui.site_settings.PermissionInfo;
import org.chromium.components.content_settings.ContentSettingValues;
import org.chromium.components.content_settings.ContentSettingsType;
import org.chromium.components.messages.MessagesTestHelper;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.net.test.EmbeddedTestServer;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.permissions.AndroidPermissionDelegate;
import org.chromium.ui.permissions.PermissionCallback;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.HashSet;
import java.util.List;
import java.util.Set;
import java.util.concurrent.TimeoutException;

/**
 * Tests for the permission update message.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@EnableFeatures({ChromeFeatureList.MESSAGES_FOR_ANDROID_PERMISSION_UPDATE})
public class PermissionUpdateMessageTest {
    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    private static final String GEOLOCATION_PAGE =
            "/chrome/test/data/geolocation/geolocation_on_load.html";

    private EmbeddedTestServer mTestServer;

    @Before
    public void setUp() throws Exception {
        mActivityTestRule.startMainActivityOnBlankPage();
        mTestServer = EmbeddedTestServer.createAndStartServer(InstrumentationRegistry.getContext());
    }

    @After
    public void tearDown() {
        mTestServer.stopAndDestroyServer();
    }

    // Ensure destroying the permission update message UI does not crash when handling geolocation
    // permissions.
    @Test
    @MediumTest
    public void testMessageShutsDownCleanlyForGeolocation()
            throws IllegalArgumentException, TimeoutException {
        ChromeTabUtils.newTabFromMenu(
                InstrumentationRegistry.getInstrumentation(), mActivityTestRule.getActivity());

        final String locationUrl = mTestServer.getURL(GEOLOCATION_PAGE);
        final PermissionInfo geolocationSettings = TestThreadUtils.runOnUiThreadBlockingNoException(
                ()
                        -> new PermissionInfo(
                                ContentSettingsType.GEOLOCATION, locationUrl, null, false));

        WindowAndroid windowAndroid = mActivityTestRule.getActivity().getWindowAndroid();
        windowAndroid.setAndroidPermissionDelegate(new TestAndroidPermissionDelegate(
                null, Arrays.asList(Manifest.permission.ACCESS_FINE_LOCATION), null));
        LocationSettingsTestUtil.setSystemLocationSettingEnabled(true);

        try {
            TestThreadUtils.runOnUiThreadBlocking(
                    ()
                            -> geolocationSettings.setContentSetting(
                                    Profile.getLastUsedRegularProfile(),
                                    ContentSettingValues.ALLOW));

            mActivityTestRule.loadUrl(mTestServer.getURL(GEOLOCATION_PAGE));

            CriteriaHelper.pollUiThread(() -> {
                Criteria.checkThat("Message is not enqueued.",
                        MessagesTestHelper.getMessageCount(windowAndroid), Matchers.is(1));
            });

            final WebContents webContents = TestThreadUtils.runOnUiThreadBlockingNoException(
                    () -> mActivityTestRule.getActivity().getActivityTab().getWebContents());
            Assert.assertFalse(webContents.isDestroyed());

            ChromeTabUtils.closeCurrentTab(
                    InstrumentationRegistry.getInstrumentation(), mActivityTestRule.getActivity());
            CriteriaHelper.pollUiThread(() -> webContents.isDestroyed());

            CriteriaHelper.pollUiThread(() -> {
                Criteria.checkThat(mActivityTestRule.getActivity()
                                           .getTabModelSelector()
                                           .getModel(false)
                                           .getCount(),
                        Matchers.is(1));
            });
        } finally {
            TestThreadUtils.runOnUiThreadBlocking(
                    ()
                            -> geolocationSettings.setContentSetting(
                                    Profile.getLastUsedRegularProfile(),
                                    ContentSettingValues.DEFAULT));
        }
    }

    private static class TestAndroidPermissionDelegate implements AndroidPermissionDelegate {
        private final Set<String> mHasPermissions;
        private final Set<String> mRequestablePermissions;
        private final Set<String> mPolicyRevokedPermissions;

        public TestAndroidPermissionDelegate(List<String> hasPermissions,
                List<String> requestablePermissions, List<String> policyRevokedPermissions) {
            mHasPermissions = new HashSet<>(
                    hasPermissions == null ? new ArrayList<String>() : hasPermissions);
            mRequestablePermissions =
                    new HashSet<>(requestablePermissions == null ? new ArrayList<String>()
                                                                 : requestablePermissions);
            mPolicyRevokedPermissions =
                    new HashSet<>(policyRevokedPermissions == null ? new ArrayList<String>()
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
}
