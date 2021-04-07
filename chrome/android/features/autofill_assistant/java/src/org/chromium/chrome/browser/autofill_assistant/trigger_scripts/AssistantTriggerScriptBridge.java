// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant.trigger_scripts;

import android.content.Context;

import androidx.annotation.Nullable;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.chrome.browser.ActivityTabProvider;
import org.chromium.chrome.browser.autofill_assistant.AssistantCoordinator;
import org.chromium.chrome.browser.autofill_assistant.AssistantDependenciesImpl;
import org.chromium.chrome.browser.autofill_assistant.AutofillAssistantClient;
import org.chromium.chrome.browser.autofill_assistant.AutofillAssistantPreferencesUtil;
import org.chromium.chrome.browser.autofill_assistant.AutofillAssistantUiController;
import org.chromium.chrome.browser.autofill_assistant.TriggerContext;
import org.chromium.chrome.browser.autofill_assistant.carousel.AssistantChip;
import org.chromium.chrome.browser.autofill_assistant.header.AssistantHeaderModel;
import org.chromium.chrome.browser.autofill_assistant.metrics.LiteScriptFinishedState;
import org.chromium.chrome.browser.autofill_assistant.onboarding.AssistantOnboardingResult;
import org.chromium.chrome.browser.autofill_assistant.onboarding.BaseOnboardingCoordinator;
import org.chromium.chrome.browser.feedback.HelpAndFeedbackLauncherImpl;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabUtils;
import org.chromium.ui.KeyboardVisibilityDelegate;

import java.util.List;

/**
 * Communicates with the native {@code TriggerScriptBridgeAndroid} to show and hide trigger
 * scripts.
 */
@JNINamespace("autofill_assistant")
public class AssistantTriggerScriptBridge {
    private final AssistantDependenciesImpl mStartupDependencies;

    private AssistantTriggerScript mTriggerScript;
    private long mNativeBridge;
    private Delegate mDelegate;
    private KeyboardVisibilityDelegate.KeyboardVisibilityListener mKeyboardVisibilityListener;
    private ActivityTabProvider.ActivityTabTabObserver mActivityTabObserver;
    private TriggerContext mTriggerContext;

    private BaseOnboardingCoordinator mOnboardingCoordinator;

    /** Interface for delegates of the {@code start} method. */
    public interface Delegate {
        void onTriggerScriptFinished(@LiteScriptFinishedState int finishedState);
    }
    public AssistantTriggerScriptBridge(AssistantDependenciesImpl startupDependencies) {
        mStartupDependencies = startupDependencies;
    }

    /**
     * Starts the trigger script for {@code initialUrl} and reports the finished state to {@code
     * delegate}.
     */
    public void start(TriggerContext triggerContext, Delegate delegate) {
        mTriggerContext = triggerContext;
        mDelegate = delegate;
        mTriggerScript = new AssistantTriggerScript(mStartupDependencies.getContext(),
                new AssistantTriggerScript.Delegate() {
                    @Override
                    public void onTriggerScriptAction(int action) {
                        safeNativeOnTriggerScriptAction(action);
                    }

                    @Override
                    public void onBottomSheetClosedWithSwipe() {
                        safeNativeOnBottomSheetClosedWithSwipe();
                    }

                    @Override
                    public boolean onBackButtonPressed() {
                        return safeNativeOnBackButtonPressed();
                    }

                    @Override
                    public void onFeedbackButtonClicked() {
                        HelpAndFeedbackLauncherImpl.getInstance().showFeedback(
                                TabUtils.getActivity(TabUtils.fromWebContents(
                                        mStartupDependencies.getWebContents())),
                                AutofillAssistantUiController.getProfile(),
                                mStartupDependencies.getWebContents().getVisibleUrl().getSpec(),
                                AssistantCoordinator.FEEDBACK_CATEGORY_TAG);
                    }
                },
                mStartupDependencies.getWebContents(),
                mStartupDependencies.getBottomSheetController(),
                mStartupDependencies.getBottomInsetProvider());

        if (mKeyboardVisibilityListener != null) {
            mStartupDependencies.getKeyboardVisibilityDelegate().removeKeyboardVisibilityListener(
                    mKeyboardVisibilityListener);
        }
        mKeyboardVisibilityListener = this::safeNativeOnKeyboardVisibilityChanged;
        mStartupDependencies.getKeyboardVisibilityDelegate().addKeyboardVisibilityListener(
                mKeyboardVisibilityListener);

        mActivityTabObserver =
                new ActivityTabProvider.ActivityTabTabObserver(
                        mStartupDependencies.getActivityTabProvider(), true) {
                    @Override
                    public void onInteractabilityChanged(Tab tab, boolean isInteractable) {
                        safeNativeOnTabInteractabilityChanged(isInteractable);
                    }
                };

        // Request the client to start the trigger script. Native will then bind itself to this java
        // instance via setNativePtr.
        AutofillAssistantClient.fromWebContents(mStartupDependencies.getWebContents())
                .startTriggerScript(mTriggerContext, this);
    }

