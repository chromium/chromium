// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.dom_distiller;

import android.app.Activity;

import androidx.appcompat.app.AlertDialog;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;

import org.chromium.base.metrics.RecordUserAction;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ActivityUtils;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabUtils;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetControllerProvider;
import org.chromium.components.dom_distiller.core.DomDistillerFeatures;
import org.chromium.content_public.browser.WebContents;

/** Java implementation of dom_distiller::android::DistillerUIHandleAndroid. */
@JNINamespace("dom_distiller::android")
@NullMarked
public final class DomDistillerUiUtils {
    /**
     * A static method for native code to call to open the distiller UI settings.
     *
     * @param webContents The WebContents containing the distilled content.
     */
    @CalledByNative
    public static void openSettings(@Nullable WebContents webContents) {
        if (webContents == null) return;

        if (DomDistillerFeatures.sReaderModeDistillInApp.isEnabled()) {
            openSettingsInBottomSheet(
                    TabUtils.fromWebContents(webContents), /* showFullSheet= */ true);
        } else {
            openSettingsInDialog(webContents);
        }
    }

    /**
     * Opens the settings in a bottom-sheet.
     *
     * @param tab The tab in which the bottomsheet is shown.
     * @param showFullSheet Whether the bottomsheet should be shown fully, if false it's shown in a
     *     peeked state.
     */
    public static void openSettingsInBottomSheet(@Nullable Tab tab, boolean showFullSheet) {
        assert DomDistillerFeatures.sReaderModeDistillInApp.isEnabled();
        if (tab == null || tab.getWindowAndroid() == null) return;

        BottomSheetController bottomSheetController =
                BottomSheetControllerProvider.from(tab.getWindowAndroid());
        if (bottomSheetController == null) return;

        ReaderModeBottomSheetCoordinator readerModeBottomSheetCoordinator =
                new ReaderModeBottomSheetCoordinator(
                        tab.getContext(), tab.getProfile(), bottomSheetController);
        readerModeBottomSheetCoordinator.show(showFullSheet);
    }

    static void openSettingsInDialog(WebContents webContents) {
        Activity activity = ActivityUtils.getActivityFromWebContents(webContents);
        if (activity == null) return;

        RecordUserAction.record("DomDistiller.Android.DistilledPagePrefsOpened");
        AlertDialog.Builder builder =
                new AlertDialog.Builder(activity, R.style.ThemeOverlay_BrowserUI_AlertDialog);
        builder.setView(
                DistilledPagePrefsView.create(
                        activity,
                        DomDistillerServiceFactory.getForProfile(
                                        Profile.fromWebContents(webContents))
                                .getDistilledPagePrefs()));
        builder.show();
    }

    private DomDistillerUiUtils() {}
}
