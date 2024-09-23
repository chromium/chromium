// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.lens;

import androidx.annotation.IntDef;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.metrics.RecordUserAction;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/** Static utility methods to support user action logging for Lens entry points. */
public class LensMetrics {
    public static final String AMBIENT_SEARCH_QUERY_HISTOGRAM = "Search.Ambient.Query";
    public static final String SEARCH_CAMERA_OPEN_HISTOGRAM = "Search.Image.Camera.Open";

    // Note: these values must match the LensSupportStatus enum in enums.xml.
    // Only add new values at the end, right before NUM_ENTRIES.
    @IntDef({
        LensSupportStatus.LENS_SEARCH_SUPPORTED,
        LensSupportStatus.NON_GOOGLE_SEARCH_ENGINE,
        LensSupportStatus.ACTIVITY_NOT_ACCESSIBLE,
        LensSupportStatus.OUT_OF_DATE,
        LensSupportStatus.SEARCH_BY_IMAGE_UNAVAILABLE,
        LensSupportStatus.LEGACY_OS,
        LensSupportStatus.INVALID_PACKAGE,
        LensSupportStatus.LENS_SHOP_SUPPORTED,
        LensSupportStatus.LENS_SHOP_AND_SEARCH_SUPPORTED,
        LensSupportStatus.CAMERA_NOT_AVAILABLE,
        LensSupportStatus.DISABLED_ON_LOW_END_DEVICE,
        LensSupportStatus.AGSA_VERSION_NOT_SUPPORTED,
        LensSupportStatus.DISABLED_ON_INCOGNITO,
        LensSupportStatus.DISABLED_ON_TABLET,
        LensSupportStatus.DISABLED_FOR_ENTERPRISE_USER
    })
    @Retention(RetentionPolicy.SOURCE)
    public static @interface LensSupportStatus {
        int LENS_SEARCH_SUPPORTED = 0;
        int NON_GOOGLE_SEARCH_ENGINE = 1;
        int ACTIVITY_NOT_ACCESSIBLE = 2;
        int OUT_OF_DATE = 3;
        int SEARCH_BY_IMAGE_UNAVAILABLE = 4;
        int LEGACY_OS = 5;
        int INVALID_PACKAGE = 6;
        int LENS_SHOP_SUPPORTED = 7;
        int LENS_SHOP_AND_SEARCH_SUPPORTED = 8;
        int CAMERA_NOT_AVAILABLE = 9;
        int DISABLED_ON_LOW_END_DEVICE = 10;
        int AGSA_VERSION_NOT_SUPPORTED = 11;
        int DISABLED_ON_INCOGNITO = 12;
        int DISABLED_ON_TABLET = 13;
        int DISABLED_FOR_ENTERPRISE_USER = 14;
        int NUM_ENTRIES = 15;
    }

    // Note: These values must match the AmbientSearchEntryPoint enum in enums.xml.
    // Only add new values at the end, right before NUM_ENTRIES.
    @IntDef({
        AmbientSearchEntryPoint.CONTEXT_MENU_SEARCH_IMAGE_WITH_GOOGLE_LENS,
        AmbientSearchEntryPoint.CONTEXT_MENU_SEARCH_IMAGE_WITH_WEB,
        AmbientSearchEntryPoint.CONTEXT_MENU_SEARCH_REGION_WITH_GOOGLE_LENS,
        AmbientSearchEntryPoint.CONTEXT_MENU_SEARCH_REGION_WITH_WEB,
        AmbientSearchEntryPoint.CONTEXT_MENU_SEARCH_WEB_FOR,
        AmbientSearchEntryPoint.OMNIBOX,
        AmbientSearchEntryPoint.NEW_TAB_PAGE,
        AmbientSearchEntryPoint.QUICK_ACTION_SEARCH_WIDGET,
        AmbientSearchEntryPoint.KEYBOARD,
        AmbientSearchEntryPoint.SPOTLIGHT,
        AmbientSearchEntryPoint.APP_ICON_LONG_PRESS,
        AmbientSearchEntryPoint.PLUS_BUTTON,
        AmbientSearchEntryPoint.WEB_SEARCH_BAR,
        AmbientSearchEntryPoint.COMPANION_REGION_SEARCH,
        AmbientSearchEntryPoint.TRANSLATE_ONEBOX,
        AmbientSearchEntryPoint.INTENTS,
        AmbientSearchEntryPoint.WEB_IMAGES_SEARCH_BAR,
        AmbientSearchEntryPoint.WHATS_NEW_PROMO,
        AmbientSearchEntryPoint.NUM_ENTRIES
    })
    @Retention(RetentionPolicy.SOURCE)
    public static @interface AmbientSearchEntryPoint {
        int CONTEXT_MENU_SEARCH_IMAGE_WITH_GOOGLE_LENS = 0;
        int CONTEXT_MENU_SEARCH_IMAGE_WITH_WEB = 1;
        int CONTEXT_MENU_SEARCH_REGION_WITH_GOOGLE_LENS = 2;
        int CONTEXT_MENU_SEARCH_REGION_WITH_WEB = 3;
        int CONTEXT_MENU_SEARCH_WEB_FOR = 4;
        int OMNIBOX = 5;
        int NEW_TAB_PAGE = 6;
        int QUICK_ACTION_SEARCH_WIDGET = 7;
        int KEYBOARD = 8;
        int SPOTLIGHT = 9;
        int APP_ICON_LONG_PRESS = 10;
        int PLUS_BUTTON = 11;
        int WEB_SEARCH_BAR = 12;
        int COMPANION_REGION_SEARCH = 13;
        int TRANSLATE_ONEBOX = 14;
        int INTENTS = 15;
        int WEB_IMAGES_SEARCH_BAR = 16;
        int WHATS_NEW_PROMO = 17;
        int NUM_ENTRIES = 18;
    }

