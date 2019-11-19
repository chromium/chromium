// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant;

import android.transition.ChangeBounds;
import android.transition.Fade;
import android.transition.TransitionManager;
import android.transition.TransitionSet;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.LinearLayout;
import android.widget.ScrollView;

import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.ObserverList;
import org.chromium.chrome.autofill_assistant.R;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.autofill_assistant.carousel.AssistantActionsCarouselCoordinator;
import org.chromium.chrome.browser.autofill_assistant.carousel.AssistantCarouselCoordinator;
import org.chromium.chrome.browser.autofill_assistant.carousel.AssistantChip;
import org.chromium.chrome.browser.autofill_assistant.carousel.AssistantSuggestionsCarouselCoordinator;
import org.chromium.chrome.browser.autofill_assistant.details.AssistantDetailsCoordinator;
import org.chromium.chrome.browser.autofill_assistant.form.AssistantFormCoordinator;
import org.chromium.chrome.browser.autofill_assistant.header.AssistantHeaderCoordinator;
import org.chromium.chrome.browser.autofill_assistant.header.AssistantHeaderModel;
import org.chromium.chrome.browser.autofill_assistant.infobox.AssistantInfoBoxCoordinator;
import org.chromium.chrome.browser.autofill_assistant.user_data.AssistantCollectUserDataCoordinator;
import org.chromium.chrome.browser.autofill_assistant.user_data.AssistantCollectUserDataModel;
import org.chromium.chrome.browser.compositor.CompositorViewResizer;
import org.chromium.chrome.browser.tab.TabViewAndroidDelegate;
import org.chromium.chrome.browser.widget.bottomsheet.BottomSheetContent;
import org.chromium.chrome.browser.widget.bottomsheet.BottomSheetController;
import org.chromium.chrome.browser.widget.bottomsheet.EmptyBottomSheetObserver;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.modelutil.ListModel;

/**
 * Coordinator responsible for the Autofill Assistant bottom bar.
 */
