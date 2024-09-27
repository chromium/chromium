// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.chrome.browser.toolbar;

import static org.junit.Assert.assertEquals;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.spy;

import android.content.Context;
import android.view.ContextThemeWrapper;

import androidx.annotation.ColorRes;
import androidx.test.annotation.UiThreadTest;
import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.MockitoAnnotations;

import org.chromium.base.ContextUtils;
import org.chromium.base.ThreadUtils;
import org.chromium.base.UserDataHost;
import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.omnibox.ChromeAutocompleteSchemeClassifier;
import org.chromium.chrome.browser.omnibox.ChromeAutocompleteSchemeClassifierJni;
import org.chromium.chrome.browser.omnibox.NewTabPageDelegate;
import org.chromium.chrome.browser.pdf.PdfUtils.PdfPageType;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileManager;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TrustedCdn;
import org.chromium.chrome.browser.theme.ThemeUtils;
import org.chromium.chrome.browser.ui.theme.BrandedColorScheme;
import org.chromium.chrome.test.util.ToolbarUnitTestUtils;
import org.chromium.components.security_state.ConnectionSecurityLevel;
import org.chromium.components.security_state.SecurityStateModel;
import org.chromium.components.security_state.SecurityStateModelJni;
import org.chromium.content_public.browser.test.NativeLibraryTestUtils;
import org.chromium.url.GURL;
import org.chromium.url.JUnitTestGURLs;

import java.util.Random;
import java.util.concurrent.ExecutionException;

/** Instrumentation tests for the toolbar security icon. */
@RunWith(BaseJUnit4ClassRunner.class)
@Batch(Batch.UNIT_TESTS)
@DisableFeatures({ChromeFeatureList.OMNIBOX_UPDATED_CONNECTION_SECURITY_INDICATORS})
public final class ToolbarSecurityIconTest {
    private static final boolean IS_SMALL_DEVICE = true;
    private static final boolean IS_OFFLINE_PAGE = true;
    private static final boolean IS_PAINT_PREVIEW = true;
    private static final int[] SECURITY_LEVELS =
            new int[] {
                ConnectionSecurityLevel.NONE,
                ConnectionSecurityLevel.WARNING,
                ConnectionSecurityLevel.DANGEROUS,
                ConnectionSecurityLevel.SECURE
            };

    @Rule public JniMocker mocker = new JniMocker();

    @Mock private Tab mTab;

    @Mock SecurityStateModel.Natives mSecurityStateMocks;

    @Mock private LocationBarModel mLocationBarModel;

    @Mock private LocationBarModel.Natives mLocationBarModelJni;

    @Mock private ChromeAutocompleteSchemeClassifier.Natives mChromeAutocompleteSchemeClassifierJni;

    @Mock private Profile mMockProfile;

    @Mock private TrustedCdn mTrustedCdn;

