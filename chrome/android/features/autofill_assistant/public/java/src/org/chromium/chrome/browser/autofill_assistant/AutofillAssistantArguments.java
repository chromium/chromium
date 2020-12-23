// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant;

import android.os.Bundle;
import android.text.TextUtils;

import androidx.annotation.Nullable;

import java.net.URLDecoder;
import java.util.HashMap;
import java.util.Map;

/**
 * Parameter class to pass into Autofill Assistant starting procedure.
 */
public class AutofillAssistantArguments {
    /**
     * Builder class for {@link AutofillAssistantArguments}.
     */
    public static class Builder {
        private AutofillAssistantArguments mArguments;

        private Builder(AutofillAssistantArguments arguments) {
            mArguments = arguments;
        }

        public Builder fromBundle(@Nullable Bundle bundle) {
            if (bundle != null) {
                for (String key : bundle.keySet()) {
                    Object value = bundle.get(key);
                    if (value == null) {
                        continue;
                    }
                    if (key.startsWith(INTENT_SPECIAL_PREFIX)) {
                        if (key.equals(INTENT_SPECIAL_PREFIX + PARAMETER_EXPERIMENT_IDS)) {
                            mArguments.addExperimentIds(value.toString());
                        }
                        // There are no other special parameters.
                    } else if (key.startsWith(INTENT_EXTRA_PREFIX)) {
                        if (key.equals(INTENT_EXTRA_PREFIX + PARAMETER_EXPERIMENT_IDS)) {
                            mArguments.addExperimentIds(value.toString());
                        } else {
                            mArguments.mAutofillAssistantParameters.put(
                                    key.substring(INTENT_EXTRA_PREFIX.length()), value);
                        }
                    } else {
                        mArguments.mIntentExtras.put(key, value);
                    }
                }
            }

            return this;
        }

        public Builder withInitialUrl(String url) {
            mArguments.mInitialUrl = url;
            return this;
        }

        public Builder addParameter(String key, Object value) {
            mArguments.mAutofillAssistantParameters.put(key, value);
            return this;
        }

        public AutofillAssistantArguments build() {
            return mArguments;
        }
    }

    private static final String UTF8 = "UTF-8";

    /**
     * Prefix for Intent extras relevant to this feature.
     *
     * <p>Intent starting with this prefix are reported to the controller as parameters, except for
     * the ones starting with {@code INTENT_SPECIAL_PREFIX}.
     */
    private static final String INTENT_EXTRA_PREFIX =
            "org.chromium.chrome.browser.autofill_assistant.";

    /** Prefix for intent extras which are not parameters. */
    private static final String INTENT_SPECIAL_PREFIX = INTENT_EXTRA_PREFIX + "special.";

    /** Special parameter that enables the feature. */
    private static final String PARAMETER_ENABLED = "ENABLED";

    /**
     * Special bool parameter that MUST be present in all intents. It allows the caller to either
     * request immediate start of autobot (if set to true), or a delayed start using trigger scripts
     * (if set to false). If this is set to false, one of the trigger script parameters must be set
     * as well (@code{PARAMETER_REQUEST_TRIGGER_SCRIPT} or @code{PARAMETER_TRIGGER_SCRIPTS_BASE64}).
     */
    public static final String PARAMETER_START_IMMEDIATELY = "START_IMMEDIATELY";

    /** Special parameter for the calling account. */
    private static final String PARAMETER_CALLER_ACCOUNT = "CALLER_ACCOUNT";

    /** Special parameter for user email. */
    private static final String PARAMETER_USER_EMAIL = "USER_EMAIL";

    /** Special parameter for first time user script path. */
    static final String PARAMETER_TRIGGER_FIRST_TIME_USER = "TRIGGER_FIRST_TIME_USER";

    /** Special parameter for returning user script path. */
    static final String PARAMETER_TRIGGER_RETURNING_TIME_USER = "TRIGGER_RETURNING_USER";

    // Deprecated, remove as soon as possible.
    /** Special output parameter that should hold which of the trigger scripts was used, if any. */
    static final String PARAMETER_TRIGGER_SCRIPT_USED = "TRIGGER_SCRIPT_USED";

    /** Special parameter for declaring a user to be in a lite script experiment. */
    static final String PARAMETER_LITE_SCRIPT_EXPERIMENT = "TRIGGER_SCRIPT_EXPERIMENT";

    /**
     * Special parameter for instructing the client to request and run a trigger script prior to
     * starting the regular flow.
     */
    static final String PARAMETER_REQUEST_TRIGGER_SCRIPT = "REQUEST_TRIGGER_SCRIPT";

    /**
     * Special parameter to allow injecting a base64-encoded GetTriggerScriptsResponseProto. When
     * specified, the client will directly use the specified trigger scripts instead of fetching
     * them from a remote backend. Takes precedence over PARAMETER_REQUEST_TRIGGER_SCRIPT.
     */
    static final String PARAMETER_TRIGGER_SCRIPTS_BASE64 = "TRIGGER_SCRIPTS_BASE64";

