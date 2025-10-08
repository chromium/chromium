// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp_customization.theme.chrome_colors;

import static android.view.View.INVISIBLE;
import static android.view.View.VISIBLE;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.clearInvocations;
import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.verify;

import static org.chromium.chrome.browser.ntp_customization.NtpCustomizationCoordinator.BottomSheetType.CHROME_COLORS;
import static org.chromium.chrome.browser.ntp_customization.NtpCustomizationCoordinator.BottomSheetType.THEME;

import android.content.Context;
import android.graphics.Color;
import android.view.ContextThemeWrapper;
import android.view.View;
import android.view.View.MeasureSpec;
import android.widget.EditText;
import android.widget.ImageView;

import androidx.annotation.ColorInt;
import androidx.recyclerview.widget.GridLayoutManager;
import androidx.test.core.app.ApplicationProvider;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.ntp_customization.BottomSheetDelegate;
import org.chromium.chrome.browser.ntp_customization.NtpCustomizationUtils;
import org.chromium.chrome.browser.ntp_customization.NtpCustomizationUtils.NtpBackgroundImageType;
import org.chromium.chrome.browser.ntp_customization.R;
import org.chromium.chrome.browser.ntp_customization.theme.chrome_colors.NtpChromeColorsCoordinator.ColorGridView;
import org.chromium.chrome.browser.ntp_customization.theme.chrome_colors.NtpThemeColorInfo.NtpThemeColorId;
import org.chromium.ui.widget.ButtonCompat;