    @Nullable
    public BaseOnboardingCoordinator getOnboardingCoordinator() {
        return mOnboardingCoordinator;
    }

    /**
     * Re-creates the header and returns the new header model. Must be called before every
     * invocation of {@code showTriggerScript}. It is not possible to persist headers across
     * multiple shown trigger scripts.
     */
    @CalledByNative
    private AssistantHeaderModel createHeaderAndGetModel() {
        return mTriggerScript.createHeaderAndGetModel();
    }

    @CalledByNative
    private Context getContext() {
        return mStartupDependencies.getContext();
    }

    /** Returns whether the user has seen a trigger script before or not. */
    @CalledByNative
    private static boolean isFirstTimeTriggerScriptUser() {
        return AutofillAssistantPreferencesUtil.isAutofillAssistantFirstTimeTriggerScriptUser();
    }

    /**
     * Used by native to update and show the UI. The header should be created and updated using
     * {@code createHeaderAndGetModel} prior to calling this function.
     * @return true if the trigger script was displayed, else false.
     */
    @CalledByNative
    private boolean showTriggerScript(String[] cancelPopupMenuItems, int[] cancelPopupMenuActions,
            List<AssistantChip> leftAlignedChips, int[] leftAlignedChipsActions,
            List<AssistantChip> rightAlignedChips, int[] rightAlignedChipsActions,
            boolean resizeVisualViewport, boolean scrollToHide) {
        // Trigger scripts currently do not support switching activities (such as CCT->tab).
        // TODO(b/171776026): Re-inject dependencies on activity change to support CCT->tab.
        if (TabUtils.getActivity(TabUtils.fromWebContents(mStartupDependencies.getWebContents()))
                != mStartupDependencies.getContext()) {
            return false;
        }

        // NOTE: the cancel popup menu must be set before the chips are bound.
        mTriggerScript.setCancelPopupMenu(cancelPopupMenuItems, cancelPopupMenuActions);
        mTriggerScript.setLeftAlignedChips(leftAlignedChips, leftAlignedChipsActions);
        mTriggerScript.setRightAlignedChips(rightAlignedChips, rightAlignedChipsActions);
        boolean shown = mTriggerScript.show(resizeVisualViewport, scrollToHide);

        // A trigger script was displayed, users are no longer considered first-time users.
        if (shown) {
            AutofillAssistantPreferencesUtil.setAutofillAssistantFirstTimeTriggerScriptUser(false);
        }
        return shown;
    }

    @CalledByNative
    private void hideTriggerScript() {
        mTriggerScript.hide();
    }

    @CalledByNative
    private void onOnboardingRequested(boolean isDialogOnboardingEnabled) {
        if (!AutofillAssistantPreferencesUtil.getShowOnboarding()) {
            safeNativeOnOnboardingFinished(
                    /* onboardingShown= */ false, /* result= */ AssistantOnboardingResult.ACCEPTED);
            return;
        }

        showOnboardingForTriggerScript(isDialogOnboardingEnabled);
    }

