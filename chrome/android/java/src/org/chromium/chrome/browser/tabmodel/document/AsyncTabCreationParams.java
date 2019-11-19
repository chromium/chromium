// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabmodel.document;

import android.content.ComponentName;
import android.content.Intent;

import org.chromium.chrome.browser.ServiceTabLauncher;
import org.chromium.chrome.browser.document.ChromeLauncherActivity;
import org.chromium.chrome.browser.offlinepages.downloads.OfflinePageDownloadBridge;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.AsyncTabParams;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.browser.WebContents;

/**
 * Data that will be used later when a Tab is opened via an intent. Often only the necessary
 * subset of the data will be set. All data is removed once the Tab finishes initializing.
 */
public class AsyncTabCreationParams implements AsyncTabParams {
    /** Parameters used for opening a URL in the new Tab. */
    private final LoadUrlParams mLoadUrlParams;

    /** The original intent. Set only by the {@link ChromeLauncherActivity}. */
    private final Intent mOriginalIntent;

    /** WebContents object to initialize the Tab with. Set only by the {@link TabDelegate}. */
    private final WebContents mWebContents;

    /** The tab launch request ID from the {@link ServiceTabLauncher}. **/
    private final Integer mRequestId;

    /** Specifies which component to fire the Intent at. */
    private final ComponentName mComponentName;

    /** Create parameters for creating a Tab asynchronously. */
    public AsyncTabCreationParams(LoadUrlParams loadUrlParams) {
        this(loadUrlParams, null, null, null, null);
    }

    /** Called by {@link TabDelegate} for creating new a Tab with a pre-existing WebContents. */
    public AsyncTabCreationParams(LoadUrlParams loadUrlParams, WebContents webContents) {
        this(loadUrlParams, null, webContents, null, null);
        assert webContents != null;
    }

    /** Called by {@link ServiceTabLauncher} to create tabs via service workers. */
    public AsyncTabCreationParams(LoadUrlParams loadUrlParams, Integer requestId) {
        this(loadUrlParams, null, null, requestId, null);
        assert requestId != null;
    }

    /** Called by {@link OfflinePageDownloadBridge} to create tabs for Offline Pages. */
    public AsyncTabCreationParams(LoadUrlParams loadUrlParams, ComponentName name) {
        this(loadUrlParams, null, null, null, name);
        assert name != null;
    }

    @Override
    public LoadUrlParams getLoadUrlParams() {
        return mLoadUrlParams;
    }

    @Override
    public Intent getOriginalIntent() {
        return mOriginalIntent;
    }

    @Override
    public Integer getRequestId() {
        return mRequestId;
    }

    @Override
    public WebContents getWebContents() {
        return mWebContents;
    }

    @Override
    public ComponentName getComponentName() {
        return mComponentName;
    }

    private AsyncTabCreationParams(LoadUrlParams loadUrlParams, Intent originalIntent,
            WebContents webContents, Integer requestId, ComponentName componentName) {
        assert loadUrlParams != null;

        // These parameters are set in very, very specific and exclusive circumstances.
        if (originalIntent != null) {
            assert webContents == null && requestId == null && componentName == null;
        }
        if (webContents != null) {
            assert originalIntent == null && requestId == null && componentName == null;
        }
        if (requestId != null) {
            assert originalIntent == null && webContents == null && componentName == null;
        }
        if (componentName != null) {
            assert originalIntent == null && webContents == null && requestId == null;
        }

        mLoadUrlParams = loadUrlParams;
        mRequestId = requestId;
        mWebContents = webContents;
        mOriginalIntent = originalIntent;
        mComponentName = componentName;
    }

    @Override
    public Tab getTabToReparent() {
        return null;
    }

    @Override
    public void destroy() {
        if (mWebContents != null) mWebContents.destroy();
    }
}
