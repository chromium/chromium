// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs;

import org.chromium.base.UserData;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.tab.Tab;

import java.util.List;

/**
 * The lifetime of a tab is complicated and not always associated with an Activity. Offline page
 * info could be required at any time, so we store it with the tab itself.
 */
@NullMarked
public class TwaOfflineDataProvider implements UserData {
    private static final Class<TwaOfflineDataProvider> USER_DATA_KEY = TwaOfflineDataProvider.class;

    private final String mInitialUrlToLoad;
    private final @Nullable List<String> mAdditionalTwaOrigins;
    private final String mClientPackageName;

    public static @Nullable TwaOfflineDataProvider from(Tab tab) {
        if (tab == null) return null;
        return tab.getUserDataHost().getUserData(USER_DATA_KEY);
    }

    public static TwaOfflineDataProvider createFor(
            Tab tab,
            String initialUrlToLoad,
            @Nullable List<String> additionalTwaOrigins,
            String clientPackageName) {
        return tab.getUserDataHost()
                .setUserData(
                        USER_DATA_KEY,
                        new TwaOfflineDataProvider(
                                initialUrlToLoad, additionalTwaOrigins, clientPackageName));
    }

    private TwaOfflineDataProvider(
            String initialUrlToLoad,
            @Nullable List<String> additionalTwaOrigins,
            String clientPackageName) {
        mInitialUrlToLoad = initialUrlToLoad;
        mAdditionalTwaOrigins = additionalTwaOrigins;
        mClientPackageName = clientPackageName;
    }

    public String getInitialUrlToLoad() {
        return mInitialUrlToLoad;
    }

    public @Nullable List<String> getAdditionalTwaOrigins() {
        return mAdditionalTwaOrigins;
    }

    public String getClientPackageName() {
        return mClientPackageName;
    }
}
