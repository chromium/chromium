// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant.trigger_scripts;

import android.content.Context;
import android.view.LayoutInflater;
import android.view.ViewGroup;
import android.widget.LinearLayout;
import android.widget.ScrollView;
import android.widget.Space;

import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.chrome.autofill_assistant.R;
import org.chromium.chrome.browser.autofill_assistant.AssistantBottomBarDelegate;
import org.chromium.chrome.browser.autofill_assistant.AssistantBottomSheetContent;
import org.chromium.chrome.browser.autofill_assistant.AssistantRootViewContainer;
import org.chromium.chrome.browser.autofill_assistant.BottomSheetUtils;
import org.chromium.chrome.browser.autofill_assistant.carousel.AssistantChip;
import org.chromium.chrome.browser.autofill_assistant.carousel.AssistantChipViewHolder;
import org.chromium.chrome.browser.autofill_assistant.generic_ui.AssistantDimension;
import org.chromium.chrome.browser.autofill_assistant.header.AssistantHeaderCoordinator;
import org.chromium.chrome.browser.autofill_assistant.header.AssistantHeaderModel;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.StateChangeReason;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetObserver;
import org.chromium.components.browser_ui.bottomsheet.EmptyBottomSheetObserver;

import java.util.ArrayList;
import java.util.List;

/**
 * Represents the UI for a particular trigger script. Notifies a delegate about user interactions.
 */
public class AssistantTriggerScript {
    /** Interface for delegates of this class who will be notified of user interactions. */
    public interface Delegate {
        void onTriggerScriptAction(@TriggerScriptAction int action);
        void onBottomSheetClosedWithSwipe();
        void onBackButtonPressed();
        void onFeedbackButtonClicked();
    }

    @Nullable
    private final AssistantBottomSheetContent mContent;
    private final Context mContext;
    private final Delegate mDelegate;
    private final BottomSheetController mBottomSheetController;
    private final BottomSheetObserver mBottomSheetObserver;

    private final AssistantRootViewContainer mRootViewContainer;
    private final ScrollView mScrollableContent;
    private final AssistantHeaderCoordinator mHeaderCoordinator;
    private final AssistantHeaderModel mHeaderModel;
    private final LinearLayout mChipsContainer;
    private final int mInnerChipSpacing;

    private final List<AssistantChip> mLeftAlignedChips = new ArrayList<>();
    private final List<AssistantChip> mRightAlignedChips = new ArrayList<>();

    private final List<String> mCancelPopupItems = new ArrayList<>();
    private final List<Integer> mCancelPopupActions = new ArrayList<>();

    private boolean mAnimateBottomSheet = true;

    public AssistantTriggerScript(
            Context context, Delegate delegate, BottomSheetController controller) {
        assert delegate != null;
        mContext = context;
        mDelegate = delegate;
        mBottomSheetController = controller;
        mBottomSheetObserver = new EmptyBottomSheetObserver() {
            @Override
            public void onSheetClosed(@StateChangeReason int reason) {
                if (reason == StateChangeReason.SWIPE) {
                    mDelegate.onBottomSheetClosedWithSwipe();
                }
            }
        };

        mContent =
                new AssistantBottomSheetContent(mContext, () -> new AssistantBottomBarDelegate() {
                    @Override
                    public boolean onBackButtonPressed() {
                        // We need to handle this event, because by default the bottom sheet will
                        // close when the back button is pressed.
                        mDelegate.onBackButtonPressed();
                        return true;
                    }

                    @Override
                    public void onBottomSheetClosedWithSwipe() {
                        // TODO(micantox): introduce a dedicated AssistantBottomSheetContent
                        // delegate, rather than reuse the bottom bar delegate, as it forces
                        // implementation of methods that the bottom sheet will never invoke.
                        assert false : "This should never happen";
                        mDelegate.onBottomSheetClosedWithSwipe();
                    }
                });
        // Allow swipe-to-dismiss.
        mContent.setPeekModeDisabled(true);

        mHeaderModel = new AssistantHeaderModel();
        mHeaderModel.set(
                AssistantHeaderModel.FEEDBACK_BUTTON_CALLBACK, mDelegate::onFeedbackButtonClicked);
        mHeaderCoordinator = new AssistantHeaderCoordinator(context, mHeaderModel);

        mChipsContainer = new LinearLayout(context);
        mChipsContainer.setOrientation(LinearLayout.HORIZONTAL);
        int horizontalMargin = context.getResources().getDimensionPixelSize(
                R.dimen.autofill_assistant_bottombar_horizontal_spacing);
        int verticalMargin = AssistantDimension.getPixelSizeDp(context, 16);
        mChipsContainer.setPadding(
                horizontalMargin, verticalMargin, horizontalMargin, verticalMargin);
        mInnerChipSpacing = mContext.getResources().getDimensionPixelSize(
                R.dimen.autofill_assistant_actions_spacing);

        mRootViewContainer = (AssistantRootViewContainer) LayoutInflater.from(context).inflate(
                R.layout.autofill_assistant_bottom_sheet_content, /* root= */ null);
        mScrollableContent = mRootViewContainer.findViewById(R.id.scrollable_content);
        mRootViewContainer.addView(mHeaderCoordinator.getView(), 0);
        mRootViewContainer.addView(mChipsContainer,
                new LinearLayout.LayoutParams(
                        ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT));
        mContent.setContent(mRootViewContainer, mScrollableContent);
    }