/** Unit tests for {@link NtpChromeColorsCoordinator}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
@Features.EnableFeatures({ChromeFeatureList.NEW_TAB_PAGE_CUSTOMIZATION_V2})
public class NtpChromeColorsCoordinatorUnitTest {

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private BottomSheetDelegate mBottomSheetDelegate;
    @Mock private Runnable mOnChromeColorSelectedCallback;

    private NtpChromeColorsCoordinator mCoordinator;
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
        View backButton = mBottomSheetView.findViewById(R.id.back_button);
        ImageView learnMoreButton = mBottomSheetView.findViewById(R.id.learn_more_button);

        assertTrue(backButton.hasOnClickListeners());
        assertTrue(learnMoreButton.hasOnClickListeners());

        mCoordinator.destroy();

        assertFalse(backButton.hasOnClickListeners());
        assertFalse(learnMoreButton.hasOnClickListeners());
    }

    @Test
    @Features.EnableFeatures(
            ChromeFeatureList.NEW_TAB_PAGE_CUSTOMIZATION_V2 + ":show_color_picker/true")
    public void testDestroy_colorPickerView() {
        createCoordinator();
        ButtonCompat saveColorButton = mBottomSheetView.findViewById(R.id.save_color_button);
        assertTrue(saveColorButton.hasOnClickListeners());

        mCoordinator.destroy();
        assertFalse(saveColorButton.hasOnClickListeners());
    }

    @Test
    public void testColorGridView_onMeasure() {
        ColorGridView gridView = new ColorGridView(mContext, null);
        GridLayoutManager layoutManager = spy(new GridLayoutManager(mContext, 1));
        gridView.setLayoutManager(layoutManager);

        int itemWidth = 50;
        int spacing = 10;
        gridView.init(itemWidth, spacing);

        // Test case 1: width allows for exactly 3 items
        int width1 = 3 * (itemWidth + spacing);
        gridView.measure(
                MeasureSpec.makeMeasureSpec(width1, MeasureSpec.EXACTLY),
                MeasureSpec.makeMeasureSpec(100, MeasureSpec.EXACTLY));
        verify(layoutManager).setSpanCount(eq(3));

        // Test case 2: width allows for 2.5 items, should round down to 2
        int width2 = 2 * (itemWidth + spacing) + (itemWidth / 2);
        gridView.measure(
                MeasureSpec.makeMeasureSpec(width2, MeasureSpec.EXACTLY),
                MeasureSpec.makeMeasureSpec(100, MeasureSpec.EXACTLY));
        verify(layoutManager).setSpanCount(eq(2));

        // Test case 3: width allows for less than 1 item, should be 1
        int width3 = itemWidth / 2;
        gridView.measure(
                MeasureSpec.makeMeasureSpec(width3, MeasureSpec.EXACTLY),
                MeasureSpec.makeMeasureSpec(100, MeasureSpec.EXACTLY));
        verify(layoutManager).setSpanCount(eq(1));

        // Test case 4: width is same as before, should not change span count.
        gridView.measure(
                MeasureSpec.makeMeasureSpec(width3, MeasureSpec.EXACTLY),
                MeasureSpec.makeMeasureSpec(100, MeasureSpec.EXACTLY));
        verify(layoutManager).setSpanCount(eq(1));
    }

    @Test
    public void testOnItemClicked_callsCallback() {
        ColorGridView gridView = mBottomSheetView.findViewById(R.id.chrome_colors_recycler_view);
        NtpChromeColorsAdapter adapter = (NtpChromeColorsAdapter) gridView.getAdapter();
        assertNotNull(adapter);

        // Click the first item.
        adapter.getOnItemClickedCallbackForTesting().onResult(adapter.getColorsForTesting().get(0));

        // Verify the callback is called.
        verify(mOnChromeColorSelectedCallback).run();
    }

    @Test
    public void testOnItemClicked_noPrimaryColorSelected() {
        @Nullable NtpThemeColorInfo primaryColorInfo = mCoordinator.getPrimaryColorInfoForTesting();

        assertNull(primaryColorInfo);
        NtpThemeColorInfo colorInfo =
                NtpThemeColorUtils.createNtpThemeColorInfo(
                        mContext, NtpThemeColorInfo.NtpThemeColorId.BLUE);
        mCoordinator.onItemClicked(colorInfo);

        verify(mBottomSheetDelegate).onNewColorSelected(eq(true));
    }

    @Test
    @Features.EnableFeatures({ChromeFeatureList.NEW_TAB_PAGE_CUSTOMIZATION_V2})
    public void testOnItemClicked_withPrimaryColorSelected() {
        @NtpThemeColorId int colorId = NtpThemeColorId.BLUE;
        NtpThemeColorInfo colorInfo = NtpThemeColorUtils.createNtpThemeColorInfo(mContext, colorId);
        NtpThemeColorInfo colorInfo1 =
                NtpThemeColorUtils.createNtpThemeColorInfo(
                        mContext, NtpThemeColorInfo.NtpThemeColorId.LIGHT_BLUE);
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

        mCoordinator.onItemClicked(colorInfo1);
        verify(mBottomSheetDelegate).onNewColorSelected(eq(true));

        NtpCustomizationUtils.resetSharedPreferenceForTesting();
    }

    @Test
    @Features.EnableFeatures(
            ChromeFeatureList.NEW_TAB_PAGE_CUSTOMIZATION_V2 + ":show_color_picker/true")
    public void testCustomColorPicker_saveButton() {
        createCoordinator();
        View customColorPickerContainer =
                mBottomSheetView.findViewById(R.id.custom_color_picker_container);
        assertEquals(View.VISIBLE, customColorPickerContainer.getVisibility());

        EditText backgroundColorInput = mBottomSheetView.findViewById(R.id.background_color_input);
        EditText primaryColorInput = mBottomSheetView.findViewById(R.id.primary_color_input);
        ButtonCompat saveColorButton = mBottomSheetView.findViewById(R.id.save_color_button);

        String backgroundColorHex = "#FF0000";
        String primaryColorHex = "#00FF00";
        @ColorInt int backgroundColor = Color.parseColor(backgroundColorHex);
        @ColorInt int primaryColor = Color.parseColor(primaryColorHex);
        backgroundColorInput.setText(backgroundColorHex);
        primaryColorInput.setText(primaryColorHex);

        // Verifies that both background color and primary color are saved to the SharedPreference.
        saveColorButton.performClick();
        assertEquals(
                backgroundColor, NtpCustomizationUtils.getBackgroundColorFromSharedPreference(-1));
        assertEquals(
                primaryColor,
                NtpCustomizationUtils.getPrimaryColorFromCustomizedThemeColor(mContext).intValue());
    }

    @Test
    @Features.EnableFeatures(
            ChromeFeatureList.NEW_TAB_PAGE_CUSTOMIZATION_V2 + ":show_color_picker/true")
    public void testsUpdateColorCircle() {
        createCoordinator();
        ImageView colorCircleImageView = mCoordinator.getBackgroundColorCircleImageViewForTesting();
        assertNotNull(colorCircleImageView);
        assertEquals(INVISIBLE, colorCircleImageView.getVisibility());

        // Verifies null is returned for an invalid hexadecimal string.
        assertNull(mCoordinator.updateColorCircle("FF00", colorCircleImageView));
        assertEquals(INVISIBLE, colorCircleImageView.getVisibility());

        // Verifies the method returns the expected color value for a valid hexadecimal string.
        String colorHex = "#FF0000";
        @ColorInt int color = Color.parseColor(colorHex);
        assertEquals(
                color, mCoordinator.updateColorCircle(colorHex, colorCircleImageView).intValue());
        assertEquals(VISIBLE, colorCircleImageView.getVisibility());

        // Verifies the missing "#" will be added automatically, and the method returns the expected
        // color value.
        colorHex = "FF0000";
        assertEquals(
                color, mCoordinator.updateColorCircle(colorHex, colorCircleImageView).intValue());
        assertEquals(VISIBLE, colorCircleImageView.getVisibility());
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
    }
}
