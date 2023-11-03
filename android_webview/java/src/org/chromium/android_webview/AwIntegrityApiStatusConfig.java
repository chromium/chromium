// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview;

import androidx.annotation.IntDef;

import java.lang.annotation.ElementType;
import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.lang.annotation.Target;
import java.util.Collections;
import java.util.Map;
import org.chromium.android_webview.common.Lifetime;

/**
 * Stores configuration for the WebView Media Integrity API. Configuration is used to set permission
 * levels for origin sites through defaults and override rules.
 */
@Lifetime.WebView
public class AwIntegrityApiStatusConfig {
    @Target(ElementType.TYPE_USE)
    @Retention(RetentionPolicy.SOURCE)
    @IntDef({ApiStatus.DISABLED, ApiStatus.ENABLED_WITHOUT_APP_IDENTITY, ApiStatus.ENABLED})
    public @interface ApiStatus {
        int DISABLED = 0;
        int ENABLED_WITHOUT_APP_IDENTITY = 1;
        int ENABLED = 2;
    }

    private @ApiStatus int mDefaultStatus;
    private Map<String, @ApiStatus Integer> mOverrideRulesToPermission;

    public AwIntegrityApiStatusConfig() {
        mDefaultStatus = ApiStatus.ENABLED;
        mOverrideRulesToPermission = Collections.emptyMap();
    }

    public void setApiStatus(
            @ApiStatus int defaultStatus, Map<String, @ApiStatus Integer> permissionConfig) {
        mOverrideRulesToPermission = permissionConfig;
        mDefaultStatus = defaultStatus;
    }

    @ApiStatus
    public int getDefaultStatus() {
        return mDefaultStatus;
    }

    public Map<String, @ApiStatus Integer> getOverrideRules() {
        return mOverrideRulesToPermission;
    }
}
