// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.dom_distiller;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.app.Activity;
import android.view.MotionEvent;
import android.view.View;
import android.widget.FrameLayout;

import androidx.core.view.AccessibilityDelegateCompat;
import androidx.core.view.ViewCompat;
import androidx.core.view.accessibility.AccessibilityNodeInfoCompat;
import androidx.test.filters.SmallTest;

import com.google.android.material.button.MaterialButton;
import com.google.android.material.slider.Slider;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Robolectric;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.base.test.util.UserActionTester;
import org.chromium.chrome.R;
import org.chromium.components.dom_distiller.core.DistilledPagePrefs;
import org.chromium.dom_distiller.mojom.FontFamily;
import org.chromium.dom_distiller.mojom.Theme;

/** Tests for the {@link ReaderModePrefsView} class. */
@RunWith(BaseRobolectricTestRunner.class)
public class ReaderModePrefsViewTest {

    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    private ReaderModePrefsView mReaderModePrefsView;
    private UserActionTester mActionTester;

    @Mock private DistilledPagePrefs mDistilledPagePrefs;

    @Before
    public void setUp() {
        Activity activity = Robolectric.buildActivity(Activity.class).create().get();
        activity.setTheme(R.style.Theme_BrowserUI_DayNight);
        FrameLayout parent = new FrameLayout(activity);

        when(mDistilledPagePrefs.getTheme()).thenReturn(Theme.LIGHT);
        when(mDistilledPagePrefs.getFontFamily()).thenReturn(FontFamily.SANS_SERIF);
        when(mDistilledPagePrefs.getFontScaling()).thenReturn(1.0f);

        mReaderModePrefsView = ReaderModePrefsView.create(parent.getContext(), mDistilledPagePrefs);
        mActionTester = new UserActionTester();
    }

    @After
    public void tearDown() {
        mActionTester.tearDown();
    }

    @Test
    @SmallTest
    public void testInitialState() {
        // Verify that the initial state of the view is correct.
        assertTrue(
                ((MaterialButton) mReaderModePrefsView.findViewById(R.id.light_mode)).isChecked());
        assertTrue(
                ((MaterialButton) mReaderModePrefsView.findViewById(R.id.font_sans_serif))
                        .isChecked());
        Slider slider = (Slider) mReaderModePrefsView.findViewById(R.id.font_size_slider);
        assertEquals(1.0f, slider.getValue(), 0.0f);
    }

    @Test
    @SmallTest
    public void testThemeButtons() {
        when(mDistilledPagePrefs.getTheme())
                .thenReturn(Theme.LIGHT, Theme.LIGHT, Theme.DARK, Theme.SEPIA);
        // Test clicking the already selected theme.
        HistogramWatcher histogramLight =
                HistogramWatcher.newBuilder()
                        .expectNoRecords("DomDistiller.Android.ThemeSelected")
                        .build();
        mReaderModePrefsView.findViewById(R.id.light_mode).performClick();
        verify(mDistilledPagePrefs, never()).setUserPrefTheme(Theme.LIGHT);
        Assert.assertEquals(0, mActionTester.getActionCount("DomDistiller.Android.ThemeChanged"));
        histogramLight.assertExpected();

        HistogramWatcher histogramDark =
                HistogramWatcher.newBuilder()
                        .expectIntRecord("DomDistiller.Android.ThemeSelected", Theme.DARK)
                        .build();
        mReaderModePrefsView.findViewById(R.id.dark_mode).performClick();
        verify(mDistilledPagePrefs).setUserPrefTheme(Theme.DARK);
        Assert.assertEquals(1, mActionTester.getActionCount("DomDistiller.Android.ThemeChanged"));
        histogramDark.assertExpected();

        HistogramWatcher histogramSepia =
                HistogramWatcher.newBuilder()
                        .expectIntRecord("DomDistiller.Android.ThemeSelected", Theme.SEPIA)
                        .build();
        mReaderModePrefsView.findViewById(R.id.sepia_mode).performClick();
        verify(mDistilledPagePrefs).setUserPrefTheme(Theme.SEPIA);
        Assert.assertEquals(2, mActionTester.getActionCount("DomDistiller.Android.ThemeChanged"));
        histogramSepia.assertExpected();

        // Test clicking the first option again.
        HistogramWatcher histogramLight2 =
                HistogramWatcher.newBuilder()
                        .expectIntRecord("DomDistiller.Android.ThemeSelected", Theme.LIGHT)
                        .build();
        mReaderModePrefsView.findViewById(R.id.light_mode).performClick();
        verify(mDistilledPagePrefs).setUserPrefTheme(Theme.LIGHT);
        Assert.assertEquals(3, mActionTester.getActionCount("DomDistiller.Android.ThemeChanged"));
        histogramLight2.assertExpected();
    }

