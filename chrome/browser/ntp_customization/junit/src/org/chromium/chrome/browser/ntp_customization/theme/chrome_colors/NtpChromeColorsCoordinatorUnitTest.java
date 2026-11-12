// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp_customization.theme.chrome_colors;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertThrows;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.argThat;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.clearInvocations;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.verify;

import static org.chromium.chrome.browser.ntp_customization.NtpCustomizationCoordinator.BottomSheetType.CHROME_COLORS;
import static org.chromium.chrome.browser.ntp_customization.NtpCustomizationCoordinator.BottomSheetType.THEME;

import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.Color;
import android.graphics.Matrix;
import android.text.TextWatcher;
import android.view.ContextThemeWrapper;
import android.view.View;
import android.view.View.MeasureSpec;
import android.widget.CompoundButton.OnCheckedChangeListener;

import androidx.annotation.ColorInt;
import androidx.recyclerview.widget.GridLayoutManager;
import androidx.recyclerview.widget.RecyclerView;
import androidx.test.core.app.ApplicationProvider;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.ntp_customization.BottomSheetDelegate;
import org.chromium.chrome.browser.ntp_customization.NtpCustomizationConfigManager;
import org.chromium.chrome.browser.ntp_customization.NtpCustomizationUtils;
import org.chromium.chrome.browser.ntp_customization.NtpCustomizationUtils.NtpBackgroundType;
import org.chromium.chrome.browser.ntp_customization.R;
import org.chromium.chrome.browser.ntp_customization.theme.chrome_colors.NtpThemeColorInfo.NtpThemeColorId;
import org.chromium.chrome.browser.ntp_customization.theme.theme_collections.CustomBackgroundInfo;
import org.chromium.chrome.browser.ntp_customization.theme.upload_image.BackgroundImageInfo;
import org.chromium.chrome.browser.ntp_customization.theme_sync.data.NtpBackgroundDataColor;
import org.chromium.chrome.browser.ntp_customization.theme_sync.data.NtpBackgroundDataThemeCollection;
import org.chromium.chrome.browser.ntp_customization.theme_sync.data.NtpBackgroundDataUploadImage;
import org.chromium.chrome.browser.ntp_customization.theme_sync.data.PlatformType;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.url.JUnitTestGURLs;

