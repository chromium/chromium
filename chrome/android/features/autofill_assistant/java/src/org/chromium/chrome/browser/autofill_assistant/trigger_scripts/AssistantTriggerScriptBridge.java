// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant.trigger_scripts;

import android.content.Context;

import androidx.annotation.NonNull;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.chrome.browser.autofill_assistant.AutofillAssistantClient;
import org.chromium.chrome.browser.autofill_assistant.carousel.AssistantChip;
import org.chromium.chrome.browser.autofill_assistant.header.AssistantHeaderModel;
import org.chromium.chrome.browser.autofill_assistant.metrics.LiteScriptFinishedState;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.content_public.browser.WebContents;

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
            @NonNull WebContents webContents, @NonNull String initialUrl,
            Map<String, String> scriptParameters, String experimentIds, Delegate delegate) {
        mDelegate = delegate;
        mContext = context;
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
            public void onBackButtonPressed() {
                safeNativeOnBackButtonPressed();
            }

            @Override
            public void onFeedbackButtonClicked() {
                safeNativeOnFeedbackButtonClicked();
            }
        }, bottomSheetController);

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

    private void safeNativeOnBackButtonPressed() {
        if (mNativeBridge != 0) {
            AssistantTriggerScriptBridgeJni.get().onBackButtonPressed(
                    mNativeBridge, AssistantTriggerScriptBridge.this);
        }
    }

    private void safeNativeOnFeedbackButtonClicked() {
        if (mNativeBridge != 0) {
            AssistantTriggerScriptBridgeJni.get().onFeedbackButtonClicked(
                    mNativeBridge, AssistantTriggerScriptBridge.this);
        }
    }

    @NativeMethods
    interface Natives {
        void onTriggerScriptAction(long nativeTriggerScriptBridgeAndroid,
                AssistantTriggerScriptBridge caller, int action);
        void onBottomSheetClosedWithSwipe(
                long nativeTriggerScriptBridgeAndroid, AssistantTriggerScriptBridge caller);
        void onBackButtonPressed(
                long nativeTriggerScriptBridgeAndroid, AssistantTriggerScriptBridge caller);
        void onFeedbackButtonClicked(
                long nativeTriggerScriptBridgeAndroid, AssistantTriggerScriptBridge caller);
    }
}