    // Note: These values must match the CameraOpenEntryPoint enum in enums.xml.
    // Only add new values at the end, right before NUM_ENTRIES.
    @IntDef({
        CameraOpenEntryPoint.OMNIBOX,
        CameraOpenEntryPoint.NEW_TAB_PAGE,
        CameraOpenEntryPoint.QUICK_ACTION_SEARCH_WIDGET,
        CameraOpenEntryPoint.TASKS_SURFACE,
        CameraOpenEntryPoint.KEYBOARD,
        CameraOpenEntryPoint.SPOTLIGHT,
        CameraOpenEntryPoint.APP_ICON_LONG_PRESS,
        CameraOpenEntryPoint.PLUS_BUTTON,
        CameraOpenEntryPoint.WEB_SEARCH_BAR,
        CameraOpenEntryPoint.TRANSLATE_ONEBOX,
        CameraOpenEntryPoint.INTENTS,
        CameraOpenEntryPoint.WEB_IMAGES_SEARCH_BAR,
        CameraOpenEntryPoint.WHATS_NEW_PROMO,
        CameraOpenEntryPoint.GOOGLE_BOTTOM_BAR,
        CameraOpenEntryPoint.NUM_ENTRIES
    })
    @Retention(RetentionPolicy.SOURCE)
    public static @interface CameraOpenEntryPoint {
        int OMNIBOX = 0;
        int NEW_TAB_PAGE = 1;
        int QUICK_ACTION_SEARCH_WIDGET = 2;
        int TASKS_SURFACE = 3;
        int KEYBOARD = 4;
        int SPOTLIGHT = 5;
        int APP_ICON_LONG_PRESS = 6;
        int PLUS_BUTTON = 7;
        int WEB_SEARCH_BAR = 8;
        int TRANSLATE_ONEBOX = 9;
        int INTENTS = 10;
        int WEB_IMAGES_SEARCH_BAR = 11;
        int WHATS_NEW_PROMO = 12;
        int GOOGLE_BOTTOM_BAR = 13;
        int NUM_ENTRIES = 14;
    }

    /** Record an ambient search query along with the entry point that initiated. */
    public static void recordAmbientSearchQuery(@AmbientSearchEntryPoint int entryPoint) {
        RecordHistogram.recordEnumeratedHistogram(
                AMBIENT_SEARCH_QUERY_HISTOGRAM, entryPoint, AmbientSearchEntryPoint.NUM_ENTRIES);
    }

    /** Record an intent sent to Lens to open the viewfinder with the entry point used. */
    public static void recordCameraOpen(@CameraOpenEntryPoint int entryPoint) {
        RecordHistogram.recordEnumeratedHistogram(
                SEARCH_CAMERA_OPEN_HISTOGRAM, entryPoint, CameraOpenEntryPoint.NUM_ENTRIES);
    }

