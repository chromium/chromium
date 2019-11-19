// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.features.start_surface;

import android.content.Context;
import android.net.Uri;
import android.provider.Browser;

import androidx.annotation.Nullable;
import androidx.browser.customtabs.CustomTabsIntent;

import org.chromium.chrome.browser.IntentHandler;
import org.chromium.chrome.browser.native_page.NativePageNavigationDelegate;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.start_surface.R;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.ui.mojom.WindowOpenDisposition;

/** Implementation of the {@link NativePageNavigationDelegate} for the explore surface. */
class ExploreSurfaceNavigationDelegate implements NativePageNavigationDelegate {
    private final Context mContext;

    ExploreSurfaceNavigationDelegate(Context context) {
        mContext = context;
    }

    @Override
    public boolean isOpenInNewWindowEnabled() {
        return false;
    }

    // TODO(crbug.com/982018): Experiment opening feeds in normal Tabs.
    @Override
    @Nullable
    public Tab openUrl(int windowOpenDisposition, LoadUrlParams loadUrlParams) {
        CustomTabsIntent.Builder builder = new CustomTabsIntent.Builder();
        builder.setShowTitle(true);
        builder.setStartAnimations(mContext, R.anim.abc_grow_fade_in_from_bottom, 0);
        builder.setExitAnimations(mContext, 0, R.anim.abc_shrink_fade_out_from_bottom);
        CustomTabsIntent customTabsIntent = builder.build();
        customTabsIntent.intent.setPackage(mContext.getPackageName());
        customTabsIntent.intent.putExtra(IntentHandler.EXTRA_OPEN_NEW_INCOGNITO_TAB,
                (windowOpenDisposition == WindowOpenDisposition.OFF_THE_RECORD) ? true : false);
        customTabsIntent.intent.putExtra(Browser.EXTRA_APPLICATION_ID, mContext.getPackageName());
        customTabsIntent.launchUrl(mContext, Uri.parse(loadUrlParams.getUrl()));

        // TODO(crbug.com/982018): Return the opened tab and make sure it is opened in incoginito
        // mode accordingly (note that payment window supports incognito mode).
        return null;
    }
}
