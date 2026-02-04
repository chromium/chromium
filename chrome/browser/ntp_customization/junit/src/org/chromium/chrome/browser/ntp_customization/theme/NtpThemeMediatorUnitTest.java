// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp_customization.theme;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.clearInvocations;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.reset;
import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import static org.chromium.chrome.browser.ntp_customization.NtpCustomizationUtils.NtpBackgroundType.CHROME_COLOR;
import static org.chromium.chrome.browser.ntp_customization.NtpCustomizationUtils.NtpBackgroundType.COLOR_FROM_HEX;
import static org.chromium.chrome.browser.ntp_customization.NtpCustomizationUtils.NtpBackgroundType.DEFAULT;
import static org.chromium.chrome.browser.ntp_customization.NtpCustomizationUtils.NtpBackgroundType.IMAGE_FROM_DISK;
import static org.chromium.chrome.browser.ntp_customization.NtpCustomizationUtils.NtpBackgroundType.THEME_COLLECTION;
import static org.chromium.chrome.browser.ntp_customization.NtpCustomizationViewProperties.BACK_PRESS_HANDLER;
import static org.chromium.chrome.browser.ntp_customization.theme.NtpThemeProperty.IS_SECTION_SELECTED;
import static org.chromium.chrome.browser.ntp_customization.theme.NtpThemeProperty.LEADING_ICON_FOR_THEME_COLLECTIONS;
import static org.chromium.chrome.browser.ntp_customization.theme.NtpThemeProperty.LEARN_MORE_BUTTON_CLICK_LISTENER;
import static org.chromium.chrome.browser.ntp_customization.theme.NtpThemeProperty.SECTION_ON_CLICK_LISTENER;

import android.content.Context;
import android.graphics.Bitmap;
import android.net.Uri;
import android.util.Pair;
import android.view.ContextThemeWrapper;
import android.view.View;

import androidx.test.core.app.ApplicationProvider;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.Callback;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.chrome.browser.ntp_customization.BottomSheetDelegate;
import org.chromium.chrome.browser.ntp_customization.NtpCustomizationConfigManager;
import org.chromium.chrome.browser.ntp_customization.NtpCustomizationCoordinator.BottomSheetType;
import org.chromium.chrome.browser.ntp_customization.NtpCustomizationUtils;
import org.chromium.chrome.browser.ntp_customization.NtpCustomizationUtils.NtpBackgroundType;
import org.chromium.chrome.browser.ntp_customization.NtpCustomizationViewProperties;
import org.chromium.chrome.browser.ntp_customization.R;
import org.chromium.chrome.browser.ntp_customization.theme.theme_collections.BackgroundCollection;
import org.chromium.chrome.browser.ntp_customization.theme.theme_collections.NtpThemeCollectionManager;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.image_fetcher.ImageFetcher;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.url.GURL;

import java.util.ArrayList;
import java.util.List;

/** Tests for {@link NtpThemeMediator}. */
@RunWith(BaseRobolectricTestRunner.class)
public class NtpThemeMediatorUnitTest {

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private Profile mProfile;
    @Mock private Callback<Bitmap> mOnImageSelectedCallback;
    @Mock private BottomSheetDelegate mBottomSheetDelegate;
    @Mock private View mView;
    @Mock private NtpCustomizationConfigManager mNtpCustomizationConfigManager;
    @Mock private Uri mUri;
    @Mock private NtpThemeDelegate mNtpThemeDelegate;
    @Mock private NtpThemeCollectionManager mNtpThemeCollectionManager;
    @Mock private ImageFetcher mImageFetcher;

    private PropertyModel mBottomSheetPropertyModel;
    private PropertyModel mThemePropertyModel;
    private NtpThemeMediator mMediator;
    private Context mContext;

