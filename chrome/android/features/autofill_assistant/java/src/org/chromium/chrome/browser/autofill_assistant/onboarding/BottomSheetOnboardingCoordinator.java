// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant.onboarding;

import android.content.Context;
import android.view.Gravity;
import android.view.ViewGroup;
import android.view.ViewGroup.LayoutParams;
import android.widget.LinearLayout;
import android.widget.ScrollView;
import android.widget.Space;

import androidx.annotation.Nullable;

import org.chromium.base.Callback;
import org.chromium.chrome.autofill_assistant.R;
import org.chromium.chrome.browser.autofill_assistant.AssistantBottomBarDelegate;
import org.chromium.chrome.browser.autofill_assistant.AssistantBottomSheetContent;
import org.chromium.chrome.browser.autofill_assistant.BottomSheetUtils;
import org.chromium.chrome.browser.autofill_assistant.LayoutUtils;
import org.chromium.chrome.browser.autofill_assistant.overlay.AssistantOverlayCoordinator;
import org.chromium.chrome.browser.autofill_assistant.overlay.AssistantOverlayModel;
import org.chromium.chrome.browser.autofill_assistant.overlay.AssistantOverlayState;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider;
import org.chromium.chrome.browser.compositor.CompositorViewHolder;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetContent;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.SheetState;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetObserver;
import org.chromium.components.browser_ui.bottomsheet.EmptyBottomSheetObserver;
import org.chromium.components.browser_ui.widget.scrim.ScrimCoordinator;

import java.util.Map;

/**
 * Coordinator responsible for showing the bottom sheet onboarding screen when the user is using the
 * Autofill Assistant for the first time.
 */
class BottomSheetOnboardingCoordinator extends BaseOnboardingCoordinator {
    @Nullable
    private AssistantBottomSheetContent mContent;
    private BottomSheetObserver mBottomSheetObserver;
    private final BottomSheetController mController;
    private final BrowserControlsStateProvider mBrowserControls;
    private final CompositorViewHolder mCompositorViewHolder;
    private final ScrimCoordinator mScrimCoordinator;

    @Nullable
    AssistantOverlayCoordinator mOverlayCoordinator;

    BottomSheetOnboardingCoordinator(String experimentIds, Map<String, String> parameters,
            Context context, BottomSheetController controller,
            BrowserControlsStateProvider browserControls, CompositorViewHolder compositorViewHolder,
            ScrimCoordinator scrim) {
        super(experimentIds, parameters, context);
        this.mController = controller;
        this.mBrowserControls = browserControls;
        this.mCompositorViewHolder = compositorViewHolder;
        this.mScrimCoordinator = scrim;
    }

    @Override
    ScrollView createViewImpl() {
        ScrollView baseView = (ScrollView) LayoutUtils.createInflater(mContext).inflate(
                R.layout.autofill_assistant_base_onboarding, /* root= */ null);
        ViewGroup onboardingContentContainer =
                baseView.findViewById(R.id.onboarding_layout_container);

        LinearLayout buttonsLayout = new LinearLayout(mContext);
        buttonsLayout.setLayoutParams(new LinearLayout.LayoutParams(
                LayoutParams.MATCH_PARENT, LayoutParams.WRAP_CONTENT));
        buttonsLayout.setGravity(Gravity.BOTTOM | Gravity.CENTER);
        buttonsLayout.setOrientation(LinearLayout.HORIZONTAL);

        LayoutUtils.createInflater(mContext).inflate(
                R.layout.autofill_assistant_onboarding_no_button, /* root= */ buttonsLayout);

        Space space = new Space(mContext);
        LinearLayout.LayoutParams spaceLayoutParams =
                new LinearLayout.LayoutParams(0, LayoutParams.MATCH_PARENT);
        spaceLayoutParams.weight = 1;
        space.setLayoutParams(spaceLayoutParams);
        buttonsLayout.addView(space);

        LayoutUtils.createInflater(mContext).inflate(
                R.layout.autofill_assistant_onboarding_yes_button, /* root= */ buttonsLayout);

        onboardingContentContainer.addView(buttonsLayout);
        return baseView;
    }

    @Override
    void initViewImpl(Callback<Integer> callback) {
        // If there's a tab, cover it with an overlay.
        AssistantOverlayModel overlayModel = new AssistantOverlayModel();
        mOverlayCoordinator = new AssistantOverlayCoordinator(getContext(), mBrowserControls,
                mCompositorViewHolder, mScrimCoordinator, overlayModel);
        overlayModel.set(AssistantOverlayModel.STATE, AssistantOverlayState.FULL);

        mBottomSheetObserver = new EmptyBottomSheetObserver() {
            @Override
            public void onSheetStateChanged(int newState) {
                if (mOverlayCoordinator == null) {
                    return;
                }

                if (newState == SheetState.HIDDEN) {
                    mOverlayCoordinator.suppress();
                }
                if (newState == SheetState.PEEK || newState == SheetState.HALF
                        || newState == SheetState.FULL) {
                    mOverlayCoordinator.restore();
                }
            }
        };
        mController.addObserver(mBottomSheetObserver);

        AssistantBottomBarDelegate delegate = new AssistantBottomBarDelegate() {
            @Override
            public boolean onBackButtonPressed() {
                onUserAction(
                        /* result= */ AssistantOnboardingResult.DISMISSED, callback);
                return true;
            }

            @Override
            public void onBottomSheetClosedWithSwipe() {}
        };
        BottomSheetContent currentSheetContent = mController.getCurrentSheetContent();
        if (currentSheetContent instanceof AssistantBottomSheetContent) {
            mContent = (AssistantBottomSheetContent) currentSheetContent;
            mContent.setDelegate(() -> delegate);
        } else {
            mContent = new AssistantBottomSheetContent(getContext(), () -> delegate);
        }
    }

    @Override
    void showViewImpl() {
        mContent.setContent(mView, mView);
        BottomSheetUtils.showContentAndMaybeExpand(
                mController, mContent, /* shouldExpand = */ true, mAnimate);
    }

    /**
     * Transfers ownership of the controls to the caller, returns the overlay coordinator, if one
     * was created.
     *
     * <p>This call is only useful when called from inside a callback provided to {@link #show}, as
     * before that there are no controls and after that the coordinator automatically hides them.
     * This call allows callbacks to reuse the controls setup for onboarding and provide a smooth
     * transition.
     */
    @Nullable
    @Override
    public AssistantOverlayCoordinator transferControls() {
        assert isInProgress();
        mContent = null;
        AssistantOverlayCoordinator coordinator = mOverlayCoordinator;
        mOverlayCoordinator = null;
        return coordinator;
    }

    /** Hides the UI, if one is shown. */
    @Override
    public void hide() {
        mController.removeObserver(mBottomSheetObserver);
        if (mContent != null) {
            mController.hideContent(mContent, /* animate= */ mAnimate);
            mContent = null;
        }
        if (mOverlayCoordinator != null) {
            mOverlayCoordinator.destroy();
            mOverlayCoordinator = null;
        }
        destroy();
    }

    @Override
    public boolean isInProgress() {
        return mContent != null;
    }
}