    /**
     * Special output boolean parameter that will be set to true for regular scripts that were
     * started with a trigger script.
     */
    static final String PARAMETER_STARTED_WITH_TRIGGER_SCRIPT = "STARTED_WITH_TRIGGER_SCRIPT";

    /**
     * Identifier used by parameters/or special intent that indicates experiments passed from
     * the caller.
     */
    private static final String PARAMETER_EXPERIMENT_IDS = "EXPERIMENT_IDS";

    private Map<String, Object> mAutofillAssistantParameters;
    private Map<String, Object> mIntentExtras;
    private StringBuilder mExperimentIds;
    private String mInitialUrl;

    private AutofillAssistantArguments() {
        mAutofillAssistantParameters = new HashMap<>();
        mIntentExtras = new HashMap<>();
        mExperimentIds = new StringBuilder();
    }

    public static Builder newBuilder() {
        return new Builder(new AutofillAssistantArguments());
    }

    /**
     * In M74 experiment ids might come from parameters. This function merges both experiment ids
     * from special intent and parameters.
     */
    private void addExperimentIds(@Nullable String experimentIds) {
        if (TextUtils.isEmpty(experimentIds)) {
            return;
        }
        if (mExperimentIds.length() > 0 && !mExperimentIds.toString().endsWith(",")) {
            mExperimentIds.append(",");
        }
        mExperimentIds.append(experimentIds);
    }

    private boolean getBooleanParameter(String key) {
        Object value = mAutofillAssistantParameters.get(key);
        if (!(value instanceof Boolean)) { // Also catches null.
            return false;
        }

        return (Boolean) value;
    }

    @Nullable
    private String getStringParameter(String key) {
        Object value = mAutofillAssistantParameters.get(key);
        if (!(value instanceof String)) { // Also catches null.
            return null;
        }

        return decode((String) value);
    }

    private String decode(String value) {
        try {
            return URLDecoder.decode(value, UTF8);
        } catch (java.io.UnsupportedEncodingException e) {
            throw new IllegalStateException("Encoding not available.", e);
        }
    }

    public Map<String, String> getParameters() {
        Map<String, String> map = new HashMap<>();

        for (String key : mAutofillAssistantParameters.keySet()) {
            if (PARAMETER_ENABLED.equals(key) || PARAMETER_CALLER_ACCOUNT.equals(key)) {
                continue;
            }
            map.put(key, decode(mAutofillAssistantParameters.get(key).toString()));
        }

        return map;
    }

    /** Returns whether all mandatory script parameters are set. */
    public boolean areMandatoryParametersSet() {
        if (!getBooleanParameter(PARAMETER_ENABLED)
                || mAutofillAssistantParameters.get(PARAMETER_START_IMMEDIATELY) == null) {
            return false;
        }
        if (!getBooleanParameter(PARAMETER_START_IMMEDIATELY)) {
            return requestsTriggerScript() || containsBase64TriggerScripts()
                    || containsTriggerScript();
        }
        return true;
    }

    /**
     * Searches the merged experiment ids in normal and special parameters.
     * @return comma separated list of experiment ids, or empty string.
     */
    public String getExperimentIds() {
        return mExperimentIds.toString();
    }

    /** Returns whether the lite-script experiment flag is set to true. */
    public boolean isLiteScriptExperiment() {
        return getBooleanParameter(PARAMETER_LITE_SCRIPT_EXPERIMENT);
    }

    /**
     * Finds the caller account from the CALLER_ACCOUNT entry.
     * @return caller account or null.
     */
    @Nullable
    public String getCallerAccount() {
        return getStringParameter(PARAMETER_CALLER_ACCOUNT);
    }

    /**
     * Finds the user email from the USER_EMAIL entry or from the ACCOUNT_NAME extra.
     * @return user email or null.
     */
    @Nullable
    public String getUserName() {
        String fromParameter = getStringParameter(PARAMETER_USER_EMAIL);
        if (!TextUtils.isEmpty(fromParameter)) {
            return fromParameter;
        }

        for (String extra : mIntentExtras.keySet()) {
            if (extra.endsWith("ACCOUNT_NAME")) {
                return mIntentExtras.get(extra).toString();
            }
        }

        return null;
    }

    public String getInitialUrl() {
        return mInitialUrl;
    }

    /** Whether the caller requests the client to fetch trigger scripts from a remote endpoint. */
    public boolean requestsTriggerScript() {
        return getBooleanParameter(PARAMETER_REQUEST_TRIGGER_SCRIPT);
    }

    /** Whether the caller specified a base64-encoded trigger scripts response or not. */
    public boolean containsBase64TriggerScripts() {
        return !TextUtils.isEmpty(getStringParameter(PARAMETER_TRIGGER_SCRIPTS_BASE64));
    }

    /** Deprecated. Whether the caller provides script paths for lite scripts to execute. */
    public boolean containsTriggerScript() {
        return !TextUtils.isEmpty(getStringParameter(PARAMETER_TRIGGER_FIRST_TIME_USER))
                && !TextUtils.isEmpty(getStringParameter(PARAMETER_TRIGGER_RETURNING_TIME_USER));
    }
}
