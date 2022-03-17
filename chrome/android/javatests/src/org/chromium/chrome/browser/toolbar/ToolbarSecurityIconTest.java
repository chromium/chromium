// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.chrome.browser.toolbar;

import static org.junit.Assert.assertEquals;
import static org.mockito.Mockito.any;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.spy;

import android.content.Context;
import android.view.ContextThemeWrapper;

import androidx.test.filters.SmallTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.MockitoAnnotations;

import org.chromium.base.ContextUtils;
import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.UiThreadTest;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.omnibox.NewTabPageDelegate;
import org.chromium.chrome.browser.omnibox.SearchEngineLogoUtils;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.TabImpl;
import org.chromium.chrome.test.util.ToolbarTestUtils;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.components.prefs.PrefService;
import org.chromium.components.security_state.ConnectionSecurityLevel;
import org.chromium.components.security_state.SecurityStateModel;
import org.chromium.components.security_state.SecurityStateModelJni;
import org.chromium.content_public.browser.test.NativeLibraryTestUtils;
import org.chromium.content_public.browser.test.util.TestThreadUtils;

/**
 * Instrumentation tests for the toolbar security icon.
 */
@RunWith(BaseJUnit4ClassRunner.class)
@Batch(Batch.UNIT_TESTS)
@Features.DisableFeatures({ChromeFeatureList.OMNIBOX_UPDATED_CONNECTION_SECURITY_INDICATORS,
        ChromeFeatureList.LOCATION_BAR_MODEL_OPTIMIZATIONS})
public final class ToolbarSecurityIconTest {
    private static final boolean IS_SMALL_DEVICE = true;
    private static final boolean IS_OFFLINE_PAGE = true;
    private static final boolean IS_PREVIEW = true;
    private static final boolean IS_PAINT_PREVIEW = true;
    private static final int[] SECURITY_LEVELS =
            new int[] {ConnectionSecurityLevel.NONE, ConnectionSecurityLevel.WARNING,
                    ConnectionSecurityLevel.DANGEROUS, ConnectionSecurityLevel.SECURE};

    @Rule
    public TestRule mProcessor = new Features.JUnitProcessor();

    @Rule
    public JniMocker mocker = new JniMocker();

    @Mock
    private TabImpl mTab;
    @Mock
    SecurityStateModel.Natives mSecurityStateMocks;

    @Mock
    private LocationBarModel mLocationBarModel;
    @Mock
    private SearchEngineLogoUtils mSearchEngineLogoUtils;

    @Mock
    private PrefService mMockPrefService;
    @Mock
    private Profile mMockProfile;

