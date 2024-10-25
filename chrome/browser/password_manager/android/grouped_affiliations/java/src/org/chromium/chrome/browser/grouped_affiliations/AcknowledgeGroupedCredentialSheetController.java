// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.grouped_affiliations;

import android.content.Context;
import android.view.LayoutInflater;
import android.view.View;

import org.chromium.base.Callback;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetObserver;
import org.chromium.components.browser_ui.bottomsheet.EmptyBottomSheetObserver;

/**
 * Controller, which displays the acknowledgement bottom sheet before filling credential that were
 * originally saved for another web site or app grouped with current site.
 */
public class AcknowledgeGroupedCredentialSheetController {
    private final Context mContext;
    private final BottomSheetController mBottomSheetController;
    private final Callback<Boolean> mOnSheetDismissed;
    private AcknowledgeGroupedCredentialSheetView mView;
    private final BottomSheetObserver mBottomSheetObserver =
            new EmptyBottomSheetObserver() {
                @Override
                public void onSheetClosed(@BottomSheetController.StateChangeReason int reason) {
                    super.onSheetClosed(reason);
                    if (mBottomSheetController.getCurrentSheetContent() != null
                            && mBottomSheetController.getCurrentSheetContent() == mView) {
                        mBottomSheetController.removeObserver(mBottomSheetObserver);
                        onDismissed(false);
                    }
                }
            };

    public AcknowledgeGroupedCredentialSheetController(
            Context context,
            BottomSheetController bottomSheetController,
            Callback<Boolean> onSheetDismissed) {
        mContext = context;
        mBottomSheetController = bottomSheetController;
        mOnSheetDismissed = onSheetDismissed;
    }

    public void show(String currentOrigin, String credentialOrigin) {
        mBottomSheetController.addObserver(mBottomSheetObserver);
        mBottomSheetController.requestShowContent(
                createView(currentOrigin, credentialOrigin), true);
    }

    public void dismiss() {
        mBottomSheetController.hideContent(mView, true);
    }

    public void onDismissed(boolean accepted) {
        mOnSheetDismissed.onResult(accepted);
    }

    public void onClick(boolean accepted) {
        onDismissed(accepted);
        dismiss();
    }

    private AcknowledgeGroupedCredentialSheetView createView(
            String currentOrigin, String credentialOrigin) {
        View contentView =
                LayoutInflater.from(mContext)
                        .inflate(R.layout.acknowledge_grouped_credential_sheet_content, null);
        mView =
                new AcknowledgeGroupedCredentialSheetView(
                        contentView, currentOrigin, credentialOrigin, this::onClick);
        return mView;
    }
}
