// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.password_manager;

import org.chromium.base.Log;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.chrome.browser.app.ChromeActivity;
import org.chromium.chrome.browser.autofill_assistant.AutofillAssistantArguments;
import org.chromium.chrome.browser.autofill_assistant.AutofillAssistantFacade;
import org.chromium.ui.base.WindowAndroid;

/** Class for starting a password change flow in Autofill Assistant. */
public class PasswordChangeLauncher {
    private static final String TAG = "AutofillAssistant";

    /**
     * Name for the parameter that stores session username. Should be synced with
     * |kSessionUsernameParameterName| from
     * components/autofill_assistant/browser/controller.cc
     * TODO(b/151401974): Eliminate duplicate parameter definitions.
     */
    private static final String PASSWORD_CHANGE_USERNAME_PARAMETER = "PASSWORD_CHANGE_USERNAME";
    private static final String INTENT_PARAMETER = "INTENT";
    private static final String INTENT = "PASSWORD_CHANGE";

    @CalledByNative
    public static void start(WindowAndroid windowAndroid, String origin, String username) {
        ChromeActivity activity = (ChromeActivity) windowAndroid.getActivity().get();
        if (activity == null) {
            Log.v(TAG, "Failed to retrieve ChromeActivity.");
            return;
        }
        AutofillAssistantFacade.start(activity,
                AutofillAssistantArguments.newBuilder()
                        .withInitialUrl(origin)
                        .addParameter(PASSWORD_CHANGE_USERNAME_PARAMETER, username)
                        .addParameter(INTENT_PARAMETER, INTENT)
                        .addParameter(AutofillAssistantArguments.PARAMETER_START_IMMEDIATELY, true)
                        .build());
    }
}
