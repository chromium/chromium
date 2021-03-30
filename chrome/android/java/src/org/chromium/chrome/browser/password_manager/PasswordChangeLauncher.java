// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.password_manager;

import org.chromium.base.Log;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.chrome.browser.app.ChromeActivity;
import org.chromium.chrome.browser.autofill_assistant.AutofillAssistantFacade;
import org.chromium.chrome.browser.autofill_assistant.TriggerContext;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.url.GURL;

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
    private static final String DEBUG_BUNDLE_ID = "DEBUG_BUNDLE_ID";
    private static final String DEBUG_SOCKET_ID = "DEBUG_SOCKET_ID";

    @CalledByNative
    public static void start(WindowAndroid windowAndroid, GURL origin, String username) {
        start(windowAndroid, origin, username, "", "");
    }

    public static void start(WindowAndroid windowAndroid, GURL origin, String username,
            String debugBundleId, String debutSocketId) {
        ChromeActivity activity = (ChromeActivity) windowAndroid.getActivity().get();
        if (activity == null) {
            Log.v(TAG, "Failed to retrieve ChromeActivity.");
            return;
        }
        AutofillAssistantFacade.start(activity,
                TriggerContext.newBuilder()
                        .withInitialUrl(origin.getSpec())
                        .addParameter(DEBUG_BUNDLE_ID, debugBundleId)
                        .addParameter(DEBUG_SOCKET_ID, debutSocketId)
                        .addParameter(PASSWORD_CHANGE_USERNAME_PARAMETER, username)
                        .addParameter(INTENT_PARAMETER, INTENT)
                        .addParameter(TriggerContext.PARAMETER_START_IMMEDIATELY, true)
                        .build());
    }
}
