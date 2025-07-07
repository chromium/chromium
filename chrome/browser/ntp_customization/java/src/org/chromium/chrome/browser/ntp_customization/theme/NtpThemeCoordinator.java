// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp_customization.theme;

import static org.chromium.chrome.browser.ntp_customization.NtpCustomizationCoordinator.BottomSheetType.THEME;
import static org.chromium.chrome.browser.ntp_customization.theme.NtpThemeProperty.THEME_KEYS;

import android.content.Context;
import android.support.annotation.IntDef;
import android.view.LayoutInflater;

import androidx.activity.ComponentActivity;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.ntp_customization.BottomSheetDelegate;
import org.chromium.chrome.browser.ntp_customization.BottomSheetViewBinder;
import org.chromium.chrome.browser.ntp_customization.NtpCustomizationConfigManager;
import org.chromium.chrome.browser.ntp_customization.NtpCustomizationViewProperties;
import org.chromium.chrome.browser.ntp_customization.R;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/** Coordinator for the NTP appearance settings bottom sheet in the NTP customization. */
@NullMarked
public class NtpThemeCoordinator {

    /** NTP appearance sections that are shown in the theme bottom sheet. */
    @IntDef({
        NTPThemeBottomSheetSection.CHROME_DEFAULT,
        NTPThemeBottomSheetSection.UPLOAD_AN_IMAGE,
        NTPThemeBottomSheetSection.CHROME_COLORS,
        NTPThemeBottomSheetSection.THEME_COLLECTIONS,
        NTPThemeBottomSheetSection.NUM_ENTRIES
    })
    @Retention(RetentionPolicy.SOURCE)
    public @interface NTPThemeBottomSheetSection {
        int CHROME_DEFAULT = 0;
        int UPLOAD_AN_IMAGE = 1;
        int CHROME_COLORS = 2;
        int THEME_COLLECTIONS = 3;
        int NUM_ENTRIES = 4;
    }

    private NtpThemeMediator mMediator;
    private NtpThemeBottomSheetView mNtpThemeBottomSheetView;

    public NtpThemeCoordinator(Context context, BottomSheetDelegate delegate, Profile profile) {
        mNtpThemeBottomSheetView =
                (NtpThemeBottomSheetView)
                        LayoutInflater.from(context)
                                .inflate(
                                        R.layout.ntp_customization_theme_bottom_sheet_layout,
                                        null,
                                        false);

        delegate.registerBottomSheetLayout(THEME, mNtpThemeBottomSheetView);

        // The bottomSheetPropertyModel is responsible for managing the back press handler of the
        // back button in the bottom sheet.
        PropertyModel bottomSheetPropertyModel =
                new PropertyModel(NtpCustomizationViewProperties.BOTTOM_SHEET_KEYS);
        PropertyModelChangeProcessor.create(
                bottomSheetPropertyModel, mNtpThemeBottomSheetView, BottomSheetViewBinder::bind);

        // The themePropertyModel is responsible for managing the learn more button in the theme
        // bottom sheet.
        PropertyModel themePropertyModel = new PropertyModel(THEME_KEYS);
        PropertyModelChangeProcessor.create(
                themePropertyModel,
                mNtpThemeBottomSheetView,
                NtpThemeViewBinder::bindThemeBottomSheet);

        var activityResultRegistry =
                context instanceof ComponentActivity
                        ? ((ComponentActivity) context).getActivityResultRegistry()
                        : null;

        mMediator =
                new NtpThemeMediator(
                        context,
                        bottomSheetPropertyModel,
                        themePropertyModel,
                        delegate,
                        profile,
                        NtpCustomizationConfigManager.getInstance(),
                        activityResultRegistry);
    }

    public void destroy() {
        mMediator.destroy();
        mNtpThemeBottomSheetView.destroy();
    }

    NtpThemeMediator getMediatorForTesting() {
        return mMediator;
    }

    void setMediatorForTesting(NtpThemeMediator mediator) {
        mMediator = mediator;
    }

    void setNtpThemeBottomSheetViewForTesting(NtpThemeBottomSheetView ntpThemeBottomSheetView) {
        mNtpThemeBottomSheetView = ntpThemeBottomSheetView;
    }
}