    @Before
    public void setUp() {
        mContext =
                new ContextThemeWrapper(
                        ApplicationProvider.getApplicationContext(),
                        R.style.Theme_BrowserUI_DayNight);
        when(mView.getContext()).thenReturn(mContext);
        NtpCustomizationUtils.setImageFetcherForTesting(mImageFetcher);

        mBottomSheetPropertyModel =
                new PropertyModel(NtpCustomizationViewProperties.BOTTOM_SHEET_KEYS);
        mThemePropertyModel = spy(new PropertyModel(NtpThemeProperty.THEME_KEYS));
    }

    @Test
    public void testConstructor_shouldShowAlone() {
        createMediator(/* shouldShowAlone= */ true);

        assertNull(mBottomSheetPropertyModel.get(BACK_PRESS_HANDLER));
        assertNotNull(mThemePropertyModel.get(LEARN_MORE_BUTTON_CLICK_LISTENER));
        assertNotNull(mThemePropertyModel.get(SECTION_ON_CLICK_LISTENER));
    }

    @Test
    public void testConstructor_shouldNotShowAlone() {
        createMediator(/* shouldShowAlone= */ false);

        assertNotNull(mBottomSheetPropertyModel.get(BACK_PRESS_HANDLER));
        mBottomSheetPropertyModel.get(BACK_PRESS_HANDLER).onClick(mView);
        verify(mBottomSheetDelegate).backPressOnCurrentBottomSheet();
    }

    @Test
    public void testDestroy() {
        createMediator(/* shouldShowAlone= */ false);

        assertNotNull(mBottomSheetPropertyModel.get(BACK_PRESS_HANDLER));
        assertNotNull(mThemePropertyModel.get(LEARN_MORE_BUTTON_CLICK_LISTENER));

        mMediator.destroy();

        assertNull(mBottomSheetPropertyModel.get(BACK_PRESS_HANDLER));
        assertNull(mThemePropertyModel.get(LEARN_MORE_BUTTON_CLICK_LISTENER));
        verify(mThemePropertyModel)
                .set(eq(SECTION_ON_CLICK_LISTENER), eq(new Pair<>(DEFAULT, null)));
        verify(mThemePropertyModel)
                .set(eq(SECTION_ON_CLICK_LISTENER), eq(new Pair<>(IMAGE_FROM_DISK, null)));
        verify(mThemePropertyModel)
                .set(eq(SECTION_ON_CLICK_LISTENER), eq(new Pair<>(CHROME_COLOR, null)));
        verify(mThemePropertyModel)
                .set(eq(SECTION_ON_CLICK_LISTENER), eq(new Pair<>(THEME_COLLECTION, null)));
    }

    @Test
    public void testHandleChromeDefaultSectionClick() {
        createMediator(/* shouldShowAlone= */ true);

        String histogramName = "NewTabPage.Customization.BottomSheet.Shown";
        HistogramWatcher histogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        histogramName, BottomSheetType.CHROME_DEFAULT);

