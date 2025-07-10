// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp_customization.most_visited_tiles;

import static org.chromium.chrome.browser.ntp_customization.NtpCustomizationCoordinator.BottomSheetType.MVT;

import android.content.Context;
import android.view.LayoutInflater;
import android.view.View;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.ntp_customization.BottomSheetDelegate;
import org.chromium.chrome.browser.ntp_customization.BottomSheetViewBinder;
import org.chromium.chrome.browser.ntp_customization.NtpCustomizationViewProperties;
import org.chromium.chrome.browser.ntp_customization.R;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

/** Coordinator for the Most Visited Tiles settings bottom sheet. */
@NullMarked
public class MvtSettingsCoordinator {
    private MvtSettingsMediator mMediator;

    public MvtSettingsCoordinator(Context context, BottomSheetDelegate delegate) {
        View view =
                LayoutInflater.from(context)
                        .inflate(R.layout.ntp_customization_mvt_bottom_sheet, null, false);
        delegate.registerBottomSheetLayout(MVT, view);

        PropertyModel bottomSheetPropertyModel =
                new PropertyModel(NtpCustomizationViewProperties.BOTTOM_SHEET_KEYS);
        PropertyModelChangeProcessor.create(
                bottomSheetPropertyModel, view, BottomSheetViewBinder::bind);

        mMediator = new MvtSettingsMediator(bottomSheetPropertyModel, delegate);
    }

    public void destroy() {
        mMediator.destroy();
    }

    MvtSettingsMediator getMediatorForTesting() {
        return mMediator;
    }

    void setMediatorForTesting(MvtSettingsMediator mediator) {
        mMediator = mediator;
    }
}
