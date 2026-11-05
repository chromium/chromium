// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview;

import android.net.Uri;

import org.chromium.android_webview.common.Lifetime;
import org.chromium.android_webview.common.MediaIntegrityApiStatus;
import org.chromium.components.origin_matcher.OriginMatcher;

import java.util.ArrayList;
import java.util.Collections;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

/**
 * Stores configuration for the WebView Media Integrity API. Configuration is used to set permission
 * levels for origin sites through defaults and override rules. Origin site URIs are matched against
 * these override rules with an {@link OriginMatcher}.
 */
@Lifetime.WebView
public class AwMediaIntegrityApiStatusConfig {

    // A URI may match multiple origin patterns but we must return the least permissive
    // option. Hence we look for matches in the following order.
    private static final int[] sStatusByPriority = {
        MediaIntegrityApiStatus.DISABLED,
        MediaIntegrityApiStatus.ENABLED_WITHOUT_APP_IDENTITY,
        MediaIntegrityApiStatus.ENABLED
    };
    private Map<@MediaIntegrityApiStatus Integer, List<String>> mPermissionToRules;

    private @MediaIntegrityApiStatus int mDefaultStatus;
    private Map<String, @MediaIntegrityApiStatus Integer> mOverrideRulesToPermission;

    public AwMediaIntegrityApiStatusConfig() {
        mDefaultStatus = MediaIntegrityApiStatus.ENABLED;
        mOverrideRulesToPermission = Collections.emptyMap();
        mPermissionToRules = Collections.emptyMap();
    }

    public void setApiAvailabilityRules(
            @MediaIntegrityApiStatus int defaultStatus,
            Map<String, @MediaIntegrityApiStatus Integer> permissionConfig) {
        mDefaultStatus = defaultStatus;
        OriginMatcher validationMatcher = new OriginMatcher();
        List<String> badRules;
        try {
            badRules = validationMatcher.setRuleList(new ArrayList<>(permissionConfig.keySet()));
        } finally {
            validationMatcher.destroy();
        }
        if (badRules.size() > 0) {
            throw new IllegalArgumentException(
                    "Badly formed rules: " + String.join(", ", badRules));
        }
        mOverrideRulesToPermission = permissionConfig;
        populateRulesForLookup(permissionConfig);
    }

    @MediaIntegrityApiStatus
    public int getDefaultStatus() {
        return mDefaultStatus;
    }

    public Map<String, @MediaIntegrityApiStatus Integer> getOverrideRules() {
        return mOverrideRulesToPermission;
    }

    @MediaIntegrityApiStatus
    public int getStatusForUri(Uri uri) {
        if (mOverrideRulesToPermission.isEmpty()) {
            return mDefaultStatus;
        }
        // AwMediaIntegrityApiStatusConfig has no explicit destroy() lifecycle, so persist
        // rules as strings and scope native OriginMatcher instances to each lookup.
        OriginMatcher matcher = new OriginMatcher();
        try {
            for (@MediaIntegrityApiStatus int status : sStatusByPriority) {
                List<String> rules = mPermissionToRules.get(status);
                if (rules != null && !rules.isEmpty()) {
                    matcher.setRuleList(rules);
                    if (matcher.matchesOrigin(uri)) {
                        return status;
                    }
                }
            }
            // Uri does not match any override rules
            return mDefaultStatus;
        } finally {
            matcher.destroy();
        }
    }

    private void populateRulesForLookup(
            Map<String, @MediaIntegrityApiStatus Integer> permissionConfig) {
        Map<@MediaIntegrityApiStatus Integer, List<String>> newPatterns = new HashMap<>();
        for (@MediaIntegrityApiStatus int status : sStatusByPriority) {
            newPatterns.put(status, new ArrayList<>());
        }
        for (Map.Entry<String, @MediaIntegrityApiStatus Integer> entry :
                permissionConfig.entrySet()) {
            newPatterns.get(entry.getValue()).add(entry.getKey());
        }
        for (@MediaIntegrityApiStatus int status : sStatusByPriority) {
            newPatterns.put(status, Collections.unmodifiableList(newPatterns.get(status)));
        }
        mPermissionToRules = Collections.unmodifiableMap(newPatterns);
    }
}
