// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview;

import android.net.Uri;

import androidx.annotation.IntDef;

import java.lang.annotation.ElementType;
import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.lang.annotation.Target;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collections;
import java.util.HashMap;
import java.util.List;
import java.util.Map;
import org.chromium.android_webview.common.Lifetime;

/**
 * Stores configuration for the WebView Media Integrity API. Configuration is used to set permission
 * levels for origin sites through defaults and override rules. Origin site URIs are matched against
 * these override rules with an {@link AwContentsOriginMatcher}.
 */
@Lifetime.WebView
public class AwMediaIntegrityApiStatusConfig {
    @Target(ElementType.TYPE_USE)
    @Retention(RetentionPolicy.SOURCE)
    @IntDef({
        ApiStatus.DISABLED,
        ApiStatus.ENABLED_WITHOUT_APP_IDENTITY,
        ApiStatus.ENABLED
    })
    public @interface ApiStatus {
        int DISABLED = 0;
        int ENABLED_WITHOUT_APP_IDENTITY = 1;
        int ENABLED = 2;
    }

    // A URI may match multiple origin patterns but we must return the least permissive
    // option. Hence we look for matches in the following order.
    private static final int[] sStatusByPriority = {
        ApiStatus.DISABLED, ApiStatus.ENABLED_WITHOUT_APP_IDENTITY, ApiStatus.ENABLED
    };
    private final AwContentsOriginMatcher mRuleValidationMatcher = new AwContentsOriginMatcher();
    private final Map<@ApiStatus Integer, AwContentsOriginMatcher> mPermissionToMatcher;

    private @ApiStatus int mDefaultStatus;
    private Map<String, @ApiStatus Integer> mOverrideRulesToPermission;

    public AwMediaIntegrityApiStatusConfig() {
        mDefaultStatus = ApiStatus.ENABLED;
        mOverrideRulesToPermission = Collections.emptyMap();
        Map<@ApiStatus Integer, AwContentsOriginMatcher> matcherMap = new HashMap<>();
        for (@ApiStatus int status : sStatusByPriority) {
            matcherMap.put(status, new AwContentsOriginMatcher());
        }
        mPermissionToMatcher = Collections.unmodifiableMap(matcherMap);
    }

    public void setApiAvailabilityRules(
        @ApiStatus int defaultStatus, Map<String, @ApiStatus Integer> permissionConfig) {
        mDefaultStatus = defaultStatus;
        String[] badRules =
            mRuleValidationMatcher.updateRuleList(new ArrayList<>(permissionConfig.keySet()));
        if (badRules.length > 0) {
            throw new IllegalArgumentException(
                "Badly formed rules: " + Arrays.toString(badRules));
        }
        mOverrideRulesToPermission = permissionConfig;
        populateMatchersForLookup(permissionConfig);
    }

    @ApiStatus
    public int getDefaultStatus() {
        return mDefaultStatus;
    }

    public Map<String, @ApiStatus Integer> getOverrideRules() {
        return mOverrideRulesToPermission;
    }

    @ApiStatus
    public int getStatusForUri(Uri uri) {
        for (@ApiStatus int status : sStatusByPriority) {
            if (mPermissionToMatcher.get(status).matchesOrigin(uri)) {
                return status;
            }
        }
        // Uri does not match any override rules
        return mDefaultStatus;
    }

    private void populateMatchersForLookup(Map<String, @ApiStatus Integer> permissionConfig) {
        Map<Integer, List<String>> newPatterns = new HashMap<>();
        for (int status : sStatusByPriority) {
            newPatterns.put(status, new ArrayList());
        }
        for (Map.Entry<String, @ApiStatus Integer> entry : permissionConfig.entrySet()) {
            newPatterns.get(entry.getValue()).add(entry.getKey());
        }
        for (int status : sStatusByPriority) {
            mPermissionToMatcher.get(status).updateRuleList(newPatterns.get(status));
        }
    }
}
