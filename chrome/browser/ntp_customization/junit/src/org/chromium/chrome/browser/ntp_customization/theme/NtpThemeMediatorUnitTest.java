// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp_customization.theme;

import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.reset;
import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import static org.chromium.chrome.browser.ntp_customization.NtpCustomizationUtils.NtpBackgroundImageType.CHROME_COLOR;
import static org.chromium.chrome.browser.ntp_customization.NtpCustomizationUtils.NtpBackgroundImageType.DEFAULT;
import static org.chromium.chrome.browser.ntp_customization.NtpCustomizationUtils.NtpBackgroundImageType.IMAGE_FROM_DISK;
import static org.chromium.chrome.browser.ntp_customization.NtpCustomizationViewProperties.BACK_PRESS_HANDLER;
import static org.chromium.chrome.browser.ntp_customization.theme.NtpThemeProperty.IS_SECTION_TRAILING_ICON_VISIBLE;
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
import org.chromium.chrome.browser.ntp_customization.NtpCustomizationUtils.NtpBackgroundImageType;
import org.chromium.chrome.browser.ntp_customization.NtpCustomizationViewProperties;
import org.chromium.chrome.browser.ntp_customization.R;
import org.chromium.chrome.browser.ntp_customization.theme.theme_collections.NtpThemeCollectionManager;
import org.chromium.ui.modelutil.PropertyModel;

/** Tests for {@link NtpThemeMediator}. */
@RunWith(BaseRobolectricTestRunner.class)
public class NtpThemeMediatorUnitTest {

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private Callback<Bitmap> mOnImageSelectedCallback;
    @Mock private BottomSheetDelegate mBottomSheetDelegate;
    @Mock private View mView;
    @Mock private NtpCustomizationConfigManager mNtpCustomizationConfigManager;
    @Mock private Uri mUri;
    @Mock private NtpThemeDelegate mNtpThemeDelegate;
    @Mock private NtpThemeCollectionManager mNtpThemeCollectionManager;

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
    }

    @Test
    public void testHandleChromeDefaultSectionClick() {
        createMediator(/* shouldShowAlone= */ true);

        String histogramName = "NewTabPage.Customization.BottomSheet.Shown";
        HistogramWatcher histogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        histogramName, BottomSheetType.CHROME_DEFAULT);

        mMediator.handleChromeDefaultSectionClick(mView);
        verify(mNtpCustomizationConfigManager)
                .onBackgroundColorChanged(eq(mContext), eq(null), eq(DEFAULT));
        verify(mNtpThemeCollectionManager).resetCustomBackground();
        histogramWatcher.assertExpected();
    }

    @Test
    public void testHandleThemeCollectionsSectionClick() {
        createMediator(/* shouldShowAlone= */ true);

        mMediator.handleThemeCollectionsSectionClick(mView);

        verify(mNtpThemeDelegate).onThemeCollectionsClicked(any(Runnable.class));
    }

    @Test
    public void testHandleChromeColorsSectionClick() {
        createMediator(/* shouldShowAlone= */ true);

        mMediator.handleChromeColorsSectionClick(mView);

        verify(mNtpThemeDelegate).onChromeColorsClicked();
    }

    @Test
    public void testSetLeadingIconForThemeCollectionsSection() {
        createMediator(/* shouldShowAlone= */ true);

        Pair<Integer, Integer> drawablePair =
                mThemePropertyModel.get(LEADING_ICON_FOR_THEME_COLLECTIONS);
        assertNotNull(drawablePair);
    }

    @Test
    public void testIconVisibilityAfterClickingDefault() {
        createMediator(true);
        // Reset calls from constructor to have a clean slate.
        reset(mThemePropertyModel);

        mMediator.handleChromeDefaultSectionClick(mView);

        verify(mThemePropertyModel)
                .set(eq(IS_SECTION_TRAILING_ICON_VISIBLE), eq(new Pair<>(DEFAULT, true)));
        verify(mThemePropertyModel)
                .set(eq(IS_SECTION_TRAILING_ICON_VISIBLE), eq(new Pair<>(IMAGE_FROM_DISK, false)));
        verify(mThemePropertyModel)
                .set(eq(IS_SECTION_TRAILING_ICON_VISIBLE), eq(new Pair<>(CHROME_COLOR, false)));
    }

    @Test
    public void testIconVisibilityUnaffectedByIndirectActions() {
        createMediator(true);
        // Reset calls from constructor.
        reset(mThemePropertyModel);

        mMediator.handleChromeColorsSectionClick(mView);
        mMediator.handleThemeCollectionsSectionClick(mView);

        // Verify no visibility changes happened directly.
        verify(mThemePropertyModel, never()).set(eq(IS_SECTION_TRAILING_ICON_VISIBLE), any());
    }

    @Test
    public void testInitTrailingIcon() {
        NtpCustomizationUtils.setNtpBackgroundImageTypeToSharedPreference(CHROME_COLOR);
        createMediator(true);
        verify(mThemePropertyModel)
                .set(eq(IS_SECTION_TRAILING_ICON_VISIBLE), eq(new Pair<>(CHROME_COLOR, true)));
        NtpCustomizationUtils.resetSharedPreferenceForTesting();
    }

    @Test
    public void testHandleSectionClick_onNewColorSelected() {
        createMediator(/* shouldShowAlone= */ true);
        when(mNtpCustomizationConfigManager.getBackgroundImageType())
                .thenReturn(NtpBackgroundImageType.DEFAULT);

        // Verifies the case of background type from default to default.
        mMediator.handleChromeDefaultSectionClick(mView);
        verify(mBottomSheetDelegate, never()).onNewColorSelected(anyBoolean());

        // Verifies the case of background type from upload-image to default.
        when(mNtpCustomizationConfigManager.getBackgroundImageType())
                .thenReturn(NtpBackgroundImageType.IMAGE_FROM_DISK);
        mMediator.handleChromeDefaultSectionClick(mView);
        verify(mBottomSheetDelegate).onNewColorSelected(eq(true));

        // Verifies the case of background type from chrome-color to default.
        when(mNtpCustomizationConfigManager.getBackgroundImageType())
                .thenReturn(NtpBackgroundImageType.CHROME_COLOR);
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
    }

    private void createMediator(boolean shouldShowAlone) {
        when(mBottomSheetDelegate.shouldShowAlone()).thenReturn(shouldShowAlone);
        mMediator =
                new NtpThemeMediator(
                        mContext,
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
                .set(eq(IS_SECTION_TRAILING_ICON_VISIBLE), eq(new Pair<>(CHROME_COLOR, true)));
        verify(mThemePropertyModel)
                .set(eq(IS_SECTION_TRAILING_ICON_VISIBLE), eq(new Pair<>(DEFAULT, false)));
        verify(mThemePropertyModel)
                .set(eq(IS_SECTION_TRAILING_ICON_VISIBLE), eq(new Pair<>(IMAGE_FROM_DISK, false)));

        verify(mNtpThemeCollectionManager).resetCustomBackground();
    }
}
