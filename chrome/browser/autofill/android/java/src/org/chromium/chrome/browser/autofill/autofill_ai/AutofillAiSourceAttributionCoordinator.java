// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.autofill_ai;

import android.content.Context;
import android.view.View;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.autofill.R;
import org.chromium.chrome.browser.autofill.autofill_ai.AutofillAiSourceAttributionProperties.ItemType;
import org.chromium.components.autofill.autofill_ai.AutofillAiSourceAttributionInfo;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.StateChangeReason;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetObserver;
import org.chromium.ui.modelutil.LayoutViewBuilder;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.SimpleRecyclerViewAdapter;

import java.util.List;

/**
 * Coordinates the Autofill AI source attribution bottom sheet.
 *
 * <p>Manages the lifecycle of the sheet, connects the ModelList to the view via {@link
 * SimpleRecyclerViewAdapter}, delegates business logic to {@link
 * AutofillAiSourceAttributionMediator}, and observes {@link BottomSheetController}.
 */
@NullMarked
public class AutofillAiSourceAttributionCoordinator {
    private final BottomSheetController mBottomSheetController;
    private final Runnable mOnDismissedCallback;
    private final AutofillAiSourceAttributionContent mContent;
    private final ModelList mModelList;
    private final SimpleRecyclerViewAdapter mAdapter;
    private final AutofillAiSourceAttributionMediator mMediator;
    private final BottomSheetObserver mSheetObserver;
    private boolean mIsShowing;
    private boolean mIsDestroyed;

    /**
     * Constructs the coordinator.
     *
     * @param context The context for inflating views.
     * @param controller The bottom sheet controller to show and hide the sheet.
     * @param sources The list of source attribution items to display.
     * @param subtitle The subtitle describing the target entity.
     * @param onDismissedCallback Invoked when the bottom sheet is dismissed or destroyed.
     */
    public AutofillAiSourceAttributionCoordinator(
            Context context,
            BottomSheetController controller,
            List<AutofillAiSourceAttributionInfo> sources,
            String subtitle,
            Runnable onDismissedCallback) {
        mBottomSheetController = controller;
        mOnDismissedCallback = onDismissedCallback;
        AutofillAiSourceAttributionView view = new AutofillAiSourceAttributionView(context);
        mContent = new AutofillAiSourceAttributionContent(view);
        mModelList = new ModelList();
        mAdapter = new SimpleRecyclerViewAdapter(mModelList);
        mAdapter.registerType(
                ItemType.HEADER,
                new LayoutViewBuilder<View>(R.layout.autofill_ai_attribution_header_item),
                AutofillAiSourceAttributionViewBinder::bindHeader);
        mAdapter.registerType(
                ItemType.SOURCE_CARD,
                new LayoutViewBuilder<View>(R.layout.autofill_ai_source_card_item),
                AutofillAiSourceAttributionViewBinder::bindSourceCard);
        view.setAdapter(mAdapter);
        mMediator = new AutofillAiSourceAttributionMediator(context, mModelList, sources, subtitle);
        mSheetObserver =
                new BottomSheetObserver() {
                    @Override
                    public void onSheetClosed(@StateChangeReason int reason) {
                        if (mBottomSheetController.getCurrentSheetContent() != mContent) {
                            return;
                        }
                        RecordHistogram.recordEnumeratedHistogram(
                                "Autofill.Ai.AttributionSheet.Dismissed",
                                reason,
                                StateChangeReason.MAX_VALUE + 1);
                        mIsShowing = false;
                        destroy();
                    }
                };
    }

    public boolean requestShowContent() {
        if (mIsDestroyed || mIsShowing) {
            return false;
        }
        boolean shown = mBottomSheetController.requestShowContent(mContent, /* animate= */ true);
        RecordHistogram.recordBooleanHistogram("Autofill.Ai.AttributionSheet.Shown", shown);
        if (!shown) {
            destroy();
            return false;
        }
        mBottomSheetController.addObserver(mSheetObserver);
        mIsShowing = true;
        return true;
    }

    public void destroy() {
        if (mIsDestroyed) {
            return;
        }
        mIsDestroyed = true;
        if (mIsShowing) {
            mBottomSheetController.removeObserver(mSheetObserver);
            mBottomSheetController.hideContent(
                    mContent, /* animate= */ false, StateChangeReason.NONE);
            mIsShowing = false;
        }
        mMediator.destroy();
        mContent.destroy();
        mAdapter.destroy();
        mOnDismissedCallback.run();
    }

    ModelList getModelListForTesting() {
        return mModelList;
    }

    AutofillAiSourceAttributionContent getContentForTesting() {
        return mContent;
    }

    boolean isShowingForTesting() {
        return mIsShowing;
    }

    BottomSheetObserver getSheetObserverForTesting() {
        return mSheetObserver;
    }
}
