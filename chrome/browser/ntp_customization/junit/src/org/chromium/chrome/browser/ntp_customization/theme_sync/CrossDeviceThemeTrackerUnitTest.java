// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp_customization.theme_sync;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.app.Activity;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Robolectric;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.RobolectricUtil;
import org.chromium.base.test.util.Features;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.ntp_customization.NtpCustomizationUtils;
import org.chromium.chrome.browser.ntp_customization.theme.chrome_colors.NtpThemeColorInfo.NtpThemeColorId;
import org.chromium.chrome.browser.ntp_customization.theme_sync.data.NtpBackgroundDataColor;
import org.chromium.chrome.browser.ntp_customization.theme_sync.data.NtpBackgroundDataGroup;
import org.chromium.chrome.browser.ntp_customization.theme_sync.data.NtpBackgroundDataManager;
import org.chromium.chrome.browser.ntp_customization.theme_sync.data.PlatformType;
import org.chromium.chrome.browser.profiles.Profile;

import java.lang.reflect.Method;
import java.util.Collections;

/** Unit tests for {@link CrossDeviceThemeTracker}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
@Features.EnableFeatures({ChromeFeatureList.NEW_TAB_PAGE_CUSTOMIZATION_V2})
public class CrossDeviceThemeTrackerUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private CrossDeviceThemeTracker.Natives mNatives;
    @Mock private Profile mProfile;

    private Activity mActivity;

    @Before
    public void setUp() {
        mActivity = Robolectric.buildActivity(Activity.class).setup().get();
        CrossDeviceThemeTracker.setInstanceForTesting(mNatives);
        NtpCustomizationUtils.resetSharedPreferenceForTesting();
    }

    @After
    public void tearDown() {
        NtpCustomizationUtils.resetSharedPreferenceForTesting();
    }

    @Test
    public void testGetForProfile() {
        CrossDeviceThemeTracker tracker = mock(CrossDeviceThemeTracker.class);
        when(mNatives.getForProfile(mProfile)).thenReturn(tracker);

        assertEquals(tracker, CrossDeviceThemeTracker.getForProfile(mProfile));
        verify(mNatives).getForProfile(mProfile);
    }

    @Test
    public void testGetThemes() throws Exception {
        NtpBackgroundDataColor remoteColor =
                new NtpBackgroundDataColor(
                        mActivity, PlatformType.DESKTOP, NtpThemeColorId.NTP_COLORS_VIOLET, false);
        when(mNatives.getThemes(eq(1L), any())).thenReturn(Collections.singletonList(remoteColor));

        Method createMethod = CrossDeviceThemeTracker.class.getDeclaredMethod("create", long.class);
        createMethod.setAccessible(true);
        CrossDeviceThemeTracker tracker = (CrossDeviceThemeTracker) createMethod.invoke(null, 1L);

        assertEquals(Collections.singletonList(remoteColor), tracker.getThemes(mActivity));
        verify(mNatives).getThemes(1L, mActivity);
    }

    @Test
    public void testSetActivity_FlushesPendingSyncData() throws Exception {
        NtpBackgroundDataColor remoteColor =
                new NtpBackgroundDataColor(
                        mActivity, PlatformType.DESKTOP, NtpThemeColorId.NTP_COLORS_VIOLET, false);
        when(mNatives.getThemes(eq(1L), any())).thenReturn(Collections.singletonList(remoteColor));

        Method createMethod = CrossDeviceThemeTracker.class.getDeclaredMethod("create", long.class);
        createMethod.setAccessible(true);
        CrossDeviceThemeTracker tracker = (CrossDeviceThemeTracker) createMethod.invoke(null, 1L);

        assertTrue(tracker.getHasPendingSyncDataForTesting());

        // Setting Activity flushes pending sync data to SharedPreferences.
        tracker.setActivity(mActivity);
        RobolectricUtil.runAllBackgroundAndUi();

        assertFalse(tracker.getHasPendingSyncDataForTesting());
        NtpBackgroundDataManager manager = new NtpBackgroundDataManager(mActivity);
        NtpBackgroundDataGroup desktopGroup =
                manager.getBackgroundDataGroupFromSharedPreference(PlatformType.DESKTOP);
        assertNotNull(desktopGroup);
        assertTrue(desktopGroup.getList().contains(remoteColor));
    }

    @Test
    public void testNotifyThemesChanged_WithActivity_SyncsImmediately() throws Exception {
        NtpBackgroundDataColor remoteColor =
                new NtpBackgroundDataColor(
                        mActivity, PlatformType.DESKTOP, NtpThemeColorId.NTP_COLORS_VIOLET, false);
        when(mNatives.getThemes(eq(1L), any())).thenReturn(Collections.singletonList(remoteColor));

        Method createMethod = CrossDeviceThemeTracker.class.getDeclaredMethod("create", long.class);
        createMethod.setAccessible(true);
        CrossDeviceThemeTracker tracker = (CrossDeviceThemeTracker) createMethod.invoke(null, 1L);
        tracker.setActivity(mActivity);
        RobolectricUtil.runAllBackgroundAndUi();

        // Simulate notifyThemesChanged() called from native while Activity is set.
        Method notifyMethod =
                CrossDeviceThemeTracker.class.getDeclaredMethod("notifyThemesChanged");
        notifyMethod.setAccessible(true);
        notifyMethod.invoke(tracker);

        RobolectricUtil.runAllBackgroundAndUi();

        assertFalse(tracker.getHasPendingSyncDataForTesting());
        NtpBackgroundDataManager manager = new NtpBackgroundDataManager(mActivity);
        NtpBackgroundDataGroup desktopGroup =
                manager.getBackgroundDataGroupFromSharedPreference(PlatformType.DESKTOP);
        assertNotNull(desktopGroup);
        assertTrue(desktopGroup.getList().contains(remoteColor));
    }

    @Test
    public void testNotifyThemesChanged_WithoutActivity_SetsPendingFlag() throws Exception {
        Method createMethod = CrossDeviceThemeTracker.class.getDeclaredMethod("create", long.class);
        createMethod.setAccessible(true);
        CrossDeviceThemeTracker tracker = (CrossDeviceThemeTracker) createMethod.invoke(null, 1L);

        // Clear activity.
        tracker.setActivity(null);

        // Simulate notifyThemesChanged() called from native with no Activity set.
        Method notifyMethod =
                CrossDeviceThemeTracker.class.getDeclaredMethod("notifyThemesChanged");
        notifyMethod.setAccessible(true);
        notifyMethod.invoke(tracker);

        assertTrue(tracker.getHasPendingSyncDataForTesting());
    }

    @Test
    public void testNotifyThemesChanged_NotifiesObservers() throws Exception {
        Method createMethod = CrossDeviceThemeTracker.class.getDeclaredMethod("create", long.class);
        createMethod.setAccessible(true);
        CrossDeviceThemeTracker tracker = (CrossDeviceThemeTracker) createMethod.invoke(null, 1L);

        CrossDeviceThemeTracker.Observer observer = mock(CrossDeviceThemeTracker.Observer.class);
        tracker.addObserver(observer);

        Method notifyMethod =
                CrossDeviceThemeTracker.class.getDeclaredMethod("notifyThemesChanged");
        notifyMethod.setAccessible(true);
        notifyMethod.invoke(tracker);

        verify(observer).onThemesChanged();
    }
}
