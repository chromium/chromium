// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import android.annotation.SuppressLint;
import android.content.Context;
import android.net.Uri;
import android.view.ContextThemeWrapper;
import android.view.InflateException;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.view.ViewStub;
import android.widget.FrameLayout;

import androidx.annotation.IntDef;
import androidx.annotation.VisibleForTesting;
import androidx.asynclayoutinflater.appcompat.AsyncAppCompatFactory;

import org.chromium.base.Log;
import org.chromium.base.ThreadUtils;
import org.chromium.base.TraceEvent;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.base.library_loader.LibraryLoader;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.task.AsyncTask;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.crash.ChromePureJavaExceptionReporter;
import org.chromium.chrome.browser.flags.BooleanCachedFieldTrialParameter;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tab.TabObserver;
import org.chromium.chrome.browser.tabmodel.TabCreator;
import org.chromium.chrome.browser.toolbar.ControlContainer;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.WebContentsObserver;
import org.chromium.ui.LayoutInflaterUtils;

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

    /**
     * Observes spare WebContents deaths. In case of death, records stats, and cleanup the objects.
     */
    private class RenderProcessGoneObserver extends WebContentsObserver {
        @Override
        public void renderProcessGone() {
            destroySpareWebContentsInternal();
        }
    }

    /**
     * Records stats, observes crashes, and cleans up spareTab object.
     */
    private TabObserver mSpareTabObserver = new EmptyTabObserver() {
        @Override
        // Invoked when tab crashes, or when the associated renderer process is killed.
        public void onCrash(Tab tab) {
            mSpareTabFinalStatus = SpareTabFinalStatus.TAB_CRASHED;
            destroySpareTabInternal();
        }
    };

    @SuppressLint("StaticFieldLeak")
    private static WarmupManager sWarmupManager;

    private final Set<String> mDnsRequestsInFlight;
    private final Map<String, Profile> mPendingPreconnectWithProfile;

    private int mToolbarContainerId;
    private ViewGroup mMainView;
    @VisibleForTesting
    WebContents mSpareWebContents;
    private RenderProcessGoneObserver mObserver;

    // Stores a prebuilt tab. To load a URL, this can be used if available instead of creating one
    // from scratch.
    @VisibleForTesting
    Tab mSpareTab;

    /**
     * Returns true if SPARE_TAB feature is enabled.
     */
    private static boolean isSpareTabEnabled() {
        return ChromeFeatureList.isEnabled(ChromeFeatureList.SPARE_TAB);
    }

    // Feature Param to control initializing renderer creation with spare tab creation. This
    // initializes a renderer process and creates a RenderFrame in it for the initial
    // RenderFrameHost. By default we don't initialize renderer.
    public static final String SPARE_TAB_INITIALIZE_RENDERER_PARAM =
            "spare_tab_initialize_renderer";
    public static final BooleanCachedFieldTrialParameter SPARE_TAB_INITIALIZE_RENDERER =
            new BooleanCachedFieldTrialParameter(
                    ChromeFeatureList.SPARE_TAB, SPARE_TAB_INITIALIZE_RENDERER_PARAM, false);

    /**
     * Represents various states of spareTab.
     *
     * These values are persisted to logs. Entries should not be renumbered and
     * numeric values should never be reused. See tools/metrics/histograms/enums.xml.
     */
    @IntDef({SpareTabFinalStatus.TAB_CREATED_BUT_NOT_USED,
            SpareTabFinalStatus.TAB_CREATION_IN_PROGRESS, SpareTabFinalStatus.TAB_USED,
            SpareTabFinalStatus.TAB_CRASHED, SpareTabFinalStatus.TAB_DESTROYED,
            SpareTabFinalStatus.NUM_ENTRIES})
    @Retention(RetentionPolicy.SOURCE)
    public @interface SpareTabFinalStatus {
        int TAB_CREATED_BUT_NOT_USED = 0;
        int TAB_CREATION_IN_PROGRESS = 1;
        int TAB_USED = 2;
        int TAB_CRASHED = 3;
        int TAB_DESTROYED = 4;
        int NUM_ENTRIES = 5;
    }
    @SpareTabFinalStatus
    int mSpareTabFinalStatus;

    /**
     * Records the spareTab final status.
     * @param status Status to be recorded in the enumerated histogram.
     */
    private void recordSpareTabFinalStatusHistogram(@SpareTabFinalStatus int status) {
        RecordHistogram.recordEnumeratedHistogram(
                "Android.SpareTab.FinalStatus", status, SpareTabFinalStatus.NUM_ENTRIES);
    }

    /**
     * Destroys the spare Tab if there is one and sets mSpareTab to null.
     */
    public void destroySpareTab() {
        try (TraceEvent e = TraceEvent.scoped("WarmupManager.destroySpareTab")) {
            ThreadUtils.assertOnUiThread();

            mSpareTabFinalStatus = SpareTabFinalStatus.TAB_DESTROYED;
            destroySpareTabInternal();
        }
    }

    private void destroySpareTabInternal() {
        // Don't do anything if the spare tab doesn't exist.
        if (mSpareTab == null) return;

        // Record the SpareTabFinalStatus once its destroyed.
        recordSpareTabFinalStatusHistogram(mSpareTabFinalStatus);

        mSpareTab.destroy();
        mSpareTab = null;
    }

    /**
     * Creates and initializes a spare Tab of TabLaunchType type, to be used for a subsequent
     * navigation.
     *
     * This creates a WebContents and initializes renderer if SPARE_TAB_INITIALIZE_RENDERER is true.
     * It can be picked up by any tab with TabLaunchType as type. Can be called multiple times, and
     * must be called from the UI thread.
     *
     * @param tabCreator The {@link TabCreator} object to create spareTab.
     * @param type The LaunchType while creating this spareTab.
     *
     */
    public void createSpareTab(TabCreator tabCreator, @TabLaunchType int type) {
        try (TraceEvent e = TraceEvent.scoped("WarmupManager.createSpareTab")) {
            // Return without creating spare Tab if spareTab feature isn't enabled.
            if (!isSpareTabEnabled()) return;

            mSpareTabFinalStatus = SpareTabFinalStatus.TAB_CREATION_IN_PROGRESS;
            ThreadUtils.assertOnUiThread();

            // Ensure native is initialized before creating spareTab.
            assert LibraryLoader.getInstance().isInitialized();

            if (mSpareTab != null) {
                // If a spare Tab is already present for the launch type don't create a new one.
                if (mSpareTab.getLaunchType() == type) return;

                // Destroy the old spare tab before creating new one.
                destroySpareTab();
            }

            // We don't handle spare tab creation if tabCreator is null.
            if (tabCreator == null) return;

            // Initializes renderer with WebContents creation if enabled.
            boolean initialize_renderer = SPARE_TAB_INITIALIZE_RENDERER.getValue();

            // Build a spare detached tab.
            Tab spareTab = tabCreator.buildDetachedSpareTab(type, initialize_renderer);

            mSpareTab = spareTab;
            assert mSpareTab != null : "Building a spare detached tab shouldn't return null.";

            mSpareTabFinalStatus = SpareTabFinalStatus.TAB_CREATED_BUT_NOT_USED;
        }

        // Ensure that the TabObserver is set before adding it.
        assert mSpareTabObserver != null;
        if (mSpareTab != null) {
            mSpareTab.addObserver(mSpareTabObserver);
        }
    }

    /**
     * Returns a spare Tab or null, depending on the availability of one.
     *
     * @param incognito whether tab is used in incognito mode or not.
     * @param type TabLaunchType of the requested tab.
     *
     * @return a Tab, or null.
     */
    public Tab takeSpareTab(boolean incognito, @TabLaunchType int type) {
        try (TraceEvent e = TraceEvent.scoped("WarmupManager.takeSpareTab")) {
            if (!canUseSpareTab(type)) return null;

            // We should only invoke this when the spare tab feature is enabled.
            assert isSpareTabEnabled();

            ThreadUtils.assertOnUiThread();

            // Only use spareTab for non-incognito mode.
            if (incognito) return null;

            // Remove the spareTab observer before using it.
            mSpareTab.removeObserver(mSpareTabObserver);

            Tab spareTab = mSpareTab;
            mSpareTab = null;

            mSpareTabFinalStatus = SpareTabFinalStatus.TAB_USED;

            // Record the SpareTabFinalStatus once its used.
            recordSpareTabFinalStatusHistogram(mSpareTabFinalStatus);
            return spareTab;
        }
    }

    /**
     * @return Whether a spare tab is available.
     */
    public boolean hasSpareTab() {
        return mSpareTab != null;
    }

    /**
     * @param tab Tab to compare with SpareTab with.
     *
     * @return Returns true if tab is same as spare tab.
     */
    public boolean isSpareTab(Tab tab) {
        if (mSpareTab == null) return false;

        assert mSpareTab.isHidden() : "Spare tab is not hidden";
        return mSpareTab == tab;
    }

    /**
     * Various conditions are checked to determine whether the spare tab can be used to load a URL.
     * In order to load a URL, the tab properties must match the tab that is being used.
     *
     * @param type TabLaunchType of the requested tab.
     *
     * @return Whether a spare tab can be used for next navigation or false otherwise.
     */
    public boolean canUseSpareTab(@TabLaunchType int type) {
        // If there is no spareTab return false.
        if (!hasSpareTab()) return false;

        // Ensure that TabLaunchType matches when using spareTab for navigation.
        if (mSpareTab.getLaunchType() != type) return false;

        return true;
    }

    /**
     * Removes the singleton instance for the WarmupManager for testing.
     */
    public static void deInitForTesting() {
        sWarmupManager = null;
    }

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
        try (TraceEvent e = TraceEvent.scoped("WarmupManager.inflateViewHierarchy")) {
            ContextThemeWrapper context =
                    new ContextThemeWrapper(baseContext, ActivityUtils.getThemeId());
            FrameLayout contentHolder = new FrameLayout(context);
            var layoutInflater = LayoutInflater.from(context);
            layoutInflater.setFactory2(new AsyncAppCompatFactory());
            ViewGroup mainView = (ViewGroup) LayoutInflaterUtils.inflate(
                    layoutInflater, R.layout.main, contentHolder);
            if (toolbarContainerId != ActivityUtils.NO_RESOURCE_ID) {
                ViewStub stub = (ViewStub) mainView.findViewById(R.id.control_container_stub);
                stub.setLayoutResource(toolbarContainerId);
                stub.inflate();
            }
            // It cannot be assumed that the result of toolbarContainerStub.inflate() will be
            // the control container since it may be wrapped in another view.
            ControlContainer controlContainer =
                    (ControlContainer) mainView.findViewById(R.id.control_container);

            if (toolbarId != ActivityUtils.NO_RESOURCE_ID && controlContainer != null) {
                controlContainer.initWithToolbar(toolbarId);
            }
            return mainView;
        } catch (InflateException e) {
            // Warmup manager is only a performance improvement. If inflation failed, it will be
            // redone when the CCT is actually launched using an activity context. So, swallow
            // exceptions here to improve resilience. See https://crbug.com/606715.
            Log.e(TAG, "Inflation exception.", e);
            // An exception caught here may indicate a real bug in production code. We report the
            // exceptions to monitor any spikes or stacks that point to Chrome code.
            Throwable throwable = new Throwable(
                    "This is not a crash. See https://crbug.com/1259276 for details.", e);
            ChromePureJavaExceptionReporter.reportJavaException(throwable);
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
        try (TraceEvent e = TraceEvent.scoped("WarmupManager.maybePrefetchDnsForUrlInBackground")) {
            ThreadUtils.assertOnUiThread();
            prefetchDnsForUrlInBackground(url);
        }
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
        try (TraceEvent e =
                        TraceEvent.scoped("WarmupManager.startPreconnectPredictorInitialization")) {
            ThreadUtils.assertOnUiThread();
            WarmupManagerJni.get().startPreconnectPredictorInitialization(profile);
        }
    }

    /** Asynchronously preconnects to a given URL if the data reduction proxy is not in use.
     *
     * @param profile The profile to use for the preconnection.
     * @param url The URL we want to preconnect to.
     */
    public void maybePreconnectUrlAndSubResources(Profile profile, String url) {
        try (TraceEvent e = TraceEvent.scoped("WarmupManager.maybePreconnectUrlAndSubResources")) {
            ThreadUtils.assertOnUiThread();

            Uri uri = Uri.parse(url);
            if (uri == null) return;
            String scheme = uri.normalizeScheme().getScheme();
            if (!UrlConstants.HTTP_SCHEME.equals(scheme)
                    && !UrlConstants.HTTPS_SCHEME.equals(scheme)) {
                return;
            }

            // If there is already a DNS request in flight for this URL, then the preconnection will
            // start by issuing a DNS request for the same domain, as the result is not cached.
            // However, such a DNS request has already been sent from this class, so it is better to
            // wait for the answer to come back before preconnecting. Otherwise, the preconnection
            // logic will wait for the result of the second DNS request, which should arrive after
            // the result of the first one. Note that we however need to wait for the main thread to
            // be available in this case, since the preconnection will be sent from
            // AsyncTask.onPostExecute(), which may delay it.
            if (mDnsRequestsInFlight.contains(url)) {
                // Note that if two requests come for the same URL with two different profiles, the
                // last one will win.
                mPendingPreconnectWithProfile.put(url, profile);
            } else {
                WarmupManagerJni.get().preconnectUrlAndSubresources(profile, url);
            }
        }
    }

    /**
     * Creates and initializes a spare WebContents, to be used in a subsequent navigation.
     *
     * This creates a renderer that is suitable for any navigation. It can be picked up by any tab.
     * Can be called multiple times, and must be called from the UI thread.
     */
    public void createSpareWebContents() {
        try (TraceEvent e = TraceEvent.scoped("WarmupManager.createSpareWebContents")) {
            ThreadUtils.assertOnUiThread();
            if (!LibraryLoader.getInstance().isInitialized() || mSpareWebContents != null) return;

            mSpareWebContents = new WebContentsFactory().createWebContentsWithWarmRenderer(
                    Profile.getLastUsedRegularProfile(), true /* initiallyHidden */);
            mObserver = new RenderProcessGoneObserver();
            mSpareWebContents.addObserver(mObserver);
        }
    }

    /**
     * Destroys the spare WebContents if there is one.
     */
    public void destroySpareWebContents() {
        try (TraceEvent e = TraceEvent.scoped("WarmupManager.destroySpareWebContents")) {
            ThreadUtils.assertOnUiThread();
            if (mSpareWebContents == null) return;
            destroySpareWebContentsInternal();
        }
    }

    /**
     * Returns a spare WebContents or null, depending on the availability of one.
     *
     * The parameters are the same as for {@link WebContentsFactory#createWebContents()}.
     * @param forCCT Whether this WebContents is being taken by CCT.
     *
     * @return a WebContents, or null.
     */
    public WebContents takeSpareWebContents(boolean incognito, boolean initiallyHidden) {
        try (TraceEvent e = TraceEvent.scoped("WarmupManager.takeSpareWebContents")) {
            ThreadUtils.assertOnUiThread();
            if (incognito) return null;
            WebContents result = mSpareWebContents;
            if (result == null) return null;
            mSpareWebContents = null;
            result.removeObserver(mObserver);
            mObserver = null;
            if (!initiallyHidden) result.onShow();
            return result;
        }
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

    @NativeMethods
    interface Natives {
        void startPreconnectPredictorInitialization(Profile profile);
        void preconnectUrlAndSubresources(Profile profile, String url);
    }
}
