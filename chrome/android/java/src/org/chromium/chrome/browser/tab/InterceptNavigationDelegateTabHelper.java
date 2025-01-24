// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab;

import org.chromium.base.UserData;
import org.chromium.components.external_intents.InterceptNavigationDelegateImpl;

/** Class that glues InterceptNavigationDelegateImpl objects to Tabs. */
public class InterceptNavigationDelegateTabHelper implements UserData {
    private static final Class<InterceptNavigationDelegateTabHelper> USER_DATA_KEY =
            InterceptNavigationDelegateTabHelper.class;

    private InterceptNavigationDelegateImpl mInterceptNavigationDelegate;
    private InterceptNavigationDelegateClientImpl mInterceptNavigationDelegateClient;

    public static void setDelegateForTesting(Tab tab, InterceptNavigationDelegateImpl delegate) {
        InterceptNavigationDelegateTabHelper helper =
                tab.getUserDataHost().getUserData(USER_DATA_KEY);
        helper.mInterceptNavigationDelegate = delegate;
    }

    public static void createForTab(Tab tab) {
        assert get(tab) == null;
        tab.getUserDataHost()
                .setUserData(USER_DATA_KEY, new InterceptNavigationDelegateTabHelper(tab));
    }

    public static InterceptNavigationDelegateImpl get(Tab tab) {
        InterceptNavigationDelegateTabHelper helper =
                tab.getUserDataHost().getUserData(USER_DATA_KEY);
        if (helper == null) return null;
        return helper.mInterceptNavigationDelegate;
    }

    InterceptNavigationDelegateTabHelper(Tab tab) {
        mInterceptNavigationDelegateClient = new InterceptNavigationDelegateClientImpl(tab);
        mInterceptNavigationDelegate =
                new InterceptNavigationDelegateImpl(mInterceptNavigationDelegateClient);
        mInterceptNavigationDelegateClient.initializeWithDelegate(mInterceptNavigationDelegate);
    }

    @Override
    public void destroy() {
        mInterceptNavigationDelegateClient.destroy();
    }
}
