// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.hub;

import android.content.Context;

/** Factory for creating {@link HubManager}. */
public class HubManagerFactory {
    /**
     * Creates a new instance of {@link HubManagerImpl}.
     *
     * @param context The {@link Context} hosting the Hub.
     * @param paneListBuilder The {@link PaneListBuilder} which is consumed to build a {@link
     *     PaneManager}
     * @return an instance of {@link HubManagerImpl}.
     */
    public static HubManager createHubManager(Context context, PaneListBuilder paneListBuilder) {
        return new HubManagerImpl(context, paneListBuilder);
    }
}
