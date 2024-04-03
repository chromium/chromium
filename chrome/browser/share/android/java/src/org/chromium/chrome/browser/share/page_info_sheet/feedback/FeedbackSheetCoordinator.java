// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.share.page_info_sheet.feedback;

import android.content.Context;
import android.view.LayoutInflater;
import android.view.View;

import androidx.annotation.StringRes;
import androidx.constraintlayout.widget.ConstraintLayout.LayoutParams;

import org.chromium.chrome.R;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

import java.util.List;

/**
 * Coordinator of page summary feedback sheet.
 *
 * <p>Shows a bottom sheet with radio buttons to specify the feedback type the user wants to
 * provide.
 */
public class FeedbackSheetCoordinator {

    private final View mView;
    private final FeedbackSheetMediator mMediator;

    public interface Delegate {

        /**
         * Called when user selects a feedback type option.
         *
         * @param key The key of the selected feedback type.
         */
        void onAccepted(String key);

        /** Called when user clicks cancel or dismisses the sheet. */
        void onCanceled();

        /**
         * Provides a list of feedback options to the sheet. Each element is a pair of strings, the
         * first one is a non-visible key, the second one is a UI string.
         */
        List<FeedbackOption> getAvailableOptions();
    }

    /** Encapsulates the display text and id of a radio button item. */
    public static class FeedbackOption {
        public final String optionKey;
        public final @StringRes int displayTextId;

        public FeedbackOption(String optionKey, @StringRes int displayTextId) {
            this.optionKey = optionKey;
            this.displayTextId = displayTextId;
        }
    }

    /**
     * Creates the coordinator.
     *
     * @param context Context used to inflate the view.
     * @param feedbackDelegate Delegate that provides a list of feedback types and receives the user
     *     response.
     * @param bottomSheetController The bottom sheet controller where this bottom sheet will be
     *     shown.
     */
    public FeedbackSheetCoordinator(
            Context context,
            FeedbackSheetCoordinator.Delegate feedbackDelegate,
            BottomSheetController bottomSheetController) {
        mView = LayoutInflater.from(context).inflate(R.layout.page_summary_feedback_sheet, null);
        // This view will be displayed inside a FrameLayout. If its LayoutParams are not set before
        // then FrameLayout will set a default value, which is LayoutParams.MATCH_PARENT for height
        // and width.
        mView.setLayoutParams(
                new LayoutParams(
                        /* width= */ LayoutParams.MATCH_PARENT,
                        /* height= */ LayoutParams.WRAP_CONTENT));

        mMediator =
                new FeedbackSheetMediator(
                        feedbackDelegate, new FeedbackSheetContent(mView), bottomSheetController);
        PropertyModelChangeProcessor.create(
                mMediator.getModel(), mView, FeedbackSheetViewBinder::bind);
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
