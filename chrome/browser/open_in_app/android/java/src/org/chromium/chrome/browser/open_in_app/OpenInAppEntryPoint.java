// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.open_in_app;

import android.content.Context;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.content.pm.ResolveInfo;
import android.graphics.drawable.Drawable;
import android.net.Uri;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.PackageManagerUtils;
import org.chromium.base.supplier.NullableObservableSupplier;
import org.chromium.base.task.AsyncTask;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabSupplierObserver;
import org.chromium.chrome.browser.tabmodel.TabClosureParams;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorSupplier;
import org.chromium.components.embedder_support.util.UrlUtilities;
import org.chromium.components.external_intents.ExternalNavigationHandler;
import org.chromium.content_public.browser.NavigationHandle;
import org.chromium.content_public.browser.WebContentsObserver;
import org.chromium.url.GURL;

import java.util.ArrayList;
import java.util.List;

/** Abstract base entry point class for Open in App. */
@NullMarked
public abstract class OpenInAppEntryPoint implements OpenInAppMenuItemProvider {
    private final TabSupplierObserver mTabSupplierObserver;
    private @Nullable Tab mCurrentTab;
    private @Nullable OpenInAppDelegate mOpenInAppDelegate;

    sealed interface ResolveResult
            permits ResolveResult.Info, ResolveResult.ResolverActivity, ResolveResult.None {
        final class Info implements ResolveResult {
            public final ResolveInfo resolveInfo;

            Info(ResolveInfo resolveInfo) {
                this.resolveInfo = resolveInfo;
            }
        }

        final class ResolverActivity implements ResolveResult {}

        final class None implements ResolveResult {}
    }

    private final WebContentsObserver mWebContentsObserver =
            new WebContentsObserver() {
                @Override
                public void didFinishNavigationInPrimaryMainFrame(
                        NavigationHandle navigationHandle) {
                    if (navigationHandle.hasCommitted()
                            && UrlUtilities.isHttpOrHttps(navigationHandle.getUrl())) {
                        GURL url = navigationHandle.getUrl();
                        var delegate = mOpenInAppDelegate;
                        if (delegate == null) return;

                        delegate.setLastNavigatedUrl(url);
                        Intent targetIntent = new Intent(Intent.ACTION_VIEW);
                        targetIntent.setData(Uri.parse(url.getSpec()));

                        // New navigation committed, so we should clear the open in app info to
                        // prevent trying to open an app based on outdated info.
                        updateOpenInAppInfo(delegate, null);

                        new AsyncTask<ResolveResult>() {
                            @Override
                            protected ResolveResult doInBackground() {
                                ResolveInfo resolveActivity =
                                        PackageManagerUtils.resolveActivity(
                                                targetIntent, PackageManager.MATCH_DEFAULT_ONLY);

                                if (resolveActivity == null) return new ResolveResult.None();

                                List<ResolveInfo> resolveInfos =
                                        PackageManagerUtils.queryIntentActivities(
                                                targetIntent,
                                                PackageManager.GET_RESOLVED_FILTER
                                                        | PackageManager.MATCH_DEFAULT_ONLY);

                                var browserPackages =
                                        ExternalNavigationHandler.getInstalledBrowserPackages();

                                if (ExternalNavigationHandler.resolvesToChooser(
                                        resolveActivity, resolveInfos)) {
                                    ArrayList<ResolveInfo> nonBrowserApps = new ArrayList<>();
                                    for (var info : resolveInfos) {
                                        if (!browserPackages.contains(
                                                info.activityInfo.packageName)) {
                                            nonBrowserApps.add(info);
                                        }
                                    }
                                    if (nonBrowserApps.isEmpty()) {
                                        return new ResolveResult.None();
                                    }
                                    if (nonBrowserApps.size() > 1) {
                                        return new ResolveResult.ResolverActivity();
                                    }
                                    return new ResolveResult.Info(nonBrowserApps.get(0));
                                }

                                if (browserPackages.contains(
                                        resolveActivity.activityInfo.packageName)) {
                                    return new ResolveResult.None();
                                }

                                return new ResolveResult.Info(resolveActivity);
                            }

                            @Override
                            protected void onPostExecute(ResolveResult result) {
                                onResolveInfosFetched(
                                        delegate,
                                        result,
                                        targetIntent,
                                        url,
                                        navigationHandle.getNavigationId());
                            }
                        }.executeWithTaskTraits(TaskTraits.UI_DEFAULT);
                    }
                }
            };

    protected OpenInAppDelegate.@Nullable OpenInAppInfo mOpenInAppInfo;
    protected final Context mContext;

