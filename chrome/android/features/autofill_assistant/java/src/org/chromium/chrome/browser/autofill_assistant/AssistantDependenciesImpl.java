// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant;

import android.content.Context;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import org.chromium.base.Callback;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.chrome.browser.ActivityTabProvider;
import org.chromium.chrome.browser.autofill_assistant.onboarding.AssistantOnboardingResult;
import org.chromium.chrome.browser.autofill_assistant.onboarding.BaseOnboardingCoordinator;
import org.chromium.chrome.browser.autofill_assistant.onboarding.OnboardingCoordinatorFactory;
import org.chromium.chrome.browser.autofill_assistant.overlay.AssistantOverlayCoordinator;
import org.chromium.chrome.browser.autofill_assistant.trigger_scripts.AssistantTriggerScriptBridge;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider;
import org.chromium.chrome.browser.compositor.CompositorViewHolder;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.ActivityKeyboardVisibilityDelegate;
import org.chromium.ui.base.ApplicationViewportInsetSupplier;

import java.util.Map;

/**
 * Concrete implementation of the AssistantDependencies interface. Provides the dependencies
 * necessary to start an autofill-assistant flow.
 */
@JNINamespace("autofill_assistant")
public class AssistantDependenciesImpl implements AssistantDependencies {
    // Dependencies tied to the activity.
    private final Context mContext;
    private final BottomSheetController mBottomSheetController;
    private final BrowserControlsStateProvider mBrowserControls;
    private final ActivityKeyboardVisibilityDelegate mKeyboardVisibilityDelegate;
    private final ApplicationViewportInsetSupplier mBottomInsetProvider;
    private final ActivityTabProvider mActivityTabProvider;
    private final CompositorViewHolder mCompositorViewHolder;

    // Dependencies tied to the web_contents.
    private final OnboardingCoordinatorFactory mOnboardingCoordinatorFactory;
    private final AssistantTriggerScriptBridge mTriggerScriptBridge;
    private final WebContents mWebContents;

    /**
     * The currently shown onboarding coordinator, if any. Only set while the onboarding is shown.
     */
    private @Nullable BaseOnboardingCoordinator mOnboardingCoordinator;

    /** The most recently shown onboarding overlay coordinator, if any. */
    private @Nullable AssistantOverlayCoordinator mOnboardingOverlayCoordinator;

    AssistantDependenciesImpl(BottomSheetController bottomSheetController,
            BrowserControlsStateProvider browserControls, CompositorViewHolder compositorViewHolder,
            Context context, @NonNull WebContents webContents,
            ActivityKeyboardVisibilityDelegate keyboardVisibilityDelegate,
            ApplicationViewportInsetSupplier bottomInsetProvider,
            ActivityTabProvider activityTabProvider) {
        mOnboardingCoordinatorFactory = new OnboardingCoordinatorFactory(
                context, bottomSheetController, browserControls, compositorViewHolder);
        mContext = context;
        mWebContents = webContents;
        mBottomSheetController = bottomSheetController;
        mBrowserControls = browserControls;
        mKeyboardVisibilityDelegate = keyboardVisibilityDelegate;
        mBottomInsetProvider = bottomInsetProvider;
        mActivityTabProvider = activityTabProvider;
        mCompositorViewHolder = compositorViewHolder;
        mTriggerScriptBridge = new AssistantTriggerScriptBridge(this);
    }

    /**
     * Shows the onboarding to the user. Also takes ownership of the shown overlay coordinator after
     * the onboarding is finished such that the regular startup can reuse it.
     */
    @Override
    public void showOnboarding(boolean useDialogOnboarding, String experimentIds,
            Map<String, String> parameters, Callback<Integer> callback) {
        hideOnboarding();
        if (useDialogOnboarding) {
            mOnboardingCoordinator =
                    mOnboardingCoordinatorFactory.createDialogOnboardingCoordinator(
                            experimentIds, parameters);
        } else {
            mOnboardingCoordinator =
                    mOnboardingCoordinatorFactory.createBottomSheetOnboardingCoordinator(
                            experimentIds, parameters);
        }

        mOnboardingCoordinator.show(result -> {
            // Note: only transfer the controls in the ACCEPTED case, as it will prevent
            // the bottom sheet from hiding after the callback is done.
            if (result == AssistantOnboardingResult.ACCEPTED) {
                mOnboardingOverlayCoordinator = mOnboardingCoordinator.transferControls();
            }
            callback.onResult(result);
        });
    }

    @Override
    public void hideOnboarding() {
        if (mOnboardingCoordinator != null) {
            mOnboardingCoordinator.hide();
            mOnboardingCoordinator = null;
        }
    }

    /**
     * Transfers ownership of the overlay coordinator shown during the most recent onboarding, if
     * any.
     */
    @CalledByNative
    public @Nullable AssistantOverlayCoordinator transferOnboardingOverlayCoordinator() {
        AssistantOverlayCoordinator overlayCoordinator = mOnboardingOverlayCoordinator;
        mOnboardingOverlayCoordinator = null;
        return overlayCoordinator;
    }

    public WebContents getWebContents() {
        return mWebContents;
    }
    public AssistantTriggerScriptBridge getTriggerScriptBridge() {
        return mTriggerScriptBridge;
    }
    public Context getContext() {
        return mContext;
    }
    public BottomSheetController getBottomSheetController() {
        return mBottomSheetController;
    }
    public BrowserControlsStateProvider getBrowserControls() {
        return mBrowserControls;
    }
    public ActivityKeyboardVisibilityDelegate getKeyboardVisibilityDelegate() {
        return mKeyboardVisibilityDelegate;
    }
    public ApplicationViewportInsetSupplier getBottomInsetProvider() {
        return mBottomInsetProvider;
    }
    public ActivityTabProvider getActivityTabProvider() {
        return mActivityTabProvider;
    }
    public CompositorViewHolder getCompositorViewHolder() {
        return mCompositorViewHolder;
    }
}
