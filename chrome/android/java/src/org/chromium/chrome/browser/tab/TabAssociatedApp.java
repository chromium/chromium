// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab;

import android.text.TextUtils;

import org.chromium.base.ContextUtils;
import org.chromium.chrome.browser.tabmodel.TabLaunchType;
import org.chromium.content_public.browser.ImeAdapter;
import org.chromium.content_public.browser.ImeEventObserver;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.PageTransition;

/**
 * User data for a {@link Tab} managing an ID of an external application that opened it.
 */
public final class TabAssociatedApp extends TabWebContentsUserData implements ImeEventObserver {
    private static final Class<TabAssociatedApp> USER_DATA_KEY = TabAssociatedApp.class;

    /**
     * The external application that this Tab is associated with (null if not associated with any
     * app). Allows reusing of tabs opened from the same application.
     */
    private String mId;

    public static TabAssociatedApp from(Tab tab) {
        TabAssociatedApp app = get(tab);
        if (app == null) {
            app = tab.getUserDataHost().setUserData(USER_DATA_KEY, new TabAssociatedApp(tab));
        }
        return app;
    }

    private TabAssociatedApp(Tab tab) {
        super(tab);
        tab.addObserver(new EmptyTabObserver() {
            @Override
            public void onInitialized(Tab tab, TabState tabState) {
                if (tabState != null) setAppId(tabState.openerAppId);
            }

            @Override
            public void onLoadUrl(Tab tab, LoadUrlParams params, int loadType) {
                // Clear the app association if the user navigated to a different page from the
                // omnibox.
                if ((params.getTransitionType() & PageTransition.FROM_ADDRESS_BAR)
                        == PageTransition.FROM_ADDRESS_BAR) {
                    mId = null;
                }
            }

            @Override
            public void onDestroyed(Tab tab) {
                tab.removeObserver(this);
            }
        });
    }

    private static TabAssociatedApp get(Tab tab) {
        return tab.getUserDataHost().getUserData(USER_DATA_KEY);
    }

    /**
     * @see #getAppId()
     */
    public static String getAppId(Tab tab) {
        TabAssociatedApp app = get(tab);
        return app != null ? app.getAppId() : null;
    }

    /**
     * @return Whether or not the tab was opened by an app other than Chrome.
     */
    public static boolean isOpenedFromExternalApp(Tab tab) {
        TabAssociatedApp app = get(tab);
        if (app == null) return false;

        String packageName = ContextUtils.getApplicationContext().getPackageName();
        return tab.getLaunchType() == TabLaunchType.FROM_EXTERNAL_APP
                && !TextUtils.equals(app.getAppId(), packageName);
    }

    @Override
    public void initWebContents(WebContents webContents) {
        ImeAdapter.fromWebContents(webContents).addEventObserver(this);
    }

    @Override
    public void cleanupWebContents(WebContents webContents) {}

    /**
     * Associates this tab with the external app with the specified id. Once a Tab is associated
     * with an app, it is reused when a new page is opened from that app (unless the user typed in
     * the location bar or in the page, in which case the tab is dissociated from any app)
     *
     * @param appId The ID of application associated with the tab.
     */
    public void setAppId(String name) {
        mId = name;
    }

    /**
     * @see #setAppId(String) for more information.
     *
     * @return The id of the application associated with that tab (null if not
     *         associated with an app).
     */
    public String getAppId() {
        return mId;
    }

    // ImeEventObserver

    @Override
    public void onImeEvent() {
        // Some text was set in the page. Don't reuse it if a tab is open from the same
        // external application, we might lose some user data.
        mId = null;
    }
}
