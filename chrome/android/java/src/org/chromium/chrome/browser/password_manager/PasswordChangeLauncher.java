// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.password_manager;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.chrome.browser.autofill_assistant.AutofillAssistantFacade;
import org.chromium.components.autofill_assistant.TriggerContext;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.url.GURL;

import java.io.UnsupportedEncodingException;
import java.net.URLEncoder;

/** Class for starting a password change flow in Autofill Assistant. */
public class PasswordChangeLauncher {
    /**
     * Name for the parameter that stores session username. Should be synced with
     * |kSessionUsernameParameterName| from
     * components/autofill_assistant/browser/controller.cc
     * TODO(b/151401974): Eliminate duplicate parameter definitions.
     */
    private static final String PASSWORD_CHANGE_USERNAME_PARAMETER = "PASSWORD_CHANGE_USERNAME";
    private static final String PASSWORD_CHANGE_SKIP_LOGIN_PARAMETER = "PASSWORD_CHANGE_SKIP_LOGIN";
    private static final String INTENT_PARAMETER = "INTENT";
    private static final String SOURCE_PARAMETER = "SOURCE";
    private static final String INTENT = "PASSWORD_CHANGE";
    private static final String DEBUG_BUNDLE_ID = "DEBUG_BUNDLE_ID";
    private static final String DEBUG_SOCKET_ID = "DEBUG_SOCKET_ID";

    private static final int IN_CHROME_CALLER = 7;
    private static final int SOURCE_PASSWORD_CHANGE_LEAK_WARNING = 10;
    private static final int SOURCE_PASSWORD_CHANGE_SETTINGS = 11;

    private static final String ENCODING = "UTF-8";

    @CalledByNative
    public static void start(
            WindowAndroid windowAndroid, GURL origin, String username, boolean skipLogin) {
        start(windowAndroid, origin, username, skipLogin, "", "");
    }

    public static void start(WindowAndroid windowAndroid, GURL origin, String username,
            boolean skipLogin, String debugBundleId, String debutSocketId) {
        // Note: All string-typed parameters must be URL-encoded, because the
        // corresponding extraction logic will URL-*de*code them before use,
        // see TriggerContext.java.
        try {
            AutofillAssistantFacade.start(windowAndroid.getActivity().get(),
                    TriggerContext.newBuilder()
                            .addParameter(DEBUG_BUNDLE_ID, debugBundleId)
                            .addParameter(DEBUG_SOCKET_ID, debutSocketId)
                            .addParameter(PASSWORD_CHANGE_USERNAME_PARAMETER,
                                    URLEncoder.encode(username, ENCODING))
                            .addParameter(PASSWORD_CHANGE_SKIP_LOGIN_PARAMETER, skipLogin)
                            .addParameter(INTENT_PARAMETER, INTENT)
                            .addParameter(TriggerContext.PARAMETER_START_IMMEDIATELY, true)
                            .addParameter(TriggerContext.PARAMETER_ENABLED, true)
                            .addParameter(TriggerContext.PARAMETER_ORIGINAL_DEEPLINK,
                                    URLEncoder.encode(origin.getSpec(), ENCODING))
                            .addParameter(TriggerContext.PARAMETER_CALLER, IN_CHROME_CALLER)
                            .addParameter(SOURCE_PARAMETER,
                                    skipLogin ? SOURCE_PASSWORD_CHANGE_LEAK_WARNING
                                              : SOURCE_PASSWORD_CHANGE_SETTINGS)
                            .build());
        } catch (UnsupportedEncodingException e) {
            throw new IllegalStateException("Encoding not available.", e);
        }
    }
}
