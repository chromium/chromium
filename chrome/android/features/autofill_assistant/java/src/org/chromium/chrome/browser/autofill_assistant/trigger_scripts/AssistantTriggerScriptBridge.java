// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant.trigger_scripts;

import android.content.Context;

import androidx.annotation.NonNull;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.chrome.browser.autofill_assistant.AssistantCoordinator;
import org.chromium.chrome.browser.autofill_assistant.AutofillAssistantClient;
import org.chromium.chrome.browser.autofill_assistant.AutofillAssistantPreferencesUtil;
import org.chromium.chrome.browser.autofill_assistant.AutofillAssistantUiController;
import org.chromium.chrome.browser.autofill_assistant.carousel.AssistantChip;
import org.chromium.chrome.browser.autofill_assistant.header.AssistantHeaderModel;
import org.chromium.chrome.browser.autofill_assistant.metrics.LiteScriptFinishedState;
import org.chromium.chrome.browser.feedback.HelpAndFeedbackLauncherImpl;
import org.chromium.chrome.browser.tab.TabUtils;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.KeyboardVisibilityDelegate;
import org.chromium.ui.base.ActivityKeyboardVisibilityDelegate;

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
    private ActivityKeyboardVisibilityDelegate mKeyboardVisibilityDelegate;
    private KeyboardVisibilityDelegate.KeyboardVisibilityListener mKeyboardVisibilityListener;

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
            @NonNull WebContents webContents, @NonNull String initialUrl,
            Map<String, String> scriptParameters, String experimentIds, Delegate delegate) {
        mDelegate = delegate;
        mContext = context;
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
        }, bottomSheetController);

        if (mKeyboardVisibilityListener != null) {
            mKeyboardVisibilityDelegate.removeKeyboardVisibilityListener(
                    mKeyboardVisibilityListener);
        }
        mKeyboardVisibilityListener = this::safeNativeOnKeyboardVisibilityChanged;
        mKeyboardVisibilityDelegate.addKeyboardVisibilityListener(mKeyboardVisibilityListener);
        // Request the client to start the trigger script. Native will then bind itself to this java
        // instance via setNativePtr.
        AutofillAssistantClient.fromWebContents(webContents)
                .startTriggerScript(this, initialUrl, scriptParameters, experimentIds);
    }

    @CalledByNative
    private AssistantHeaderModel getHeaderModel() {
        return mTriggerScript.getHeaderModel();
    }

    @CalledByNative
    private Context getContext() {
        return mContext;
    }

    /**
     * Used by native to update and show the UI. The header should be updated using {@code
     * getHeaderModel} prior to calling this function.
     */
    @CalledByNative
    private void showTriggerScript(String[] cancelPopupMenuItems, int[] cancelPopupMenuActions,
            List<AssistantChip> leftAlignedChips, int[] leftAlignedChipsActions,
            List<AssistantChip> rightAlignedChips, int[] rightAlignedChipsActions) {
        // NOTE: the cancel popup menu must be set before the chips are bound.
        mTriggerScript.setCancelPopupMenu(cancelPopupMenuItems, cancelPopupMenuActions);
        mTriggerScript.setLeftAlignedChips(leftAlignedChips, leftAlignedChipsActions);
        mTriggerScript.setRightAlignedChips(rightAlignedChips, rightAlignedChipsActions);
        mTriggerScript.show();

        // A trigger script was displayed, users are no longer considered first-time users.
        AutofillAssistantPreferencesUtil.setAutofillAssistantReturningLiteScriptUser();
    }

    @CalledByNative
    private void hideTriggerScript() {
        mTriggerScript.hide();
    }

    @CalledByNative
    private void onTriggerScriptFinished(@LiteScriptFinishedState int state) {
        mDelegate.onTriggerScriptFinished(state);
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
    }
}
