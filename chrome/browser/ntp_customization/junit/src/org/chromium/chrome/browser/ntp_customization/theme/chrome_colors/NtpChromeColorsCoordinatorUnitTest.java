// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp_customization.theme.chrome_colors;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.clearInvocations;
import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import static org.chromium.chrome.browser.ntp_customization.NtpCustomizationCoordinator.BottomSheetType.CHROME_COLORS;
import static org.chromium.chrome.browser.ntp_customization.NtpCustomizationCoordinator.BottomSheetType.THEME;

import android.content.Context;
import android.graphics.Color;
import android.text.TextWatcher;
import android.view.ContextThemeWrapper;
import android.view.View;
import android.view.View.MeasureSpec;
import android.widget.CompoundButton.OnCheckedChangeListener;

import androidx.annotation.ColorInt;
import androidx.recyclerview.widget.GridLayoutManager;
import androidx.test.core.app.ApplicationProvider;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.ntp_customization.BottomSheetDelegate;
import org.chromium.chrome.browser.ntp_customization.NtpCustomizationConfigManager;
import org.chromium.chrome.browser.ntp_customization.NtpCustomizationUtils;
import org.chromium.chrome.browser.ntp_customization.NtpCustomizationUtils.NtpBackgroundImageType;
import org.chromium.chrome.browser.ntp_customization.R;
import org.chromium.chrome.browser.ntp_customization.theme.chrome_colors.NtpThemeColorInfo.NtpThemeColorId;
import org.chromium.ui.modelutil.PropertyModel;

/** Unit tests for {@link NtpChromeColorsCoordinator}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
@Features.EnableFeatures({ChromeFeatureList.NEW_TAB_PAGE_CUSTOMIZATION_V2})
public class NtpChromeColorsCoordinatorUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private BottomSheetDelegate mBottomSheetDelegate;
    @Mock private Runnable mOnChromeColorSelectedCallback;
    @Mock private View.OnClickListener mOnClickListener;
    @Mock private TextWatcher mTextWatcher;
    @Captor private ArgumentCaptor<TextWatcher> mTextWatcherCaptor;

    private NtpChromeColorsCoordinator mCoordinator;
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
                mPropertyModel.get(NtpChromeColorsProperties.LEARN_MORE_BUTTON_CLICK_LISTENER));
        assertNotNull(
                mPropertyModel.get(NtpChromeColorsProperties.BACKGROUND_COLOR_INPUT_TEXT_WATCHER));
        assertNotNull(
                mPropertyModel.get(NtpChromeColorsProperties.PRIMARY_COLOR_INPUT_TEXT_WATCHER));
        assertNotNull(mPropertyModel.get(NtpChromeColorsProperties.SAVE_BUTTON_CLICK_LISTENER));
        assertNotNull(
                mPropertyModel.get(
                        NtpChromeColorsProperties.DAILY_REFRESH_SWITCH_ON_CHECKED_CHANGE_LISTENER));
        assertEquals(
                NtpCustomizationUtils.getIsChromeColorDailyRefreshEnabledFromSharedPreference(),
                mPropertyModel.get(NtpChromeColorsProperties.IS_DAILY_REFRESH_SWITCH_CHECKED));
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
                mPropertyModel.get(NtpChromeColorsProperties.LEARN_MORE_BUTTON_CLICK_LISTENER));
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
        assertNull(mPropertyModel.get(NtpChromeColorsProperties.LEARN_MORE_BUTTON_CLICK_LISTENER));
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
        NtpCustomizationUtils.setIsChromeColorDailyRefreshEnabledToSharedPreference(true);
        createCoordinator();
        OnCheckedChangeListener dailyRefreshSwitchChangeListener =
                mPropertyModel.get(
                        NtpChromeColorsProperties.DAILY_REFRESH_SWITCH_ON_CHECKED_CHANGE_LISTENER);
        assertNotNull(dailyRefreshSwitchChangeListener);
        assertTrue(NtpCustomizationUtils.getIsChromeColorDailyRefreshEnabledFromSharedPreference());

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
        gridRecyclerView.setItemWidth(itemWidth);
        gridRecyclerView.setSpacing(spacing);

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
        NtpCustomizationUtils.setNtpThemeColorIdToSharedPreference(colorId);
        NtpCustomizationUtils.setNtpBackgroundImageTypeToSharedPreference(
                NtpBackgroundImageType.CHROME_COLOR);
        assertEquals(colorId, NtpCustomizationUtils.getNtpThemeColorIdFromSharedPreference());

        createCoordinator();
        assertEquals(
                colorInfo.primaryColorResId,
                mCoordinator.getPrimaryColorInfoForTesting().primaryColorResId);

        mCoordinator.onItemClicked(colorInfo);
        verify(mBottomSheetDelegate).onNewColorSelected(eq(false));
        verify(mOnChromeColorSelectedCallback).run();
        assertEquals(colorId, NtpCustomizationUtils.getNtpThemeColorIdFromSharedPreference());

        mCoordinator.onItemClicked(colorInfo1);
        verify(mBottomSheetDelegate).onNewColorSelected(eq(true));
        verify(mOnChromeColorSelectedCallback, times(2)).run();
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
                NtpCustomizationUtils.getPrimaryColorFromCustomizedThemeColor(mContext).intValue());
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

    private void createCoordinator() {
        clearInvocations(mBottomSheetDelegate);

        mCoordinator =
                new NtpChromeColorsCoordinator(
                        mContext, mBottomSheetDelegate, mOnChromeColorSelectedCallback);

        ArgumentCaptor<View> viewCaptor = ArgumentCaptor.forClass(View.class);
        verify(mBottomSheetDelegate)
                .registerBottomSheetLayout(eq(CHROME_COLORS), viewCaptor.capture());
        mBottomSheetView = viewCaptor.getValue();
        mPropertyModel = mCoordinator.getPropertyModelForTesting();
    }
}
