// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab;

import static org.chromium.build.NullUtil.assumeNonNull;

import org.chromium.base.UserData;
import org.chromium.base.UserDataHost;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.components.external_intents.InterceptNavigationDelegateImpl;

/** Class that glues InterceptNavigationDelegateImpl objects to Tabs. */
@NullMarked
public class InterceptNavigationDelegateTabHelper implements UserData {
    private static final Class<InterceptNavigationDelegateTabHelper> USER_DATA_KEY =
            InterceptNavigationDelegateTabHelper.class;

    private InterceptNavigationDelegateImpl mInterceptNavigationDelegate;
    private final InterceptNavigationDelegateClientImpl mInterceptNavigationDelegateClient;

    public static void setDelegateForTesting(Tab tab, InterceptNavigationDelegateImpl delegate) {
        InterceptNavigationDelegateTabHelper helper =
                tab.getUserDataHost().getUserData(USER_DATA_KEY);
        assumeNonNull(helper).mInterceptNavigationDelegate = delegate;
    }

    public static void createForTab(Tab tab) {
        tab.getUserDataHost()
                .setUserData(USER_DATA_KEY, new InterceptNavigationDelegateTabHelper(tab));
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

    /** Retrieve an InterceptNavigationDelegateTabHelper instance for a Tab. */
    public static @Nullable InterceptNavigationDelegateTabHelper getFromTab(Tab tab) {
        UserDataHost host = tab.getUserDataHost();
        if (host == null) {
            return null;
        }
        return host.getUserData(USER_DATA_KEY);
    }

    /**
     * Returns this InterceptNavigationDelegateTabHelper instance implementation of
     * InterceptNavigationDelegate
     */
    public InterceptNavigationDelegateImpl getInterceptNavigationDelegate() {
        return mInterceptNavigationDelegate;
    }
}