        mMediator.handleChromeDefaultSectionClick(mView);
        verify(mNtpCustomizationConfigManager).onBackgroundReset();
        verify(mNtpThemeCollectionManager).resetCustomBackground();
        histogramWatcher.assertExpected();
    }

    @Test
    public void testHandleThemeCollectionsSectionClick() {
        createMediator(/* shouldShowAlone= */ true);
        List<BackgroundCollection> collections = new ArrayList<>();
        doAnswer(
                        invocation -> {
                            Callback<List<BackgroundCollection>> callback =
                                    invocation.getArgument(0);
                            callback.onResult(collections);
                            return null;
                        })
                .when(mNtpThemeCollectionManager)
                .getBackgroundCollections(any(Callback.class));

        mMediator.handleThemeCollectionsSectionClick(mView);

        verify(mNtpThemeDelegate).onThemeCollectionsClicked(any(Runnable.class), eq(collections));
    }

    @Test
    public void testHandleChromeColorsSectionClick() {
        createMediator(/* shouldShowAlone= */ true);

        mMediator.handleChromeColorsSectionClick(mView);

        verify(mNtpThemeDelegate).onChromeColorsClicked();
    }

    @Test
    public void testSetLeadingIconFromBitmaps() {
        createMediator(true);
        Bitmap bitmap1 = Bitmap.createBitmap(1, 1, Bitmap.Config.ARGB_8888);
        Bitmap bitmap2 = Bitmap.createBitmap(1, 1, Bitmap.Config.ARGB_8888);
        mMediator.setLeadingIconFromBitmaps(bitmap1, bitmap2);
        verify(mThemePropertyModel).set(eq(LEADING_ICON_FOR_THEME_COLLECTIONS), any(Pair.class));
    }

    @Test
    public void testIconVisibilityAfterClickingDefault() {
        createMediator(true);
        // Reset calls from constructor to have a clean slate.
        reset(mThemePropertyModel);

        mMediator.handleChromeDefaultSectionClick(mView);

        verify(mThemePropertyModel).set(eq(IS_SECTION_SELECTED), eq(new Pair<>(DEFAULT, true)));
        verify(mThemePropertyModel)
                .set(eq(IS_SECTION_SELECTED), eq(new Pair<>(IMAGE_FROM_DISK, false)));
        verify(mThemePropertyModel)
                .set(eq(IS_SECTION_SELECTED), eq(new Pair<>(CHROME_COLOR, false)));
        verify(mThemePropertyModel)
                .set(eq(IS_SECTION_SELECTED), eq(new Pair<>(THEME_COLLECTION, false)));
    }

    @Test
    public void testIconVisibilityUnaffectedByIndirectActions() {
        createMediator(true);
        // Reset calls from constructor.
        reset(mThemePropertyModel);

        mMediator.handleChromeColorsSectionClick(mView);
        mMediator.handleThemeCollectionsSectionClick(mView);

        // Verify no visibility changes happened directly.
        verify(mThemePropertyModel, never()).set(eq(IS_SECTION_SELECTED), any());
    }

    @Test
    public void testInitTrailingIcon() {
        NtpCustomizationUtils.setNtpBackgroundTypeToSharedPreference(CHROME_COLOR);
        createMediator(true);
        verify(mThemePropertyModel)
                .set(eq(IS_SECTION_SELECTED), eq(new Pair<>(CHROME_COLOR, true)));
        NtpCustomizationUtils.resetSharedPreferenceForTesting();
    }

    @Test
    public void testHandleSectionClick_onNewColorSelected() {
        createMediator(/* shouldShowAlone= */ true);
        when(mNtpCustomizationConfigManager.getBackgroundType())
                .thenReturn(NtpBackgroundType.DEFAULT);

        // Verifies the case of background type from default to default.
        mMediator.handleChromeDefaultSectionClick(mView);
        verify(mBottomSheetDelegate, never()).onNewColorSelected(anyBoolean());

        // Verifies the case of background type from upload-image to default.
        when(mNtpCustomizationConfigManager.getBackgroundType())
                .thenReturn(NtpBackgroundType.IMAGE_FROM_DISK);
        mMediator.handleChromeDefaultSectionClick(mView);
        verify(mBottomSheetDelegate).onNewColorSelected(eq(true));

        // Verifies the case of background type from chrome-color to default.
        when(mNtpCustomizationConfigManager.getBackgroundType())
                .thenReturn(NtpBackgroundType.CHROME_COLOR);
        mMediator.handleChromeDefaultSectionClick(mView);
        verify(mBottomSheetDelegate, times(2)).onNewColorSelected(eq(true));
    }

    @Test
    public void testOnUploadImageResult_nullUri() {
        createMediator(true);
        String histogramName = "NewTabPage.Customization.BottomSheet.Shown";
        HistogramWatcher histogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        histogramName, BottomSheetType.UPLOAD_IMAGE);

        mMediator.onUploadImageResult(null);
        histogramWatcher.assertExpected();
        verify(mNtpThemeCollectionManager, never()).selectLocalBackgroundImage();
    }

    @Test
    public void testOnUploadImageResult_validUri() {
        createMediator(true);
        String histogramName = "NewTabPage.Customization.BottomSheet.Shown";
        HistogramWatcher histogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        histogramName, BottomSheetType.UPLOAD_IMAGE);

        mMediator.onUploadImageResult(mUri);
        histogramWatcher.assertExpected();
        verify(mNtpThemeCollectionManager).selectLocalBackgroundImage();
    }

    private void createMediator(boolean shouldShowAlone) {
        when(mBottomSheetDelegate.shouldShowAlone()).thenReturn(shouldShowAlone);
        mMediator =
                new NtpThemeMediator(
                        mContext,
                        mProfile,
                        mBottomSheetPropertyModel,
                        mThemePropertyModel,
                        mBottomSheetDelegate,
                        mNtpCustomizationConfigManager,
                        /* activityResultRegistry= */ null,
                        mOnImageSelectedCallback,
                        mNtpThemeDelegate,
                        mNtpThemeCollectionManager);
    }

    @Test
    public void testUpdateForChoosingDefaultOrChromeColorOption() {
        createMediator(true);
        reset(mThemePropertyModel);

        mMediator.updateForChoosingDefaultOrChromeColorOption(CHROME_COLOR);

        verify(mThemePropertyModel)
                .set(eq(IS_SECTION_SELECTED), eq(new Pair<>(CHROME_COLOR, true)));
        verify(mThemePropertyModel).set(eq(IS_SECTION_SELECTED), eq(new Pair<>(DEFAULT, false)));
        verify(mThemePropertyModel)
                .set(eq(IS_SECTION_SELECTED), eq(new Pair<>(IMAGE_FROM_DISK, false)));
        verify(mThemePropertyModel)
                .set(eq(IS_SECTION_SELECTED), eq(new Pair<>(THEME_COLLECTION, false)));

        verify(mNtpThemeCollectionManager).resetCustomBackground();
    }

    @Test
    public void testFetchImageOrRunCallback_withUrl() {
        createMediator(true);
        Callback<Bitmap> callback = mock(Callback.class);
        GURL url = new GURL("http://test.com");
        mMediator.fetchImageOrRunCallback(mImageFetcher, url, callback);
        verify(mImageFetcher).fetchImage(any(), eq(callback));
    }

    @Test
    public void testCreateBitmapCallback() {
        createMediator(true);
        Bitmap[] bitmaps = new Bitmap[1];
        Runnable runnable = mock(Runnable.class);
        Callback<Bitmap> callback = mMediator.createBitmapCallback(bitmaps, 0, runnable);

        Bitmap bitmap = Bitmap.createBitmap(1, 1, Bitmap.Config.ARGB_8888);
        callback.onResult(bitmap);

        assertEquals(bitmap, bitmaps[0]);
        verify(runnable).run();
    }

    @Test
    public void testFetchAndSetThemeCollectionsLeadingIcon() {
        createMediator(true);
        mMediator = spy(mMediator);

        List<BackgroundCollection> collections = new ArrayList<>();
        // Add enough collections to trigger both fetches.
        for (int i = 0; i < 6; i++) {
            collections.add(mock(BackgroundCollection.class));
        }
        doAnswer(
                        invocation -> {
                            Callback<List<BackgroundCollection>> callback =
                                    invocation.getArgument(0);
                            callback.onResult(collections);
                            return null;
                        })
                .when(mNtpThemeCollectionManager)
                .getBackgroundCollections(any(Callback.class));

        mMediator.fetchAndSetThemeCollectionsLeadingIcon();

        verify(mMediator, times(2)).fetchImageOrRunCallback(eq(mImageFetcher), any(), any());
    }

    @Test
    public void testUpdateTrailingIconVisibilityForSectionType() {
        createMediator(true);

        clearInvocations(mThemePropertyModel);
        mMediator.updateTrailingIconVisibilityForSectionType(DEFAULT);
        verify(mThemePropertyModel).set(IS_SECTION_SELECTED, new Pair<>(DEFAULT, true));
        verify(mThemePropertyModel).set(IS_SECTION_SELECTED, new Pair<>(IMAGE_FROM_DISK, false));
        verify(mThemePropertyModel).set(IS_SECTION_SELECTED, new Pair<>(CHROME_COLOR, false));
        verify(mThemePropertyModel).set(IS_SECTION_SELECTED, new Pair<>(THEME_COLLECTION, false));
        verify(mThemePropertyModel).set(IS_SECTION_SELECTED, new Pair<>(COLOR_FROM_HEX, false));

        clearInvocations(mThemePropertyModel);
        mMediator.updateTrailingIconVisibilityForSectionType(IMAGE_FROM_DISK);
        verify(mThemePropertyModel).set(IS_SECTION_SELECTED, new Pair<>(DEFAULT, false));
        verify(mThemePropertyModel).set(IS_SECTION_SELECTED, new Pair<>(IMAGE_FROM_DISK, true));
        verify(mThemePropertyModel).set(IS_SECTION_SELECTED, new Pair<>(CHROME_COLOR, false));
        verify(mThemePropertyModel).set(IS_SECTION_SELECTED, new Pair<>(THEME_COLLECTION, false));
        verify(mThemePropertyModel).set(IS_SECTION_SELECTED, new Pair<>(COLOR_FROM_HEX, false));

        clearInvocations(mThemePropertyModel);
        mMediator.updateTrailingIconVisibilityForSectionType(CHROME_COLOR);
        verify(mThemePropertyModel).set(IS_SECTION_SELECTED, new Pair<>(DEFAULT, false));
        verify(mThemePropertyModel).set(IS_SECTION_SELECTED, new Pair<>(IMAGE_FROM_DISK, false));
        verify(mThemePropertyModel).set(IS_SECTION_SELECTED, new Pair<>(CHROME_COLOR, true));
        verify(mThemePropertyModel).set(IS_SECTION_SELECTED, new Pair<>(THEME_COLLECTION, false));
        // Verifies that COLOR_FROM_HEX doesn't override the visibility of CHROME_COLOR since they
        // share the same image icon.
        verify(mThemePropertyModel, never())
                .set(IS_SECTION_SELECTED, new Pair<>(COLOR_FROM_HEX, /* visible= */ false));

        clearInvocations(mThemePropertyModel);
        mMediator.updateTrailingIconVisibilityForSectionType(COLOR_FROM_HEX);
        verify(mThemePropertyModel).set(IS_SECTION_SELECTED, new Pair<>(DEFAULT, false));
        verify(mThemePropertyModel).set(IS_SECTION_SELECTED, new Pair<>(IMAGE_FROM_DISK, false));
        verify(mThemePropertyModel).set(IS_SECTION_SELECTED, new Pair<>(CHROME_COLOR, false));
        verify(mThemePropertyModel).set(IS_SECTION_SELECTED, new Pair<>(THEME_COLLECTION, false));
        verify(mThemePropertyModel).set(IS_SECTION_SELECTED, new Pair<>(COLOR_FROM_HEX, true));

        clearInvocations(mThemePropertyModel);
        mMediator.updateTrailingIconVisibilityForSectionType(THEME_COLLECTION);
        verify(mThemePropertyModel).set(IS_SECTION_SELECTED, new Pair<>(DEFAULT, false));
        verify(mThemePropertyModel).set(IS_SECTION_SELECTED, new Pair<>(IMAGE_FROM_DISK, false));
        verify(mThemePropertyModel).set(IS_SECTION_SELECTED, new Pair<>(CHROME_COLOR, false));
        verify(mThemePropertyModel).set(IS_SECTION_SELECTED, new Pair<>(THEME_COLLECTION, true));
        verify(mThemePropertyModel).set(IS_SECTION_SELECTED, new Pair<>(COLOR_FROM_HEX, false));
    }
}
