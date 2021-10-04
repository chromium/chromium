// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.enterprise.util;

import org.chromium.base.annotations.NativeMethods;
import org.chromium.chrome.browser.profiles.Profile;

/**
 * Util methods for interacting with managed browser (enterprise) state.
 */
public class ManagedBrowserUtils {
    /**
     * Wrapper around native call to determine if policies have been applied for this browser.
     */
    public static boolean hasBrowserPoliciesApplied(Profile profile) {
        return ManagedBrowserUtilsJni.get().hasBrowserPoliciesApplied(profile);
    }

    public static boolean isAccountManaged(Profile profile) {
        // TODO(https://crbug.com/1121153): return true if the account is managed
        return false;
    }

    public static String getAccountManagerName(Profile profile) {
        // TODO(https://crbug.com/1121153): if known, return the account name
        return "";
    }

    @NativeMethods
    public interface Natives {
        boolean hasBrowserPoliciesApplied(Profile profile);
    }
}
