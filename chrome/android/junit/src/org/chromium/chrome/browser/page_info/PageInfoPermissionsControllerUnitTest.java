// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.page_info;

import static org.junit.Assert.assertEquals;

import android.content.Context;

import androidx.test.core.app.ApplicationProvider;
import androidx.test.filters.SmallTest;

import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.ParameterizedRobolectricTestRunner;

import org.chromium.base.test.BaseRobolectricTestRule;
import org.chromium.chrome.R;
import org.chromium.components.page_info.PageInfoPermissionsController;
import org.chromium.components.page_info.PageInfoPermissionsController.PermissionObject;

import java.util.Arrays;
import java.util.Collection;
import java.util.List;

/** Tests the functionality of the SingleWebsiteSettings.java page. */
@RunWith(ParameterizedRobolectricTestRunner.class)
public class PageInfoPermissionsControllerUnitTest {
    private static PermissionObject createPermission(
            String name, String nameMidSentence, boolean allowed, int warningResId) {
        PermissionObject permission = new PermissionObject();
        permission.name = name;
        permission.nameMidSentence = nameMidSentence;
        permission.allowed = allowed;
        permission.warningTextResource = warningResId;
        return permission;
    }

    private static final PermissionObject LOCATION_OS_WARNING =
            createPermission(
                    "Location", "location", true, R.string.page_info_android_location_blocked);
    private static final PermissionObject LOCATION_ALLOWED =
            createPermission("Location", "location", true, 0);
    private static final PermissionObject LOCATION_BLOCKED =
            createPermission("Location", "location", false, 0);
    private static final PermissionObject SOUND_ALLOWED =
            createPermission("Sound", "sound", true, 0);
    private static final PermissionObject SOUND_BLOCKED =
            createPermission("Sound", "sound", false, 0);
    private static final PermissionObject VR_ALLOWED =
            createPermission("Virtual reality", "virtual reality", true, 0);
    private static final PermissionObject VR_BLOCKED =
            createPermission("Virtual reality", "virtual reality", false, 0);
    private static final PermissionObject AR_ALLOWED =
            createPermission("Augmented reality", "augmented reality", true, 0);
    private static final PermissionObject AR_BLOCKED =
            createPermission("Augmented reality", "augmented reality", false, 0);

    @Rule(order = -2)
    public BaseRobolectricTestRule mBaseRule = new BaseRobolectricTestRule();

    private Context mContext;
    private String mTestName;
    private List<PermissionObject> mPermissions;
    private String mExpectedString;

    public PageInfoPermissionsControllerUnitTest(
            String testName, List<PermissionObject> permissions, String expectedString) {
        mContext = ApplicationProvider.getApplicationContext();
        this.mTestName = testName;
        this.mPermissions = permissions;
        this.mExpectedString = expectedString;
    }

    @ParameterizedRobolectricTestRunner.Parameters
    public static Collection testCases() {
        return Arrays.asList(
                new Object[][] {
                    {"No Permissions", Arrays.asList(), null},
                    {
                        "OS Warning",
                        Arrays.asList(LOCATION_OS_WARNING, SOUND_ALLOWED, VR_ALLOWED),
                        "Location - Turned off for this device"
                    },
                    {"One Permission Allowed", Arrays.asList(LOCATION_ALLOWED), "Location allowed"},
                    {"One Permission Blocked", Arrays.asList(LOCATION_BLOCKED), "Location blocked"},
                    {
                        "Two Permissions Allowed",
                        Arrays.asList(LOCATION_ALLOWED, SOUND_ALLOWED),
                        "Location and sound allowed"
                    },
                    {
                        "Two Permissions Blocked",
                        Arrays.asList(LOCATION_BLOCKED, SOUND_BLOCKED),
                        "Location and sound blocked"
                    },
                    {
                        "Two Permissions Mixed",
                        Arrays.asList(LOCATION_ALLOWED, SOUND_BLOCKED),
                        "Location allowed, sound blocked"
                    },
                    {
                        "Two Permissions Mixed Reverse Order",
                        Arrays.asList(LOCATION_BLOCKED, SOUND_ALLOWED),
                        "Sound allowed, location blocked"
                    },
                    {
                        "Multiple Permissions Allowed",
                        Arrays.asList(LOCATION_ALLOWED, SOUND_ALLOWED, VR_ALLOWED),
                        "Location, sound, and 1 more allowed"
                    },
                    {
                        "Multiple Permissions Allowed 2",
                        Arrays.asList(LOCATION_ALLOWED, SOUND_ALLOWED, VR_ALLOWED, AR_ALLOWED),
                        "Location, sound, and 2 more allowed"
                    },
                    {
                        "Multiple Permissions Blocked",
                        Arrays.asList(LOCATION_BLOCKED, SOUND_BLOCKED, VR_BLOCKED),
                        "Location, sound, and 1 more blocked"
                    },
                    {
                        "Multiple Permissions Blocked 2",
                        Arrays.asList(LOCATION_BLOCKED, SOUND_BLOCKED, VR_BLOCKED, AR_BLOCKED),
                        "Location, sound, and 2 more blocked"
                    },
                    {
                        "Multiple Permissions Mixed",
                        Arrays.asList(LOCATION_ALLOWED, SOUND_BLOCKED, VR_BLOCKED),
                        "Location, sound, and 1 more"
                    },
                    {
                        "Multiple Permissions Mixed 2",
                        Arrays.asList(LOCATION_ALLOWED, SOUND_BLOCKED, VR_BLOCKED, AR_BLOCKED),
                        "Location, sound, and 2 more"
                    }
                });
    }

    @Test
    @SmallTest
    public void testSummaryStringIsCorrectForPermissions() {
        assertEquals(
                mTestName,
                PageInfoPermissionsController.getPermissionSummaryString(
                        this.mPermissions, mContext.getResources()),
                mExpectedString);
    }
}
