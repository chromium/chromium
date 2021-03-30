// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant;

import android.app.Activity;
import android.transition.ChangeBounds;
import android.transition.Fade;
import android.transition.TransitionManager;
import android.transition.TransitionSet;
import android.view.View;
import android.view.ViewGroup;
import android.widget.LinearLayout;
import android.widget.ScrollView;

import androidx.annotation.Nullable;

import org.chromium.base.Callback;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.task.PostTask;
import org.chromium.chrome.autofill_assistant.R;
import org.chromium.chrome.browser.autofill_assistant.carousel.AssistantActionsCarouselCoordinator;
import org.chromium.chrome.browser.autofill_assistant.carousel.AssistantCarouselModel;
import org.chromium.chrome.browser.autofill_assistant.details.AssistantDetailsCoordinator;
import org.chromium.chrome.browser.autofill_assistant.form.AssistantFormCoordinator;
import org.chromium.chrome.browser.autofill_assistant.form.AssistantFormModel;
import org.chromium.chrome.browser.autofill_assistant.generic_ui.AssistantGenericUiCoordinator;
import org.chromium.chrome.browser.autofill_assistant.generic_ui.AssistantGenericUiModel;
import org.chromium.chrome.browser.autofill_assistant.header.AssistantHeaderCoordinator;
import org.chromium.chrome.browser.autofill_assistant.header.AssistantHeaderModel;
import org.chromium.chrome.browser.autofill_assistant.infobox.AssistantInfoBoxCoordinator;
import org.chromium.chrome.browser.autofill_assistant.overlay.AssistantOverlayCoordinator;
import org.chromium.chrome.browser.autofill_assistant.user_data.AssistantCollectUserDataCoordinator;
import org.chromium.chrome.browser.autofill_assistant.user_data.AssistantCollectUserDataModel;
import org.chromium.chrome.browser.image_fetcher.ImageFetcherConfig;
import org.chromium.chrome.browser.image_fetcher.ImageFetcherFactory;
import org.chromium.chrome.browser.ui.TabObscuringHandler;
import org.chromium.chrome.browser.util.ChromeAccessibilityUtil;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetContent;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.SheetState;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.StateChangeReason;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetObserver;
import org.chromium.components.browser_ui.bottomsheet.EmptyBottomSheetObserver;
import org.chromium.content_public.browser.UiThreadTaskTraits;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.ApplicationViewportInsetSupplier;
import org.chromium.ui.util.TokenHolder;

/**
 * Coordinator responsible for the Autofill Assistant bottom bar.
 */
class AssistantBottomBarCoordinator implements AssistantPeekHeightCoordinator.Delegate {
    private static final int FADE_OUT_TRANSITION_TIME_MS = 150;
    private static final int FADE_IN_TRANSITION_TIME_MS = 150;
    private static final int CHANGE_BOUNDS_TRANSITION_TIME_MS = 250;

    private final AssistantModel mModel;
    private final AssistantOverlayCoordinator mOverlayCoordinator;
    private final BottomSheetController mBottomSheetController;
    private final TabObscuringHandler mTabObscuringHandler;
    private final AssistantBottomSheetContent mContent;
    private final ScrollView mScrollableContent;
    private final AssistantRootViewContainer mRootViewContainer;
    @Nullable
    private WebContents mWebContents;
    private ApplicationViewportInsetSupplier mWindowApplicationInsetSupplier;
    private final ChromeAccessibilityUtil.Observer mAccessibilityObserver;
    private final BottomSheetObserver mBottomSheetObserver;

    // Child coordinators.
    private final AssistantHeaderCoordinator mHeaderCoordinator;
    private final AssistantDetailsCoordinator mDetailsCoordinator;
    private final AssistantFormCoordinator mFormCoordinator;
    private final AssistantActionsCarouselCoordinator mActionsCoordinator;
    private final AssistantPeekHeightCoordinator mPeekHeightCoordinator;
    private final ObservableSupplierImpl<Integer> mInsetSupplier = new ObservableSupplierImpl<>();
    private AssistantInfoBoxCoordinator mInfoBoxCoordinator;
    private AssistantCollectUserDataCoordinator mPaymentRequestCoordinator;
    private final AssistantGenericUiCoordinator mGenericUiCoordinator;

