// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp_customization.theme;

import static org.junit.Assert.assertNotNull;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import static org.chromium.chrome.browser.ntp_customization.NtpCustomizationCoordinator.BottomSheetType.THEME;

import android.content.Context;
import android.view.ContextThemeWrapper;

import androidx.test.core.app.ApplicationProvider;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.ntp_customization.BottomSheetDelegate;
import org.chromium.chrome.browser.ntp_customization.NtpCustomizationConfigManager;
import org.chromium.chrome.browser.ntp_customization.NtpCustomizationCoordinator.BottomSheetType;
import org.chromium.chrome.browser.ntp_customization.NtpCustomizationUtils.NtpBackgroundImageType;
import org.chromium.chrome.browser.ntp_customization.R;
import org.chromium.chrome.browser.ntp_customization.theme.theme_collections.CustomBackgroundInfo;
import org.chromium.chrome.browser.ntp_customization.theme.theme_collections.NtpThemeCollectionBridge;
import org.chromium.chrome.browser.ntp_customization.theme.theme_collections.NtpThemeCollectionBridgeJni;
import org.chromium.chrome.browser.ntp_customization.theme.theme_collections.NtpThemeCollectionManager;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.url.GURL;

/** Unit tests for {@link NtpThemeCoordinator}. */
@RunWith(BaseRobolectricTestRunner.class)
public class NtpThemeCoordinatorUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private BottomSheetDelegate mBottomSheetDelegate;
    @Mock private Profile mProfile;
    @Mock private Runnable mDismissBottomSheet;
    @Mock private NtpThemeCollectionBridge.Natives mNtpThemeCollectionBridgeJniMock;
    @Mock private NtpCustomizationConfigManager mNtpCustomizationConfigManager;
    @Mock private NtpThemeBottomSheetView mNtpThemeBottomSheetView;

    private Context mContext;
    private NtpThemeCoordinator mCoordinator;
    private NtpThemeMediator mMediator;

    @Before
    public void setUp() {
        mContext =
                new ContextThemeWrapper(
                        ApplicationProvider.getApplicationContext(),
                        R.style.Theme_BrowserUI_DayNight);
        NtpThemeCollectionBridgeJni.setInstanceForTesting(mNtpThemeCollectionBridgeJniMock);
        when(mNtpThemeCollectionBridgeJniMock.init(any(), any())).thenReturn(1L);
        NtpCustomizationConfigManager.setInstanceForTesting(mNtpCustomizationConfigManager);

        mCoordinator =
                new NtpThemeCoordinator(
                        mContext, mBottomSheetDelegate, mProfile, mDismissBottomSheet);

        mMediator = mCoordinator.getMediatorForTesting();
        mCoordinator.setMediatorForTesting(mMediator);
        mCoordinator.setNtpThemeBottomSheetViewForTesting(mNtpThemeBottomSheetView);
    }

    @Test
    public void testConstructor() {
        assertNotNull(mCoordinator.getMediatorForTesting());
    }

    @Test
    public void testRegisterBottomSheetLayout() {
        verify(mBottomSheetDelegate).registerBottomSheetLayout(eq(THEME), any());
    }

    @Test
    public void testDestroy() {
        mCoordinator.destroy();

        verify(mNtpThemeBottomSheetView).destroy();
    }

    @Test
    public void testOnPreviewClosed() {
        boolean isImageSelected = false;
        mCoordinator.onPreviewClosed(isImageSelected);

        verify(mBottomSheetDelegate, never()).onNewColorSelected(anyBoolean());
        verify(mDismissBottomSheet).run();

        isImageSelected = true;
        mCoordinator.onPreviewClosed(isImageSelected);

        verify(mBottomSheetDelegate).onNewColorSelected(eq(true));
        verify(mDismissBottomSheet, times(2)).run();
    }

    @Test
    public void testOnChromeColorsClicked() {
        mCoordinator.getNtpThemeDelegateForTesting().onChromeColorsClicked();
        verify(mBottomSheetDelegate).showBottomSheet(eq(BottomSheetType.CHROME_COLORS));
    }

    @Test
    public void onThemeCollectionsClicked() {
        mCoordinator.getNtpThemeDelegateForTesting().onThemeCollectionsClicked();
        verify(mBottomSheetDelegate).showBottomSheet(eq(BottomSheetType.THEME_COLLECTIONS));
    }

    @Test
    public void testOnThemeImageSelectedCallback() {
        mMediator = spy(mCoordinator.getMediatorForTesting());
        mCoordinator.setMediatorForTesting(mMediator);
        NtpThemeCollectionManager ntpThemeCollectionManager =
                mCoordinator.getNtpThemeManagerForTesting();

        ntpThemeCollectionManager.onCustomBackgroundImageUpdated(
                new CustomBackgroundInfo(new GURL("http://test.com"), "collection", false, false));

        verify(mBottomSheetDelegate).onNewColorSelected(eq(true));
        verify(mMediator)
                .updateTrailingIconVisibilityForSectionType(
                        NtpBackgroundImageType.THEME_COLLECTION);
    }
}
