// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.policy;

import android.content.Context;
import android.os.Bundle;

import androidx.annotation.VisibleForTesting;

import org.chromium.components.policy.AppRestrictionsProvider;

/**
 * Does the plumbing between the policies collected via Android's App Restriction system and the
 * native policy system, filtering out the ones not explicitly targeted at WebView.
 */
@VisibleForTesting
public class AwPolicyProvider extends AppRestrictionsProvider {
    /** Policies targeted to WebView should be prefixed by this string.*/
    public static final String POLICY_PREFIX = "com.android.browser:";

    public AwPolicyProvider(Context context) {
        super(context);
    }

    @Override
    public void notifySettingsAvailable(Bundle newAppRestrictions) {
        Bundle filteredRestrictions = null;
        if (newAppRestrictions != null) {
            filteredRestrictions = new Bundle();

            for (String key : newAppRestrictions.keySet()) {
                if (!key.startsWith(POLICY_PREFIX)) continue;

                filteredRestrictions.putSerializable(
                        key.substring(POLICY_PREFIX.length()),
                        newAppRestrictions.getSerializable(key));
            }
        }
        super.notifySettingsAvailable(filteredRestrictions);
    }
}