    @Before
    public void setUp() throws ExecutionException {
        MockitoAnnotations.initMocks(this);

        NativeLibraryTestUtils.loadNativeLibraryNoBrowserProcess();
        mocker.mock(SecurityStateModelJni.TEST_HOOKS, mSecurityStateMocks);
        mocker.mock(
                ChromeAutocompleteSchemeClassifierJni.TEST_HOOKS,
                mChromeAutocompleteSchemeClassifierJni);
        mocker.mock(
                org.chromium.chrome.browser.toolbar.LocationBarModelJni.TEST_HOOKS,
                mLocationBarModelJni);

        GURL exampleUrl = JUnitTestGURLs.EXAMPLE_URL;
        doReturn(exampleUrl)
                .when(mLocationBarModelJni)
                .getUrlOfVisibleNavigationEntry(Mockito.anyLong(), Mockito.any());
        doReturn(exampleUrl.getSpec())
                .when(mLocationBarModelJni)
                .getFormattedFullURL(Mockito.anyLong(), Mockito.any());
        doReturn(exampleUrl.getSpec())
                .when(mLocationBarModelJni)
                .getURLForDisplay(Mockito.anyLong(), Mockito.any());
        doReturn(new Random().nextLong()).when(mLocationBarModelJni).init(Mockito.any());

        Context context =
                new ContextThemeWrapper(
                        ContextUtils.getApplicationContext(), R.style.Theme_BrowserUI_DayNight);
        mLocationBarModel =
                spy(
                        new LocationBarModel(
                                context,
                                NewTabPageDelegate.EMPTY,
                                (url) -> url.getSpec(),
                                ToolbarUnitTestUtils.OFFLINE_STATUS));
        ProfileManager.setLastUsedProfileForTesting(mMockProfile);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mLocationBarModel.initializeWithNative();
                    UserDataHost userDataHost = new UserDataHost();
                    userDataHost.setUserData(TrustedCdn.USER_DATA_KEY, mTrustedCdn);
                    doReturn(userDataHost).when(mTab).getUserDataHost();
                });
    }

    @After
    public void tearDown() throws ExecutionException {
        mLocationBarModel.destroy();
    }

    @Test
    @SmallTest
    @UiThreadTest
    public void testGetSecurityLevel() {
        assertEquals(
                ConnectionSecurityLevel.NONE,
                mLocationBarModel.getSecurityLevel(null, !IS_OFFLINE_PAGE));
        assertEquals(
                ConnectionSecurityLevel.NONE,
                mLocationBarModel.getSecurityLevel(null, IS_OFFLINE_PAGE));
        assertEquals(
                ConnectionSecurityLevel.NONE,
                mLocationBarModel.getSecurityLevel(mTab, IS_OFFLINE_PAGE));

        for (int securityLevel : SECURITY_LEVELS) {
            doReturn(securityLevel).when(mLocationBarModel).getSecurityLevelFromStateModel(any());
            assertEquals(
                    "Wrong security level returned for " + securityLevel,
                    securityLevel,
                    mLocationBarModel.getSecurityLevel(mTab, !IS_OFFLINE_PAGE));
        }

        doReturn(ConnectionSecurityLevel.SECURE)
                .when(mLocationBarModel)
                .getSecurityLevelFromStateModel(any());
        doReturn(new GURL("https://example.com")).when(mTrustedCdn).getPublisherUrl();
        assertEquals(
                "Wrong security level returned for HTTPS publisher URL",
                ConnectionSecurityLevel.SECURE,
                mLocationBarModel.getSecurityLevel(mTab, !IS_OFFLINE_PAGE));
        doReturn(new GURL("http://example.com")).when(mTrustedCdn).getPublisherUrl();
        assertEquals(
                "Wrong security level returned for HTTP publisher URL",
                ConnectionSecurityLevel.WARNING,
                mLocationBarModel.getSecurityLevel(mTab, !IS_OFFLINE_PAGE));

        doReturn(ConnectionSecurityLevel.DANGEROUS)
                .when(mLocationBarModel)
                .getSecurityLevelFromStateModel(any());
        doReturn(null).when(mTrustedCdn).getPublisherUrl();
        assertEquals(
                "Wrong security level returned for publisher URL on insecure page",
                ConnectionSecurityLevel.DANGEROUS,
                mLocationBarModel.getSecurityLevel(mTab, !IS_OFFLINE_PAGE));
    }

    @Test
    @SmallTest
    @UiThreadTest
    @Feature({"Omnibox"})
    public void testGetSecurityIconResource() {
        for (int securityLevel : SECURITY_LEVELS) {
            assertEquals(
                    "Wrong phone resource for security level " + securityLevel,
                    R.drawable.ic_offline_pin_24dp,
                    mLocationBarModel.getSecurityIconResource(
                            securityLevel,
                            IS_SMALL_DEVICE,
                            IS_OFFLINE_PAGE,
                            !IS_PAINT_PREVIEW,
                            PdfPageType.NONE));
            assertEquals(
                    "Wrong tablet resource for security level " + securityLevel,
                    R.drawable.ic_offline_pin_24dp,
                    mLocationBarModel.getSecurityIconResource(
                            securityLevel,
                            !IS_SMALL_DEVICE,
                            IS_OFFLINE_PAGE,
                            !IS_PAINT_PREVIEW,
                            PdfPageType.NONE));

            assertEquals(
                    "Wrong phone resource for security level " + securityLevel,
                    R.drawable.omnibox_info,
                    mLocationBarModel.getSecurityIconResource(
                            securityLevel,
                            IS_SMALL_DEVICE,
                            IS_OFFLINE_PAGE,
                            IS_PAINT_PREVIEW,
                            PdfPageType.NONE));
            assertEquals(
                    "Wrong tablet resource for security level " + securityLevel,
                    R.drawable.omnibox_info,
                    mLocationBarModel.getSecurityIconResource(
                            securityLevel,
                            !IS_SMALL_DEVICE,
                            IS_OFFLINE_PAGE,
                            IS_PAINT_PREVIEW,
                            PdfPageType.NONE));

            assertEquals(
                    "Wrong phone resource for security level " + securityLevel,
                    R.drawable.omnibox_info,
                    mLocationBarModel.getSecurityIconResource(
                            securityLevel,
                            IS_SMALL_DEVICE,
                            !IS_OFFLINE_PAGE,
                            !IS_PAINT_PREVIEW,
                            PdfPageType.TRANSIENT_SECURE));
            assertEquals(
                    "Wrong tablet resource for security level " + securityLevel,
                    R.drawable.omnibox_info,
                    mLocationBarModel.getSecurityIconResource(
                            securityLevel,
                            !IS_SMALL_DEVICE,
                            !IS_OFFLINE_PAGE,
                            !IS_PAINT_PREVIEW,
                            PdfPageType.TRANSIENT_SECURE));

            assertEquals(
                    "Wrong phone resource for security level " + securityLevel,
                    R.drawable.omnibox_not_secure_warning,
                    mLocationBarModel.getSecurityIconResource(
                            securityLevel,
                            IS_SMALL_DEVICE,
                            !IS_OFFLINE_PAGE,
                            !IS_PAINT_PREVIEW,
                            PdfPageType.TRANSIENT_INSECURE));
            assertEquals(
                    "Wrong tablet resource for security level " + securityLevel,
                    R.drawable.omnibox_not_secure_warning,
                    mLocationBarModel.getSecurityIconResource(
                            securityLevel,
                            !IS_SMALL_DEVICE,
                            !IS_OFFLINE_PAGE,
                            !IS_PAINT_PREVIEW,
                            PdfPageType.TRANSIENT_INSECURE));

            assertEquals(
                    "Wrong phone resource for security level " + securityLevel,
                    R.drawable.omnibox_info,
                    mLocationBarModel.getSecurityIconResource(
                            securityLevel,
                            IS_SMALL_DEVICE,
                            !IS_OFFLINE_PAGE,
                            !IS_PAINT_PREVIEW,
                            PdfPageType.LOCAL));
            assertEquals(
                    "Wrong tablet resource for security level " + securityLevel,
                    R.drawable.omnibox_info,
                    mLocationBarModel.getSecurityIconResource(
                            securityLevel,
                            !IS_SMALL_DEVICE,
                            !IS_OFFLINE_PAGE,
                            !IS_PAINT_PREVIEW,
                            PdfPageType.LOCAL));
        }

        assertEquals(
                R.drawable.omnibox_info,
                mLocationBarModel.getSecurityIconResource(
                        ConnectionSecurityLevel.NONE,
                        IS_SMALL_DEVICE,
                        !IS_OFFLINE_PAGE,
                        !IS_PAINT_PREVIEW,
                        PdfPageType.NONE));
        assertEquals(
                R.drawable.omnibox_info,
                mLocationBarModel.getSecurityIconResource(
                        ConnectionSecurityLevel.NONE,
                        !IS_SMALL_DEVICE,
                        !IS_OFFLINE_PAGE,
                        !IS_PAINT_PREVIEW,
                        PdfPageType.NONE));

        assertEquals(
                R.drawable.omnibox_not_secure_warning,
                mLocationBarModel.getSecurityIconResource(
                        ConnectionSecurityLevel.WARNING,
                        IS_SMALL_DEVICE,
                        !IS_OFFLINE_PAGE,
                        !IS_PAINT_PREVIEW,
                        PdfPageType.NONE));
        assertEquals(
                R.drawable.omnibox_not_secure_warning,
                mLocationBarModel.getSecurityIconResource(
                        ConnectionSecurityLevel.WARNING,
                        !IS_SMALL_DEVICE,
                        !IS_OFFLINE_PAGE,
                        !IS_PAINT_PREVIEW,
                        PdfPageType.NONE));

        assertEquals(
                R.drawable.omnibox_dangerous,
                mLocationBarModel.getSecurityIconResource(
                        ConnectionSecurityLevel.DANGEROUS,
                        IS_SMALL_DEVICE,
                        !IS_OFFLINE_PAGE,
                        !IS_PAINT_PREVIEW,
                        PdfPageType.NONE));
        assertEquals(
                R.drawable.omnibox_dangerous,
                mLocationBarModel.getSecurityIconResource(
                        ConnectionSecurityLevel.DANGEROUS,
                        !IS_SMALL_DEVICE,
                        !IS_OFFLINE_PAGE,
                        !IS_PAINT_PREVIEW,
                        PdfPageType.NONE));

        assertEquals(
                R.drawable.omnibox_https_valid,
                mLocationBarModel.getSecurityIconResource(
                        ConnectionSecurityLevel.SECURE_WITH_POLICY_INSTALLED_CERT,
                        IS_SMALL_DEVICE,
                        !IS_OFFLINE_PAGE,
                        !IS_PAINT_PREVIEW,
                        PdfPageType.NONE));
        assertEquals(
                R.drawable.omnibox_https_valid,
                mLocationBarModel.getSecurityIconResource(
                        ConnectionSecurityLevel.SECURE_WITH_POLICY_INSTALLED_CERT,
                        !IS_SMALL_DEVICE,
                        !IS_OFFLINE_PAGE,
                        !IS_PAINT_PREVIEW,
                        PdfPageType.NONE));

        assertEquals(
                R.drawable.omnibox_https_valid,
                mLocationBarModel.getSecurityIconResource(
                        ConnectionSecurityLevel.SECURE,
                        IS_SMALL_DEVICE,
                        !IS_OFFLINE_PAGE,
                        !IS_PAINT_PREVIEW,
                        PdfPageType.NONE));
        assertEquals(
                R.drawable.omnibox_https_valid,
                mLocationBarModel.getSecurityIconResource(
                        ConnectionSecurityLevel.SECURE,
                        !IS_SMALL_DEVICE,
                        !IS_OFFLINE_PAGE,
                        !IS_PAINT_PREVIEW,
                        PdfPageType.NONE));
    }

    @Test
    @SmallTest
    @UiThreadTest
    public void testGetSecurityIconColorWithSecurityLevel_DangerousWebsite() {
        assertEquals(
                R.color.default_red,
                mLocationBarModel.getSecurityIconColorWithSecurityLevel(
                        /* connectionSecurityLevel= */ ConnectionSecurityLevel.DANGEROUS,
                        /* brandedColorScheme= */ BrandedColorScheme.APP_DEFAULT,
                        /* isIncognito= */ false));
    }

    @Test
    @SmallTest
    @UiThreadTest
    public void testGetSecurityIconColorWithSecurityLevel_DangerousWebsiteWithIncognito() {
        assertEquals(
                R.color.baseline_error_80,
                mLocationBarModel.getSecurityIconColorWithSecurityLevel(
                        /* connectionSecurityLevel= */ ConnectionSecurityLevel.DANGEROUS,
                        /* brandedColorScheme= */ BrandedColorScheme.APP_DEFAULT,
                        /* isIncognito= */ true));
    }

    @Test
    @SmallTest
    @UiThreadTest
    public void testGetSecurityIconColorWithSecurityLevel_NonDangerousWebsite() {
        final @ConnectionSecurityLevel int brandedColorScheme = BrandedColorScheme.APP_DEFAULT;
        final @ColorRes int defaultColorRes =
                ThemeUtils.getThemedToolbarIconTintRes(brandedColorScheme);

        for (int connectionSecurityLevel : SECURITY_LEVELS) {
            if (connectionSecurityLevel != ConnectionSecurityLevel.DANGEROUS) {
                assertEquals(
                        defaultColorRes,
                        mLocationBarModel.getSecurityIconColorWithSecurityLevel(
                                connectionSecurityLevel,
                                brandedColorScheme,
                                /* isIncognito= */ false));
            }
        }
    }

    @Test
    @SmallTest
    @UiThreadTest
    public void testGetSecurityIconColorWithSecurityLevel_BrandedTheme() {
        final @ColorRes int defaultColorResLight =
                ThemeUtils.getThemedToolbarIconTintRes(BrandedColorScheme.LIGHT_BRANDED_THEME);
        final @ColorRes int defaultColorResDark =
                ThemeUtils.getThemedToolbarIconTintRes(BrandedColorScheme.DARK_BRANDED_THEME);

        for (int connectionSecurityLevel : SECURITY_LEVELS) {
            assertEquals(
                    defaultColorResLight,
                    mLocationBarModel.getSecurityIconColorWithSecurityLevel(
                            connectionSecurityLevel,
                            /* brandedColorScheme= */ BrandedColorScheme.LIGHT_BRANDED_THEME,
                            /* isIncognito= */ false));
            assertEquals(
                    defaultColorResDark,
                    mLocationBarModel.getSecurityIconColorWithSecurityLevel(
                            connectionSecurityLevel,
                            /* brandedColorScheme= */ BrandedColorScheme.DARK_BRANDED_THEME,
                            /* isIncognito= */ false));
        }
    }
}
