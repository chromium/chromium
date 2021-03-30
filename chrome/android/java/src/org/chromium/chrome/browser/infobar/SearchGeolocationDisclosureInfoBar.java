// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.infobar;

import android.content.Context;

import org.chromium.base.ContextUtils;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.settings.SettingsLauncherImpl;
import org.chromium.components.browser_ui.settings.SettingsLauncher;
import org.chromium.components.browser_ui.site_settings.SingleWebsiteSettings;
import org.chromium.components.infobars.InfoBar;
import org.chromium.components.infobars.InfoBarLayout;

/**
 * An infobar to disclose to the user that the default search engine has geolocation access by
 * default.
 */
public class SearchGeolocationDisclosureInfoBar extends InfoBar {
    private final int mInlineLinkRangeStart;
    private final int mInlineLinkRangeEnd;

    @CalledByNative
    private static InfoBar show(
            int iconId, String messageText, int inlineLinkRangeStart, int inlineLinkRangeEnd) {
        return new SearchGeolocationDisclosureInfoBar(
                iconId, messageText, inlineLinkRangeStart, inlineLinkRangeEnd);
    }

    /**
     * Creates the infobar.
     * @param iconDrawableId       Drawable ID corresponding to the icon that the infobar will show.
     * @param messageText          Message to display in the infobar.
     * @param inlineLinkRangeStart Beginning of the link in the message.
     * @param inlineLinkRangeEnd   End of the link in the message.
     */
    private SearchGeolocationDisclosureInfoBar(int iconDrawableId, String messageText,
            int inlineLinkRangeStart, int inlineLinkRangeEnd) {
        super(iconDrawableId, R.color.infobar_icon_drawable_color, messageText, null);
        mInlineLinkRangeStart = inlineLinkRangeStart;
        mInlineLinkRangeEnd = inlineLinkRangeEnd;
    }

    @Override
    public void createContent(InfoBarLayout layout) {
        super.createContent(layout);
        layout.setInlineMessageLink(mInlineLinkRangeStart, mInlineLinkRangeEnd);
    }

    @Override
    public int getPriority() {
        return InfoBarPriority.CRITICAL;
    }

    @CalledByNative
    private static void showSettingsPage(String searchUrl) {
        Context context = ContextUtils.getApplicationContext();
        SettingsLauncher settingsLauncher = new SettingsLauncherImpl();
        settingsLauncher.launchSettingsActivity(context, SingleWebsiteSettings.class,
                SingleWebsiteSettings.createFragmentArgsForSite(searchUrl));
    }
}