    /**
     * Set up the lock icon policy for Mock PrefService.
     * @param isPolicyEnabled If true, omnibox must show the lock icon.
     */
    private void setupLockIconPolicyForTests(boolean isPolicyEnabled) {
        Mockito.when(mMockPrefService.isManagedPreference(
                             ChromePreferenceKeys.LOCK_ICON_IN_ADDRESS_BAR_ENABLED))
                .thenReturn(isPolicyEnabled);
        Mockito.when(mMockPrefService.getBoolean(
                             ChromePreferenceKeys.LOCK_ICON_IN_ADDRESS_BAR_ENABLED))
                .thenReturn(isPolicyEnabled);
    }

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);

        NativeLibraryTestUtils.loadNativeLibraryNoBrowserProcess();

        mocker.mock(SecurityStateModelJni.TEST_HOOKS, mSecurityStateMocks);

        Context context =
                new ContextThemeWrapper(ContextUtils.getApplicationContext(), R.style.ColorOverlay);
        // clang-format off
        mLocationBarModel = spy(
                new LocationBarModel(context, NewTabPageDelegate.EMPTY,
                        (url) -> url.getSpec(), (window) -> null, ToolbarTestUtils.OFFLINE_STATUS,
                        mSearchEngineLogoUtils));
        // clang-format on
        Profile.setLastUsedProfileForTesting(mMockProfile);
        TestThreadUtils.runOnUiThreadBlocking(() -> mLocationBarModel.initializeWithNative());

        doReturn(mMockPrefService).when(mLocationBarModel).getPrefService();
        setupLockIconPolicyForTests(false);
    }

    @Test
    @SmallTest
    @UiThreadTest
    public void testGetSecurityLevel() {
        assertEquals(ConnectionSecurityLevel.NONE,
                mLocationBarModel.getSecurityLevel(null, !IS_OFFLINE_PAGE, null));
        assertEquals(ConnectionSecurityLevel.NONE,
                mLocationBarModel.getSecurityLevel(null, IS_OFFLINE_PAGE, null));
        assertEquals(ConnectionSecurityLevel.NONE,
                mLocationBarModel.getSecurityLevel(mTab, IS_OFFLINE_PAGE, null));

        for (int securityLevel : SECURITY_LEVELS) {
            doReturn(securityLevel).when(mLocationBarModel).getSecurityLevelFromStateModel(any());
            assertEquals("Wrong security level returned for " + securityLevel, securityLevel,
                    mLocationBarModel.getSecurityLevel(mTab, !IS_OFFLINE_PAGE, null));
        }

        doReturn(ConnectionSecurityLevel.SECURE)
                .when(mLocationBarModel)
                .getSecurityLevelFromStateModel(any());
        assertEquals("Wrong security level returned for HTTPS publisher URL",
                ConnectionSecurityLevel.SECURE,
                mLocationBarModel.getSecurityLevel(mTab, !IS_OFFLINE_PAGE, "https://example.com"));
        assertEquals("Wrong security level returned for HTTP publisher URL",
                ConnectionSecurityLevel.WARNING,
                mLocationBarModel.getSecurityLevel(mTab, !IS_OFFLINE_PAGE, "http://example.com"));

        doReturn(ConnectionSecurityLevel.DANGEROUS)
                .when(mLocationBarModel)
                .getSecurityLevelFromStateModel(any());
        assertEquals("Wrong security level returned for publisher URL on insecure page",
                ConnectionSecurityLevel.DANGEROUS,
                mLocationBarModel.getSecurityLevel(mTab, !IS_OFFLINE_PAGE, null));
    }

    @Test
    @SmallTest
    @UiThreadTest
    @Feature({"Omnibox"})
    public void testGetSecurityIconResource() {
        for (int securityLevel : SECURITY_LEVELS) {
            assertEquals("Wrong phone resource for security level " + securityLevel,
                    R.drawable.ic_offline_pin_24dp,
                    mLocationBarModel.getSecurityIconResource(
                            securityLevel, IS_SMALL_DEVICE, IS_OFFLINE_PAGE, !IS_PAINT_PREVIEW));
            assertEquals("Wrong tablet resource for security level " + securityLevel,
                    R.drawable.ic_offline_pin_24dp,
                    mLocationBarModel.getSecurityIconResource(
                            securityLevel, !IS_SMALL_DEVICE, IS_OFFLINE_PAGE, !IS_PAINT_PREVIEW));

            assertEquals("Wrong phone resource for security level " + securityLevel,
                    R.drawable.omnibox_info,
                    mLocationBarModel.getSecurityIconResource(
                            securityLevel, IS_SMALL_DEVICE, IS_OFFLINE_PAGE, IS_PAINT_PREVIEW));
            assertEquals("Wrong tablet resource for security level " + securityLevel,
                    R.drawable.omnibox_info,
                    mLocationBarModel.getSecurityIconResource(
                            securityLevel, !IS_SMALL_DEVICE, IS_OFFLINE_PAGE, IS_PAINT_PREVIEW));
        }

        assertEquals(0,
                mLocationBarModel.getSecurityIconResource(ConnectionSecurityLevel.NONE,
                        IS_SMALL_DEVICE, !IS_OFFLINE_PAGE, !IS_PAINT_PREVIEW));
        assertEquals(R.drawable.omnibox_info,
                mLocationBarModel.getSecurityIconResource(ConnectionSecurityLevel.NONE,
                        !IS_SMALL_DEVICE, !IS_OFFLINE_PAGE, !IS_PAINT_PREVIEW));

        assertEquals(R.drawable.omnibox_not_secure_warning,
                mLocationBarModel.getSecurityIconResource(ConnectionSecurityLevel.WARNING,
                        IS_SMALL_DEVICE, !IS_OFFLINE_PAGE, !IS_PAINT_PREVIEW));
        assertEquals(R.drawable.omnibox_not_secure_warning,
                mLocationBarModel.getSecurityIconResource(ConnectionSecurityLevel.WARNING,
                        !IS_SMALL_DEVICE, !IS_OFFLINE_PAGE, !IS_PAINT_PREVIEW));

        assertEquals(R.drawable.omnibox_not_secure_warning,
                mLocationBarModel.getSecurityIconResource(ConnectionSecurityLevel.DANGEROUS,
                        IS_SMALL_DEVICE, !IS_OFFLINE_PAGE, !IS_PAINT_PREVIEW));
        assertEquals(R.drawable.omnibox_not_secure_warning,
                mLocationBarModel.getSecurityIconResource(ConnectionSecurityLevel.DANGEROUS,
                        !IS_SMALL_DEVICE, !IS_OFFLINE_PAGE, !IS_PAINT_PREVIEW));

        assertEquals(R.drawable.omnibox_https_valid,
                mLocationBarModel.getSecurityIconResource(
                        ConnectionSecurityLevel.SECURE_WITH_POLICY_INSTALLED_CERT, IS_SMALL_DEVICE,
                        !IS_OFFLINE_PAGE, !IS_PAINT_PREVIEW));
        assertEquals(R.drawable.omnibox_https_valid,
                mLocationBarModel.getSecurityIconResource(
                        ConnectionSecurityLevel.SECURE_WITH_POLICY_INSTALLED_CERT, !IS_SMALL_DEVICE,
                        !IS_OFFLINE_PAGE, !IS_PAINT_PREVIEW));

        assertEquals(R.drawable.omnibox_https_valid,
                mLocationBarModel.getSecurityIconResource(ConnectionSecurityLevel.SECURE,
                        IS_SMALL_DEVICE, !IS_OFFLINE_PAGE, !IS_PAINT_PREVIEW));
        assertEquals(R.drawable.omnibox_https_valid,
                mLocationBarModel.getSecurityIconResource(ConnectionSecurityLevel.SECURE,
                        !IS_SMALL_DEVICE, !IS_OFFLINE_PAGE, !IS_PAINT_PREVIEW));
    }

    @Test
    @SmallTest
    @UiThreadTest
    @Feature({"Omnibox"})
    @Features.EnableFeatures(ChromeFeatureList.OMNIBOX_UPDATED_CONNECTION_SECURITY_INDICATORS)
    public void testLockIconPolicyDisabled() {
        setupLockIconPolicyForTests(false);

        assertEquals(R.drawable.omnibox_https_valid_arrow,
                mLocationBarModel.getSecurityIconResource(ConnectionSecurityLevel.SECURE,
                        IS_SMALL_DEVICE, !IS_OFFLINE_PAGE, !IS_PAINT_PREVIEW));
        assertEquals(R.drawable.omnibox_https_valid_arrow,
                mLocationBarModel.getSecurityIconResource(ConnectionSecurityLevel.SECURE,
                        !IS_SMALL_DEVICE, !IS_OFFLINE_PAGE, !IS_PAINT_PREVIEW));
    }

    @Test
    @SmallTest
    @UiThreadTest
    @Feature({"Omnibox"})
    @Features.EnableFeatures(ChromeFeatureList.OMNIBOX_UPDATED_CONNECTION_SECURITY_INDICATORS)
    public void testLockIconPolicyEnabled() {
        setupLockIconPolicyForTests(true);

        // When the policy is enabled, omnibox should keep showing the lock icon.
        assertEquals(R.drawable.omnibox_https_valid,
                mLocationBarModel.getSecurityIconResource(ConnectionSecurityLevel.SECURE,
                        IS_SMALL_DEVICE, !IS_OFFLINE_PAGE, !IS_PAINT_PREVIEW));
        assertEquals(R.drawable.omnibox_https_valid,
                mLocationBarModel.getSecurityIconResource(ConnectionSecurityLevel.SECURE,
                        !IS_SMALL_DEVICE, !IS_OFFLINE_PAGE, !IS_PAINT_PREVIEW));
    }
}