/** Unit tests for {@link NtpChromeColorsCoordinator}. */
@RunWith(BaseRobolectricTestRunner.class)
@Features.EnableFeatures({ChromeFeatureList.NEW_TAB_PAGE_CUSTOMIZATION_V2})
public class NtpChromeColorsCoordinatorUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private BottomSheetDelegate mBottomSheetDelegate;
    @Mock private Runnable mOnChromeColorSelectedCallback;
    @Mock private View.OnClickListener mOnClickListener;
    @Mock private TextWatcher mTextWatcher;

    private static final String FILE_ID_HASH = "fileIdHash";

    private NtpChromeColorsCoordinator mCoordinator;
    private NtpCustomizationConfigManager mNtpCustomizationConfigManager;
    private PropertyModel mPropertyModel;
    private Context mContext;
    private View mBottomSheetView;

    @Before
    public void setUp() {
        mContext =
                new ContextThemeWrapper(
                        ApplicationProvider.getApplicationContext(),
                        R.style.Theme_BrowserUI_DayNight);
        NtpCustomizationUtils.resetSharedPreferenceForTesting();
        mNtpCustomizationConfigManager = spy(new NtpCustomizationConfigManager());
        NtpCustomizationConfigManager.setInstanceForTesting(mNtpCustomizationConfigManager);

        createCoordinator();
    }

    @After
    public void tearDown() {
        NtpCustomizationUtils.resetSharedPreferenceForTesting();
        NtpCustomizationConfigManager.getInstance().resetForTesting();
    }

    @Test
    @Features.EnableFeatures(
            ChromeFeatureList.NEW_TAB_PAGE_CUSTOMIZATION_V2 + ":show_color_picker/true")
    public void testConstructor() {
        mCoordinator.destroy();
        createCoordinator();

        assertNotNull(mPropertyModel.get(NtpChromeColorsProperties.BACK_BUTTON_CLICK_LISTENER));
        assertNotNull(
                mPropertyModel.get(NtpChromeColorsProperties.BACKGROUND_COLOR_INPUT_TEXT_WATCHER));
        assertNotNull(
                mPropertyModel.get(NtpChromeColorsProperties.PRIMARY_COLOR_INPUT_TEXT_WATCHER));
        assertNotNull(mPropertyModel.get(NtpChromeColorsProperties.SAVE_BUTTON_CLICK_LISTENER));
        assertNotNull(
                mPropertyModel.get(
                        NtpChromeColorsProperties.DAILY_REFRESH_SWITCH_ON_CHECKED_CHANGE_LISTENER));
        assertFalse(mPropertyModel.get(NtpChromeColorsProperties.IS_DAILY_REFRESH_SWITCH_CHECKED));
    }

    @Test
    public void testBackButton() {
        View backButton = mBottomSheetView.findViewById(R.id.back_button);
        assertNotNull(backButton);
        assertTrue(backButton.hasOnClickListeners());

        backButton.performClick();

        verify(mBottomSheetDelegate).showBottomSheet(eq(THEME));
    }

    @Test
    public void testDestroy() {
        mPropertyModel.set(
                NtpChromeColorsProperties.BACKGROUND_COLOR_INPUT_TEXT_WATCHER, mTextWatcher);
        mPropertyModel.set(
                NtpChromeColorsProperties.PRIMARY_COLOR_INPUT_TEXT_WATCHER, mTextWatcher);
        mPropertyModel.set(NtpChromeColorsProperties.SAVE_BUTTON_CLICK_LISTENER, mOnClickListener);

        assertNotNull(mPropertyModel.get(NtpChromeColorsProperties.BACK_BUTTON_CLICK_LISTENER));
        assertNotNull(
                mPropertyModel.get(NtpChromeColorsProperties.BACKGROUND_COLOR_INPUT_TEXT_WATCHER));
        assertNotNull(
                mPropertyModel.get(NtpChromeColorsProperties.PRIMARY_COLOR_INPUT_TEXT_WATCHER));
        assertNotNull(mPropertyModel.get(NtpChromeColorsProperties.SAVE_BUTTON_CLICK_LISTENER));
        assertNotNull(
                mPropertyModel.get(
                        NtpChromeColorsProperties.DAILY_REFRESH_SWITCH_ON_CHECKED_CHANGE_LISTENER));

        mCoordinator.destroy();

        assertNull(mPropertyModel.get(NtpChromeColorsProperties.BACK_BUTTON_CLICK_LISTENER));
        assertNull(
                mPropertyModel.get(NtpChromeColorsProperties.BACKGROUND_COLOR_INPUT_TEXT_WATCHER));
        assertNull(mPropertyModel.get(NtpChromeColorsProperties.PRIMARY_COLOR_INPUT_TEXT_WATCHER));
        assertNull(mPropertyModel.get(NtpChromeColorsProperties.SAVE_BUTTON_CLICK_LISTENER));
        assertNull(
                mPropertyModel.get(
                        NtpChromeColorsProperties.DAILY_REFRESH_SWITCH_ON_CHECKED_CHANGE_LISTENER));
    }

    @Test
    public void testDestroy_logMetricsWithSingleClick() {
        String histogramName = "NewTabPage.Customization.Theme.ChromeColor.Click";
        createCoordinator();

        NtpThemeColorInfo blueColor =
                NtpThemeColorUtils.createNtpThemeColorInfo(
                        mContext, NtpThemeColorInfo.NtpThemeColorId.NTP_COLORS_BLUE);
        mCoordinator.onItemClicked(blueColor);

        HistogramWatcher watcher =
                HistogramWatcher.newSingleRecordWatcher(histogramName, blueColor.id);
        mCoordinator.destroy();
        watcher.assertExpected();
    }

    @Test
    public void testDestroy_logMetricsWithMultipleClicks() {
        String histogramName = "NewTabPage.Customization.Theme.ChromeColor.Click";
        createCoordinator();

        NtpThemeColorInfo blueColor =
                NtpThemeColorUtils.createNtpThemeColorInfo(
                        mContext, NtpThemeColorInfo.NtpThemeColorId.NTP_COLORS_BLUE);
        mCoordinator.onItemClicked(blueColor);

        NtpThemeColorInfo aquaColor =
                NtpThemeColorUtils.createNtpThemeColorInfo(
                        mContext, NtpThemeColorInfo.NtpThemeColorId.NTP_COLORS_AQUA);
        mCoordinator.onItemClicked(aquaColor);

        NtpThemeColorInfo greenColor =
                NtpThemeColorUtils.createNtpThemeColorInfo(
                        mContext, NtpThemeColorInfo.NtpThemeColorId.NTP_COLORS_GREEN);
        mCoordinator.onItemClicked(greenColor);

        HistogramWatcher watcher =
                HistogramWatcher.newSingleRecordWatcher(histogramName, greenColor.id);
        mCoordinator.destroy();
        watcher.assertExpected();
    }

    @Test
    public void testDestroy_logMetricsWithDailyRefreshToggledOn() {
        OnCheckedChangeListener dailyRefreshSwitchChangeListener =
                mPropertyModel.get(
                        NtpChromeColorsProperties.DAILY_REFRESH_SWITCH_ON_CHECKED_CHANGE_LISTENER);
        assertNotNull(dailyRefreshSwitchChangeListener);
        assertFalse(
                NtpCustomizationUtils.getIsChromeColorDailyRefreshEnabledFromSharedPreference());

        dailyRefreshSwitchChangeListener.onCheckedChanged(null, true);

        String histogramName = "NewTabPage.Customization.Theme.ChromeColor.TurnOnDailyRefresh";
        HistogramWatcher histogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(histogramName, true);
        mCoordinator.destroy();
        histogramWatcher.assertExpected();
    }

    @Test
    public void testDestroy_logMetricsWithDailyRefreshToggledOff() {
        applyBlueChromeColor(/* isDailyRefreshEnabled= */ true);
        createCoordinator();
        OnCheckedChangeListener dailyRefreshSwitchChangeListener =
                mPropertyModel.get(
                        NtpChromeColorsProperties.DAILY_REFRESH_SWITCH_ON_CHECKED_CHANGE_LISTENER);
        assertNotNull(dailyRefreshSwitchChangeListener);
        assertTrue(mPropertyModel.get(NtpChromeColorsProperties.IS_DAILY_REFRESH_SWITCH_CHECKED));

        dailyRefreshSwitchChangeListener.onCheckedChanged(null, false);

        String histogramName = "NewTabPage.Customization.Theme.ChromeColor.TurnOnDailyRefresh";
        HistogramWatcher histogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(histogramName, false);
        mCoordinator.destroy();
        histogramWatcher.assertExpected();
    }

    @Test
    public void testDestroy_logMetricsWithDailyRefreshToggledMultipleTimes() {
        OnCheckedChangeListener dailyRefreshSwitchChangeListener =
                mPropertyModel.get(
                        NtpChromeColorsProperties.DAILY_REFRESH_SWITCH_ON_CHECKED_CHANGE_LISTENER);
        assertNotNull(dailyRefreshSwitchChangeListener);
        assertFalse(
                NtpCustomizationUtils.getIsChromeColorDailyRefreshEnabledFromSharedPreference());

        dailyRefreshSwitchChangeListener.onCheckedChanged(null, true);
        dailyRefreshSwitchChangeListener.onCheckedChanged(null, false);
        dailyRefreshSwitchChangeListener.onCheckedChanged(null, true);

        String histogramName = "NewTabPage.Customization.Theme.ChromeColor.TurnOnDailyRefresh";
        HistogramWatcher histogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(histogramName, true);
        mCoordinator.destroy();
        histogramWatcher.assertExpected();
    }

    @Test
    public void testColorGridRecyclerView() {
        NtpChromeColorGridRecyclerView gridRecyclerView =
                new NtpChromeColorGridRecyclerView(mContext, null);
        assertNull(gridRecyclerView.getItemAnimator());
    }

    @Test
    public void testColorGridRecyclerView_onMeasure() {
        NtpChromeColorGridRecyclerView gridRecyclerView =
                new NtpChromeColorGridRecyclerView(mContext, null);
        GridLayoutManager layoutManager = spy(new GridLayoutManager(mContext, 1));
        gridRecyclerView.setLayoutManager(layoutManager);

        int itemWidth = 50;
        int spacing = 10;
        int maxItemCount = 5;
        gridRecyclerView.setItemWidth(itemWidth);
        gridRecyclerView.setSpacing(spacing);
        gridRecyclerView.setMaxItemCount(maxItemCount);

        // Test case 1: width allows for exactly 3 items
        int width1 = 3 * (itemWidth + spacing);
        gridRecyclerView.measure(
                MeasureSpec.makeMeasureSpec(width1, MeasureSpec.EXACTLY),
                MeasureSpec.makeMeasureSpec(100, MeasureSpec.EXACTLY));
        verify(layoutManager).setSpanCount(eq(3));

        // Test case 2: width allows for 2.5 items, should round down to 2
        int width2 = 2 * (itemWidth + spacing) + (itemWidth / 2);
        gridRecyclerView.measure(
                MeasureSpec.makeMeasureSpec(width2, MeasureSpec.EXACTLY),
                MeasureSpec.makeMeasureSpec(100, MeasureSpec.EXACTLY));
        verify(layoutManager).setSpanCount(eq(2));

        // Test case 3: width allows for less than 1 item, should be 1
        int width3 = itemWidth / 2;
        gridRecyclerView.measure(
                MeasureSpec.makeMeasureSpec(width3, MeasureSpec.EXACTLY),
                MeasureSpec.makeMeasureSpec(100, MeasureSpec.EXACTLY));
        verify(layoutManager).setSpanCount(eq(1));

        // Test case 4: width is same as before, should not change span count.
        gridRecyclerView.measure(
                MeasureSpec.makeMeasureSpec(width3, MeasureSpec.EXACTLY),
                MeasureSpec.makeMeasureSpec(100, MeasureSpec.EXACTLY));
        verify(layoutManager).setSpanCount(eq(1));
    }

    @Test
    public void testAdapter_callsCallback() {
        NtpChromeColorGridRecyclerView gridRecyclerView =
                mBottomSheetView.findViewById(R.id.chrome_colors_recycler_view);
        NtpChromeColorsAdapter adapter = (NtpChromeColorsAdapter) gridRecyclerView.getAdapter();
        assertNotNull(adapter);

        // Click the first item.
        adapter.getOnItemClickedCallbackForTesting().onResult(adapter.getColorsForTesting().get(0));

        // Verify the callback is called.
        verify(mOnChromeColorSelectedCallback).run();
    }

    @Test
    public void testOnItemClicked_ntpThemeColorInfo_noPrimaryColorSelected() {
        @Nullable NtpThemeColorInfo primaryColorInfo = mCoordinator.getPrimaryColorInfoForTesting();

        assertNull(primaryColorInfo);
        NtpThemeColorInfo colorInfo =
                NtpThemeColorUtils.createNtpThemeColorInfo(
                        mContext, NtpThemeColorInfo.NtpThemeColorId.NTP_COLORS_BLUE);
        mCoordinator.onItemClicked(colorInfo);

        verify(mBottomSheetDelegate).onNewColorSelected(eq(true));
        verify(mOnChromeColorSelectedCallback).run();
    }

    @Test
    @Features.EnableFeatures({ChromeFeatureList.NEW_TAB_PAGE_CUSTOMIZATION_V2})
    public void testOnItemClicked_ntpThemeColorInfo_withPrimaryColorSelected() {
        @NtpThemeColorId int colorId = NtpThemeColorId.NTP_COLORS_BLUE;
        @NtpThemeColorId int colorId1 = NtpThemeColorId.NTP_COLORS_AQUA;
        NtpThemeColorInfo colorInfo = NtpThemeColorUtils.createNtpThemeColorInfo(mContext, colorId);
        NtpThemeColorInfo colorInfo1 =
                NtpThemeColorUtils.createNtpThemeColorInfo(mContext, colorId1);

        NtpBackgroundDataColor backgroundData =
                new NtpBackgroundDataColor(
                        PlatformType.ANDROID,
                        /* isChromeColorDailyRefreshEnabled= */ false,
                        colorInfo);
        mNtpCustomizationConfigManager.onBackgroundDataChanged(mContext, backgroundData);

        createCoordinator();
        assertEquals(
                colorInfo.primaryColorResId,
                mCoordinator.getPrimaryColorInfoForTesting().primaryColorResId);
        clearInvocations(mBottomSheetDelegate);
        clearInvocations(mOnChromeColorSelectedCallback);

        mCoordinator.onItemClicked(colorInfo);
        verify(mBottomSheetDelegate).onNewColorSelected(eq(false));
        verify(mOnChromeColorSelectedCallback).run();
        assertEquals(colorId, NtpCustomizationUtils.getNtpThemeColorIdFromSharedPreference());

        clearInvocations(mOnChromeColorSelectedCallback);
        mCoordinator.onItemClicked(colorInfo1);
        verify(mBottomSheetDelegate).onNewColorSelected(eq(true));
        verify(mOnChromeColorSelectedCallback).run();
        assertEquals(colorId1, NtpCustomizationUtils.getNtpThemeColorIdFromSharedPreference());
    }

    @Test
    public void testOnItemClicked_ntpThemeColorFromHexInfo() {
        @Nullable NtpThemeColorInfo primaryColorInfo = mCoordinator.getPrimaryColorInfoForTesting();
        assertNull(primaryColorInfo);

        @ColorInt int backgroundColor = Color.RED;
        @ColorInt int primaryColor = Color.BLUE;
        NtpThemeColorInfo colorInfo =
                new NtpThemeColorFromHexInfo(mContext, backgroundColor, primaryColor);
        mCoordinator.onItemClicked(colorInfo);

        verify(mBottomSheetDelegate).onNewColorSelected(eq(true));
        verify(mOnChromeColorSelectedCallback).run();
        assertEquals(
                backgroundColor, NtpCustomizationUtils.getBackgroundColorFromSharedPreference(-1));
        assertEquals(
                primaryColor,
                NtpCustomizationUtils.getPrimaryColorFromCustomizedThemeColor(
                                mContext, /* checkDailyRefresh= */ false)
                        .intValue());

        HistogramWatcher watcher =
                HistogramWatcher.newBuilder()
                        .expectNoRecords("NewTabPage.Customization.Theme.ChromeColor.Click")
                        .build();
        mCoordinator.destroy();
        watcher.assertExpected();
    }

    @Test
    public void testsGetColorFromHex() {
        // Verifies null is returned for an invalid hexadecimal string.
        assertNull(mCoordinator.getColorFromHex("FF00"));

        // Verifies the method returns the expected color value for a valid hexadecimal string.
        String colorHex = "#FF0000";
        @ColorInt int color = Color.parseColor(colorHex);
        assertEquals(color, mCoordinator.getColorFromHex(colorHex).intValue());

        // Verifies the missing "#" will be added automatically, and the method returns the expected
        // color value.
        colorHex = "FF0000";
        assertEquals(color, mCoordinator.getColorFromHex(colorHex).intValue());
    }

    @Test
    public void testsGetColorFromHex_ThrowsException_ForInvalidStringExplicitly() {
        String invalidColorInput = "invalid color string with spaces";

        assertThrows(IllegalArgumentException.class, () -> Color.parseColor(invalidColorInput));
    }

    @Test
    public void testDailyRefreshSwitchToggled() {
        OnCheckedChangeListener dailyRefreshSwitchChangeListener =
                mPropertyModel.get(
                        NtpChromeColorsProperties.DAILY_REFRESH_SWITCH_ON_CHECKED_CHANGE_LISTENER);
        assertNotNull(dailyRefreshSwitchChangeListener);

        dailyRefreshSwitchChangeListener.onCheckedChanged(null, true);
        assertTrue(NtpCustomizationUtils.getIsChromeColorDailyRefreshEnabledFromSharedPreference());

        dailyRefreshSwitchChangeListener.onCheckedChanged(null, false);
        assertFalse(
                NtpCustomizationUtils.getIsChromeColorDailyRefreshEnabledFromSharedPreference());
    }

    @Test
    public void testOnDailyRefreshSwitchToggled() {
        mCoordinator.onDailyRefreshSwitchToggled(/* buttonView= */ null, /* isChecked= */ true);

        assertTrue(mCoordinator.getIsDailyRefreshEnabledForTesting());
        assertTrue(mCoordinator.getIsDailyRefreshToggledForTesting());
        assertTrue(NtpCustomizationUtils.getIsChromeColorDailyRefreshEnabledFromSharedPreference());

        mCoordinator.onDailyRefreshSwitchToggled(/* buttonView= */ null, /* isChecked= */ false);

        assertFalse(mCoordinator.getIsDailyRefreshEnabledForTesting());
        assertTrue(mCoordinator.getIsDailyRefreshToggledForTesting());
        assertFalse(
                NtpCustomizationUtils.getIsChromeColorDailyRefreshEnabledFromSharedPreference());
    }

    @Test
    public void testOnDailyRefreshSwitchToggled_dailyRefreshTurnedOn_commitsDailyColor() {
        testOnDailyRefreshSwitchToggledCommitsImpl(/* isChecked= */ true);
    }

    @Test
    public void testOnDailyRefreshSwitchToggled_dailyRefreshTurnedOff_commitsStaticColor() {
        testOnDailyRefreshSwitchToggledCommitsImpl(/* isChecked= */ false);
    }

    @Test
    public void testDoNotReCreateWithSamePrimaryColorSelected() {
        // Sets the current theme as Chrome color.
        NtpThemeColorInfo colorInfo =
                NtpThemeColorUtils.createNtpThemeColorInfo(
                        mContext, NtpThemeColorInfo.NtpThemeColorId.NTP_COLORS_BLUE);
        NtpBackgroundDataColor backgroundData =
                new NtpBackgroundDataColor(
                        PlatformType.ANDROID,
                        /* isChromeColorDailyRefreshEnabled= */ false,
                        colorInfo);
        mNtpCustomizationConfigManager.onBackgroundDataChanged(mContext, backgroundData);

        createCoordinator();
        // Verifies the primary color of the chrome color bottom sheet matches the current color
        // theme.
        assertEquals(colorInfo, mCoordinator.getPrimaryColorInfoForTesting());

        NtpThemeColorInfo colorInfo1 =
                NtpThemeColorUtils.createNtpThemeColorInfo(
                        mContext, NtpThemeColorInfo.NtpThemeColorId.NTP_COLORS_AQUA);
        clearInvocations(mBottomSheetDelegate);
        // Verifies to notify the mBottomSheetDelegate a different color is selected.
        mCoordinator.onItemClicked(colorInfo1);
        verify(mBottomSheetDelegate).onNewColorSelected(eq(true));

        // Changes the theme to an image background theme.
        Bitmap bitmap = Bitmap.createBitmap(100, 100, Bitmap.Config.ARGB_8888);
        BackgroundImageInfo backgroundImageInfo =
                new BackgroundImageInfo(new Matrix(), new Matrix(), null, null);
        NtpBackgroundDataUploadImage uploadImageData =
                new NtpBackgroundDataUploadImage(
                        PlatformType.ANDROID,
                        backgroundImageInfo,
                        bitmap,
                        /* primaryColor= */ null,
                        FILE_ID_HASH);
        mNtpCustomizationConfigManager.onBackgroundDataChanged(mContext, uploadImageData);

        // Reshows the chrome color bottom sheet and chooses the original color info.
        mCoordinator.prepareToShow();
        mCoordinator.onItemClicked(colorInfo);
        // Verifies to notify the mBottomSheetDelegate with the same color is selected.
        verify(mBottomSheetDelegate).onNewColorSelected(eq(false));
    }

    @Test
    public void testPrepareToShow() {
        assertFalse(mPropertyModel.get(NtpChromeColorsProperties.IS_DAILY_REFRESH_SWITCH_CHECKED));
        assertEquals(NtpBackgroundType.DEFAULT, mNtpCustomizationConfigManager.getBackgroundType());

        // When no customized theme is selected, verifies that the Chrome color bottom sheet is
        // opened with daily refresh toggle turned off and no highlighted Chrome color item.
        verifyIsDailyRefreshCheckedState(
                /* expectedToggleEnabled= */ false, RecyclerView.NO_POSITION);

        // Sets the current theme as Chrome color.
        int expectedPosition = NtpThemeColorId.NTP_COLORS_BLUE - 1;
        applyBlueChromeColor(/* isDailyRefreshEnabled= */ false);

        // Verifies that the Chrome color bottom sheet is opened with highlighted item.
        verifyIsDailyRefreshCheckedState(/* expectedToggleEnabled= */ false, expectedPosition);

        // If daily refresh is turned on for the Chrome color, verifies that the Chrome color bottom
        // sheet is opened with daily refresh toggle turned on.
        applyBlueChromeColor(/* isDailyRefreshEnabled= */ true);
        verifyIsDailyRefreshCheckedState(/* expectedToggleEnabled= */ true, expectedPosition);

        // Selects an image background theme.
        Bitmap bitmap = Bitmap.createBitmap(100, 100, Bitmap.Config.ARGB_8888);
        BackgroundImageInfo backgroundImageInfo =
                new BackgroundImageInfo(new Matrix(), new Matrix(), null, null);
        NtpBackgroundDataUploadImage uploadImageData =
                new NtpBackgroundDataUploadImage(
                        PlatformType.ANDROID,
                        backgroundImageInfo,
                        bitmap,
                        /* primaryColor= */ null,
                        FILE_ID_HASH);
        mNtpCustomizationConfigManager.onBackgroundDataChanged(mContext, uploadImageData);

        // Verifies that if the background type is no longer Chrome color before prepareToShow() is
        // called, the daily refresh toggle is turned off even though it was on for the Chrome
        // color, and there is no highlighted item.
        verifyIsDailyRefreshCheckedState(
                /* expectedToggleEnabled= */ false, RecyclerView.NO_POSITION);
    }

    @Test
    public void testPrepareToShow_syncedDailyColorNotAppliedYet_keepsSwitchOffAndSyncedColor() {
        testPrepareToShowWithSyncedColorNotAppliedYetImpl(
                /* isCurrentColorDailyRefreshEnabled= */ false);
    }

    @Test
    public void testPrepareToShow_syncedStaticColorNotAppliedYet_keepsSwitchOnAndSyncedColor() {
        testPrepareToShowWithSyncedColorNotAppliedYetImpl(
                /* isCurrentColorDailyRefreshEnabled= */ true);
    }

    @Test
    public void testOnBackgroundTypeChanged_afterSelectUploadImage() {
        setupChromeColorSelectionState();

        // Changes the NTP background to the uploaded image
        Bitmap bitmap = Bitmap.createBitmap(1, 1, Bitmap.Config.ARGB_8888);
        BackgroundImageInfo backgroundImageInfo =
                new BackgroundImageInfo(new Matrix(), new Matrix(), null, null);
        NtpBackgroundDataUploadImage uploadImageData =
                new NtpBackgroundDataUploadImage(
                        PlatformType.ANDROID,
                        backgroundImageInfo,
                        bitmap,
                        /* primaryColor= */ null,
                        FILE_ID_HASH);
        mNtpCustomizationConfigManager.onBackgroundDataChanged(mContext, uploadImageData);

        mCoordinator.onBackgroundTypeChanged();

        verifyChromeColorSelectionCleared();
    }

    @Test
    public void testOnBackgroundTypeChanged_afterSelectChromeDefault() {
        setupChromeColorSelectionState();

        // Changes the NTP background to the default background
        mNtpCustomizationConfigManager.onBackgroundDataChanged(
                mContext, /* backgroundData= */ null);
        mCoordinator.onBackgroundTypeChanged();

        verifyChromeColorSelectionCleared();
    }

    @Test
    public void testOnBackgroundTypeChanged_afterSelectThemeCollection() {
        setupChromeColorSelectionState();

        // Changes the NTP background to a theme collection image
        Bitmap bitmap = Bitmap.createBitmap(1, 1, Bitmap.Config.ARGB_8888);
        BackgroundImageInfo backgroundImageInfo =
                new BackgroundImageInfo(new Matrix(), new Matrix(), null, null);
        CustomBackgroundInfo customBackgroundInfo =
                new CustomBackgroundInfo(
                        JUnitTestGURLs.EXAMPLE_URL,
                        /* collectionId= */ "test",
                        /* isUploadedImage= */ false,
                        /* isDailyRefreshEnabled= */ false);
        NtpBackgroundDataThemeCollection backgroundData =
                new NtpBackgroundDataThemeCollection(
                        PlatformType.ANDROID,
                        customBackgroundInfo,
                        backgroundImageInfo,
                        bitmap,
                        /* primaryColor= */ null,
                        /* fileIdHash= */ null);
        mNtpCustomizationConfigManager.onBackgroundDataChanged(mContext, backgroundData);

        mCoordinator.onBackgroundTypeChanged();

        verifyChromeColorSelectionCleared();
    }

    @Test
    public void testOnBackgroundTypeChanged_afterSelectChromeColors() {
        setupChromeColorSelectionState();

        mCoordinator.onBackgroundTypeChanged();

        // Verifies the selection state is not cleared
        assertEquals(5, (int) mPropertyModel.get(NtpChromeColorsProperties.HIGHLIGHTED_ITEM_INDEX));
        assertTrue(mPropertyModel.get(NtpChromeColorsProperties.IS_DAILY_REFRESH_SWITCH_CHECKED));
        assertTrue(mCoordinator.getIsDailyRefreshEnabledForTesting());
    }

    /**
     * Turns the daily refresh switch on or off for a blue Chrome color that has the opposite
     * setting, and checks that the color is committed with the new setting and saved to the theme
     * history.
     */
    private void testOnDailyRefreshSwitchToggledCommitsImpl(boolean isChecked) {
        applyBlueChromeColor(/* isDailyRefreshEnabled= */ !isChecked);
        createCoordinator();
        clearInvocations(mNtpCustomizationConfigManager);

        mCoordinator.onDailyRefreshSwitchToggled(/* buttonView= */ null, isChecked);

        NtpBackgroundDataColor expectedColor =
                new NtpBackgroundDataColor(
                        mContext, PlatformType.ANDROID, NtpThemeColorId.NTP_COLORS_BLUE, isChecked);
        verify(mNtpCustomizationConfigManager)
                .onBackgroundDataChanged(
                        eq(mContext), argThat(expectedColor::hasSameThemeAndColor));
        verify(mNtpCustomizationConfigManager)
                .maybeSaveUserSelectedBackgroundTypeToSharedPreference(mContext);
    }

    /**
     * Applies the blue Chrome color with the given daily refresh setting, saves a synced aqua
     * Chrome color with the opposite setting that isn't applied yet, and shows the bottom sheet.
     * Checks that the switch shows the setting of the blue color, and that showing the bottom sheet
     * neither commits a color nor changes the synced color or its saved setting.
     */
    private void testPrepareToShowWithSyncedColorNotAppliedYetImpl(
            boolean isCurrentColorDailyRefreshEnabled) {
        applyBlueChromeColor(isCurrentColorDailyRefreshEnabled);
        NtpBackgroundDataColor syncedColor =
                new NtpBackgroundDataColor(
                        mContext,
                        PlatformType.ANDROID,
                        NtpThemeColorId.NTP_COLORS_AQUA,
                        !isCurrentColorDailyRefreshEnabled);
        mNtpCustomizationConfigManager.onSyncedChromeColorChanged(mContext, syncedColor);
        clearInvocations(mNtpCustomizationConfigManager);

        createCoordinator();

        assertEquals(
                isCurrentColorDailyRefreshEnabled,
                mPropertyModel.get(NtpChromeColorsProperties.IS_DAILY_REFRESH_SWITCH_CHECKED));
        verify(mNtpCustomizationConfigManager, never()).onBackgroundDataChanged(any(), any());
        assertEquals(syncedColor, mNtpCustomizationConfigManager.getSyncedNtpBackgroundData());
        assertEquals(
                !isCurrentColorDailyRefreshEnabled,
                NtpCustomizationUtils.getIsChromeColorDailyRefreshEnabledFromSharedPreference());
    }

    /** Shows the bottom sheet and checks the daily refresh switch and the highlighted color. */
    private void verifyIsDailyRefreshCheckedState(
            boolean expectedToggleEnabled, int expectedHighlightedItemPosition) {
        mCoordinator.prepareToShow();
        assertEquals(
                expectedToggleEnabled,
                mPropertyModel.get(NtpChromeColorsProperties.IS_DAILY_REFRESH_SWITCH_CHECKED));
        NtpChromeColorsAdapter adapter =
                (NtpChromeColorsAdapter)
                        mPropertyModel.get(NtpChromeColorsProperties.RECYCLER_VIEW_ADAPTER);
        assertEquals(expectedHighlightedItemPosition, adapter.getSelectedPositionForTesting());
    }

    private void createCoordinator() {
        clearInvocations(mBottomSheetDelegate);

        mCoordinator =
                new NtpChromeColorsCoordinator(
                        mContext, mBottomSheetDelegate, mOnChromeColorSelectedCallback);
        mCoordinator.prepareToShow();

        ArgumentCaptor<View> viewCaptor = ArgumentCaptor.forClass(View.class);
        verify(mBottomSheetDelegate)
                .registerBottomSheetLayout(eq(CHROME_COLORS), viewCaptor.capture());
        mBottomSheetView = viewCaptor.getValue();
        mPropertyModel = mCoordinator.getPropertyModelForTesting();
    }

    /** Applies the blue Chrome color with the given daily refresh setting as the NTP theme. */
    private void applyBlueChromeColor(boolean isDailyRefreshEnabled) {
        NtpThemeColorInfo colorInfo =
                NtpThemeColorUtils.createNtpThemeColorInfo(
                        mContext, NtpThemeColorInfo.NtpThemeColorId.NTP_COLORS_BLUE);
        mNtpCustomizationConfigManager.onBackgroundDataChanged(
                mContext,
                new NtpBackgroundDataColor(PlatformType.ANDROID, isDailyRefreshEnabled, colorInfo));
    }

    private void setupChromeColorSelectionState() {
        applyBlueChromeColor(/* isDailyRefreshEnabled= */ false);
        mPropertyModel.set(NtpChromeColorsProperties.HIGHLIGHTED_ITEM_INDEX, 5);
        mPropertyModel.set(NtpChromeColorsProperties.IS_DAILY_REFRESH_SWITCH_CHECKED, true);
        mCoordinator.onDailyRefreshSwitchToggled(/* buttonView= */ null, /* isChecked= */ true);
    }

    private void verifyChromeColorSelectionCleared() {
        assertEquals(
                RecyclerView.NO_POSITION,
                (int) mPropertyModel.get(NtpChromeColorsProperties.HIGHLIGHTED_ITEM_INDEX));
        assertFalse(mPropertyModel.get(NtpChromeColorsProperties.IS_DAILY_REFRESH_SWITCH_CHECKED));
        assertFalse(mCoordinator.getIsDailyRefreshEnabledForTesting());
        assertNotEquals(
                NtpBackgroundType.CHROME_COLOR, mNtpCustomizationConfigManager.getBackgroundType());
    }
}