    @CalledByNative
    private void onTriggerScriptFinished(@LiteScriptFinishedState int state) {
        if (state == LiteScriptFinishedState.LITE_SCRIPT_PROMPT_FAILED_CANCEL_FOREVER) {
            AutofillAssistantPreferencesUtil.setProactiveHelpSwitch(false);
        }
        mDelegate.onTriggerScriptFinished(state);
    }

    @CalledByNative
    private static boolean isProactiveHelpEnabled() {
        return AutofillAssistantPreferencesUtil.isProactiveHelpOn();
    }

    @CalledByNative
    private void setNativePtr(long nativePtr) {
        mNativeBridge = nativePtr;
    }

    @CalledByNative
    private void clearNativePtr() {
        mNativeBridge = 0;
        mTriggerScript.destroy();
        mStartupDependencies.getKeyboardVisibilityDelegate().removeKeyboardVisibilityListener(
                mKeyboardVisibilityListener);
        mActivityTabObserver.destroy();
    }

    // On finishing of onboarding for trigger scripts.
    private void safeNativeOnOnboardingFinished(
            boolean onboardingShown, @AssistantOnboardingResult int result) {
        if (mNativeBridge != 0) {
            AssistantTriggerScriptBridgeJni.get().onOnboardingFinished(
                    mNativeBridge, AssistantTriggerScriptBridge.this, onboardingShown, result);
        }
    }

    private void safeNativeOnTriggerScriptAction(int action) {
        if (mNativeBridge != 0) {
            AssistantTriggerScriptBridgeJni.get().onTriggerScriptAction(
                    mNativeBridge, AssistantTriggerScriptBridge.this, action);
        }
    }

    private void safeNativeOnBottomSheetClosedWithSwipe() {
        if (mNativeBridge != 0) {
            AssistantTriggerScriptBridgeJni.get().onBottomSheetClosedWithSwipe(
                    mNativeBridge, AssistantTriggerScriptBridge.this);
        }
    }

    private boolean safeNativeOnBackButtonPressed() {
        if (mNativeBridge != 0) {
            return AssistantTriggerScriptBridgeJni.get().onBackButtonPressed(
                    mNativeBridge, AssistantTriggerScriptBridge.this);
        }
        return false;
    }

    private void safeNativeOnKeyboardVisibilityChanged(boolean visible) {
        if (mNativeBridge != 0) {
            AssistantTriggerScriptBridgeJni.get().onKeyboardVisibilityChanged(
                    mNativeBridge, AssistantTriggerScriptBridge.this, visible);
        }
    }

    private void safeNativeOnTabInteractabilityChanged(boolean interactable) {
        if (mNativeBridge != 0) {
            AssistantTriggerScriptBridgeJni.get().onTabInteractabilityChanged(
                    mNativeBridge, AssistantTriggerScriptBridge.this, interactable);
        }
    }

    private void showOnboardingForTriggerScript(boolean isDialogOnboardingEnabled) {
        mStartupDependencies.showOnboarding(isDialogOnboardingEnabled, mTriggerContext,
                result -> { safeNativeOnOnboardingFinished(/* onboardingShown= */ true, result); });
    }

    @NativeMethods
    interface Natives {
        void onTriggerScriptAction(long nativeTriggerScriptBridgeAndroid,
                AssistantTriggerScriptBridge caller, int action);
        void onBottomSheetClosedWithSwipe(
                long nativeTriggerScriptBridgeAndroid, AssistantTriggerScriptBridge caller);
        boolean onBackButtonPressed(
                long nativeTriggerScriptBridgeAndroid, AssistantTriggerScriptBridge caller);
        void onKeyboardVisibilityChanged(long nativeTriggerScriptBridgeAndroid,
                AssistantTriggerScriptBridge caller, boolean visible);
        void onTabInteractabilityChanged(long nativeTriggerScriptBridgeAndroid,
                AssistantTriggerScriptBridge caller, boolean interactable);
        void onOnboardingFinished(long nativeTriggerScriptBridgeAndroid,
                AssistantTriggerScriptBridge caller, boolean onboardingShown,
                @AssistantOnboardingResult int result);
    }
}
