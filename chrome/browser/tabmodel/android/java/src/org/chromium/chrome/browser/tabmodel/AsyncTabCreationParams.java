// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabmodel;

import android.content.ComponentName;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.browser.WebContents;

/**
 * Data that will be used later when a Tab is opened via an intent. Often only the necessary subset
 * of the data will be set. All data is removed once the Tab finishes initializing.
 */
@NullMarked
public class AsyncTabCreationParams implements AsyncTabParams {
    /** Parameters used for opening a URL in the new Tab. */
    private final LoadUrlParams mLoadUrlParams;

    /** WebContents object to initialize the Tab with. Set only by the TabDelegate. */
    private final @Nullable WebContents mWebContents;

    /** The tab launch request ID from the ServiceTabLauncher. */
    private final @Nullable Integer mRequestId;

    /** Specifies which component to fire the Intent at. */
    private final @Nullable ComponentName mComponentName;

    /** Create parameters for creating a Tab asynchronously. */
    public AsyncTabCreationParams(LoadUrlParams loadUrlParams) {
        this(loadUrlParams, null, null, null);
    }

    /** Called by TabDelegate for creating new a Tab with a pre-existing WebContents. */
    public AsyncTabCreationParams(LoadUrlParams loadUrlParams, WebContents webContents) {
        this(loadUrlParams, webContents, null, null);
        assert webContents != null;
    }

    /** Called by ServiceTabLauncher to create tabs via service workers. */
    public AsyncTabCreationParams(LoadUrlParams loadUrlParams, Integer requestId) {
        this(loadUrlParams, null, requestId, null);
        assert requestId != null;
    }

    /** Called by OfflinePageDownloadBridge to create tabs for Offline Pages. */
    public AsyncTabCreationParams(LoadUrlParams loadUrlParams, ComponentName name) {
        this(loadUrlParams, null, null, name);
        assert name != null;
    }

    @Override
    public LoadUrlParams getLoadUrlParams() {
        return mLoadUrlParams;
    }

    @Override
    public @Nullable Integer getRequestId() {
        return mRequestId;
    }

    @Override
    public @Nullable WebContents getWebContents() {
        return mWebContents;
    }

    @Override
    public @Nullable ComponentName getComponentName() {
        return mComponentName;
    }

    private AsyncTabCreationParams(
            LoadUrlParams loadUrlParams,
            @Nullable WebContents webContents,
            @Nullable Integer requestId,
            @Nullable ComponentName componentName) {
        assert loadUrlParams != null;

        // These parameters are set in very, very specific and exclusive circumstances.
        if (webContents != null) {
            assert requestId == null && componentName == null;
        }
        if (requestId != null) {
            assert webContents == null && componentName == null;
        }
        if (componentName != null) {
            assert webContents == null && requestId == null;
        }

        mLoadUrlParams = loadUrlParams;
        mRequestId = requestId;
        mWebContents = webContents;
        mComponentName = componentName;
    }

    @Override
    public @Nullable Tab getTabToReparent() {
        return null;
    }

    @Override
    public void destroy() {
        if (mWebContents != null) mWebContents.destroy();
    }
}
