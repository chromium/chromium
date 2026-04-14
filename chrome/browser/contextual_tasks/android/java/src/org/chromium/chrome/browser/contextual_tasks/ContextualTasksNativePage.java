// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.contextual_tasks;

import android.app.Activity;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.content.WebContentsFactory;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab_bottom_sheet.CoBrowseViews;
import org.chromium.chrome.browser.ui.native_page.BasicNativePage;
import org.chromium.chrome.browser.ui.native_page.NativePageHost;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.components.embedder_support.util.UrlUtilities;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.WebContentsObserver;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.url.GURL;

/**
 * Native page for contextual tasks. This will wrap around the CoBrowseViews which basically contain
 * 1. WebUI page (chrome://contextual-tasks?chrome_task_id=<TASK_ID>) for showing the actual
 * content. 2. Fusebox at the bottom of the page.
 */
@NullMarked
public class ContextualTasksNativePage extends BasicNativePage {
    private @Nullable CoBrowseViews mCoBrowseViews;
    private @Nullable WebContents mWebContents;
    private @Nullable WebContentsObserver mWebContentsObserver;

    /**
     * Constructs a new ContextualTasksNativePage.
     *
     * @param activity The activity hosting the page.
     * @param host The NativePageHost to use.
     * @param windowAndroid The WindowAndroid to use.
     * @param profile The profile to use.
     */
    public ContextualTasksNativePage(
            Activity activity, NativePageHost host, WindowAndroid windowAndroid, Profile profile) {
        super(host);

        // 1. Create WebContents.
        mWebContents =
                WebContentsFactory.createWebContents(
                        profile, /* initiallyHidden= */ false, /* initializeRenderer= */ false);

        // TODO(crbug.com/501840805): Delete this whole class as it's not necessary any more.
    }

    @Override
    public String getTitle() {
        assert mWebContents != null;
        return mWebContents.getTitle();
    }

    @Override
    public String getHost() {
        return UrlConstants.CONTEXTUAL_TASKS_HOST;
    }

    @Override
    public void updateForUrl(@Nullable String url) {
        if (url == null) return;
        super.updateForUrl(url);
        GURL gurl = new GURL(url);

        String taskId =
                UrlUtilities.getValueForKeyInQuery(
                        gurl, ContextualTasksUtils.URL_QUERY_PARAM_TASK_ID);
        if (taskId == null) return;

        assert mWebContents != null;

        mWebContents
                .getNavigationController()
                .loadUrl(new LoadUrlParams(ContextualTasksUtils.createWebUiUrl(taskId)));
    }

    @Override
    public void destroy() {
        if (mWebContentsObserver != null) {
            mWebContentsObserver.observe(null);
            mWebContentsObserver = null;
        }
        if (mCoBrowseViews != null) {
            // TODO(crbug.com/498290223): Proper cleanup of CoBrowseViews should be handled via
            // native.
            mCoBrowseViews = null;
        }
        if (mWebContents != null) {
            mWebContents.destroy();
            mWebContents = null;
        }
        super.destroy();
    }
}
