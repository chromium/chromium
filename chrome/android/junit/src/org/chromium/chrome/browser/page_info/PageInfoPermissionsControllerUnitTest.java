// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.page_info;

import static org.junit.Assert.assertEquals;

import android.content.Context;

import androidx.test.core.app.ApplicationProvider;
import androidx.test.filters.SmallTest;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.ParameterizedRobolectricTestRunner;

import org.chromium.chrome.R;
import org.chromium.components.page_info.PageInfoPermissionsController;
import org.chromium.components.page_info.PageInfoView.PermissionRowParams;

import java.util.Arrays;
import java.util.Collection;
import java.util.List;

/**
 * Tests the functionality of the SingleWebsiteSettings.java page.
 */
@RunWith(ParameterizedRobolectricTestRunner.class)
public class PageInfoPermissionsControllerUnitTest {
    private static PermissionRowParams createPermission(
            String name, boolean allowed, int warningResId) {
        PermissionRowParams permission = new PermissionRowParams();
        permission.name = name;
        permission.allowed = allowed;
        permission.warningTextResource = warningResId;
        return permission;
    }
    private static final PermissionRowParams LOCATION_OS_WARNING =
            createPermission("Location", true, R.string.page_info_android_location_blocked);
    private static final PermissionRowParams LOCATION_ALLOWED =
            createPermission("Location", true, 0);
    private static final PermissionRowParams LOCATION_BLOCKED =
            createPermission("Location", false, 0);
    private static final PermissionRowParams SOUND_ALLOWED = createPermission("Sound", true, 0);
    private static final PermissionRowParams SOUND_BLOCKED = createPermission("Sound", false, 0);
    private static final PermissionRowParams VR_ALLOWED = createPermission("VR", true, 0);
    private static final PermissionRowParams VR_BLOCKED = createPermission("VR", false, 0);
    private static final PermissionRowParams AR_ALLOWED = createPermission("AR", true, 0);
    private static final PermissionRowParams AR_BLOCKED = createPermission("AR", false, 0);

    private Context mContext;
    private String mTestName;
    private List<PermissionRowParams> mPermissions;
    private String mExpectedString;

    public PageInfoPermissionsControllerUnitTest(
            String testName, List<PermissionRowParams> permissions, String expectedString) {
        mContext = ApplicationProvider.getApplicationContext();
        this.mTestName = testName;
        this.mPermissions = permissions;
        this.mExpectedString = expectedString;
    }

    @ParameterizedRobolectricTestRunner.Parameters
    public static Collection testCases() {
        return Arrays.asList(new Object[][] {{"No Permissions", Arrays.asList(), null},
                {"OS Warning", Arrays.asList(LOCATION_OS_WARNING, SOUND_ALLOWED, VR_ALLOWED),
                        "Location - Turned off for this device"},
                {"One Permission Allowed", Arrays.asList(LOCATION_ALLOWED), "Location allowed"},
                {"One Permission Blocked", Arrays.asList(LOCATION_BLOCKED), "Location blocked"},
                {"Two Permissions Allowed", Arrays.asList(LOCATION_ALLOWED, SOUND_ALLOWED),
                        "Location and Sound allowed"},
                {"Two Permissions Blocked", Arrays.asList(LOCATION_BLOCKED, SOUND_BLOCKED),
                        "Location and Sound blocked"},
                {"Two Permissions Mixed", Arrays.asList(LOCATION_ALLOWED, SOUND_BLOCKED),
                        "Location allowed, Sound blocked"},
                {"Two Permissions Mixed Reverse Order",
                        Arrays.asList(LOCATION_BLOCKED, SOUND_ALLOWED),
                        "Sound allowed, Location blocked"},
                {"Multiple Permissions Allowed",
                        Arrays.asList(LOCATION_ALLOWED, SOUND_ALLOWED, VR_ALLOWED),
                        "Location, Sound, and 1 more allowed"},
                {"Multiple Permissions Allowed 2",
                        Arrays.asList(LOCATION_ALLOWED, SOUND_ALLOWED, VR_ALLOWED, AR_ALLOWED),
                        "Location, Sound, and 2 more allowed"},
                {"Multiple Permissions Blocked",
                        Arrays.asList(LOCATION_BLOCKED, SOUND_BLOCKED, VR_BLOCKED),
                        "Location, Sound, and 1 more blocked"},
                {"Multiple Permissions Blocked 2",
                        Arrays.asList(LOCATION_BLOCKED, SOUND_BLOCKED, VR_BLOCKED, AR_BLOCKED),
                        "Location, Sound, and 2 more blocked"},
                {"Multiple Permissions Mixed",
                        Arrays.asList(LOCATION_ALLOWED, SOUND_BLOCKED, VR_BLOCKED),
                        "Location, Sound, and 1 more"},
                {"Multiple Permissions Mixed 2",
                        Arrays.asList(LOCATION_ALLOWED, SOUND_BLOCKED, VR_BLOCKED, AR_BLOCKED),
                        "Location, Sound, and 2 more"}});
    }

    @Test
    @SmallTest
    public void testSummaryStringIsCorrectForPermissions() {
        assertEquals(mTestName,
                PageInfoPermissionsController.getPermissionSummaryString(
                        this.mPermissions, mContext.getResources()),
                mExpectedString);
    }
}