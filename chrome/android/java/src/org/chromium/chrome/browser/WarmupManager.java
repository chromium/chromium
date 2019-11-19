// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import android.annotation.SuppressLint;
import android.content.Context;
import android.net.Uri;
import android.os.SystemClock;
import android.view.ContextThemeWrapper;
import android.view.InflateException;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.view.ViewStub;
import android.widget.FrameLayout;

import androidx.annotation.IntDef;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.Log;
import org.chromium.base.StrictModeContext;
import org.chromium.base.ThreadUtils;
import org.chromium.base.TraceEvent;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.base.library_loader.LibraryLoader;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.task.AsyncTask;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.toolbar.ControlContainer;
import org.chromium.chrome.browser.util.UrlConstants;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.WebContentsObserver;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.net.InetAddress;
import java.net.MalformedURLException;
import java.net.URL;
import java.net.UnknownHostException;
import java.util.HashMap;
import java.util.HashSet;
import java.util.Map;
import java.util.Set;

/**
 * This class is a singleton that holds utilities for warming up Chrome and prerendering urls
 * without creating the Activity.
 *
 * This class is not thread-safe and must only be used on the UI thread.
 */
public class WarmupManager {
    private static final String TAG = "WarmupManager";

    @VisibleForTesting
    static final String WEBCONTENTS_STATUS_HISTOGRAM = "CustomTabs.SpareWebContents.Status2";

    public static final boolean FOR_CCT = true;

    // See CustomTabs.SpareWebContentsStatus histogram. Append-only.
    @IntDef({WebContentsStatus.CREATED, WebContentsStatus.USED, WebContentsStatus.KILLED,
            WebContentsStatus.DESTROYED, WebContentsStatus.STOLEN})
    @Retention(RetentionPolicy.SOURCE)
    @interface WebContentsStatus {
        @VisibleForTesting
        int CREATED = 0;
        @VisibleForTesting
        int USED = 1;
        @VisibleForTesting
        int KILLED = 2;
        @VisibleForTesting
        int DESTROYED = 3;
        @VisibleForTesting
        int STOLEN = 4;
        int NUM_ENTRIES = 5;
    }

    /**
     * Observes spare WebContents deaths. In case of death, records stats, and cleanup the objects.
     */
    private class RenderProcessGoneObserver extends WebContentsObserver {
        @Override
        public void renderProcessGone(boolean wasOomProtected) {
            long elapsed = SystemClock.elapsedRealtime() - mWebContentsCreationTimeMs;
            RecordHistogram.recordLongTimesHistogram(
                    "CustomTabs.SpareWebContents.TimeBeforeDeath", elapsed);
            recordWebContentsStatus(WebContentsStatus.KILLED);
            destroySpareWebContentsInternal();
        }
    }

    @SuppressLint("StaticFieldLeak")
    private static WarmupManager sWarmupManager;

    private final Set<String> mDnsRequestsInFlight;
    private final Map<String, Profile> mPendingPreconnectWithProfile;

    private int mToolbarContainerId;
    private ViewGroup mMainView;
    @VisibleForTesting
    WebContents mSpareWebContents;
    private long mWebContentsCreationTimeMs;
    private RenderProcessGoneObserver mObserver;
    private boolean mWebContentsCreatedForCCT;

    /**
     * @return The singleton instance for the WarmupManager, creating one if necessary.
     */
    public static WarmupManager getInstance() {
        ThreadUtils.assertOnUiThread();
        if (sWarmupManager == null) sWarmupManager = new WarmupManager();
        return sWarmupManager;
    }

    private WarmupManager() {
        mDnsRequestsInFlight = new HashSet<>();
        mPendingPreconnectWithProfile = new HashMap<>();
    }

    /**
     * Inflates and constructs the view hierarchy that the app will use.
     * @param baseContext The base context to use for creating the ContextWrapper.
     * @param toolbarContainerId Id of the toolbar container.
     * @param toolbarId The toolbar's layout ID.
     */
    public void initializeViewHierarchy(Context baseContext, int toolbarContainerId,
            int toolbarId) {
        ThreadUtils.assertOnUiThread();
        if (mMainView != null && mToolbarContainerId == toolbarContainerId) return;
        mMainView = inflateViewHierarchy(baseContext, toolbarContainerId, toolbarId);
        mToolbarContainerId = toolbarContainerId;
    }

