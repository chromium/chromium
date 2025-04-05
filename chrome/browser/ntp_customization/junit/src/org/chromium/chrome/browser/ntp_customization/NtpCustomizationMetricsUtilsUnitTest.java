// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp_customization;

import static org.chromium.chrome.browser.magic_stack.ModuleDelegate.ModuleType.PRICE_CHANGE;
import static org.chromium.chrome.browser.ntp_customization.NtpCustomizationCoordinator.BottomSheetType.MAIN;
import static org.chromium.chrome.browser.ntp_customization.NtpCustomizationCoordinator.BottomSheetType.NTP_CARDS;
import static org.chromium.chrome.browser.ntp_customization.NtpCustomizationCoordinator.EntryPointType.MAIN_MENU;
import static org.chromium.chrome.browser.ntp_customization.NtpCustomizationCoordinator.EntryPointType.TOOL_BAR;

import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.chrome.browser.magic_stack.ModuleDelegate;
import org.chromium.chrome.browser.ntp_customization.NtpCustomizationCoordinator.BottomSheetType;

/** Unit tests for {@link NtpCustomizationMetricsUtils} */
@RunWith(BaseRobolectricTestRunner.class)
public class NtpCustomizationMetricsUtilsUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Test
    public void testRecordBottomSheetShown() {
        String histogramName = "NewTabPage.Customization.BottomSheet.Shown";
        @BottomSheetType int[] bottomSheetTypes = new int[] {MAIN, NTP_CARDS};

        for (@BottomSheetType int type : bottomSheetTypes) {
            HistogramWatcher histogramWatcher =
                    HistogramWatcher.newSingleRecordWatcher(histogramName, type);
            NtpCustomizationMetricsUtils.recordBottomSheetShown(type);
            histogramWatcher.assertExpected();
        }
    }

    @Test
    public void testRecordOpenBottomSheetEntry() {
        String histogramName = "NewTabPage.Customization.OpenBottomSheetEntry";
        @NtpCustomizationCoordinator.EntryPointType
        int[] entryPointTypes = new int[] {MAIN_MENU, TOOL_BAR};

        for (@NtpCustomizationCoordinator.EntryPointType int type : entryPointTypes) {
            HistogramWatcher histogramWatcher =
                    HistogramWatcher.newSingleRecordWatcher(histogramName, type);
            NtpCustomizationMetricsUtils.recordOpenBottomSheetEntry(type);
            histogramWatcher.assertExpected();
        }
    }

    @Test
    public void testRecordModuleToggledInBottomSheet() {
        @ModuleDelegate.ModuleType int moduleType = PRICE_CHANGE;
        boolean isEnabled = true;
        String histogramName = "NewTabPage.Customization.TurnOnModule";

        HistogramWatcher histogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(histogramName, moduleType);
        NtpCustomizationMetricsUtils.recordModuleToggledInBottomSheet(moduleType, isEnabled);
        histogramWatcher.assertExpected();

        isEnabled = false;
        histogramName = "NewTabPage.Customization.TurnOffModule";

        histogramWatcher = HistogramWatcher.newSingleRecordWatcher(histogramName, moduleType);
        NtpCustomizationMetricsUtils.recordModuleToggledInBottomSheet(moduleType, isEnabled);
        histogramWatcher.assertExpected();
    }
}
