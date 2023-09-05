// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill;

import android.content.Context;
import android.net.Uri;

import androidx.annotation.VisibleForTesting;
import androidx.browser.customtabs.CustomTabsIntent;

import org.chromium.chrome.browser.layouts.LayoutStateProvider;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.components.autofill.payments.AutofillSaveCardUiInfo;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;

/**
 * Coordinator of the autofill save card UI.
 *
 * <p>This component shows a bottom sheet to let the user choose to save a payment card (either
 * locally or uploaded).
 */
public class AutofillSaveCardBottomSheetCoordinator {
    private final Context mContext;
    private final AutofillSaveCardBottomSheetBridge mBridge;
    private final AutofillSaveCardBottomSheetMediator mMediator;

    /**
     * Creates the coordinator.
     *
     * @param context The context for this component.
     * @param bottomSheetController The bottom sheet controller where this bottom sheet will be
     * shown.
     * @param layoutStateProvider The LayoutStateProvider used to detect when the bottom sheet needs
     * to be hidden after a change of layout (e.g. to the tab switcher).
     * @param tabModel The TabModel used to detect when the bottom sheet needs to be hidden after
     * a tab change.
     * @param uiInfo The assets (icons and text) displayed in the bottom sheet.
     * @param bridge The bridge to signal UI flow events (OnUiShown, OnUiAccepted, etc.) to.
     */
    public AutofillSaveCardBottomSheetCoordinator(Context context,
            BottomSheetController bottomSheetController, LayoutStateProvider layoutStateProvider,
            TabModel tabModel, AutofillSaveCardUiInfo uiInfo,
            AutofillSaveCardBottomSheetBridge bridge) {
        mContext = context;
        mBridge = bridge;
        mMediator = new AutofillSaveCardBottomSheetMediator(
                new AutofillSaveCardBottomSheetContent(context), uiInfo, bottomSheetController,
                layoutStateProvider, tabModel, this::launchCctOnLegalMessageClick, bridge);
    }

    @VisibleForTesting
    /*package*/ AutofillSaveCardBottomSheetCoordinator(Context context,
            AutofillSaveCardBottomSheetBridge bridge,
            AutofillSaveCardBottomSheetMediator mediator) {
        mContext = context;
        mBridge = bridge;
        mMediator = mediator;
    }

    /**
     * Request to show the bottom sheet.
     *
     * <p>Calls {@link AutofillSaveCardBottomSheetBridge#onUiShown} if the bottom sheet was shown.
     */
    public void requestShowContent() {
        mMediator.requestShowContent();
    }

    @VisibleForTesting
    /* package */ void launchCctOnLegalMessageClick(String url) {
        new CustomTabsIntent.Builder().setShowTitle(true).build().launchUrl(
                mContext, Uri.parse(url));
    }

    /** Destroys this component hiding the bottom sheet if needed. */
    public void destroy() {
        mMediator.destroy();
    }
}
