// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.Mockito.when;

import android.app.Activity;
import android.content.Intent;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Robolectric;
import org.robolectric.Shadows;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.components.plus_addresses.PlusAddressesMetricsRecorder;
import org.chromium.components.signin.base.CoreAccountInfo;
import org.chromium.components.signin.identitymanager.IdentityManager;
import org.chromium.ui.base.TestActivity;

/** Unit tests for {@link PlusAddressesHelperTest}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
@Batch(Batch.UNIT_TESTS)
@EnableFeatures(ChromeFeatureList.PLUS_ADDRESS_ANDROID_OPEN_GMS_CORE_MANAGEMENT_PAGE)
public class PlusAddressesHelperTest {
    private static final String PLUS_ADDRESS_MANAGEMENT_URL = "https://manage.plus.addresses.com";

    @Rule public JniMocker mJniMocker = new JniMocker();
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private Profile mProfile;
    @Mock private IdentityManager mIdentityManager;
    @Mock private IdentityServicesProvider mIdentityServicesProvider;
    @Mock private PlusAddressesHelper.Natives mPlusAddressesHelperJni;

    private Activity mActivity;

    @Before
    public void setUp() {
        mJniMocker.mock(PlusAddressesHelperJni.TEST_HOOKS, mPlusAddressesHelperJni);
        when(mPlusAddressesHelperJni.getPlusAddressManagementUrl())
                .thenReturn(PLUS_ADDRESS_MANAGEMENT_URL);

        IdentityServicesProvider.setInstanceForTests(mIdentityServicesProvider);

        mActivity = Robolectric.setupActivity(TestActivity.class);
    }

    @Test
    @DisableFeatures(ChromeFeatureList.PLUS_ADDRESS_ANDROID_OPEN_GMS_CORE_MANAGEMENT_PAGE)
    public void testOpensCctWithFeatureDisabled_NoIdentityManager() {
        // The metric is not logged if the GMS Core Account Settings feature is not enabled.
        HistogramWatcher histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectNoRecords(
                                PlusAddressesMetricsRecorder
                                        .UMA_PLUS_ADDRESSES_OPEN_ACCOUNT_SETTINGS)
                        .build();
        PlusAddressesHelper.openManagePlusAddresses(mActivity, mProfile);

        var shadowActivity = Shadows.shadowOf(mActivity);
        Intent cctIntent = shadowActivity.getNextStartedActivity();
        assertNull(cctIntent);
        histogramWatcher.assertExpected();
    }

    @Test
    @DisableFeatures(ChromeFeatureList.PLUS_ADDRESS_ANDROID_OPEN_GMS_CORE_MANAGEMENT_PAGE)
    public void testOpensCctWithFeatureDisabled() {
        // The metric is not logged if the GMS Core Account Settings feature is not enabled.
        HistogramWatcher histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectNoRecords(
                                PlusAddressesMetricsRecorder
                                        .UMA_PLUS_ADDRESSES_OPEN_ACCOUNT_SETTINGS)
                        .build();
        when(mIdentityServicesProvider.getIdentityManager(mProfile)).thenReturn(mIdentityManager);

        PlusAddressesHelper.openManagePlusAddresses(mActivity, mProfile);

        var shadowActivity = Shadows.shadowOf(mActivity);
        Intent cctIntent = shadowActivity.getNextStartedActivity();
        assertNotNull(cctIntent);
        assertEquals(cctIntent.getDataString(), PLUS_ADDRESS_MANAGEMENT_URL);
        histogramWatcher.assertExpected();
    }

    @Test
    public void testOpensGmsCore_NoIdentityManager() {
        // The metric is not logged if the IdentityManager is not available.
        HistogramWatcher histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectNoRecords(
                                PlusAddressesMetricsRecorder
                                        .UMA_PLUS_ADDRESSES_OPEN_ACCOUNT_SETTINGS)
                        .build();
        PlusAddressesHelper.openManagePlusAddresses(mActivity, mProfile);

        var shadowActivity = Shadows.shadowOf(mActivity);
        Intent gmsCoreIntent = shadowActivity.getNextStartedActivity();
        assertNull(gmsCoreIntent);
        histogramWatcher.assertExpected();
    }

    @Test
    public void testOpensGmsCore_UserNotSignedIn() {
        // The metric is not logged if the user is not signed in.
        HistogramWatcher histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectNoRecords(
                                PlusAddressesMetricsRecorder
                                        .UMA_PLUS_ADDRESSES_OPEN_ACCOUNT_SETTINGS)
                        .build();
        when(mIdentityServicesProvider.getIdentityManager(mProfile)).thenReturn(mIdentityManager);

        PlusAddressesHelper.openManagePlusAddresses(mActivity, mProfile);

        var shadowActivity = Shadows.shadowOf(mActivity);
        Intent gmsCoreIntent = shadowActivity.getNextStartedActivity();
        assertNull(gmsCoreIntent);
        histogramWatcher.assertExpected();
    }

    @Test
    public void testOpensGmsCore() {
        HistogramWatcher histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectBooleanRecord(
                                PlusAddressesMetricsRecorder
                                        .UMA_PLUS_ADDRESSES_OPEN_ACCOUNT_SETTINGS,
                                true)
                        .build();
        when(mIdentityServicesProvider.getIdentityManager(mProfile)).thenReturn(mIdentityManager);
        CoreAccountInfo accountInfo =
                CoreAccountInfo.createFromEmailAndGaiaId("test@gmail.com", "testGaiaId");
        when(mIdentityManager.getPrimaryAccountInfo(anyInt())).thenReturn(accountInfo);

        PlusAddressesHelper.openManagePlusAddresses(mActivity, mProfile);

        var shadowActivity = Shadows.shadowOf(mActivity);
        Intent gmsCoreIntent = shadowActivity.getNextStartedActivity();
        assertNotNull(gmsCoreIntent);
        histogramWatcher.assertExpected();
    }
}
