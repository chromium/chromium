// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp_customization.theme.chrome_colors;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.verify;

import static org.chromium.chrome.browser.ntp_customization.NtpCustomizationCoordinator.BottomSheetType.CHROME_COLORS;
import static org.chromium.chrome.browser.ntp_customization.NtpCustomizationCoordinator.BottomSheetType.THEME;

import android.content.Context;
import android.view.ContextThemeWrapper;
import android.view.View;
import android.view.View.MeasureSpec;
import android.widget.ImageView;

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
import org.chromium.chrome.browser.ntp_customization.BottomSheetDelegate;
import org.chromium.chrome.browser.ntp_customization.R;
import org.chromium.chrome.browser.ntp_customization.theme.chrome_colors.NtpChromeColorsCoordinator.ColorGridView;

/** Unit tests for {@link NtpChromeColorsCoordinator}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
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
        mCoordinator =
                new NtpChromeColorsCoordinator(
                        mContext, mBottomSheetDelegate, mOnChromeColorSelectedCallback);

        ArgumentCaptor<View> viewCaptor = ArgumentCaptor.forClass(View.class);
        verify(mBottomSheetDelegate)
                .registerBottomSheetLayout(eq(CHROME_COLORS), viewCaptor.capture());
        mBottomSheetView = viewCaptor.getValue();
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
}
