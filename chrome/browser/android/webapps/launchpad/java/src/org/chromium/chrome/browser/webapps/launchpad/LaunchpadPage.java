// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webapps.launchpad;

import android.annotation.SuppressLint;
import android.app.Activity;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.browser.ui.native_page.BasicNativePage;
import org.chromium.chrome.browser.ui.native_page.NativePageHost;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.ui.modaldialog.ModalDialogManager;

import java.util.List;

/**
 * Native page for launching WebApks.
 */
public class LaunchpadPage extends BasicNativePage {
    @SuppressLint("StaticFieldLeak") // Test only.
    private static LaunchpadCoordinator sLaunchpadCoordinatorForTesting;

    private LaunchpadCoordinator mLaunchpadCoordinator;
    private String mTitle;

    /**
     * Create a new instance of the app launcher page.
     * @param activity The activity to get context and launch apps.
     * @param host A NativePageHost to load URLs.
     * @param items The list of LaunchpadItems to be displayed.
     */
    public LaunchpadPage(Activity activity, NativePageHost host,
            Supplier<ModalDialogManager> modalDialogManagerSupplier, List<LaunchpadItem> items) {
        super(host);

        mTitle = host.getContext().getResources().getString(R.string.launchpad_title);
        mLaunchpadCoordinator = sLaunchpadCoordinatorForTesting != null
                ? sLaunchpadCoordinatorForTesting
                : new LaunchpadCoordinator(activity, modalDialogManagerSupplier, items);

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
        mLaunchpadCoordinator = null;
        super.destroy();
    }

    @VisibleForTesting
    static void setCoordinatorForTesting(LaunchpadCoordinator coordinator) {
        sLaunchpadCoordinatorForTesting = coordinator;
    }
}