    /** Record Lens support status for a Lens entry point. */
    public static void recordLensSupportStatus(
            String histogramName, @LensSupportStatus int reason) {
        RecordHistogram.recordEnumeratedHistogram(
                histogramName, reason, LensSupportStatus.NUM_ENTRIES);
    }

    /**
     * @return The histogram name for a Lens entry point.
     */
    public static String getSupportStatusHistogramNameByEntryPoint(
            @LensEntryPoint int lensEntryPoint) {
        switch (lensEntryPoint) {
            case LensEntryPoint.NEW_TAB_PAGE:
            case LensEntryPoint.OMNIBOX:
            case LensEntryPoint.TASKS_SURFACE:
                // Combine the LensSupportStatus reporting into one histogram for omnibox
                // entry points.
                return "Lens.Omnibox.LensSupportStatus";
            case LensEntryPoint.CONTEXT_MENU_SEARCH_MENU_ITEM:
            case LensEntryPoint.CONTEXT_MENU_SHOP_MENU_ITEM:
                return "ContextMenu.LensSupportStatus";
            case LensEntryPoint.QUICK_ACTION_SEARCH_WIDGET:
                return "Lens.QuickActionSearchWidget.LensSupportStatus";
            case LensEntryPoint.GOOGLE_BOTTOM_BAR:
                return "CustomTabs.GoogleBottomBar.LensSupportStatus";
            case LensEntryPoint.CONTEXT_MENU_CHIP:
            default:
                assert false : "Method not implemented.";
        }
        return null;
    }

    /** Record the time spent between Lens started and Lens dismissed. */
    public static void recordTimeSpentInLens(String histogramName, long timeSpentInLensMs) {
        RecordHistogram.recordLongTimesHistogram(histogramName, timeSpentInLensMs);
    }

    /**
     * Record when the Lens entry point is shown.
     * @param lensEntryPoint The entry point to be recorded.
     * @param isShown Whether the entry point is shown.
     */
    public static void recordShown(@LensEntryPoint int lensEntryPoint, boolean isShown) {
        if (!isShown) {
            return;
        }
        String actionName = getShownActionName(lensEntryPoint);
        if (actionName != null) RecordUserAction.record(actionName);
    }

    /**
     * Record when the Lens entry point is clicked.
     * @param lensEntryPoint The entry point to be recorded.
     */
    public static void recordClicked(@LensEntryPoint int lensEntryPoint) {
        String actionName = getClickedActionName(lensEntryPoint);
        if (actionName != null) RecordUserAction.record(actionName);
    }

    /** Record when the Lens entry point is shown and omnibox is focused. */
    public static void recordOmniboxFocusedWhenLensShown() {
        RecordUserAction.record("MobileOmniboxFocusedLensShown");
    }

    private static String getShownActionName(@LensEntryPoint int lensEntryPoint) {
        switch (lensEntryPoint) {
            case LensEntryPoint.NEW_TAB_PAGE:
                return "NewTabPage.SearchBox.LensShown";
            case LensEntryPoint.OMNIBOX:
                return "MobileOmniboxLensShown";
            case LensEntryPoint.TASKS_SURFACE:
                return "TasksSurface.FakeBox.LensShown";
            case LensEntryPoint.CONTEXT_MENU_SEARCH_MENU_ITEM:
            case LensEntryPoint.CONTEXT_MENU_SHOP_MENU_ITEM:
            case LensEntryPoint.CONTEXT_MENU_CHIP:
            case LensEntryPoint.GOOGLE_BOTTOM_BAR:
            default:
                assert false : "Method not implemented.";
        }
        return null;
    }

    private static String getClickedActionName(@LensEntryPoint int lensEntryPoint) {
        switch (lensEntryPoint) {
            case LensEntryPoint.NEW_TAB_PAGE:
                return "NewTabPage.SearchBox.Lens";
            case LensEntryPoint.OMNIBOX:
                return "MobileOmniboxLens";
            case LensEntryPoint.TASKS_SURFACE:
                return "TasksSurface.FakeBox.Lens";
            case LensEntryPoint.CONTEXT_MENU_SEARCH_MENU_ITEM:
            case LensEntryPoint.CONTEXT_MENU_SHOP_MENU_ITEM:
            case LensEntryPoint.CONTEXT_MENU_CHIP:
            case LensEntryPoint.GOOGLE_BOTTOM_BAR:
            default:
                assert false : "Method not implemented.";
        }
        return null;
    }
}