    // The transition triggered whenever the layout of the BottomSheet content changes.
    private final TransitionSet mLayoutTransition =
            new TransitionSet()
                    .setOrdering(TransitionSet.ORDERING_SEQUENTIAL)
                    .addTransition(new Fade(Fade.OUT).setDuration(FADE_OUT_TRANSITION_TIME_MS))
                    .addTransition(new ChangeBounds().setDuration(CHANGE_BOUNDS_TRANSITION_TIME_MS))
                    .addTransition(new Fade(Fade.IN).setDuration(FADE_IN_TRANSITION_TIME_MS));

    @AssistantViewportMode
    private int mViewportMode = AssistantViewportMode.NO_RESIZE;

    // Stores the viewport mode for cases where talkback is enabled. During that time, the viewport
    // is forced to RESIZE_VISUAL_VIEWPORT. If talkback gets disabled, the last mode stored in this
    // field will be applied.
    @AssistantViewportMode
    private int mTargetViewportMode = AssistantViewportMode.NO_RESIZE;

    /** A token held while the assistant is obscuring all tabs. */
    private int mObscuringToken;

    AssistantBottomBarCoordinator(Activity activity, AssistantModel model,
            AssistantOverlayCoordinator overlayCoordinator, BottomSheetController controller,
            ApplicationViewportInsetSupplier applicationViewportInsetSupplier,
            TabObscuringHandler tabObscuringHandler) {
        mModel = model;
        mOverlayCoordinator = overlayCoordinator;
        mBottomSheetController = controller;
        mTabObscuringHandler = tabObscuringHandler;

        mWindowApplicationInsetSupplier = applicationViewportInsetSupplier;
        mWindowApplicationInsetSupplier.addSupplier(mInsetSupplier);

        BottomSheetContent currentSheetContent = controller.getCurrentSheetContent();
        if (currentSheetContent instanceof AssistantBottomSheetContent) {
            mContent = (AssistantBottomSheetContent) currentSheetContent;
            mContent.setDelegate(model::getBottomBarDelegate);
        } else {
            mContent = new AssistantBottomSheetContent(activity, model::getBottomBarDelegate);
        }

        // Replace or set the content to the actual Autofill Assistant views.
        mRootViewContainer =
                (AssistantRootViewContainer) LayoutUtils.createInflater(activity).inflate(
                        R.layout.autofill_assistant_bottom_sheet_content, /* root= */ null);
        mScrollableContent = mRootViewContainer.findViewById(R.id.scrollable_content);
        ViewGroup scrollableContentContainer =
                mScrollableContent.findViewById(R.id.scrollable_content_container);
        mContent.setContent(mRootViewContainer, mScrollableContent);

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
        mDetailsCoordinator = new AssistantDetailsCoordinator(activity, model.getDetailsModel(),
                ImageFetcherFactory.createImageFetcher(ImageFetcherConfig.DISK_CACHE_ONLY,
                        AutofillAssistantUiController.getProfile()));
        mPaymentRequestCoordinator =
                new AssistantCollectUserDataCoordinator(activity, model.getCollectUserDataModel());
        mFormCoordinator = new AssistantFormCoordinator(activity, model.getFormModel());
        mActionsCoordinator =
                new AssistantActionsCarouselCoordinator(activity, model.getActionsModel());
        mPeekHeightCoordinator = new AssistantPeekHeightCoordinator(activity, this, controller,
                mContent.getToolbarView(), mHeaderCoordinator.getView(),
                mActionsCoordinator.getView(), AssistantPeekHeightCoordinator.PeekMode.HANDLE);
        mGenericUiCoordinator =
                new AssistantGenericUiCoordinator(activity, model.getGenericUiModel());

        // We don't want to animate the carousels children views as they are already animated by the
        // recyclers ItemAnimator, so we exclude them to avoid a clash between the animations.
        mLayoutTransition.excludeChildren(mActionsCoordinator.getView(), /* exclude= */ true);
        mLayoutTransition.excludeChildren(
                mHeaderCoordinator.getCarouselView(), /* exclude= */ true);

        // do not animate the contents of the payment method section inside the section choice list,
        // since the animation is not required and causes a rendering crash.
        mLayoutTransition.excludeChildren(
                mPaymentRequestCoordinator.getView()
                        .findViewWithTag(AssistantTagsForTesting
                                                 .COLLECT_USER_DATA_PAYMENT_METHOD_SECTION_TAG)
                        .findViewWithTag(AssistantTagsForTesting.COLLECT_USER_DATA_CHOICE_LIST),
                /* exclude= */ true);

        // Add child views to bottom bar container. We put all child views in the scrollable
        // container, except the actions.
        mRootViewContainer.addView(mHeaderCoordinator.getView(), 0);
        scrollableContentContainer.addView(mInfoBoxCoordinator.getView());
        scrollableContentContainer.addView(mDetailsCoordinator.getView());
        scrollableContentContainer.addView(mPaymentRequestCoordinator.getView());
        scrollableContentContainer.addView(mFormCoordinator.getView());
        scrollableContentContainer.addView(mGenericUiCoordinator.getView());
        mRootViewContainer.addView(mActionsCoordinator.getView());

        // Set children top margins to have a spacing between them.
        int childSpacing = activity.getResources().getDimensionPixelSize(
                R.dimen.autofill_assistant_bottombar_vertical_spacing);
        setChildMarginTop(mDetailsCoordinator.getView(), childSpacing);
        setChildMarginTop(mPaymentRequestCoordinator.getView(), childSpacing);
        setChildMarginTop(mFormCoordinator.getView(), childSpacing);
        setChildMarginTop(mGenericUiCoordinator.getView(), childSpacing);

        // Hide the carousels when they are empty.
        hideWhenEmpty(mActionsCoordinator.getView(), model.getActionsModel());

        // Set the horizontal margins of children. We don't set them on the payment request, the
        // carousels or the form to allow them to take the full width of the sheet.
        setHorizontalMargins(mInfoBoxCoordinator.getView());
        setHorizontalMargins(mDetailsCoordinator.getView());

        mBottomSheetObserver = new EmptyBottomSheetObserver() {
            @Override
            public void onSheetStateChanged(int newState) {
                if (newState == SheetState.PEEK || newState == SheetState.HALF
                        || newState == SheetState.FULL) {
                    mModel.setBottomSheetState(newState);
                }

                // Note: recycler view updates while the bottom sheet is SCROLLING result in a
                // BottomSheet assertion.
                if (newState != BottomSheetController.SheetState.SCROLLING) {
                    maybeShowHeaderChips();
                }

                if (newState == SheetState.HIDDEN) {
                    mOverlayCoordinator.suppress();
                }
                if (newState == SheetState.PEEK || newState == SheetState.HALF
                        || newState == SheetState.FULL) {
                    mOverlayCoordinator.restore();
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

            @Override
            public void onSheetClosed(@StateChangeReason int reason) {
                AssistantBottomBarDelegate delegate = mModel.getBottomBarDelegate();
                if (reason == StateChangeReason.SWIPE && delegate != null) {
                    delegate.onBottomSheetClosedWithSwipe();
                }
            }
        };
        controller.addObserver(mBottomSheetObserver);

        // Show or hide the bottom sheet content when the Autofill Assistant visibility is changed.
        model.addObserver((source, propertyKey) -> {
            if (AssistantModel.VISIBLE == propertyKey) {
                if (model.get(AssistantModel.VISIBLE)) {
                    showContent(/* shouldExpand = */ false, /* animate = */ false);
                } else {
                    hide();
                }
            } else if (AssistantModel.ALLOW_TALKBACK_ON_WEBSITE == propertyKey) {
                if (!model.get(AssistantModel.ALLOW_TALKBACK_ON_WEBSITE)) {
                    mObscuringToken = tabObscuringHandler.obscureAllTabs();
                } else {
                    tabObscuringHandler.unobscureAllTabs(mObscuringToken);
                    mObscuringToken = TokenHolder.INVALID_TOKEN;
                }
            } else if (AssistantModel.WEB_CONTENTS == propertyKey) {
                mWebContents = model.get(AssistantModel.WEB_CONTENTS);
            } else if (AssistantModel.TALKBACK_SHEET_SIZE_FRACTION == propertyKey) {
                mRootViewContainer.setTalkbackViewSizeFraction(
                        model.get(AssistantModel.TALKBACK_SHEET_SIZE_FRACTION));
                updateVisualViewportHeight();
            } else if (AssistantModel.PEEK_MODE_DISABLED == propertyKey) {
                mContent.setPeekModeDisabled(model.get(AssistantModel.PEEK_MODE_DISABLED));
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
                    mRootViewContainer.setClipChildren(canScroll);
                });

        mAccessibilityObserver = (talkbackEnabled) -> {
            setViewportMode(talkbackEnabled ? AssistantViewportMode.RESIZE_VISUAL_VIEWPORT
                                            : mTargetViewportMode);
            PostTask.runOrPostTask(UiThreadTaskTraits.DEFAULT, mRootViewContainer::requestLayout);
        };
        ChromeAccessibilityUtil.get().addObserver(mAccessibilityObserver);
    }

    AssistantActionsCarouselCoordinator getActionsCarouselCoordinator() {
        return mActionsCoordinator;
    }

    private void setupAnimations(AssistantModel model, ViewGroup rootView) {
        model.getHeaderModel().addObserver((source, propertyKey) -> {
            if (propertyKey == AssistantHeaderModel.CHIPS_VISIBLE
                    || propertyKey == AssistantHeaderModel.CHIPS) {
                // The PostTask is necessary as a workaround for the sticky button occasionally not
                // showing, since the chip changes are now issued in the following UI iteration, the
                // same needs to be done for the corresponding animations.
                // TODO(b/164389932): Figure out a better fix that doesn't require issuing the
                // change in the following UI iteration.
                PostTask.postTask(UiThreadTaskTraits.DEFAULT, () -> animateChildren(rootView));
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
        model.getFormModel().addObserver((source, propertyKey) -> {
            if (AssistantFormModel.INPUTS == propertyKey) {
                animateChildren(rootView);
            }
        });

        model.getGenericUiModel().addObserver((source, propertyKey) -> {
            if (AssistantGenericUiModel.VIEW == propertyKey) {
                animateChildren(rootView);
            }
        });
    }

    private void animateChildren(ViewGroup rootView) {
        TransitionManager.beginDelayedTransition(rootView, mLayoutTransition);
    }

    private void maybeShowHeaderChips() {
        boolean showChips =
                mBottomSheetController.getSheetState() == BottomSheetController.SheetState.PEEK
                && mPeekHeightCoordinator.getPeekMode()
                        == AssistantPeekHeightCoordinator.PeekMode.HANDLE_HEADER;
        mModel.getHeaderModel().set(AssistantHeaderModel.CHIPS_VISIBLE, showChips);
    }

    /**
     * Cleanup resources when this goes out of scope.
     */
    public void destroy() {
        resetVisualViewportHeight();
        mWindowApplicationInsetSupplier.removeSupplier(mInsetSupplier);
        ChromeAccessibilityUtil.get().removeObserver(mAccessibilityObserver);
        mBottomSheetController.removeObserver(mBottomSheetObserver);

        if (mObscuringToken != TokenHolder.INVALID_TOKEN) {
            mTabObscuringHandler.unobscureAllTabs(mObscuringToken);
        }

        mInfoBoxCoordinator.destroy();
        mInfoBoxCoordinator = null;
        mPaymentRequestCoordinator.destroy();
        mPaymentRequestCoordinator = null;
        mHeaderCoordinator.destroy();
        mRootViewContainer.destroy();
    }

    /** Request showing the Assistant bottom bar view and expand the sheet. */
    public void showContent(boolean shouldExpand, boolean animate) {
        freezeBottomSheetAnimation();
        BottomSheetUtils.showContentAndMaybeExpand(
                mBottomSheetController, mContent, shouldExpand, animate);
    }

    /** Hide the Assistant bottom bar view. */
    public void hide() {
        freezeBottomSheetAnimation();
        mBottomSheetController.hideContent(mContent, /* animate= */ true);
    }

    void setViewportMode(@AssistantViewportMode int mode) {
        if (mode == mViewportMode) return;
        if (ChromeAccessibilityUtil.get().isAccessibilityEnabled()
                && mode != AssistantViewportMode.RESIZE_VISUAL_VIEWPORT) {
            mTargetViewportMode = mode;
            return;
        }

        mViewportMode = mode;
        mTargetViewportMode = mode;
        updateVisualViewportHeight();
    }

    /** Set the peek mode. */
    void setPeekMode(@AssistantPeekHeightCoordinator.PeekMode int peekMode) {
        mPeekHeightCoordinator.setPeekMode(peekMode);
        maybeShowHeaderChips();
    }

    /** Expand the bottom sheet. */
    void expand() {
        mBottomSheetController.expandSheet();
    }

    /** Collapse the bottom sheet to the peek mode. */
    void collapse() {
        mBottomSheetController.collapseSheet(/* animate = */ true);
    }

    void restoreState(@SheetState int state) {
        BottomSheetUtils.restoreState(mBottomSheetController, mContent, state);
    }

    @Override
    public void setShowOnlyCarousels(boolean showOnlyCarousels) {
        mScrollableContent.setVisibility(showOnlyCarousels ? View.GONE : View.VISIBLE);
    }

    @Override
    public void onPeekHeightChanged() {
        updateVisualViewportHeight();
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
    private void hideWhenEmpty(View carouselView, AssistantCarouselModel carouselModel) {
        setCarouselVisibility(carouselView, carouselModel);
        carouselModel.addObserver(
                (source, propertyKey) -> setCarouselVisibility(carouselView, carouselModel));
    }

    private void setCarouselVisibility(View carouselView, AssistantCarouselModel carouselModel) {
        carouselView.setVisibility(carouselModel.get(AssistantCarouselModel.CHIPS).size() > 0
                        ? View.VISIBLE
                        : View.GONE);
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

    private void updateVisualViewportHeight() {
        if (mViewportMode == AssistantViewportMode.NO_RESIZE
                || mBottomSheetController.getCurrentSheetContent() != mContent) {
            resetVisualViewportHeight();
            return;
        }

        setVisualViewportResizing(mBottomSheetController.getCurrentOffset());
    }

    private void resetVisualViewportHeight() {
        setVisualViewportResizing(0);
    }

    /**
     * Shrink the visual viewport by {@code resizing} pixels.
     */
    private void setVisualViewportResizing(int resizing) {
        int currentInset = mInsetSupplier.get() != null ? mInsetSupplier.get() : 0;
        if (resizing == currentInset || mWebContents == null
                || mWebContents.getRenderWidgetHostView() == null) {
            return;
        }

        mInsetSupplier.set(resizing);
    }

    /**
     * Freeze the bottom sheet animation before expanding/collapsing it, in order to prevent crash
     * (see b/179131022).
     */
    private void freezeBottomSheetAnimation() {
        Callback<Integer> offsetController = mContent.getOffsetController();
        if (offsetController == null) {
            return;
        }
        offsetController.onResult(mBottomSheetController.getCurrentOffset());
    }
}
