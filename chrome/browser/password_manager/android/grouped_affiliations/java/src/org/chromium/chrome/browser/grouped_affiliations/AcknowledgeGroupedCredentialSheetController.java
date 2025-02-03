// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.grouped_affiliations;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.content.Context;
import android.view.LayoutInflater;
import android.view.View;

import org.chromium.base.Callback;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetObserver;
import org.chromium.components.browser_ui.bottomsheet.EmptyBottomSheetObserver;

/**
 * Controller, which displays the acknowledgement bottom sheet before filling credential that were
 * originally saved for another web site or app grouped with current site.
 */
@NullMarked
public class AcknowledgeGroupedCredentialSheetController {
    private final Context mContext;
    private final BottomSheetController mBottomSheetController;
    private final Callback<Integer> mOnSheetDismissed;
    private @Nullable AcknowledgeGroupedCredentialSheetView mView;
    private final BottomSheetObserver mBottomSheetObserver =
            new EmptyBottomSheetObserver() {
                @Override
                public void onSheetClosed(@BottomSheetController.StateChangeReason int reason) {
                    super.onSheetClosed(reason);
                    if (mBottomSheetController.getCurrentSheetContent() != null
                            && mBottomSheetController.getCurrentSheetContent() == mView) {
                        mBottomSheetController.removeObserver(mBottomSheetObserver);
                        onDismissed(DismissReason.IGNORE);
                    }
                }
            };

    public AcknowledgeGroupedCredentialSheetController(
            Context context,
            BottomSheetController bottomSheetController,
            Callback<Integer> onSheetDismissed) {
        mContext = context;
        mBottomSheetController = bottomSheetController;
        mOnSheetDismissed = onSheetDismissed;
    }

    public void show(String currentHostname, String credentialHostname) {
        mBottomSheetController.addObserver(mBottomSheetObserver);
        mBottomSheetController.requestShowContent(
                createView(currentHostname, credentialHostname), true);
    }

    public void dismiss() {
        assumeNonNull(mView);
        mBottomSheetController.hideContent(mView, true);
    }

    public void onDismissed(@DismissReason int dismissReason) {
        mOnSheetDismissed.onResult(dismissReason);
    }

    /**
     * The order of calls is important here. `onDismissed` should be called first to pass the
     * correct dismiss reason to the bridge. `dismiss` will also end up triggering `onDismissed`
     * eventually, but with a wrong dismiss reason. The second call will be swallowed in the Java
     * side of the bridge and have no real impact.
     *
     * @param dismissReason reflects the button clicked on the sheet.
     */
    public void onClick(@DismissReason int dismissReason) {
        onDismissed(dismissReason);
        dismiss();
    }

    private AcknowledgeGroupedCredentialSheetView createView(
            String currentHostname, String credentialHostname) {
        View contentView =
                LayoutInflater.from(mContext)
                        .inflate(R.layout.acknowledge_grouped_credential_sheet_content, null);
        mView =
                new AcknowledgeGroupedCredentialSheetView(
                        contentView, currentHostname, credentialHostname, this::onClick);
        return mView;
    }
}