class AssistantBottomBarCoordinator
        implements CompositorViewResizer, AssistantPeekHeightCoordinator.Delegate {
    private static final int FADE_OUT_TRANSITION_TIME_MS = 150;
    private static final int FADE_IN_TRANSITION_TIME_MS = 150;
    private static final int CHANGE_BOUNDS_TRANSITION_TIME_MS = 250;

    private final AssistantModel mModel;
    private final BottomSheetController mBottomSheetController;
    private final AssistantBottomSheetContent mContent;
    private final ScrollView mScrollableContent;
    @Nullable
    private WebContents mWebContents;

    // Child coordinators.
    private final AssistantHeaderCoordinator mHeaderCoordinator;
    private final AssistantDetailsCoordinator mDetailsCoordinator;
    private final AssistantFormCoordinator mFormCoordinator;
    private final AssistantCarouselCoordinator mSuggestionsCoordinator;
    private final AssistantCarouselCoordinator mActionsCoordinator;
    private final AssistantPeekHeightCoordinator mPeekHeightCoordinator;
    private AssistantInfoBoxCoordinator mInfoBoxCoordinator;
    private AssistantCollectUserDataCoordinator mPaymentRequestCoordinator;

    // The transition triggered whenever the layout of the BottomSheet content changes.
    private final TransitionSet mLayoutTransition =
            new TransitionSet()
                    .setOrdering(TransitionSet.ORDERING_SEQUENTIAL)
                    .addTransition(new Fade(Fade.OUT).setDuration(FADE_OUT_TRANSITION_TIME_MS))
                    .addTransition(new ChangeBounds().setDuration(CHANGE_BOUNDS_TRANSITION_TIME_MS))
                    .addTransition(new Fade(Fade.IN).setDuration(FADE_IN_TRANSITION_TIME_MS));

    private final ObserverList<CompositorViewResizer.Observer> mLayoutViewportSizeObservers =
            new ObserverList<>();
    @AssistantViewportMode
    private int mViewportMode = AssistantViewportMode.NO_RESIZE;
    private int mLastLayoutViewportResizing;
    private int mLastVisualViewportResizing;

    AssistantBottomBarCoordinator(
            ChromeActivity activity, AssistantModel model, BottomSheetController controller) {
        mModel = model;
        mBottomSheetController = controller;

        BottomSheetContent currentSheetContent = controller.getCurrentSheetContent();
        if (currentSheetContent instanceof AssistantBottomSheetContent) {
            mContent = (AssistantBottomSheetContent) currentSheetContent;
        } else {
            mContent = new AssistantBottomSheetContent(activity);
        }

        // Replace or set the content to the actual Autofill Assistant views.
        ViewGroup bottomBarView = (ViewGroup) LayoutInflater.from(activity).inflate(
                R.layout.autofill_assistant_bottom_sheet_content, /* root= */ null);
        mScrollableContent = bottomBarView.findViewById(R.id.scrollable_content);
        ViewGroup scrollableContentContainer =
                mScrollableContent.findViewById(R.id.scrollable_content_container);
        mContent.setContent(bottomBarView, mScrollableContent);

        // Set up animations. We need to setup them before initializing the child coordinators as we
        // want our observers to be triggered before the coordinators/view binders observers.
        // TODO(crbug.com/806868): We should only animate our BottomSheetContent instead of the root
        // view. However, it looks like doing that is not well supported by the BottomSheet, so the
        // BottomSheet offset is wrong during the animation.
        ViewGroup rootView = (ViewGroup) activity.findViewById(R.id.coordinator);
        setupAnimations(model, rootView);

        // Instantiate child components.
        mHeaderCoordinator = new AssistantHeaderCoordinator(activity, model.getHeaderModel());
        mInfoBoxCoordinator = new AssistantInfoBoxCoordinator(activity, model.getInfoBoxModel());
        mDetailsCoordinator = new AssistantDetailsCoordinator(activity, model.getDetailsModel());
        mPaymentRequestCoordinator =
                new AssistantCollectUserDataCoordinator(activity, model.getCollectUserDataModel());
        mFormCoordinator = new AssistantFormCoordinator(activity, model.getFormModel());
        mSuggestionsCoordinator =
                new AssistantSuggestionsCarouselCoordinator(activity, model.getSuggestionsModel());
        mActionsCoordinator =
                new AssistantActionsCarouselCoordinator(activity, model.getActionsModel());
        mPeekHeightCoordinator = new AssistantPeekHeightCoordinator(activity, this, controller,
                mContent.getToolbarView(), mHeaderCoordinator.getView(),
                mSuggestionsCoordinator.getView(), mActionsCoordinator.getView(),
                AssistantPeekHeightCoordinator.PeekMode.HANDLE);

        // We don't want to animate the carousels children views as they are already animated by the
        // recyclers ItemAnimator, so we exclude them to avoid a clash between the animations.
        mLayoutTransition.excludeChildren(mSuggestionsCoordinator.getView(), /* exclude= */ true);
        mLayoutTransition.excludeChildren(mActionsCoordinator.getView(), /* exclude= */ true);

        // do not animate the contents of the payment method section inside the section choice list,
        // since the animation is not required and causes a rendering crash.
        mLayoutTransition.excludeChildren(
                mPaymentRequestCoordinator.getView()
                        .findViewWithTag(AssistantTagsForTesting
                                                 .COLLECT_USER_DATA_PAYMENT_METHOD_SECTION_TAG)
                        .findViewWithTag(AssistantTagsForTesting.COLLECT_USER_DATA_CHOICE_LIST),
                /* exclude= */ true);

        // Add child views to bottom bar container. We put all child views in the scrollable
        // container, except the actions and suggestions.
        bottomBarView.addView(mHeaderCoordinator.getView(), 0);
        scrollableContentContainer.addView(mInfoBoxCoordinator.getView());
        scrollableContentContainer.addView(mDetailsCoordinator.getView());
        scrollableContentContainer.addView(mPaymentRequestCoordinator.getView());
        scrollableContentContainer.addView(mFormCoordinator.getView());
        bottomBarView.addView(mSuggestionsCoordinator.getView());
        bottomBarView.addView(mActionsCoordinator.getView());

        // Set children top margins to have a spacing between them.
        int childSpacing = activity.getResources().getDimensionPixelSize(
                R.dimen.autofill_assistant_bottombar_vertical_spacing);
        int suggestionsVerticalInset =
                activity.getResources().getDimensionPixelSize(R.dimen.chip_bg_vertical_inset);
        setChildMarginTop(mDetailsCoordinator.getView(), childSpacing);
        setChildMarginTop(mPaymentRequestCoordinator.getView(), childSpacing);
        setChildMarginTop(mFormCoordinator.getView(), childSpacing);
        setChildMargin(mSuggestionsCoordinator.getView(), childSpacing - suggestionsVerticalInset,
                -suggestionsVerticalInset);

        // Hide the carousels when they are empty.
        hideWhenEmpty(
                mSuggestionsCoordinator.getView(), model.getSuggestionsModel().getChipsModel());
        hideWhenEmpty(mActionsCoordinator.getView(), model.getActionsModel().getChipsModel());

        // Set the horizontal margins of children. We don't set them on the payment request and the
        // carousels to allow them to take the full width of the sheet.
        setHorizontalMargins(mInfoBoxCoordinator.getView());
        setHorizontalMargins(mDetailsCoordinator.getView());
        setHorizontalMargins(mFormCoordinator.getView());

        controller.addObserver(new EmptyBottomSheetObserver() {
            @Override
            public void onSheetStateChanged(int newState) {
                maybeShowHeaderChip();
            }

            @Override
            public void onSheetContentChanged(@Nullable BottomSheetContent newContent) {
                // TODO(crbug.com/806868): Make sure this works and does not interfere with Duet
                // once we are in ChromeTabbedActivity.
                updateLayoutViewportHeight();
                updateVisualViewportHeight();
            }

            @Override
            public void onSheetOffsetChanged(float heightFraction, float offsetPx) {
                updateVisualViewportHeight();
            }
        });

        // Show or hide the bottom sheet content when the Autofill Assistant visibility is changed.
        model.addObserver((source, propertyKey) -> {
            if (AssistantModel.VISIBLE == propertyKey) {
                if (model.get(AssistantModel.VISIBLE)) {
                    showAndExpand();
                } else {
                    hide();
                }
            } else if (AssistantModel.ALLOW_TALKBACK_ON_WEBSITE == propertyKey) {
                controller.setIsObscuringAllTabs(
                        activity, !model.get(AssistantModel.ALLOW_TALKBACK_ON_WEBSITE));
            } else if (AssistantModel.WEB_CONTENTS == propertyKey) {
                mWebContents = model.get(AssistantModel.WEB_CONTENTS);
            }
        });

        // Don't clip the content scroll view unless it is scrollable. This is necessary for shadows
        // (i.e. details shadow and carousel cancel button shadow) but we need to clip the children
        // when the ScrollView is scrollable, otherwise scrolled content will overlap with the
        // header and carousels.
        ScrollView scrollView = mScrollableContent;
        scrollView.addOnLayoutChangeListener(
                (v, left, top, right, bottom, oldLeft, oldTop, oldRight, oldBottom) -> {
                    boolean canScroll =
                            scrollView.canScrollVertically(-1) || scrollView.canScrollVertically(1);
                    mScrollableContent.setClipChildren(canScroll);
                    bottomBarView.setClipChildren(canScroll);
                });
    }

    private void setupAnimations(AssistantModel model, ViewGroup rootView) {
        // Animate when the chip in the header changes.
        model.getHeaderModel().addObserver((source, propertyKey) -> {
            if (propertyKey == AssistantHeaderModel.CHIP
                    || propertyKey == AssistantHeaderModel.CHIP_VISIBLE) {
                animateChildren(rootView);
            }
        });

        // Animate when info box changes.
        model.getInfoBoxModel().addObserver((source, propertyKey) -> animateChildren(rootView));

        // Animate when details change.
        model.getDetailsModel().addObserver((source, propertyKey) -> animateChildren(rootView));

        // Animate when a PR section is expanded.
        model.getCollectUserDataModel().addObserver((source, propertyKey) -> {
            if (propertyKey == AssistantCollectUserDataModel.EXPANDED_SECTION) {
                animateChildren(rootView);
            }
        });

        // Animate when form inputs change.
        model.getFormModel().getInputsModel().addObserver(new AbstractListObserver<Void>() {
            @Override
            public void onDataSetChanged() {
                animateChildren(rootView);
            }
        });
    }

    private void animateChildren(ViewGroup rootView) {
        TransitionManager.beginDelayedTransition(rootView, mLayoutTransition);
    }

    private void maybeShowHeaderChip() {
        boolean showChip =
                mBottomSheetController.getSheetState() == BottomSheetController.SheetState.PEEK
                && mPeekHeightCoordinator.getPeekMode()
                        == AssistantPeekHeightCoordinator.PeekMode.HANDLE_HEADER;
        mModel.getHeaderModel().set(AssistantHeaderModel.CHIP_VISIBLE, showChip);
    }

    /**
     * Cleanup resources when this goes out of scope.
     */
    public void destroy() {
        resetVisualViewportHeight();

        mInfoBoxCoordinator.destroy();
        mInfoBoxCoordinator = null;
        mPaymentRequestCoordinator.destroy();
        mPaymentRequestCoordinator = null;
        mHeaderCoordinator.destroy();
    }

    /** Request showing the Assistant bottom bar view and expand the sheet. */
    public void showAndExpand() {
        BottomSheetUtils.showContentAndExpand(
                mBottomSheetController, mContent, /* animate= */ true);
    }

    /** Hide the Assistant bottom bar view. */
    public void hide() {
        mBottomSheetController.hideContent(mContent, /* animate= */ true);
    }

    void setViewportMode(@AssistantViewportMode int mode) {
        if (mode == mViewportMode) return;

        mViewportMode = mode;
        updateVisualViewportHeight();
        updateLayoutViewportHeight();
    }

    /** Set the peek mode. */
    void setPeekMode(@AssistantPeekHeightCoordinator.PeekMode int peekMode) {
        mPeekHeightCoordinator.setPeekMode(peekMode);
        maybeShowHeaderChip();
    }

    @Override
    public void setShowOnlyCarousels(boolean showOnlyCarousels) {
        mScrollableContent.setVisibility(showOnlyCarousels ? View.GONE : View.VISIBLE);
    }

    @Override
    public void onPeekHeightChanged() {
        updateLayoutViewportHeight();
    }

    private void setChildMarginTop(View child, int marginTop) {
        setChildMargin(child, marginTop, 0);
    }

    private void setChildMargin(View child, int marginTop, int marginBottom) {
        LinearLayout.LayoutParams params = (LinearLayout.LayoutParams) child.getLayoutParams();
        params.topMargin = marginTop;
        params.bottomMargin = marginBottom;
        child.setLayoutParams(params);
    }

    /**
     * Observe {@code model} such that the associated view is made invisible when it is empty.
     */
    private void hideWhenEmpty(View carouselView, ListModel<AssistantChip> chipsModel) {
        setCarouselVisibility(carouselView, chipsModel);
        chipsModel.addObserver(new AbstractListObserver<Void>() {
            @Override
            public void onDataSetChanged() {
                setCarouselVisibility(carouselView, chipsModel);
            }
        });
    }

    private void setCarouselVisibility(View carouselView, ListModel<AssistantChip> chipsModel) {
        carouselView.setVisibility(chipsModel.size() > 0 ? View.VISIBLE : View.GONE);
    }

    @VisibleForTesting
    public AssistantCarouselCoordinator getSuggestionsCoordinator() {
        return mSuggestionsCoordinator;
    }

    private void setHorizontalMargins(View view) {
        LinearLayout.MarginLayoutParams layoutParams =
                (LinearLayout.MarginLayoutParams) view.getLayoutParams();
        int horizontalMargin = view.getContext().getResources().getDimensionPixelSize(
                R.dimen.autofill_assistant_bottombar_horizontal_spacing);
        layoutParams.setMarginStart(horizontalMargin);
        layoutParams.setMarginEnd(horizontalMargin);
        view.setLayoutParams(layoutParams);
    }

    private void updateLayoutViewportHeight() {
        setLayoutViewportResizing(getHeight());
    }

    /**
     * Shrink the layout viewport by {@code resizing} pixels. This is an expensive operation that
     * should be used with care.
     */
    private void setLayoutViewportResizing(int resizing) {
        if (resizing == mLastLayoutViewportResizing) return;
        mLastLayoutViewportResizing = resizing;

        for (Observer observer : mLayoutViewportSizeObservers) {
            observer.onHeightChanged(resizing);
        }
    }

    private void updateVisualViewportHeight() {
        if (mViewportMode != AssistantViewportMode.RESIZE_VISUAL_VIEWPORT
                || mBottomSheetController.getCurrentSheetContent() != mContent) {
            resetVisualViewportHeight();
            return;
        }

        setVisualViewportResizing((int) Math.floor(mBottomSheetController.getCurrentOffset()
                - mBottomSheetController.getTopShadowHeight()));
    }

    private void resetVisualViewportHeight() {
        setVisualViewportResizing(0);
    }

    /**
     * Shrink the visual viewport by {@code resizing} pixels. This operation is cheaper than calling
     * {@link #setLayoutViewportResizing} and can therefore be often called (e.g. during
     * animations).
     */
    private void setVisualViewportResizing(int resizing) {
        if (resizing == mLastVisualViewportResizing || mWebContents == null
                || mWebContents.getRenderWidgetHostView() == null) {
            return;
        }

        mLastVisualViewportResizing = resizing;
        TabViewAndroidDelegate chromeDelegate =
                (TabViewAndroidDelegate) mWebContents.getViewAndroidDelegate();
        assert chromeDelegate != null;
        chromeDelegate.insetViewportBottom(resizing);
    }

    // Implementation of methods from AutofillAssistantSizeManager.

    @Override
    public int getHeight() {
        if (mViewportMode == AssistantViewportMode.RESIZE_LAYOUT_VIEWPORT
                && mBottomSheetController.getCurrentSheetContent() == mContent) {
            return mPeekHeightCoordinator.getPeekHeight();
        }

        return 0;
    }

    @Override
    public void addObserver(Observer observer) {
        mLayoutViewportSizeObservers.addObserver(observer);
    }

    @Override
    public void removeObserver(Observer observer) {
        mLayoutViewportSizeObservers.removeObserver(observer);
    }
}
