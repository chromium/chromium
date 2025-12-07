// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.accessibility.settings;

import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;

/** Unit tests for {@link AccessibilitySettingsBridge}. */
@RunWith(ChromeJUnit4ClassRunner.class)
@Batch(Batch.UNIT_TESTS)
public class AccessibilitySettingsBridgeTest {
    @Mock private Profile mProfile;
    @Mock private AccessibilitySettingsBridge.Natives mAccessibilitySettingsBridgeJni;

    @Rule // initialize mocks
    public MockitoRule rule = MockitoJUnit.rule();

    @Before
    public void setUp() {
        AccessibilitySettingsBridgeJni.setInstanceForTesting(mAccessibilitySettingsBridgeJni);
    }

    @Test
    @SmallTest
    public void testIsCaretBrowsingEnabled() {
        when(mAccessibilitySettingsBridgeJni.isCaretBrowsingEnabled(mProfile)).thenReturn(true);
        Assert.assertTrue(AccessibilitySettingsBridge.isCaretBrowsingEnabled(mProfile));

        when(mAccessibilitySettingsBridgeJni.isCaretBrowsingEnabled(mProfile)).thenReturn(false);
        Assert.assertFalse(AccessibilitySettingsBridge.isCaretBrowsingEnabled(mProfile));
    }

    @Test
    @SmallTest
    public void testSetCaretBrowsingEnabledToTrue() {
        HistogramWatcher histogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        AccessibilitySettingsBridge.ACCESSIBILITY_CARET_BROWING_HISTOGRAM,
                        AccessibilitySettingsBridge.AccessibilityCaretBrowsingAction.ENABLED);

        AccessibilitySettingsBridge.setCaretBrowsingEnabled(mProfile, true);
        verify(mAccessibilitySettingsBridgeJni).setCaretBrowsingEnabled(mProfile, true);
        histogramWatcher.assertExpected();
    }

    @Test
    @SmallTest
    public void testSetCaretBrowsingEnabledToFalse() {
        HistogramWatcher histogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        AccessibilitySettingsBridge.ACCESSIBILITY_CARET_BROWING_HISTOGRAM,
                        AccessibilitySettingsBridge.AccessibilityCaretBrowsingAction.DISABLED);
        AccessibilitySettingsBridge.setCaretBrowsingEnabled(mProfile, false);
        verify(mAccessibilitySettingsBridgeJni).setCaretBrowsingEnabled(mProfile, false);
        histogramWatcher.assertExpected();
    }

    @Test
    @SmallTest
    public void testSetShowCaretBrowsingDialogPreference() {
        AccessibilitySettingsBridge.setShowCaretBrowsingDialogPreference(mProfile, true);
        verify(mAccessibilitySettingsBridgeJni)
                .setShowCaretBrowsingDialogPreference(mProfile, true);

        AccessibilitySettingsBridge.setShowCaretBrowsingDialogPreference(mProfile, false);
        verify(mAccessibilitySettingsBridgeJni)
                .setShowCaretBrowsingDialogPreference(mProfile, false);
    }

    @Test
    @SmallTest
    public void testIsShowCaretBrowsingDialogPreference() {
        when(mAccessibilitySettingsBridgeJni.isShowCaretBrowsingDialogPreference(mProfile))
                .thenReturn(true);
        Assert.assertTrue(
                AccessibilitySettingsBridge.isShowCaretBrowsingDialogPreference(mProfile));

        when(mAccessibilitySettingsBridgeJni.isShowCaretBrowsingDialogPreference(mProfile))
                .thenReturn(false);
        Assert.assertFalse(
                AccessibilitySettingsBridge.isShowCaretBrowsingDialogPreference(mProfile));
    }
}
