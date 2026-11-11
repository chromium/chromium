// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.autofill_ai;

import android.content.Context;

import org.chromium.build.annotations.NullMarked;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

/**
 * Coordinator for the Autofill AI attribution verification bottom sheet.
 *
 * <p>Assembles the MVC component by wiring {@link AutofillAiAttributionVerificationSheetMediator}
 * to {@link AutofillAiAttributionVerificationSheetView} through {@link
 * PropertyModelChangeProcessor}, and exposes the entry point to display the sheet over the active
 * attribution source tab.
 */
@NullMarked
public class AutofillAiAttributionVerificationSheetCoordinator {
    /** Delegate notified of user or lifecycle events from the verification bottom sheet. */
    public interface Delegate {
        /**
         * Invoked when the bottom sheet fails to show or is permanently dismissed (including when
         * dropped from the bottom sheet queue before becoming active).
         */
        void onDismissed();
    }

    private final AutofillAiAttributionVerificationSheetView mView;
    private final AutofillAiAttributionVerificationSheetMediator mMediator;

    /**
     * Constructs the coordinator and binds the underlying MVC model and view.
     *
     * @param context The {@link Context} used to inflate the bottom sheet layout and resolve
     *     resources.
     * @param bottomSheetController The {@link BottomSheetController} used to show and hide the
     *     sheet.
     * @param delegate The {@link Delegate} notified of sheet dismissal events.
     */
    public AutofillAiAttributionVerificationSheetCoordinator(
            Context context, BottomSheetController bottomSheetController, Delegate delegate) {
        mView = new AutofillAiAttributionVerificationSheetView(context);
        mMediator =
                new AutofillAiAttributionVerificationSheetMediator(
                        bottomSheetController, mView, delegate);
        PropertyModelChangeProcessor.create(
                mMediator.getModel(),
                mView,
                AutofillAiAttributionVerificationSheetViewBinder::bind);
    }

    /**
     * Requests showing and expanding the attribution verification bottom sheet. If {@link
     * BottomSheetController} rejects the request, {@link Delegate#onDismissed()} is invoked
     * immediately.
     */
    public void requestShowContent() {
        mMediator.requestShowContent();
    }

    PropertyModel getPropertyModelForTesting() {
        return mMediator.getModel();
    }

    AutofillAiAttributionVerificationSheetView getViewForTesting() {
        return mView;
    }
}
