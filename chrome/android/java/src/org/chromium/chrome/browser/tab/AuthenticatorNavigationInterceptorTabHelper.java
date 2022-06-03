// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab;

import org.chromium.components.external_intents.AuthenticatorNavigationInterceptor;

/**
 * Temporary wrapper to isolate downstream from the exact API surfaces used to get an
 * AuthenticatorNavigationInterceptor instance from a Tab while those API surfaces are being
 * refactored.
 * TODO(blundell): Delete this once InterceptNavigationDelegateTabHelper has landed and
 * InterceptNavigationDelegateImpl has been componentized in Chromium.
 */
public class AuthenticatorNavigationInterceptorTabHelper {
    public static AuthenticatorNavigationInterceptor getInterceptorForTab(Tab tab) {
        return InterceptNavigationDelegateTabHelper.get(tab)
                .getAuthenticatorNavigationInterceptor();
    }
}
