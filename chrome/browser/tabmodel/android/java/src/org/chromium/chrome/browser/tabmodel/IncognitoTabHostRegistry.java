// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabmodel;

import java.util.ArrayList;
import java.util.List;

/**
 * Registry for implementations of {@link IncognitoTabHost}.
 * Every host of incognito tabs must be registered here as long as it is alive, so that its
 * incognito tabs are included in such operations as removing all incognito tabs.
 */
public class IncognitoTabHostRegistry {
    private static IncognitoTabHostRegistry sInstance;

    public static IncognitoTabHostRegistry getInstance() {
        if (sInstance == null) {
            sInstance = new IncognitoTabHostRegistry();
        }
        return sInstance;
    }

    private final List<IncognitoTabHost> mHosts = new ArrayList<>();

    /** Register an IncognitoTabHost */
    public void register(IncognitoTabHost host) {
        mHosts.add(host);
    }

    /** Unregister an IncognitoTabHost */
    public void unregister(IncognitoTabHost host) {
        mHosts.remove(host);
    }

    public List<IncognitoTabHost> getHosts() {
        return mHosts;
    }
}