    @Test
    @SmallTest
    public void testFontFamilyButtons() {
        when(mDistilledPagePrefs.getFontFamily())
                .thenReturn(
                        FontFamily.SANS_SERIF,
                        FontFamily.SANS_SERIF,
                        FontFamily.SERIF,
                        FontFamily.MONOSPACE);
        // Test clicking the already selected font family.
        HistogramWatcher histogramSansSerif =
                HistogramWatcher.newBuilder()
                        .expectNoRecords("DomDistiller.Android.FontFamilySelected")
                        .build();
        mReaderModePrefsView.findViewById(R.id.font_sans_serif).performClick();
        verify(mDistilledPagePrefs, never()).setFontFamily(FontFamily.SANS_SERIF);
        Assert.assertEquals(
                0, mActionTester.getActionCount("DomDistiller.Android.FontFamilyChanged"));
        histogramSansSerif.assertExpected();

        // Test clicking a new font family.
        HistogramWatcher histogramSerif =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(
                                "DomDistiller.Android.FontFamilySelected", FontFamily.SERIF)
                        .build();
        mReaderModePrefsView.findViewById(R.id.font_serif).performClick();
        verify(mDistilledPagePrefs).setFontFamily(FontFamily.SERIF);
        Assert.assertEquals(
                1, mActionTester.getActionCount("DomDistiller.Android.FontFamilyChanged"));
        histogramSerif.assertExpected();

        HistogramWatcher histogramMonospace =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(
                                "DomDistiller.Android.FontFamilySelected", FontFamily.MONOSPACE)
                        .build();
        mReaderModePrefsView.findViewById(R.id.font_monospace).performClick();
        verify(mDistilledPagePrefs).setFontFamily(FontFamily.MONOSPACE);
        Assert.assertEquals(
                2, mActionTester.getActionCount("DomDistiller.Android.FontFamilyChanged"));
        histogramMonospace.assertExpected();

        // Test clicking the first option again.
        HistogramWatcher histogramSansSerif2 =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(
                                "DomDistiller.Android.FontFamilySelected", FontFamily.SANS_SERIF)
                        .build();
        mReaderModePrefsView.findViewById(R.id.font_sans_serif).performClick();
        verify(mDistilledPagePrefs).setFontFamily(FontFamily.SANS_SERIF);
        Assert.assertEquals(
                3, mActionTester.getActionCount("DomDistiller.Android.FontFamilyChanged"));
        histogramSansSerif2.assertExpected();
    }

    @Test
    @SmallTest
    public void testFontScalingSlider() {
        HistogramWatcher histograms =
                HistogramWatcher.newBuilder()
                        .expectIntRecord("DomDistiller.Android.FontScalingSelected", 250)
                        .build();
        Slider slider = (Slider) mReaderModePrefsView.findViewById(R.id.font_size_slider);

        // Manually force abritary size on the slider for the purposes of simulating drag motion.
        slider.measure(
                View.MeasureSpec.makeMeasureSpec(400, View.MeasureSpec.EXACTLY),
                View.MeasureSpec.makeMeasureSpec(100, View.MeasureSpec.EXACTLY));
        slider.layout(0, 0, 400, 100);

        float toX = slider.getWidth();
        float middleY = slider.getHeight() / 2.0f;

        long downTime = System.currentTimeMillis();
        long eventTime = System.currentTimeMillis();

        // Dispatch ACTION_DOWN to start the drag.
        MotionEvent downEvent =
                MotionEvent.obtain(downTime, eventTime, MotionEvent.ACTION_DOWN, 0, middleY, 0);
        slider.dispatchTouchEvent(downEvent);

        // Dispatch ACTION_MOVE to simulate the drag.
        eventTime = System.currentTimeMillis();
        MotionEvent moveEvent =
                MotionEvent.obtain(downTime, eventTime, MotionEvent.ACTION_MOVE, toX, middleY, 0);
        slider.dispatchTouchEvent(moveEvent);

        // Dispatch ACTION_UP to end the drag.
        eventTime = System.currentTimeMillis();
        MotionEvent upEvent =
                MotionEvent.obtain(downTime, eventTime, MotionEvent.ACTION_UP, toX, middleY, 0);
        slider.dispatchTouchEvent(upEvent);

        // Verify that the listener was triggered with the new value.
        verify(mDistilledPagePrefs).setFontScaling(2.5f);
        Assert.assertEquals(
                1, mActionTester.getActionCount("DomDistiller.Android.FontScalingChanged"));
        histograms.assertExpected();
    }