    /**
     * Inflates and constructs the view hierarchy that the app will use.
     * Calls to this are not restricted to the UI thread.
     * @param baseContext The base context to use for creating the ContextWrapper.
     * @param toolbarContainerId Id of the toolbar container.
     * @param toolbarId The toolbar's layout ID.
     */
    public static ViewGroup inflateViewHierarchy(
            Context baseContext, int toolbarContainerId, int toolbarId) {
        // Inflating the view hierarchy causes StrictMode violations on some
        // devices. Since layout inflation should happen on the UI thread, allow
        // the disk reads. crbug.com/644243.
        try (TraceEvent e = TraceEvent.scoped("WarmupManager.inflateViewHierarchy");
                StrictModeContext c = StrictModeContext.allowDiskReads()) {
            ContextThemeWrapper context =
                    new ContextThemeWrapper(baseContext, ChromeActivity.getThemeId());
            FrameLayout contentHolder = new FrameLayout(context);
            ViewGroup mainView =
                    (ViewGroup) LayoutInflater.from(context).inflate(R.layout.main, contentHolder);
            if (toolbarContainerId != ChromeActivity.NO_CONTROL_CONTAINER) {
                ViewStub stub = (ViewStub) mainView.findViewById(R.id.control_container_stub);
                stub.setLayoutResource(toolbarContainerId);
                stub.inflate();
            }
            // It cannot be assumed that the result of toolbarContainerStub.inflate() will be
            // the control container since it may be wrapped in another view.
            ControlContainer controlContainer =
                    (ControlContainer) mainView.findViewById(R.id.control_container);

            if (toolbarId != ChromeActivity.NO_TOOLBAR_LAYOUT && controlContainer != null) {
                controlContainer.initWithToolbar(toolbarId);
            }
            return mainView;
        } catch (InflateException e) {
            // See https://crbug.com/606715.
            Log.e(TAG, "Inflation exception.", e);
            return null;
        }
    }

    /**
     * Transfers all the children in the local view hierarchy {@link #mMainView} to the given
     * ViewGroup {@param contentView} as child.
     * @param contentView The parent ViewGroup to use for the transfer.
     */
    public void transferViewHierarchyTo(ViewGroup contentView) {
        ThreadUtils.assertOnUiThread();
        ViewGroup viewHierarchy = mMainView;
        mMainView = null;
        if (viewHierarchy == null) return;
        transferViewHeirarchy(viewHierarchy, contentView);
    }

    /**
     * Transfers all the children in one view hierarchy {@param from} to another {@param to}.
     * @param from The parent ViewGroup to transfer children from.
     * @param to The parent ViewGroup to transfer children to.
     */
    public static void transferViewHeirarchy(ViewGroup from, ViewGroup to) {
        while (from.getChildCount() > 0) {
            View currentChild = from.getChildAt(0);
            from.removeView(currentChild);
            to.addView(currentChild);
        }
    }

    /**
     * @return Whether a pre-built view hierarchy exists for the given toolbarContainerId.
     */
    public boolean hasViewHierarchyWithToolbar(int toolbarContainerId) {
        ThreadUtils.assertOnUiThread();
        return mMainView != null && mToolbarContainerId == toolbarContainerId;
    }

    /**
     * Clears the inflated view hierarchy.
     */
    public void clearViewHierarchy() {
        ThreadUtils.assertOnUiThread();
        mMainView = null;
    }

    /**
     * Launches a background DNS query for a given URL.
     *
     * @param url URL from which the domain to query is extracted.
     */
    private void prefetchDnsForUrlInBackground(final String url) {
        mDnsRequestsInFlight.add(url);
        new AsyncTask<Void>() {
            @Override
            protected Void doInBackground() {
                try (TraceEvent e =
                                TraceEvent.scoped("WarmupManager.prefetchDnsForUrlInBackground")) {
                    InetAddress.getByName(new URL(url).getHost());
                } catch (MalformedURLException e) {
                    // We don't do anything with the result of the request, it
                    // is only here to warm up the cache, thus ignoring the
                    // exception is fine.
                } catch (UnknownHostException e) {
                    // As above.
                }
                return null;
            }

            @Override
            protected void onPostExecute(Void result) {
                mDnsRequestsInFlight.remove(url);
                if (mPendingPreconnectWithProfile.containsKey(url)) {
                    Profile profile = mPendingPreconnectWithProfile.get(url);
                    mPendingPreconnectWithProfile.remove(url);
                    maybePreconnectUrlAndSubResources(profile, url);
                }
            }
        }
                .executeOnExecutor(AsyncTask.THREAD_POOL_EXECUTOR);
    }

    /** Launches a background DNS query for a given URL.
     *
     * @param context The Application context.
     * @param url URL from which the domain to query is extracted.
     */
    public void maybePrefetchDnsForUrlInBackground(Context context, String url) {
        ThreadUtils.assertOnUiThread();
            prefetchDnsForUrlInBackground(url);
    }

    /**
     * Starts asynchronous initialization of the preconnect predictor.
     *
     * Without this call, |maybePreconnectUrlAndSubresources()| will not use a database of origins
     * to connect to, unless the predictor has already been initialized in another way.
     *
     * @param profile The profile to use for the predictor.
     */
    public static void startPreconnectPredictorInitialization(Profile profile) {
        ThreadUtils.assertOnUiThread();
        WarmupManagerJni.get().startPreconnectPredictorInitialization(profile);
    }

