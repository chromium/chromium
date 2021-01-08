// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant.trigger_scripts;

import android.content.Context;

import androidx.annotation.NonNull;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.chrome.browser.ActivityTabProvider;
import org.chromium.chrome.browser.autofill_assistant.AssistantCoordinator;
import org.chromium.chrome.browser.autofill_assistant.AutofillAssistantClient;
import org.chromium.chrome.browser.autofill_assistant.AutofillAssistantPreferencesUtil;
import org.chromium.chrome.browser.autofill_assistant.AutofillAssistantUiController;
import org.chromium.chrome.browser.autofill_assistant.carousel.AssistantChip;
import org.chromium.chrome.browser.autofill_assistant.header.AssistantHeaderModel;
import org.chromium.chrome.browser.autofill_assistant.metrics.LiteScriptFinishedState;
import org.chromium.chrome.browser.feedback.HelpAndFeedbackLauncherImpl;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabUtils;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.KeyboardVisibilityDelegate;
import org.chromium.ui.base.ActivityKeyboardVisibilityDelegate;
import org.chromium.ui.base.ApplicationViewportInsetSupplier;

import java.util.List;
import java.util.Map;

/**
 * Communicates with the native {@code TriggerScriptBridgeAndroid} to show and hide trigger
 * scripts.
 */
@JNINamespace("autofill_assistant")
public class AssistantTriggerScriptBridge {
    private AssistantTriggerScript mTriggerScript;
    private long mNativeBridge;
    private Delegate mDelegate;
    private Context mContext;
    private WebContents mWebContents;
    private ActivityKeyboardVisibilityDelegate mKeyboardVisibilityDelegate;
    private KeyboardVisibilityDelegate.KeyboardVisibilityListener mKeyboardVisibilityListener;
    private ActivityTabProvider.ActivityTabTabObserver mActivityTabObserver;

    /** Interface for delegates of the {@code start} method. */
    public interface Delegate {
        void onTriggerScriptFinished(@LiteScriptFinishedState int finishedState);
    }
    public AssistantTriggerScriptBridge() {}

    /**
     * Starts the trigger script for {@code initialUrl} and reports the finished state to {@code
     * delegate}.
     */
    public void start(BottomSheetController bottomSheetController, Context context,
            ActivityKeyboardVisibilityDelegate keyboardVisibilityDelegate,
            ApplicationViewportInsetSupplier bottomInsetProvider,
            ActivityTabProvider activityTabProvider, @NonNull WebContents webContents,
            @NonNull String initialUrl, Map<String, String> scriptParameters, String experimentIds,
            Delegate delegate) {
        mDelegate = delegate;
        mContext = context;
        mWebContents = webContents;
        mKeyboardVisibilityDelegate = keyboardVisibilityDelegate;
        mTriggerScript = new AssistantTriggerScript(context, new AssistantTriggerScript.Delegate() {
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
                        TabUtils.getActivity(TabUtils.fromWebContents(webContents)),
                        AutofillAssistantUiController.getProfile(),
                        webContents.getVisibleUrl().getSpec(),
                        AssistantCoordinator.FEEDBACK_CATEGORY_TAG);
            }
        }, webContents, bottomSheetController, bottomInsetProvider);

        if (mKeyboardVisibilityListener != null) {
            mKeyboardVisibilityDelegate.removeKeyboardVisibilityListener(
                    mKeyboardVisibilityListener);
        }
        mKeyboardVisibilityListener = this::safeNativeOnKeyboardVisibilityChanged;
        mKeyboardVisibilityDelegate.addKeyboardVisibilityListener(mKeyboardVisibilityListener);

        mActivityTabObserver =
                new ActivityTabProvider.ActivityTabTabObserver(activityTabProvider, true) {
                    @Override
                    public void onInteractabilityChanged(Tab tab, boolean isInteractable) {
                        safeNativeOnTabInteractabilityChanged(isInteractable);
                    }
                };

        // Request the client to start the trigger script. Native will then bind itself to this java
        // instance via setNativePtr.
        AutofillAssistantClient.fromWebContents(webContents)
                .startTriggerScript(this, initialUrl, scriptParameters, experimentIds);
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
        return mContext;
    }

    /** Returns whether the user has seen a trigger script before or not. */
    @CalledByNative
    private static boolean isFirstTimeTriggerScriptUser() {
        return AutofillAssistantPreferencesUtil.isAutofillAssistantFirstTimeLiteScriptUser();
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
            boolean resizeVisualViewport) {
        // Trigger scripts currently do not support switching activities (such as CCT->tab).
        // TODO(b/171776026): Re-inject dependencies on activity change to support CCT->tab.
        if (TabUtils.getActivity(TabUtils.fromWebContents(mWebContents)) != mContext) {
            return false;
        }

        // NOTE: the cancel popup menu must be set before the chips are bound.
        mTriggerScript.setCancelPopupMenu(cancelPopupMenuItems, cancelPopupMenuActions);
        mTriggerScript.setLeftAlignedChips(leftAlignedChips, leftAlignedChipsActions);
        mTriggerScript.setRightAlignedChips(rightAlignedChips, rightAlignedChipsActions);
        boolean shown = mTriggerScript.show(resizeVisualViewport);

        // A trigger script was displayed, users are no longer considered first-time users.
        if (shown) {
            AutofillAssistantPreferencesUtil.setAutofillAssistantReturningLiteScriptUser();
        }
        return shown;
    }

    @CalledByNative
    private void hideTriggerScript() {
        mTriggerScript.hide();
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
        mKeyboardVisibilityDelegate.removeKeyboardVisibilityListener(mKeyboardVisibilityListener);
        mActivityTabObserver.destroy();
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
    }
}
