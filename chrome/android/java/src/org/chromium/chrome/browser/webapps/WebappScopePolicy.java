// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.chrome.browser.webapps;

import androidx.annotation.IntDef;

import org.chromium.chrome.browser.util.UrlUtilities;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * Defines which URLs are inside a web app scope as well as what to do when user navigates to them.
 */
public class WebappScopePolicy {
    @IntDef({Type.LEGACY, Type.STRICT})
    @Retention(RetentionPolicy.SOURCE)
    public @interface Type {
        // Values should be numerated from 0 and can't have gaps.
        int LEGACY = 0;
        int STRICT = 1;
        int NUM_ENTRIES = 2;
    }

    @IntDef({NavigationDirective.NORMAL_BEHAVIOR,
            NavigationDirective.IGNORE_EXTERNAL_INTENT_REQUESTS})
    @Retention(RetentionPolicy.SOURCE)
    public @interface NavigationDirective {
        // No special handling.
        int NORMAL_BEHAVIOR = 0;
        // The navigation should stay in the webapp. External intent handlers should be ignored.
        int IGNORE_EXTERNAL_INTENT_REQUESTS = 1;
    }

    /**
     * @return {@code true} if given {@code url} is in scope of a web app as defined by its
     *         {@code WebappInfo}, {@code false} otherwise.
     */
    public static boolean isUrlInScope(@Type int type, WebappInfo info, String url) {
        switch (type) {
            case Type.LEGACY:
                return UrlUtilities.sameDomainOrHost(info.url(), url, true);
            case Type.STRICT:
                return UrlUtilities.isUrlWithinScope(url, info.scopeUrl());
            default:
                assert false;
                return false;
        }
    }

    /** Applies the scope policy for navigation to {@link url}. */
    public static @NavigationDirective int applyPolicyForNavigationToUrl(
            @Type int type, WebappInfo info, String url) {
        return isUrlInScope(type, info, url) ? NavigationDirective.IGNORE_EXTERNAL_INTENT_REQUESTS
                                             : NavigationDirective.NORMAL_BEHAVIOR;
    }
}
