// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.save_card;

import android.content.Context;
import android.view.View;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetContent;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.StateChangeReason;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetObserver;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

import java.util.function.Supplier;

/**
 * Coordinator of the anchored dialog UI which can be used as a drop in replacement of bottomsheets.
 * This class displays contents in a dialog that is anchored at the top right (or left depending on
 * the locale) corner of the content area.
 */
@NullMarked
public class AnchoredDialogCoordinator {
    private final PropertyModel mModel;
    private final AnchoredDialogMediator mMediator;
    private final AnchoredDialogView mView;

    public AnchoredDialogCoordinator(
            Context context, View containerView, Supplier<Integer> verticalOffsetProvider) {
        mView = new AnchoredDialogView(context);
        mModel = new PropertyModel.Builder(AnchoredDialogProperties.ALL_KEYS).build();
        PropertyModelChangeProcessor.create(mModel, mView, AnchoredDialogViewBinder::bind);

        mMediator = new AnchoredDialogMediator(mModel, containerView, verticalOffsetProvider);
    }

    /**
     * Show the content in the dialog.
     *
     * @param content The content to be shown.
     * @return True if the content was shown, and false if it was queued for display.
     */
    public boolean requestShowContent(BottomSheetContent content) {
        return mMediator.requestShowContent(content);
    }

    /**
     * Hide content shown in the dialog.
     *
     * @param content The content to be hidden.
     * @param hideReason The reason that the content is being hidden.
     */
    public void hideContent(@Nullable BottomSheetContent content, @StateChangeReason int reason) {
        mMediator.hideContent(content, reason);
    }

    /** Adds an observer. */
    public void addObserver(BottomSheetObserver observer) {
        mMediator.addObserver(observer);
    }

    /** Removes an observer. */
    public void removeObserver(BottomSheetObserver observer) {
        mMediator.removeObserver(observer);
    }
}