    @Test
    @SmallTest
    public void testThemeButtonsAccessibility() {
        // Test container
        View themeContainer = mReaderModePrefsView.findViewById(R.id.theme_container);
        AccessibilityDelegateCompat containerDelegate =
                ViewCompat.getAccessibilityDelegate(themeContainer);
        AccessibilityNodeInfoCompat containerInfo = AccessibilityNodeInfoCompat.obtain();
        containerDelegate.onInitializeAccessibilityNodeInfo(themeContainer, containerInfo);

        AccessibilityNodeInfoCompat.CollectionInfoCompat collectionInfo =
                containerInfo.getCollectionInfo();
        assertNotNull(collectionInfo);
        assertEquals(3, collectionInfo.getColumnCount());
        assertEquals(1, collectionInfo.getRowCount());
        assertEquals(
                AccessibilityNodeInfoCompat.CollectionInfoCompat.SELECTION_MODE_SINGLE,
                collectionInfo.getSelectionMode());

        // Test buttons
        int[] buttonIds = new int[] {R.id.light_mode, R.id.sepia_mode, R.id.dark_mode};
        for (int i = 0; i < buttonIds.length; i++) {
            View button = mReaderModePrefsView.findViewById(buttonIds[i]);
            AccessibilityDelegateCompat buttonDelegate =
                    ViewCompat.getAccessibilityDelegate(button);
            AccessibilityNodeInfoCompat buttonInfo = AccessibilityNodeInfoCompat.obtain();
            buttonDelegate.onInitializeAccessibilityNodeInfo(button, buttonInfo);

            AccessibilityNodeInfoCompat.CollectionItemInfoCompat itemInfo =
                    buttonInfo.getCollectionItemInfo();
            assertNotNull(itemInfo);
            assertEquals(0, itemInfo.getRowIndex());
            assertEquals(i, itemInfo.getColumnIndex());
            assertEquals(((MaterialButton) button).isChecked(), itemInfo.isSelected());
        }
    }

    @Test
    @SmallTest
    public void testFontFamilyButtonsAccessibility() {
        // Test container
        View fontFamilyButtonContainer =
                mReaderModePrefsView.findViewById(R.id.font_family_button_container);
        AccessibilityDelegateCompat containerDelegate =
                ViewCompat.getAccessibilityDelegate(fontFamilyButtonContainer);
        AccessibilityNodeInfoCompat containerInfo = AccessibilityNodeInfoCompat.obtain();
        containerDelegate.onInitializeAccessibilityNodeInfo(
                fontFamilyButtonContainer, containerInfo);

        AccessibilityNodeInfoCompat.CollectionInfoCompat collectionInfo =
                containerInfo.getCollectionInfo();
        assertNotNull(collectionInfo);
        assertEquals(3, collectionInfo.getColumnCount());
        assertEquals(1, collectionInfo.getRowCount());
        assertEquals(
                AccessibilityNodeInfoCompat.CollectionInfoCompat.SELECTION_MODE_SINGLE,
                collectionInfo.getSelectionMode());

        // Test buttons
        int[] buttonIds = new int[] {R.id.font_sans_serif, R.id.font_serif, R.id.font_monospace};
        for (int i = 0; i < buttonIds.length; i++) {
            View button = mReaderModePrefsView.findViewById(buttonIds[i]);
            AccessibilityDelegateCompat buttonDelegate =
                    ViewCompat.getAccessibilityDelegate(button);
            AccessibilityNodeInfoCompat buttonInfo = AccessibilityNodeInfoCompat.obtain();
            buttonDelegate.onInitializeAccessibilityNodeInfo(button, buttonInfo);

            AccessibilityNodeInfoCompat.CollectionItemInfoCompat itemInfo =
                    buttonInfo.getCollectionItemInfo();
            assertNotNull(itemInfo);
            assertEquals(0, itemInfo.getRowIndex());
            assertEquals(i, itemInfo.getColumnIndex());
            assertEquals(((MaterialButton) button).isChecked(), itemInfo.isSelected());
        }
    }
}
