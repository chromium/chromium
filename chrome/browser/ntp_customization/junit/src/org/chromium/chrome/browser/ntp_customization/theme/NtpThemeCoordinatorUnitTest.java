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
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import static org.chromium.chrome.browser.ntp_customization.NtpCustomizationCoordinator.BottomSheetType.THEME;
import static org.chromium.chrome.browser.ntp_customization.NtpCustomizationUtils.NtpBackgroundType.IMAGE_FROM_DISK;

import android.content.Context;
import android.graphics.Bitmap;
import android.view.ContextThemeWrapper;

import androidx.test.core.app.ApplicationProvider;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.Callback;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.ntp_customization.BottomSheetDelegate;
import org.chromium.chrome.browser.ntp_customization.NtpCustomizationConfigManager;
import org.chromium.chrome.browser.ntp_customization.NtpCustomizationCoordinator.BottomSheetType;
import org.chromium.chrome.browser.ntp_customization.NtpCustomizationUtils;
import org.chromium.chrome.browser.ntp_customization.NtpCustomizationUtils.NtpBackgroundType;
import org.chromium.chrome.browser.ntp_customization.R;
import org.chromium.chrome.browser.ntp_customization.theme.theme_collections.BackgroundCollection;
import org.chromium.chrome.browser.ntp_customization.theme.theme_collections.CollectionImage;
import org.chromium.chrome.browser.ntp_customization.theme.theme_collections.CustomBackgroundInfo;
import org.chromium.chrome.browser.ntp_customization.theme.theme_collections.NtpThemeCollectionBridge;
import org.chromium.chrome.browser.ntp_customization.theme.theme_collections.NtpThemeCollectionBridgeJni;
import org.chromium.chrome.browser.ntp_customization.theme.theme_collections.NtpThemeCollectionManager;
import org.chromium.chrome.browser.ntp_customization.theme.theme_collections.NtpThemeCollectionsCoordinator;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.image_fetcher.ImageFetcher;
import org.chromium.url.GURL;

import java.util.ArrayList;
import java.util.List;

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
    @Mock private Runnable mResetCustomizedThemeRunnable;
    @Mock private NtpThemeCollectionsCoordinator mNtpThemeCollectionsCoordinator;
    @Mock private ImageFetcher mImageFetcher;
    @Captor private ArgumentCaptor<Callback<Bitmap>> mBitmapCallbackCaptor;

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
        NtpCustomizationUtils.setImageFetcherForTesting(mImageFetcher);

        mCoordinator =
                new NtpThemeCoordinator(
                        mContext, mBottomSheetDelegate, mProfile, mDismissBottomSheet);

        mMediator = spy(mCoordinator.getMediatorForTesting());
        mCoordinator.setMediatorForTesting(mMediator);
        mCoordinator.setNtpThemeBottomSheetViewForTesting(mNtpThemeBottomSheetView);
        mCoordinator.setNtpThemeCollectionsCoordinatorForTesting(mNtpThemeCollectionsCoordinator);
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
        verify(mDismissBottomSheet, never()).run();
        verify(mMediator, never()).updateTrailingIconVisibilityForSectionType(eq(IMAGE_FROM_DISK));

        isImageSelected = true;
        mCoordinator.onPreviewClosed(isImageSelected);

        verify(mBottomSheetDelegate).onNewColorSelected(eq(true));
        verify(mDismissBottomSheet).run();
        verify(mMediator).updateTrailingIconVisibilityForSectionType(eq(IMAGE_FROM_DISK));
    }

    @Test
    public void testOnChromeColorsClicked() {
        mCoordinator.getNtpThemeDelegateForTesting().onChromeColorsClicked();
        verify(mBottomSheetDelegate).showBottomSheet(eq(BottomSheetType.CHROME_COLORS));
    }

    @Test
    public void testOnThemeCollectionsClicked() {
        List<BackgroundCollection> collections = new ArrayList<>();
        mCoordinator
                .getNtpThemeDelegateForTesting()
                .onThemeCollectionsClicked(mResetCustomizedThemeRunnable, collections);
        verify(mBottomSheetDelegate).showBottomSheet(eq(BottomSheetType.THEME_COLLECTIONS));
    }

    @Test
    public void testOnThemeImageSelectedCallback() {
        NtpThemeCollectionManager ntpThemeCollectionManager =
                mCoordinator.getNtpThemeManagerForTesting();
        GURL url = new GURL("http://test.com");
        CollectionImage image = new CollectionImage("collection", url, url, new ArrayList<>(), url);
        ntpThemeCollectionManager.setThemeCollectionImage(image);

        ntpThemeCollectionManager.onCustomBackgroundImageUpdated(
                new CustomBackgroundInfo(url, "collection", false, false));
        verify(mImageFetcher).fetchImage(any(), mBitmapCallbackCaptor.capture());
        Bitmap bitmap = Bitmap.createBitmap(1, 1, Bitmap.Config.ARGB_8888);
        mBitmapCallbackCaptor.getValue().onResult(bitmap);

        verify(mNtpThemeCollectionsCoordinator)
                .initializeBottomSheetContent(eq(BottomSheetType.SINGLE_THEME_COLLECTION));
        verify(mNtpThemeCollectionsCoordinator)
                .initializeBottomSheetContent(eq(BottomSheetType.THEME_COLLECTIONS));
        verify(mBottomSheetDelegate).onNewColorSelected(eq(true));
        verify(mBottomSheetDelegate).onNewThemeCollectionImageSelected(eq(bitmap));
        verify(mMediator)
                .updateTrailingIconVisibilityForSectionType(NtpBackgroundType.THEME_COLLECTION);
    }

    @Test
    public void testInitializeBottomSheetContent() {
        mCoordinator.initializeBottomSheetContent(BottomSheetType.THEME_COLLECTIONS);
        verify(mNtpThemeCollectionsCoordinator)
                .initializeBottomSheetContent(BottomSheetType.THEME_COLLECTIONS);
    }
}