    public void destroy() {
        mBottomSheetController.removeObserver(mBottomSheetObserver);
        mHeaderCoordinator.destroy();
    }

    @VisibleForTesting
    public void disableBottomSheetAnimationsForTesting(boolean disable) {
        mAnimateBottomSheet = !disable;
    }

    @VisibleForTesting
    public List<AssistantChip> getLeftAlignedChipsForTest() {
        return mLeftAlignedChips;
    }
    @VisibleForTesting
    public List<AssistantChip> getRightAlignedChipsForTest() {
        return mRightAlignedChips;
    }
    @VisibleForTesting
    public AssistantBottomSheetContent getBottomSheetContentForTest() {
        return mContent;
    }

    private void addChipsToContainer(LinearLayout container, List<AssistantChip> chips) {
        for (int i = 0; i < chips.size(); ++i) {
            AssistantChipViewHolder viewHolder =
                    AssistantChipViewHolder.create(container, chips.get(i).getType());
            viewHolder.bind(chips.get(i));
            container.addView(viewHolder.getView());
            if (i < chips.size() - 1) {
                container.addView(new Space(mContext),
                        new LinearLayout.LayoutParams(
                                mInnerChipSpacing, ViewGroup.LayoutParams.MATCH_PARENT));
            }
        }
    }

    public AssistantHeaderModel getHeaderModel() {
        return mHeaderModel;
    }

    /** Binds {@code chips} to {@code actions}. */
    private void bindChips(List<AssistantChip> chips, int[] actions) {
        assert chips.size() == actions.length;
        for (int i = 0; i < chips.size(); ++i) {
            int action = actions[i];
            if (action == TriggerScriptAction.SHOW_CANCEL_POPUP) {
                chips.get(i).setPopupItems(mCancelPopupItems,
                        result -> mDelegate.onTriggerScriptAction(mCancelPopupActions.get(result)));
            } else {
                chips.get(i).setSelectedListener(
                        () -> mDelegate.onTriggerScriptAction(actions[action]));
            }
        }
    }

    public void setLeftAlignedChips(List<AssistantChip> chips, int[] actions) {
        assert chips.size() == actions.length;
        mLeftAlignedChips.clear();
        mLeftAlignedChips.addAll(chips);
        bindChips(mLeftAlignedChips, actions);
    }

    public void setRightAlignedChips(List<AssistantChip> chips, int[] actions) {
        assert chips.size() == actions.length;
        mRightAlignedChips.clear();
        mRightAlignedChips.addAll(chips);
        bindChips(mRightAlignedChips, actions);
    }

    public void setCancelPopupMenu(String[] items, int[] actions) {
        assert items.length == actions.length;
        mCancelPopupItems.clear();
        mCancelPopupActions.clear();

        for (int i = 0; i < actions.length; ++i) {
            mCancelPopupItems.add(items[i]);
            mCancelPopupActions.add(actions[i]);
        }
    }

    public void update() {
        mChipsContainer.removeAllViews();
        addChipsToContainer(mChipsContainer, mLeftAlignedChips);
        mChipsContainer.addView(new Space(mContext),
                new LinearLayout.LayoutParams(0, ViewGroup.LayoutParams.MATCH_PARENT, 1.0f));
        addChipsToContainer(mChipsContainer, mRightAlignedChips);
    }

    public void show() {
        update();
        mBottomSheetController.removeObserver(mBottomSheetObserver);
        mBottomSheetController.addObserver(mBottomSheetObserver);
        BottomSheetUtils.showContentAndMaybeExpand(mBottomSheetController, mContent,
                /* shouldExpand = */ true, /* animate = */ mAnimateBottomSheet);
    }

    public void hide() {
        mBottomSheetController.removeObserver(mBottomSheetObserver);
        mBottomSheetController.hideContent(mContent, /* animate = */ mAnimateBottomSheet);
    }
}
