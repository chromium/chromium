// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant;

import static org.chromium.chrome.browser.autofill_assistant.AutofillAssistantArguments.PARAMETER_TRIGGER_SCRIPT_USED;

import androidx.annotation.NonNull;

import org.chromium.base.Callback;
import org.chromium.base.task.PostTask;
import org.chromium.chrome.browser.autofill_assistant.metrics.LiteScriptFinishedState;
import org.chromium.chrome.browser.autofill_assistant.metrics.LiteScriptShownToUser;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider;
import org.chromium.chrome.browser.compositor.CompositorViewHolder;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.content_public.browser.UiThreadTaskTraits;
import org.chromium.content_public.browser.WebContents;

import java.util.HashMap;
import java.util.Map;

/**
 * Configures and runs a lite script and returns the result to the caller.
 */
class AutofillAssistantLiteScriptCoordinator {
    private final BottomSheetController mBottomSheetController;
    private final BrowserControlsStateProvider mBrowserControlsStateProvider;
    private final CompositorViewHolder mCompositorViewHolder;
    private final WebContents mWebContents;

    AutofillAssistantLiteScriptCoordinator(BottomSheetController bottomSheetController,
            BrowserControlsStateProvider browserControls, CompositorViewHolder compositorViewHolder,
            @NonNull WebContents webContents) {
        mBottomSheetController = bottomSheetController;
        mBrowserControlsStateProvider = browserControls;
        mCompositorViewHolder = compositorViewHolder;
        mWebContents = webContents;
    }

    void startLiteScript(String firstTimeUserScriptPath, String returningUserScriptPath,
            Callback<Boolean> onFinishedCallback) {
        String usedScriptPath =
                AutofillAssistantPreferencesUtil.isAutofillAssistantFirstTimeLiteScriptUser()
                ? firstTimeUserScriptPath
                : returningUserScriptPath;
        AutofillAssistantLiteService liteService = new AutofillAssistantLiteService(
                mWebContents, usedScriptPath, new AutofillAssistantLiteService.Delegate() {
                    @Override
                    public void onFinished(int state) {
                        handleLiteScriptResult(state, onFinishedCallback, firstTimeUserScriptPath,
                                returningUserScriptPath);
                    }

                    @Override
                    public void onScriptRunning(boolean uiShown) {
                        AutofillAssistantMetrics.recordLiteScriptShownToUser(mWebContents,
                                uiShown ? LiteScriptShownToUser.LITE_SCRIPT_SHOWN_TO_USER
                                        : LiteScriptShownToUser.LITE_SCRIPT_RUNNING);
                        if (uiShown) {
                            // The prompt was displayed on screen, hence we mark them as returning
                            // user from now on.
                            AutofillAssistantPreferencesUtil
                                    .setAutofillAssistantReturningLiteScriptUser();
                        }
                    }
                });
        AutofillAssistantServiceInjector.setServiceToInject(liteService);
        Map<String, String> parameters = new HashMap<>();
        parameters.put(PARAMETER_TRIGGER_SCRIPT_USED, usedScriptPath);
        AutofillAssistantClient.fromWebContents(mWebContents)
                .start(/* initialUrl= */ "", /* parameters= */ parameters,
                        /* experimentIds= */ "", /* callerAccount= */ "", /* userName= */ "",
                        /* isChromeCustomTab= */ true, /* onboardingCoordinator= */ null);
        AutofillAssistantServiceInjector.setServiceToInject(null);
    }

    private void handleLiteScriptResult(@LiteScriptFinishedState int finishedState,
            Callback<Boolean> onFinishedCallback, String firstTimeUserScriptPath,
            String returningUserScriptPath) {
        AutofillAssistantMetrics.recordLiteScriptFinished(mWebContents, finishedState);

        switch (finishedState) {
            case LiteScriptFinishedState.LITE_SCRIPT_PROMPT_FAILED_CLOSE:
                AutofillAssistantPreferencesUtil
                        .incrementAutofillAssistantNumberOfLiteScriptsCanceled();
            // fall through
            case LiteScriptFinishedState.LITE_SCRIPT_PROMPT_FAILED_NAVIGATE:
            case LiteScriptFinishedState.LITE_SCRIPT_PROMPT_FAILED_CONDITION_NO_LONGER_TRUE:
            case LiteScriptFinishedState.LITE_SCRIPT_PROMPT_FAILED_OTHER:
            case LiteScriptFinishedState.LITE_SCRIPT_PROMPT_SUCCEEDED:
            case LiteScriptFinishedState.LITE_SCRIPT_BROWSE_FAILED_NAVIGATE:
            case LiteScriptFinishedState.LITE_SCRIPT_BROWSE_FAILED_OTHER:
            case LiteScriptFinishedState.LITE_SCRIPT_GET_ACTIONS_FAILED:
            case LiteScriptFinishedState.LITE_SCRIPT_GET_ACTIONS_PARSE_ERROR:
            case LiteScriptFinishedState.LITE_SCRIPT_INVALID_SCRIPT:
            case LiteScriptFinishedState.LITE_SCRIPT_PATH_MISMATCH:
            case LiteScriptFinishedState.LITE_SCRIPT_SERVICE_DELETED:
            case LiteScriptFinishedState.LITE_SCRIPT_UNKNOWN_FAILURE:
            case LiteScriptFinishedState.LITE_SCRIPT_UNSAFE_ACTIONS:
                break;
        }

        if (finishedState == LiteScriptFinishedState.LITE_SCRIPT_PROMPT_SUCCEEDED) {
            onFinishedCallback.onResult(true);
        } else if (finishedState
                == LiteScriptFinishedState.LITE_SCRIPT_PROMPT_FAILED_CONDITION_NO_LONGER_TRUE) {
            // User stayed on domain without making an explicit choice. This will resurface the
            // prompt the next time they visit the trigger page (or silently go away if they
            // navigate away from target domain).
            //
            // Note: this needs to be done asynchronously, to give the old controller enough time
            // to shut down and detach from the UI.
            PostTask.postTask(UiThreadTaskTraits.DEFAULT,
                    ()
                            -> startLiteScript(firstTimeUserScriptPath, returningUserScriptPath,
                                    onFinishedCallback));
        } else {
            onFinishedCallback.onResult(false);
        }
    }
}