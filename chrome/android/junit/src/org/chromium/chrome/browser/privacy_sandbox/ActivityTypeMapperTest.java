// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.privacy_sandbox;

import static org.junit.Assert.assertEquals;
import static org.mockito.Mockito.when;

import static org.chromium.chrome.browser.privacy_sandbox.ActivityTypeMapper.OTHER_SURFACE_TYPE;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.browserservices.intents.BrowserServicesIntentDataProvider;
import org.chromium.chrome.browser.flags.ActivityType;

/**
 * Unit tests for the {@link ActivityTypeMapper} class.
 *
 * <p>This test suite ensures that the mapping from browser activity types to privacy sandbox
 * storage activity types works correctly, handling both standard activity types and special cases
 * like custom tabs and pre-first-tab activities.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class ActivityTypeMapperTest {
    @Mock BrowserServicesIntentDataProvider mMockIntentDataProvider;

    @Before
    public void setUp() {
        MockitoAnnotations.openMocks(this);
    }

    @Test
    public void testPreFirstTabActivity() {
        assertEquals(
                PrivacySandboxStorageActivityType.PRE_FIRST_TAB,
                ActivityTypeMapper.toPrivacySandboxStorageActivityType(
                        ActivityType.PRE_FIRST_TAB, mMockIntentDataProvider));
    }

    @Test
    public void testAgsaCustomTab() {
        when(mMockIntentDataProvider.getActivityType()).thenReturn(ActivityType.CUSTOM_TAB);
        when(mMockIntentDataProvider.getClientPackageName())
                .thenReturn("com.google.android.googlequicksearchbox");
        when(mMockIntentDataProvider.isPartialCustomTab()).thenReturn(false);
        assertEquals(
                PrivacySandboxStorageActivityType.AGSA_CUSTOM_TAB,
                ActivityTypeMapper.toPrivacySandboxStorageActivityType(
                        ActivityType.CUSTOM_TAB, mMockIntentDataProvider));
    }

    @Test
    public void testNonAgsaCustomTab() {
        when(mMockIntentDataProvider.getActivityType()).thenReturn(ActivityType.CUSTOM_TAB);
        when(mMockIntentDataProvider.getClientPackageName()).thenReturn("com.example.app");
        when(mMockIntentDataProvider.isPartialCustomTab()).thenReturn(false);
        assertEquals(
                PrivacySandboxStorageActivityType.NON_AGSA_CUSTOM_TAB,
                ActivityTypeMapper.toPrivacySandboxStorageActivityType(
                        ActivityType.CUSTOM_TAB, mMockIntentDataProvider));
    }

    @Test
    public void testPartialCct() {
        when(mMockIntentDataProvider.getActivityType()).thenReturn(ActivityType.CUSTOM_TAB);
        when(mMockIntentDataProvider.getClientPackageName())
                .thenReturn("com.google.android.googlequicksearchbox");
        when(mMockIntentDataProvider.isPartialCustomTab()).thenReturn(true);
        assertEquals(
                PrivacySandboxStorageActivityType.OTHER,
                ActivityTypeMapper.toPrivacySandboxStorageActivityType(
                        ActivityType.CUSTOM_TAB, mMockIntentDataProvider));
    }

    // Tests for standard activity types
    @Test
    public void testTrustedWebActivity() {
        assertEquals(
                PrivacySandboxStorageActivityType.TRUSTED_WEB_ACTIVITY,
                ActivityTypeMapper.toPrivacySandboxStorageActivityType(
                        ActivityType.TRUSTED_WEB_ACTIVITY, mMockIntentDataProvider));
        assertEquals(
                PrivacySandboxStorageActivityType.TRUSTED_WEB_ACTIVITY,
                ActivityTypeMapper.toPrivacySandboxStorageActivityType(
                        ActivityType.TRUSTED_WEB_ACTIVITY));
    }

    @Test
    public void testWebApp() {
        assertEquals(
                PrivacySandboxStorageActivityType.WEBAPP,
                ActivityTypeMapper.toPrivacySandboxStorageActivityType(
                        ActivityType.WEBAPP, mMockIntentDataProvider));
        assertEquals(
                PrivacySandboxStorageActivityType.WEBAPP,
                ActivityTypeMapper.toPrivacySandboxStorageActivityType(ActivityType.WEBAPP));
    }

    @Test
    public void testWebApk() {
        assertEquals(
                PrivacySandboxStorageActivityType.WEB_APK,
                ActivityTypeMapper.toPrivacySandboxStorageActivityType(
                        ActivityType.WEB_APK, mMockIntentDataProvider));
        assertEquals(
                PrivacySandboxStorageActivityType.WEB_APK,
                ActivityTypeMapper.toPrivacySandboxStorageActivityType(ActivityType.WEB_APK));
    }

    @Test
    public void testTabbedActivity() {
        assertEquals(
                PrivacySandboxStorageActivityType.TABBED,
                ActivityTypeMapper.toPrivacySandboxStorageActivityType(
                        ActivityType.TABBED, mMockIntentDataProvider));
        assertEquals(
                PrivacySandboxStorageActivityType.TABBED,
                ActivityTypeMapper.toPrivacySandboxStorageActivityType(ActivityType.TABBED));
    }

    @Test
    public void testUnmappedActivity() {
        assertEquals(
                PrivacySandboxStorageActivityType.OTHER,
                ActivityTypeMapper.toPrivacySandboxStorageActivityType(
                        102, mMockIntentDataProvider));
        assertEquals(
                PrivacySandboxStorageActivityType.OTHER,
                ActivityTypeMapper.toPrivacySandboxStorageActivityType(102));
    }

    @Test
    public void testCustomTabWithoutIntentProvider() {
        assertEquals(
                PrivacySandboxStorageActivityType.OTHER,
                ActivityTypeMapper.toPrivacySandboxStorageActivityType(ActivityType.CUSTOM_TAB));
    }

    @Test
    public void testAgsaCustomTabSurface() {
        when(mMockIntentDataProvider.getActivityType()).thenReturn(ActivityType.CUSTOM_TAB);
        when(mMockIntentDataProvider.getClientPackageName())
                .thenReturn("com.google.android.googlequicksearchbox");
        when(mMockIntentDataProvider.isPartialCustomTab()).thenReturn(false);
        int surfaceType =
                ActivityTypeMapper.toSurfaceType(ActivityType.CUSTOM_TAB, mMockIntentDataProvider);
        assertEquals(SurfaceType.AGACCT, surfaceType);
    }

    @Test
    public void testTabbedSurface() {
        when(mMockIntentDataProvider.getActivityType()).thenReturn(ActivityType.TABBED);
        int surfaceType =
                ActivityTypeMapper.toSurfaceType(ActivityType.TABBED, mMockIntentDataProvider);
        assertEquals(SurfaceType.BR_APP, surfaceType);
    }

    @Test
    public void testInvalidCombination() {
        int surfaceType =
                ActivityTypeMapper.toSurfaceType(ActivityType.WEB_APK, mMockIntentDataProvider);
        assertEquals(OTHER_SURFACE_TYPE, surfaceType);
    }
}
