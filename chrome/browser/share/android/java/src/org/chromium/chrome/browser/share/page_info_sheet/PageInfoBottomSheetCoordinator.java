// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.share.page_info_sheet;

import android.content.Context;
import android.view.LayoutInflater;

import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.chrome.R;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

/**
 * Coordinator of the page info bottom sheet.
 *
 * <p>This component shows a bottom sheet to let the add page info to shared links.
 */
public class PageInfoBottomSheetCoordinator {

    private final PageInfoBottomSheetMediator mMediator;

    private final PageInfoBottomSheetView mView;

    /** Interface that callers should use to provide page info and receive user events. */
    public static interface Delegate {
        /** Called by bottom sheet when user clicks the accept button. */
        void onAccept();

        /** Called by bottom sheet when user clicks the cancel button. */
        void onCancel();

        /** Called by bottom sheet when user clicks the learn more button. */
        void onLearnMore();

        /** Called by bottom sheet when user clicks the positive feedback button. */
        void onPositiveFeedback();

        /** Called by bottom sheet when user clicks the negative feedback button. */
        void onNegativeFeedback();

        /** Returns the supplier of current page info contents. */
        ObservableSupplier<PageInfoContents> getContentSupplier();
    }

    /**
     * Represents the state of fetching page info, it may be pending loading when {@code isLoading}
     * is true, finished loading when {@code isLoading} is false and failed when {@code
     * errorMessage} is not null.
     */
    public static class PageInfoContents {
        public final String resultContents;
        public final String errorMessage;
        public final boolean isLoading;

        PageInfoContents(String errorMessage) {
            this.errorMessage = errorMessage;
            resultContents = null;
            isLoading = false;
        }

        PageInfoContents(String resultContents, boolean isLoading) {
            errorMessage = null;
            this.resultContents = resultContents;
            this.isLoading = isLoading;
        }
    }

    /**
     * Creates the coordinator.
     *
     * @param context The context for this component.
     * @param pageInfoDelegate Delegate that provides page info and receives user events.
     * @param bottomSheetController The bottom sheet controller where this bottom sheet will be
     *     shown.
     */
    public PageInfoBottomSheetCoordinator(
            Context context,
            Delegate pageInfoDelegate,
            BottomSheetController bottomSheetController) {
        mView =
                (PageInfoBottomSheetView)
                        LayoutInflater.from(context)
                                .inflate(R.layout.page_info_bottom_sheet_content, null);
        mMediator =
                new PageInfoBottomSheetMediator(
                        pageInfoDelegate,
                        new PageInfoBottomSheetContent(mView),
                        bottomSheetController);
        PropertyModelChangeProcessor.create(
                mMediator.getModel(), mView, PageInfoBottomSheetViewBinder::bind);
    }

    /** Request to show the bottom sheet. */
    public void requestShowContent() {
        mMediator.requestShowContent();
    }

    /** Destroys this component hiding the bottom sheet if needed. */
    public void destroy() {
        mMediator.destroySheet();
    }
}
