// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webapps.launchpad;

import android.app.Activity;

import org.chromium.chrome.browser.ui.native_page.BasicNativePage;
import org.chromium.chrome.browser.ui.native_page.NativePageHost;
import org.chromium.components.embedder_support.util.UrlConstants;

/**
 * Native page for launching WebApks.
 */
public class LaunchpadPage extends BasicNativePage {
    private LaunchpadCoordinator mLaunchpadCoordinator;
    private String mTitle;

    /**
     * Create a new instance of the app launcher page.
     * @param activity The {@link Activity} used to get context and instantiate the
     *                 {@link LaunchpadCoordinator}.
     * @param host A NativePageHost to load URLs.
     */
    public LaunchpadPage(Activity activity, NativePageHost host) {
        super(host);

        mTitle = host.getContext().getResources().getString(R.string.launchpad_title);
        mLaunchpadCoordinator = new LaunchpadCoordinator(activity);

        initWithView(mLaunchpadCoordinator.getView());
    }

    @Override
    public String getTitle() {
        return mTitle;
    }

    @Override
    public String getHost() {
        return UrlConstants.LAUNCHPAD_HOST;
    }

    @Override
    public void destroy() {
        mLaunchpadCoordinator.destroy();
        super.destroy();
    }
}