    /** Asynchronously preconnects to a given URL if the data reduction proxy is not in use.
     *
     * @param profile The profile to use for the preconnection.
     * @param url The URL we want to preconnect to.
     */
    public void maybePreconnectUrlAndSubResources(Profile profile, String url) {
        ThreadUtils.assertOnUiThread();

        Uri uri = Uri.parse(url);
        if (uri == null) return;
        String scheme = uri.normalizeScheme().getScheme();
        if (!UrlConstants.HTTP_SCHEME.equals(scheme) && !UrlConstants.HTTPS_SCHEME.equals(scheme)) {
            return;
        }

        // If there is already a DNS request in flight for this URL, then the preconnection will
        // start by issuing a DNS request for the same domain, as the result is not cached. However,
        // such a DNS request has already been sent from this class, so it is better to wait for the
        // answer to come back before preconnecting. Otherwise, the preconnection logic will wait
        // for the result of the second DNS request, which should arrive after the result of the
        // first one. Note that we however need to wait for the main thread to be available in this
        // case, since the preconnection will be sent from AsyncTask.onPostExecute(), which may
        // delay it.
        if (mDnsRequestsInFlight.contains(url)) {
            // Note that if two requests come for the same URL with two different profiles, the last
            // one will win.
            mPendingPreconnectWithProfile.put(url, profile);
        } else {
            WarmupManagerJni.get().preconnectUrlAndSubresources(profile, url);
        }
    }

    /**
     * Warms up a spare, empty RenderProcessHost that may be used for subsequent navigations.
     *
     * The spare RenderProcessHost will be used automatically in subsequent navigations.
     * There is nothing further the WarmupManager needs to do to enable that use.
     *
     * This uses a different mechanism than createSpareWebContents, below, and is subject
     * to fewer restrictions.
     *
     * This must be called from the UI thread.
     */
    public void createSpareRenderProcessHost(Profile profile) {
        ThreadUtils.assertOnUiThread();
        if (!LibraryLoader.getInstance().isInitialized()) return;
        if (ChromeFeatureList.isEnabled(ChromeFeatureList.OMNIBOX_SPARE_RENDERER)) {
            // Spare WebContents should not be used with spare RenderProcessHosts, but if one
            // has been created, destroy it in order not to consume too many processes.
            destroySpareWebContents();
            WarmupManagerJni.get().warmupSpareRenderer(profile);
        }
    }

    /**
     * Creates and initializes a spare WebContents, to be used in a subsequent navigation.
     *
     * This creates a renderer that is suitable for any navigation. It can be picked up by any tab.
     * Can be called multiple times, and must be called from the UI thread.
     *
     * @param forCCT Whether this WebContents is being created for CCT.
     */
    public void createSpareWebContents(boolean forCCT) {
        ThreadUtils.assertOnUiThread();
        if (!LibraryLoader.getInstance().isInitialized() || mSpareWebContents != null) return;

        mWebContentsCreatedForCCT = forCCT;
        mSpareWebContents = new WebContentsFactory().createWebContentsWithWarmRenderer(
                false /* incognito */, true /* initiallyHidden */);
        mObserver = new RenderProcessGoneObserver();
        mSpareWebContents.addObserver(mObserver);
        mWebContentsCreationTimeMs = SystemClock.elapsedRealtime();
        recordWebContentsStatus(WebContentsStatus.CREATED);
    }

    /**
     * Destroys the spare WebContents if there is one.
     */
    public void destroySpareWebContents() {
        ThreadUtils.assertOnUiThread();
        if (mSpareWebContents == null) return;
        recordWebContentsStatus(WebContentsStatus.DESTROYED);
        destroySpareWebContentsInternal();
    }

    /**
     * Returns a spare WebContents or null, depending on the availability of one.
     *
     * The parameters are the same as for {@link WebContentsFactory#createWebContents()}.
     * @param forCCT Whether this WebContents is being taken by CCT.
     *
     * @return a WebContents, or null.
     */
    public WebContents takeSpareWebContents(
            boolean incognito, boolean initiallyHidden, boolean forCCT) {
        ThreadUtils.assertOnUiThread();
        if (incognito) return null;
        WebContents result = mSpareWebContents;
        if (result == null) return null;
        mSpareWebContents = null;
        result.removeObserver(mObserver);
        mObserver = null;
        if (!initiallyHidden) result.onShow();
        recordWebContentsStatus(mWebContentsCreatedForCCT == forCCT ? WebContentsStatus.USED
                                                                    : WebContentsStatus.STOLEN);
        return result;
    }

    /**
     * @return Whether a spare renderer is available.
     */
    public boolean hasSpareWebContents() {
        return mSpareWebContents != null;
    }

    private void destroySpareWebContentsInternal() {
        mSpareWebContents.removeObserver(mObserver);
        mSpareWebContents.destroy();
        mSpareWebContents = null;
        mObserver = null;
    }

    private void recordWebContentsStatus(@WebContentsStatus int status) {
        if (!mWebContentsCreatedForCCT) return;
        RecordHistogram.recordEnumeratedHistogram(
                WEBCONTENTS_STATUS_HISTOGRAM, status, WebContentsStatus.NUM_ENTRIES);
    }

    @NativeMethods
    interface Natives {
        void startPreconnectPredictorInitialization(Profile profile);
        void preconnectUrlAndSubresources(Profile profile, String url);
        void warmupSpareRenderer(Profile profile);
    }
}
