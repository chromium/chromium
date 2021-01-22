// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.share.long_screenshots;

import static org.chromium.chrome.browser.share.long_screenshots.LongScreenshotsAreaSelectionDialogProperties.CLOSE_BUTTON_CALLBACK;
import static org.chromium.chrome.browser.share.long_screenshots.LongScreenshotsAreaSelectionDialogProperties.DONE_BUTTON_CALLBACK;

import android.app.Activity;
import android.app.Dialog;
import android.view.View;
import android.widget.LinearLayout;

import org.chromium.chrome.R;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

/**
 * LongScreenshotsMediator is responsible for retrieving the long screenshot Bitmaps
 * via {@link LongScreenshotsEntryManager} and displaying them in the area selection
 * dialog.
 */
public class LongScreenshotsMediator {
    private Dialog mDialog;
    private PropertyModel mModel;
    private final Activity mActivity;
    private final EntryManager mEntryManager;

    public LongScreenshotsMediator(Activity activity, EntryManager entryManager) {
        mActivity = activity;
        mEntryManager = entryManager;
    }

    public void showAreaSelectionDialog() {
        View view = mActivity.getLayoutInflater().inflate(
                R.layout.long_screenshots_area_selection_dialog, null);
        mModel = LongScreenshotsAreaSelectionDialogProperties.defaultModelBuilder()
                         .with(DONE_BUTTON_CALLBACK, this::areaSelectionDone)
                         .with(CLOSE_BUTTON_CALLBACK, this::areaSelectionClose)
                         .build();

        PropertyModelChangeProcessor.create(
                mModel, view, LongScreenshotsAreaSelectionDialogViewBinder::bind);

        mDialog = new Dialog(mActivity, R.style.Theme_Chromium_Fullscreen);
        mDialog.addContentView(view,
                new LinearLayout.LayoutParams(LinearLayout.LayoutParams.MATCH_PARENT,
                        LinearLayout.LayoutParams.MATCH_PARENT));

        mDialog.show();
    }

    public void areaSelectionDone(View view) {
        // TODO(1163193): Delete all bitmaps.
        mDialog.cancel();
    }

    public void areaSelectionClose(View view) {
        // TODO(1163193): Delete all bitmaps.
        mDialog.cancel();
    }
}
