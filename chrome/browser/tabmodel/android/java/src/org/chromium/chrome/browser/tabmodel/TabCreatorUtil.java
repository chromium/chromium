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

/** Util class to hold shared logic for calling and implementing {@link TabCreator}s. */
@NullMarked
public class TabCreatorUtil {
    /** Prevent instantiation. */
    private TabCreatorUtil() {}

    /**
     * Convenience function for {@link TabCreator} callers to launch the NTP without explicitly
     * specifying that this is from Chrome UI. Single unified place to share the {@link
     * TabLaunchType#FROM_CHROME_UI} instead of duplicating at each call site..
     *
     * @param tabCreator The tab creator to launch the {@link Tab} with.
     */
    public static void launchNtp(TabCreator tabCreator) {
        tabCreator.launchNtp(TabLaunchType.FROM_CHROME_UI);
    }

    /**
     * Convenience function for {@link TabCreator} implementations that actually open tabs to call
     * back into themselves through {@link TabCreator#launchUrl(String, int)}. Single unified place
     * to share this code instead of duplicating it in each implementation.
     *
     * @param tabCreator The tab creator to launch the {@link Tab} with.
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
