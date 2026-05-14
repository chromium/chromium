// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp_customization.theme.tip;

import static org.chromium.chrome.browser.ntp_customization.NtpCustomizationCoordinator.BottomSheetType.THEME_TIP;

import android.content.Context;
import android.view.LayoutInflater;
import android.view.View;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.ntp_customization.BottomSheetDelegate;
import org.chromium.chrome.browser.ntp_customization.R;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

/** Coordinator for the NTP theme tip bottom sheet. */
@NullMarked
public class NtpThemeTipCoordinator {
    private final View mView;
    private final PropertyModel mModel;

    /**
     * @param context The {@link Context} used to inflate the view.
     * @param delegate The {@link BottomSheetDelegate} to register the bottom sheet layout.
     * @param onClickListener The {@link View.OnClickListener} for the customize button.
     * @param dismissBottomSheet The {@link Runnable} to dismiss the bottom sheet.
     */
    public NtpThemeTipCoordinator(
            Context context,
            BottomSheetDelegate delegate,
            View.OnClickListener onClickListener,
            Runnable dismissBottomSheet) {
        mView =
                LayoutInflater.from(context)
                        .inflate(
                                R.layout.ntp_customization_theme_tip_bottom_sheet_layout,
                                null,
                                false);

        delegate.registerBottomSheetLayout(THEME_TIP, mView);

        mModel = new PropertyModel(NtpThemeTipProperties.ALL_KEYS);
        PropertyModelChangeProcessor.create(mModel, mView, NtpThemeTipViewBinder::bind);

        mModel.set(
                NtpThemeTipProperties.CANCEL_BUTTON_CLICK_LISTENER, v -> dismissBottomSheet.run());
        mModel.set(NtpThemeTipProperties.CUSTOMIZE_BUTTON_CLICK_LISTENER, onClickListener);
    }

    public void destroy() {
        mModel.set(NtpThemeTipProperties.CANCEL_BUTTON_CLICK_LISTENER, null);
        mModel.set(NtpThemeTipProperties.CUSTOMIZE_BUTTON_CLICK_LISTENER, null);
    }

    View getViewForTesting() {
        return mView;
    }

    PropertyModel getPropertyModelForTesting() {
        return mModel;
    }
}