    /**
     * Constructor for this class.
     *
     * @param tabSupplier A supplier that notifies of tab changes.
     * @param context The {@link Context} to get resources from.
     */
    public OpenInAppEntryPoint(NullableObservableSupplier<Tab> tabSupplier, Context context) {
        mTabSupplierObserver =
                new TabSupplierObserver(tabSupplier, /* shouldTrigger= */ false) {
                    @Override
                    protected void onObservingDifferentTab(@Nullable Tab tab) {
                        mCurrentTab = tab;

                        if (mCurrentTab == null) {
                            mOpenInAppDelegate = null;
                            mWebContentsObserver.observe(null);
                            updateOpenInAppInfo(null, null);
                            return;
                        }

                        mOpenInAppDelegate = OpenInAppDelegate.from(mCurrentTab);
                        updateOpenInAppInfo(
                                mOpenInAppDelegate, mOpenInAppDelegate.getCurrentOpenInAppInfo());

                        var webContents = mCurrentTab.getWebContents();
                        mWebContentsObserver.observe(webContents);
                    }
                };
        mContext = context;
    }

    public void destroy() {
        mTabSupplierObserver.destroy();
        mWebContentsObserver.observe(null);
        mOpenInAppDelegate = null;
        mOpenInAppInfo = null;
    }

    @VisibleForTesting
    void onResolveInfosFetched(
            OpenInAppDelegate delegate,
            ResolveResult result,
            Intent targetIntent,
            GURL url,
            long navigationId) {
        GURL lastUrl = delegate.getLastNavigatedUrl();
        if (lastUrl == null || !lastUrl.equals(url)) return;

        if (result instanceof ResolveResult.None) {
            updateOpenInAppInfo(delegate, null);
            return;
        }

        CharSequence name = null;
        Drawable icon = null;
        // Having actual resolve info means the intent is going to be handled by an app and not
        // ResolverActivity.
        if (result instanceof ResolveResult.Info info) {
            var resolveInfo = info.resolveInfo;
            var pm = mContext.getPackageManager();
            String packageName = resolveInfo.activityInfo.packageName;

            var iconAndLabel =
                    ExternalNavigationHandler.getApplicationIconAndLabel(pm, packageName);
            if (iconAndLabel == null) return;

            name = iconAndLabel.second;
            icon = iconAndLabel.first;

            // We're setting the package explicitly to make sure the app that this intent launches
            // matches what's expected based on the other data in resolveActivity.
            targetIntent.setPackage(packageName);
        }

        // The app should always launch in a new task.
        targetIntent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        // Prevent the OS from showing a dialog prompting the user to open the app, or if it's
        // showing a chooser with multiple apps, they're not browsers.
        targetIntent.addFlags(Intent.FLAG_ACTIVITY_REQUIRE_NON_BROWSER);

        Runnable openInApp =
                () -> {
                    var helper = delegate.getExternalNavigationHelper();
                    if (helper != null) {
                        var openInAppInfo = delegate.getCurrentOpenInAppInfo();
                        if (openInAppInfo != null) {
                            Tab tab = delegate.getTab();
                            Runnable closeTab =
                                    () -> {
                                        var tabModelSelector =
                                                TabModelSelectorSupplier.getValueOrNullFrom(
                                                        tab.getWindowAndroid());
                                        if (tabModelSelector == null) return;
                                        tabModelSelector.tryCloseTab(
                                                TabClosureParams.closeTab(tab)
                                                        .allowUndo(false)
                                                        .build(),
                                                /* allowDialog= */ false);
                                    };
                            // This is posted to prevent a crash as a result of the WebContents
                            // being destroyed before the intent can be reported to safe browsing.
                            Runnable postClose =
                                    () -> PostTask.postTask(TaskTraits.UI_DEFAULT, closeTab);

                            if (tab.isOffTheRecord()) {
                                helper.launchExternalAppWithIncognitoConfirmation(
                                        targetIntent, navigationId, mContext, postClose);
                            } else {
                                helper.launchExternalApp(targetIntent, mContext);
                                postClose.run();
                            }
                        }
                    }
                };

        updateOpenInAppInfo(delegate, new OpenInAppDelegate.OpenInAppInfo(openInApp, name, icon));
    }

    private void updateOpenInAppInfo(
            @Nullable OpenInAppDelegate delegate,
            OpenInAppDelegate.@Nullable OpenInAppInfo openInAppInfo) {
        if (delegate != null) {
            delegate.updateOpenInAppInfo(openInAppInfo);
        }

        if (delegate == mOpenInAppDelegate) {
            mOpenInAppInfo = openInAppInfo;
            onOpenInAppInfoChanged(mOpenInAppInfo);
        }
    }

    /**
     * Subclasses should override this method to be notified when the {@link
     * OpenInAppDelegate.OpenInAppInfo} changes.
     */
    protected void onOpenInAppInfoChanged(
            OpenInAppDelegate.@Nullable OpenInAppInfo openInAppInfo) {}

    @Override
    public OpenInAppDelegate.@Nullable OpenInAppInfo getOpenInAppInfoForMenuItem() {
        return mOpenInAppInfo;
    }
}
