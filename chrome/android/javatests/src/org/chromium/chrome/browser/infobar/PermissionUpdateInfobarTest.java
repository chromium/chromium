// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.infobar;

import android.Manifest;
import android.support.test.InstrumentationRegistry;
import android.support.test.filters.MediumTest;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.browser.preferences.website.ContentSetting;
import org.chromium.chrome.browser.preferences.website.PermissionInfo;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.util.ChromeTabUtils;
import org.chromium.chrome.test.util.InfoBarTestAnimationListener;
import org.chromium.chrome.test.util.browser.LocationSettingsTestUtil;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.test.util.Criteria;
import org.chromium.content_public.browser.test.util.CriteriaHelper;
import org.chromium.net.test.EmbeddedTestServer;
import org.chromium.ui.base.AndroidPermissionDelegate;
import org.chromium.ui.base.PermissionCallback;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.HashSet;
import java.util.List;
import java.util.Set;
import java.util.concurrent.Callable;
import java.util.concurrent.TimeoutException;

/**
 * Tests for the permission update infobar.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class PermissionUpdateInfobarTest {
    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    private static final String GEOLOCATION_PAGE =
            "/chrome/test/data/geolocation/geolocation_on_load.html";
    private static final String GEOLOCATION_IFRAME_PAGE =
            "/chrome/test/data/geolocation/geolocation_iframe_on_load.html";

    private InfoBarTestAnimationListener mListener;
    private EmbeddedTestServer mTestServer;

    @Before
    public void setUp() throws Exception {
        mActivityTestRule.startMainActivityOnBlankPage();
        mTestServer = EmbeddedTestServer.createAndStartServer(InstrumentationRegistry.getContext());
    }

    @After
    public void tearDown() throws Exception {
        mTestServer.stopAndDestroyServer();
    }

    // Ensure destroying the permission update infobar does not crash when handling geolocation
    // permissions.
    @Test
    @MediumTest
    public void testInfobarShutsDownCleanlyForGeolocation()
            throws IllegalArgumentException, InterruptedException, TimeoutException {
        ChromeTabUtils.newTabFromMenu(
                InstrumentationRegistry.getInstrumentation(), mActivityTestRule.getActivity());

        // Register for animation notifications
        CriteriaHelper.pollInstrumentationThread(
                () -> mActivityTestRule.getInfoBarContainer() != null);
        InfoBarContainer container = mActivityTestRule.getInfoBarContainer();
        mListener =  new InfoBarTestAnimationListener();
        container.addAnimationListener(mListener);

        final String locationUrl = mTestServer.getURL(GEOLOCATION_PAGE);
        final PermissionInfo geolocationSettings =
                ThreadUtils.runOnUiThreadBlockingNoException(new Callable<PermissionInfo>() {
                    @Override
                    public PermissionInfo call() {
                        return new PermissionInfo(
                                PermissionInfo.Type.GEOLOCATION, locationUrl, null, false);
                    }
                });

        mActivityTestRule.getActivity().getWindowAndroid().setAndroidPermissionDelegate(
                new TestAndroidPermissionDelegate(
                        null, Arrays.asList(Manifest.permission.ACCESS_FINE_LOCATION), null));
        LocationSettingsTestUtil.setSystemLocationSettingEnabled(true);

        try {
            ThreadUtils.runOnUiThreadBlocking(new Runnable() {
                @Override
                public void run() {
                    geolocationSettings.setContentSetting(ContentSetting.ALLOW);
                }
            });

            mActivityTestRule.loadUrl(mTestServer.getURL(GEOLOCATION_PAGE));
            mListener.addInfoBarAnimationFinished("InfoBar not added");
            Assert.assertEquals(1, mActivityTestRule.getInfoBars().size());

            final WebContents webContents = ThreadUtils.runOnUiThreadBlockingNoException(
                    new Callable<WebContents>() {
                        @Override
                        public WebContents call() throws Exception {
                            return mActivityTestRule.getActivity()
                                    .getActivityTab()
                                    .getWebContents();
                        }
                    });
            Assert.assertFalse(webContents.isDestroyed());

            ChromeTabUtils.closeCurrentTab(
                    InstrumentationRegistry.getInstrumentation(), mActivityTestRule.getActivity());
            CriteriaHelper.pollUiThread(new Criteria() {
                @Override
                public boolean isSatisfied() {
                    return webContents.isDestroyed();
                }
            });

            CriteriaHelper.pollUiThread(Criteria.equals(1, new Callable<Integer>() {
                @Override
                public Integer call() {
                    return mActivityTestRule.getActivity()
                            .getTabModelSelector()
                            .getModel(false)
                            .getCount();
                }
            }));
        } finally {
            ThreadUtils.runOnUiThreadBlocking(new Runnable() {
                @Override
                public void run() {
                    geolocationSettings.setContentSetting(ContentSetting.DEFAULT);
                }
            });
        }
    }

    private static class TestAndroidPermissionDelegate implements AndroidPermissionDelegate {
        private final Set<String> mHasPermissions;
        private final Set<String> mRequestablePermissions;
        private final Set<String> mPolicyRevokedPermissions;

        public TestAndroidPermissionDelegate(
                List<String> hasPermissions,
                List<String> requestablePermissions,
                List<String> policyRevokedPermissions) {
            mHasPermissions = new HashSet<>(hasPermissions == null
                    ? new ArrayList<String>() : hasPermissions);
            mRequestablePermissions = new HashSet<>(requestablePermissions == null
                    ? new ArrayList<String>() : requestablePermissions);
            mPolicyRevokedPermissions = new HashSet<>(policyRevokedPermissions == null
                    ? new ArrayList<String>() : policyRevokedPermissions);
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
        public void requestPermissions(String[] permissions, PermissionCallback callback) {
        }

        @Override
        public boolean handlePermissionResult(
                int requestCode, String[] permissions, int[] grantResults) {
            return false;
        }
    }

}
