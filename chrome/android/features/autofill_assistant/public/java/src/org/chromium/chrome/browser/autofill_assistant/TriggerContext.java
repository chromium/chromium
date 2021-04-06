// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant;

import android.os.Bundle;
import android.text.TextUtils;

import androidx.annotation.Nullable;

import org.chromium.base.Log;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;

import java.net.URLDecoder;
import java.util.HashMap;
import java.util.Map;

/**
 * Describes the context of the trigger, which contains the script parameters along with some other
 * optional parameters.
 */
@JNINamespace("autofill_assistant")
public class TriggerContext {
    /**
     * Builder class for {@link TriggerContext}.
     */
    public static class Builder {
        private TriggerContext mTriggerContext;

        private Builder(TriggerContext arguments) {
            mTriggerContext = arguments;
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
                            mTriggerContext.addExperimentIds(value.toString());
                        }
                        // There are no other special parameters.
                    } else if (key.startsWith(INTENT_EXTRA_PREFIX)) {
                        if (key.equals(INTENT_EXTRA_PREFIX + PARAMETER_EXPERIMENT_IDS)) {
                            mTriggerContext.addExperimentIds(value.toString());
                        }
                        mTriggerContext.mScriptParameters.put(
                                key.substring(INTENT_EXTRA_PREFIX.length()), value);
                    }
                }
            }

            return this;
        }

        public Builder withInitialUrl(String url) {
            mTriggerContext.mInitialUrl = url;
            return this;
        }

        public Builder withIsCustomTab(boolean isCustomTab) {
            mTriggerContext.mIsCustomTab = isCustomTab;
            return this;
        }

        public Builder withIsDirectAction(boolean isDirectAction) {
            mTriggerContext.mIsDirectAction = isDirectAction;
            return this;
        }

        public Builder addParameter(String key, Object value) {
            mTriggerContext.mScriptParameters.put(key, value);
            return this;
        }

        public TriggerContext build() {
            return mTriggerContext;
        }
    }

    /** Used for logging. */
    private static final String TAG = "AutofillAssistant";

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

    /** Special parameter for user email. */
    private static final String PARAMETER_USER_EMAIL = "USER_EMAIL";

    /** Special parameter for declaring a user to be in a lite script experiment. */
    static final String PARAMETER_TRIGGER_SCRIPT_EXPERIMENT = "TRIGGER_SCRIPT_EXPERIMENT";

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

    /**
     * The original deeplink as indicated by the caller. Use this parameter instead of the
     * initial URL when available to avoid issues where the initial URL points to a redirect
     * rather than the actual deeplink.
     */
    private static final String PARAMETER_ORIGINAL_DEEPLINK = "ORIGINAL_DEEPLINK";

    private final Map<String, Object> mScriptParameters;
    private final StringBuilder mExperimentIds;
    private String mInitialUrl;
    private boolean mIsCustomTab;
    private boolean mIsDirectAction;

    private TriggerContext() {
        mScriptParameters = new HashMap<>();
        mExperimentIds = new StringBuilder();
    }

    public static Builder newBuilder() {
        return new Builder(new TriggerContext());
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
        Object value = mScriptParameters.get(key);
        if (!(value instanceof Boolean)) { // Also catches null.
            if (value != null) {
                Log.v(TAG, "Expected " + key + " to be boolean, but was " + value.toString());
            }
            return false;
        }

        return (Boolean) value;
    }

    @Nullable
    private String getStringParameter(String key) {
        Object value = mScriptParameters.get(key);
        if (!(value instanceof String)) { // Also catches null.
            if (value != null) {
                Log.v(TAG, "Expected " + key + " to be string, but was " + value.toString());
            }
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

        for (String key : mScriptParameters.keySet()) {
            map.put(key, decode(mScriptParameters.get(key).toString()));
        }

        return map;
    }

    /** Returns whether all mandatory script parameters are set. */
    public boolean areMandatoryParametersSet() {
        if (!getBooleanParameter(PARAMETER_ENABLED)
                || mScriptParameters.get(PARAMETER_START_IMMEDIATELY) == null) {
            return false;
        }
        if (!getBooleanParameter(PARAMETER_START_IMMEDIATELY)) {
            return containsTriggerScript();
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

    /** Returns whether the trigger-script experiment flag is set to true. */
    public boolean isTriggerScriptExperiment() {
        return getBooleanParameter(PARAMETER_TRIGGER_SCRIPT_EXPERIMENT);
    }

    /**
     * Returns the user's email as indicated by the caller, if specified.
     * @return caller's email or null.
     */
    @Nullable
    public String getCallerEmail() {
        return getStringParameter(PARAMETER_USER_EMAIL);
    }

    public String getInitialUrl() {
        return mInitialUrl;
    }

    public String getOriginalDeeplink() {
        return getStringParameter(PARAMETER_ORIGINAL_DEEPLINK);
    }

    /** Whether the caller requests the client to fetch trigger scripts from a remote endpoint. */
    public boolean requestsTriggerScript() {
        return getBooleanParameter(PARAMETER_REQUEST_TRIGGER_SCRIPT);
    }

    /** Whether the caller specified a base64-encoded trigger scripts response or not. */
    public boolean containsBase64TriggerScripts() {
        return !TextUtils.isEmpty(getStringParameter(PARAMETER_TRIGGER_SCRIPTS_BASE64));
    }

    /** Whether the caller requested a trigger script to start in any of the supported ways. */
    public boolean containsTriggerScript() {
        return requestsTriggerScript() || containsBase64TriggerScripts();
    }

    /**
     *  Returns true if this trigger context is a valid autofill-assistant trigger context.
     */
    public boolean isValid() {
        // Avoid JNI roundtrip for common case.
        if (!getBooleanParameter(PARAMETER_ENABLED)) {
            return false;
        }

        long nativeInstance = toNative();
        // TODO(b/179648654): fetch MSBB status directly in native.
        boolean isValid = TriggerContextJni.get().isValid(nativeInstance,
                Starter.getMakeSearchesAndBrowsingBetterSettingEnabled(),
                Starter.getProactiveHelpSettingEnabled(), Starter.getFeatureModuleInstalled());
        TriggerContextJni.get().destroyNative(nativeInstance);

        return isValid;
    }

    /**
     * Creates a corresponding native trigger context. Note: the returned pointer is owning and
     * must be freed with {@code destroyNative}!
     */
    public long toNative() {
        // TODO(b/179648654): preserve type information in native.
        Map<String, String> stringifiedParameters = getParameters();
        return TriggerContextJni.get().createNative(mExperimentIds.toString(),
                stringifiedParameters.keySet().toArray(new String[0]),
                stringifiedParameters.values().toArray(new String[0]), mIsCustomTab,
                mIsDirectAction, mInitialUrl);
    }

    @NativeMethods
    interface Natives {
        long createNative(String experimentIds, String[] parameterKeys, String[] parameterValues,
                boolean isCustomTab, boolean isDirectAction, String initialUrl);
        void destroyNative(long triggerContext);
        boolean isValid(long triggerContext, boolean isMsbbSettingEnabled,
                boolean isProactiveHelpSettingEnabled, boolean isFeatureModuleInstalled);
    }
}
