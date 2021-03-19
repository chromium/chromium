// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant.trigger_scripts;

import android.content.Context;
import android.view.ViewGroup;
import android.widget.LinearLayout;
import android.widget.ScrollView;
import android.widget.Space;

import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.chrome.autofill_assistant.R;
import org.chromium.chrome.browser.autofill_assistant.AssistantBottomBarDelegate;
import org.chromium.chrome.browser.autofill_assistant.AssistantBottomSheetContent;
import org.chromium.chrome.browser.autofill_assistant.AssistantRootViewContainer;
import org.chromium.chrome.browser.autofill_assistant.BottomSheetUtils;
import org.chromium.chrome.browser.autofill_assistant.LayoutUtils;
import org.chromium.chrome.browser.autofill_assistant.ScrollToHideGestureListener;
import org.chromium.chrome.browser.autofill_assistant.carousel.AssistantChip;
import org.chromium.chrome.browser.autofill_assistant.carousel.AssistantChipViewHolder;
import org.chromium.chrome.browser.autofill_assistant.generic_ui.AssistantDimension;
import org.chromium.chrome.browser.autofill_assistant.header.AssistantHeaderCoordinator;
import org.chromium.chrome.browser.autofill_assistant.header.AssistantHeaderModel;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetContent;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.StateChangeReason;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetObserver;
import org.chromium.components.browser_ui.bottomsheet.EmptyBottomSheetObserver;
import org.chromium.content_public.browser.GestureListenerManager;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.ApplicationViewportInsetSupplier;

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
        boolean onBackButtonPressed();
        void onFeedbackButtonClicked();
    }

    @Nullable
    private AssistantBottomSheetContent mContent;
    private final Context mContext;
    private final Delegate mDelegate;
    private final WebContents mWebContents;
    private final BottomSheetController mBottomSheetController;
    private final BottomSheetObserver mBottomSheetObserver;
    private final ObservableSupplierImpl<Integer> mInsetSupplier = new ObservableSupplierImpl<>();
    private final ApplicationViewportInsetSupplier mApplicationViewportInsetSupplier;

    private AssistantHeaderCoordinator mHeaderCoordinator;
    private AssistantHeaderModel mHeaderModel;
    private ScrollToHideGestureListener mGestureListener;
    private LinearLayout mChipsContainer;
    private final int mInnerChipSpacing;
    /** Whether the visual viewport should be resized while the trigger script is shown. */
    private boolean mResizeVisualViewport;

    private final List<AssistantChip> mLeftAlignedChips = new ArrayList<>();
    private final List<AssistantChip> mRightAlignedChips = new ArrayList<>();

    private final List<String> mCancelPopupItems = new ArrayList<>();
    private final List<Integer> mCancelPopupActions = new ArrayList<>();

    private boolean mAnimateBottomSheet = true;

    public AssistantTriggerScript(Context context, Delegate delegate, WebContents webContents,
            BottomSheetController controller,
            ApplicationViewportInsetSupplier applicationViewportInsetSupplier) {
        assert delegate != null;
        mContext = context;
        mDelegate = delegate;
        mWebContents = webContents;
        mBottomSheetController = controller;
        mApplicationViewportInsetSupplier = applicationViewportInsetSupplier;
        mApplicationViewportInsetSupplier.addSupplier(mInsetSupplier);
        mBottomSheetObserver = new EmptyBottomSheetObserver() {
            @Override
            public void onSheetClosed(@StateChangeReason int reason) {
                if (reason == StateChangeReason.SWIPE) {
                    mDelegate.onBottomSheetClosedWithSwipe();
                }
            }

            @Override
            public void onSheetContentChanged(@Nullable BottomSheetContent newContent) {
                // TODO(crbug.com/806868): Make sure this works and does not interfere with Duet
                // once we are in ChromeTabbedActivity.
                updateVisualViewportHeight();
            }

            @Override
            public void onSheetOffsetChanged(float heightFraction, float offsetPx) {
                updateVisualViewportHeight();
            }
        };
        mInnerChipSpacing = mContext.getResources().getDimensionPixelSize(
                R.dimen.autofill_assistant_actions_spacing);
    }

    private void createBottomSheetContents() {
        mContent = new AssistantBottomSheetContent(
                mContext, () -> new AssistantBottomBarDelegate() {
                    @Override
                    public boolean onBackButtonPressed() {
                        return mDelegate.onBackButtonPressed();
                    }

                    @Override
                    public void onBottomSheetClosedWithSwipe() {
                        // TODO(micantox): introduce a dedicated AssistantBottomSheetContent
                        // delegate, rather than reuse the bottom bar delegate, as it forces
                        // implementation of methods that the bottom sheet will never invoke.
                        assert false : "This should never happen";
                        mDelegate.onBottomSheetClosedWithSwipe();
                    }
                }) {
            @Override
            public boolean setContentSizeListener(@Nullable ContentSizeListener listener) {
                return super.setContentSizeListener(new ContentSizeListener() {
                    @Override
                    public void onSizeChanged(int width, int height, int oldWidth, int oldHeight) {
                        if (mGestureListener != null
                                && (mGestureListener.isScrolling()
                                        || mGestureListener.isSheetHidden()
                                        || mGestureListener.isSheetSettling())) {
                            // Prevent the bottom sheet from being reset and moving back
                            // to its normal position while scrolling or running a settle
                            // animation controlled by the scroll-to-hide gesture listener.
                            return;
                        }
                        // This happens when hide sheet is immediately called after expand sheet.
                        if (listener == null) {
                            return;
                        }
                        listener.onSizeChanged(width, height, oldWidth, oldHeight);
                    }
                });
            }
        };
        // Allow swipe-to-dismiss.
        mContent.setPeekModeDisabled(true);

        mChipsContainer = new LinearLayout(mContext);
        mChipsContainer.setOrientation(LinearLayout.HORIZONTAL);
        int horizontalMargin = mContext.getResources().getDimensionPixelSize(
                R.dimen.autofill_assistant_bottombar_horizontal_spacing);
        int verticalMargin = AssistantDimension.getPixelSizeDp(mContext, 16);
        mChipsContainer.setPadding(
                horizontalMargin, verticalMargin, horizontalMargin, verticalMargin);

        AssistantRootViewContainer rootViewContainer =
                (AssistantRootViewContainer) LayoutUtils.createInflater(mContext).inflate(
                        R.layout.autofill_assistant_bottom_sheet_content, /* root= */ null);
        rootViewContainer.disableTalkbackViewResizing();
        ScrollView scrollableContent = rootViewContainer.findViewById(R.id.scrollable_content);
        rootViewContainer.addView(mHeaderCoordinator.getView(), 0);
        rootViewContainer.addView(mChipsContainer,
                new LinearLayout.LayoutParams(
                        ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT));
        mContent.setContent(rootViewContainer, scrollableContent);
    }

    public void destroy() {
        disableScrollToHide();
        mBottomSheetController.removeObserver(mBottomSheetObserver);
        if (mHeaderCoordinator != null) {
            mHeaderCoordinator.destroy();
        }
        mApplicationViewportInsetSupplier.removeSupplier(mInsetSupplier);
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

    public AssistantHeaderModel createHeaderAndGetModel() {
        mHeaderModel = new AssistantHeaderModel();
        if (mHeaderCoordinator != null) {
            mHeaderCoordinator.destroy();
        }
        mHeaderCoordinator = new AssistantHeaderCoordinator(mContext, mHeaderModel);
        mHeaderModel.set(
                AssistantHeaderModel.FEEDBACK_BUTTON_CALLBACK, mDelegate::onFeedbackButtonClicked);
        return mHeaderModel;
    }

    /** Binds {@code chips} to {@code actions}. */
    private void bindChips(List<AssistantChip> chips, int[] actions) {
        assert chips.size() == actions.length;
        for (int i = 0; i < chips.size(); ++i) {
            int index = i;
            if (actions[index] == TriggerScriptAction.SHOW_CANCEL_POPUP) {
                chips.get(index).setPopupItems(mCancelPopupItems,
                        result -> mDelegate.onTriggerScriptAction(mCancelPopupActions.get(result)));
            } else {
                chips.get(index).setSelectedListener(
                        () -> mDelegate.onTriggerScriptAction(actions[index]));
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

    public boolean show(boolean resizeVisualViewport, boolean scrollToHide) {
        if (mHeaderModel == null || mHeaderCoordinator == null) {
            assert false : "createHeaderAndGetModel() must be called before show()";
            return false;
        }
        mResizeVisualViewport = resizeVisualViewport;
        createBottomSheetContents();
        update();
        mBottomSheetController.removeObserver(mBottomSheetObserver);
        mBottomSheetController.addObserver(mBottomSheetObserver);
        BottomSheetUtils.showContentAndMaybeExpand(mBottomSheetController, mContent,
                /* shouldExpand = */ true, /* animate = */ mAnimateBottomSheet);

        if (scrollToHide) enableScrollToHide();

        return true;
    }

    public void hide() {
        disableScrollToHide();
        mBottomSheetController.removeObserver(mBottomSheetObserver);
        mBottomSheetController.hideContent(mContent, /* animate = */ mAnimateBottomSheet);
        mResizeVisualViewport = false;
        updateVisualViewportHeight();
    }

    private void updateVisualViewportHeight() {
        if (!mResizeVisualViewport || mBottomSheetController.getCurrentSheetContent() != mContent) {
            setVisualViewportResizing(0);
            return;
        }

        setVisualViewportResizing(mBottomSheetController.getCurrentOffset());
    }

    /**
     * Shrink the visual viewport by {@code resizing} pixels. 0 will restore original size.
     *
     * Fork of {@code AssistantBottomBarCoordinator}. TODO(arbesser): refactor, share code.
     */
    private void setVisualViewportResizing(int resizing) {
        int currentInset = mInsetSupplier.get() != null ? mInsetSupplier.get() : 0;
        if (resizing == currentInset || mWebContents == null
                || mWebContents.getRenderWidgetHostView() == null) {
            return;
        }

        mInsetSupplier.set(resizing);
    }

    private void disableScrollToHide() {
        if (mGestureListener == null) return;

        GestureListenerManager.fromWebContents(mWebContents).removeListener(mGestureListener);
        mGestureListener = null;
    }

    private void enableScrollToHide() {
        if (mGestureListener != null) return;

        mGestureListener = new ScrollToHideGestureListener(mBottomSheetController, mContent);
        GestureListenerManager.fromWebContents(mWebContents).addListener(mGestureListener);
    }
}
