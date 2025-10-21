// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabmodel;

import org.chromium.base.TraceEvent;
import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.url_constants.UrlConstantResolver;
import org.chromium.chrome.browser.url_constants.UrlConstantResolverFactory;

/** Util class to hold shared logic for calling {@link TabCreator}s. */
@NullMarked
public class TabCreatorUtil {
    /** Prevent instantiation. */
    private TabCreatorUtil() {}

    /**
     * @param tabCreator The tab creator to create the {@link Tab} with.
     * @param profile The profile to scope dependencies to.
     * @param type Where the tab launch is coming from.
     */
    public static void launchNtp(TabCreator tabCreator, Profile profile, @TabLaunchType int type) {
        try {
            TraceEvent.begin("TabCreator.launchNtp");
            UrlConstantResolver urlConstantResolver =
                    UrlConstantResolverFactory.getForProfile(profile);
            tabCreator.launchUrl(urlConstantResolver.getNtpUrl(), type);
        } finally {
            TraceEvent.end("TabCreator.launchNtp");
        }
    }
}
