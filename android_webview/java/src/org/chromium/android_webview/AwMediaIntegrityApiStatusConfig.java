// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview;

import android.net.Uri;

import org.chromium.android_webview.common.Lifetime;
import org.chromium.android_webview.common.MediaIntegrityApiStatus;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collections;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

/**
 * Stores configuration for the WebView Media Integrity API. Configuration is used to set permission
 * levels for origin sites through defaults and override rules. Origin site URIs are matched against
 * these override rules with an {@link AwContentsOriginMatcher}.
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
    private final AwContentsOriginMatcher mRuleValidationMatcher = new AwContentsOriginMatcher();
    private final Map<@MediaIntegrityApiStatus Integer, AwContentsOriginMatcher>
            mPermissionToMatcher;

    private @MediaIntegrityApiStatus int mDefaultStatus;
    private Map<String, @MediaIntegrityApiStatus Integer> mOverrideRulesToPermission;

    public AwMediaIntegrityApiStatusConfig() {
        mDefaultStatus = MediaIntegrityApiStatus.ENABLED;
        mOverrideRulesToPermission = Collections.emptyMap();
        Map<@MediaIntegrityApiStatus Integer, AwContentsOriginMatcher> matcherMap = new HashMap<>();
        for (@MediaIntegrityApiStatus int status : sStatusByPriority) {
            matcherMap.put(status, new AwContentsOriginMatcher());
        }
        mPermissionToMatcher = Collections.unmodifiableMap(matcherMap);
    }

    public void setApiAvailabilityRules(
            @MediaIntegrityApiStatus int defaultStatus,
            Map<String, @MediaIntegrityApiStatus Integer> permissionConfig) {
        mDefaultStatus = defaultStatus;
        String[] badRules =
                mRuleValidationMatcher.updateRuleList(new ArrayList<>(permissionConfig.keySet()));
        if (badRules.length > 0) {
            throw new IllegalArgumentException("Badly formed rules: " + Arrays.toString(badRules));
        }
        mOverrideRulesToPermission = permissionConfig;
        populateMatchersForLookup(permissionConfig);
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
        for (@MediaIntegrityApiStatus int status : sStatusByPriority) {
            if (mPermissionToMatcher.get(status).matchesOrigin(uri)) {
                return status;
            }
        }
        // Uri does not match any override rules
        return mDefaultStatus;
    }

    private void populateMatchersForLookup(
            Map<String, @MediaIntegrityApiStatus Integer> permissionConfig) {
        Map<Integer, List<String>> newPatterns = new HashMap<>();
        for (int status : sStatusByPriority) {
            newPatterns.put(status, new ArrayList());
        }
        for (Map.Entry<String, @MediaIntegrityApiStatus Integer> entry :
                permissionConfig.entrySet()) {
            newPatterns.get(entry.getValue()).add(entry.getKey());
        }
        for (int status : sStatusByPriority) {
            mPermissionToMatcher.get(status).updateRuleList(newPatterns.get(status));
        }
    }
}
