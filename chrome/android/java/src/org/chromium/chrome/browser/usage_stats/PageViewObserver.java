// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.usage_stats;

import static org.chromium.build.NullUtil.assertNonNull;

import android.annotation.SuppressLint;
import android.app.Activity;
import android.app.usage.UsageStatsManager;
import android.content.Context;

import org.chromium.base.Log;
import org.chromium.base.TraceEvent;
import org.chromium.base.supplier.NullableObservableSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.tab.CurrentTabObserver;
import org.chromium.chrome.browser.tab.SadTab;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabHidingType;
import org.chromium.chrome.browser.tab.TabObserver;
import org.chromium.chrome.browser.tab.TabSelectionType;
import org.chromium.chrome.browser.tab_ui.TabContentManager;
import org.chromium.components.embedder_support.util.UrlUtilities;
import org.chromium.content_public.browser.NavigationHandle;
import org.chromium.url.GURL;

import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Method;
import java.util.function.Supplier;

/**
 * Class that observes url and tab changes in order to track when browsing stops and starts for each
 * visited fully-qualified domain name (FQDN).
 */
@SuppressLint("NewApi")
@NullMarked
public class PageViewObserver implements TabObserver {
    private static final String TAG = "PageViewObserver";

    // The following fields are non-final so they can be cleared in destroy() to break the leak
    // path through pending callbacks held by the long-lived per-Profile TokenTracker promise
    // chain. Until that promise is fulfilled, this PageViewObserver may be retained, which would
    // otherwise leak the destroyed Activity through any of these fields:
    //   - mActivity directly,
    //   - mCurrentTabObserver -> CurrentTabObserver.mTab -> TabImpl.mWindowAndroid -> Activity,
    //   - mTabContentManagerSupplier -> TabContentManager.mContext -> Activity,
    //   - mCurrentTab -> TabImpl.mDelegateFactory / mWindowAndroid -> Activity.
    // See TabSuspensionTest LeakCanary trace.
    private @Nullable Activity mActivity;
    private @Nullable CurrentTabObserver mCurrentTabObserver;
    private @Nullable Supplier<TabContentManager> mTabContentManagerSupplier;
    private final EventTracker mEventTracker;
    private final TokenTracker mTokenTracker;
    private final SuspensionTracker mSuspensionTracker;

    private @Nullable Tab mCurrentTab;
    private @Nullable String mLastFqdn;

    PageViewObserver(
            Activity activity,
            NullableObservableSupplier<Tab> tabSupplier,
            EventTracker eventTracker,
            TokenTracker tokenTracker,
            SuspensionTracker suspensionTracker,
            Supplier<TabContentManager> tabContentManagerSupplier) {
        mActivity = activity;
        mEventTracker = eventTracker;
        mTokenTracker = tokenTracker;
        mSuspensionTracker = suspensionTracker;
        mTabContentManagerSupplier = tabContentManagerSupplier;
        mCurrentTabObserver = new CurrentTabObserver(tabSupplier, this, this::activeTabChanged);
        mCurrentTabObserver.triggerWithCurrentTab();
    }

    /**
     * Releases references to the owning Activity and unregisters from the current Tab. Called by
     * the owning {@link UsageStatsService} when the Activity that created this observer is
     * destroyed.
     */
    public void destroy() {
        if (mActivity == null) return;
        // CurrentTabObserver.destroy() removes us as an observer from the current Tab.
        if (mCurrentTabObserver != null) {
            mCurrentTabObserver.destroy();
        }
        // Drop strong references that transitively reach the destroyed Activity. See the field
        // declarations above for the leak paths this addresses.
        mActivity = null;
        mCurrentTab = null;
        mCurrentTabObserver = null;
        mTabContentManagerSupplier = null;
    }

    @Override
    public void onShown(Tab tab, @TabSelectionType int type) {
        if (!tab.isLoading() && !tab.isBeingRestored()) {
            updateUrl(tab.getUrl());
        }
    }

    @Override
    public void onHidden(Tab tab, @TabHidingType int type) {
        updateUrl(null);
    }

    @Override
    public void onDidStartNavigationInPrimaryMainFrame(Tab tab, NavigationHandle navigationHandle) {
        assert tab == mCurrentTab;
        // We only want to check for suspended tabs on new navigations, not on same-document
        // navigations like fragment changes or history.pushState.
        if (navigationHandle.isSameDocument()) return;

        GURL url = navigationHandle.getUrl();
        String newFqdn = getValidFqdnOrEmptyString(url);
        // We don't call updateUrl() here to avoid reporting start events for domains
        // that never paint, e.g. link shorteners. We still need to check the SuspendedTab
        // state because a tab that's suspended can't paint, and the user could be
        // navigating away from a suspended domain.
        checkSuspendedTabState(mSuspensionTracker.isWebsiteSuspended(newFqdn), newFqdn);
    }

    @Override
    public void didFirstVisuallyNonEmptyPaint(Tab tab) {
        assert tab == mCurrentTab;

        updateUrl(tab.getUrl());
    }

    @Override
    public void onCrash(Tab tab) {
        updateUrl(null);
    }

    /** Notify PageViewObserver that {@code fqdn} was just suspended or un-suspended. */
    public void notifySiteSuspensionChanged(String fqdn, boolean isSuspended) {
        if (mCurrentTab == null || !mCurrentTab.isInitialized()) return;
        // mTabContentManagerSupplier is only nulled in destroy(), which also nulls mCurrentTab.
        SuspendedTab suspendedTab =
                SuspendedTab.from(mCurrentTab, assertNonNull(mTabContentManagerSupplier));
        if (fqdn.equals(mLastFqdn) || fqdn.equals(suspendedTab.getFqdn())) {
            if (checkSuspendedTabState(isSuspended, fqdn)) {
                reportStop();
            }
        }
    }

    /**
     * Updates our state from the previous url to {@code newUrl}. This can result in any/all of the
     * following:
     *
     * <pre>
     * 1. Suspension or un-suspension of mCurrentTab.
     * 2. Reporting a stop event for mLastFqdn.
     * 3. Reporting a start event for the fqdn of {@code newUrl}.
     * </pre>
     */
    private void updateUrl(@Nullable GURL newUrl) {
        String newFqdn = getValidFqdnOrEmptyString(newUrl);
        boolean isSameDomain = newFqdn.equals(mLastFqdn);
        boolean isValidProtocol = newUrl != null && UrlUtilities.isHttpOrHttps(newUrl);

        boolean isSuspended = mSuspensionTracker.isWebsiteSuspended(newFqdn);
        boolean didSuspend = checkSuspendedTabState(isSuspended, newFqdn);

        if (mLastFqdn != null && (didSuspend || !isSameDomain)) {
            reportStop();
        }

        if (isValidProtocol && !isSuspended && !isSameDomain) {
            mLastFqdn = newFqdn;
            mEventTracker.addWebsiteEvent(
                    new WebsiteEvent(
                            System.currentTimeMillis(), mLastFqdn, WebsiteEvent.EventType.START));
            reportToPlatformIfDomainIsTracked("reportUsageStart", mLastFqdn);
        }
    }

    /**
     * Hides or shows the SuspendedTab for mCurrentTab, based on:
     *
     * <pre>
     * 1. If it is currently shown or hidden
     * 2. Its current fqdn, if any.
     * 3. If fqdn is newly suspended or not.
     * </pre>
     *
     * There are really only two important cases; either the SuspendedTab is showing and should be
     * hidden, or it's hidden and should be shown.
     */
    private boolean checkSuspendedTabState(boolean isNewlySuspended, String fqdn) {
        if (mCurrentTab == null) return false;
        // mTabContentManagerSupplier is only nulled in destroy(), which also nulls mCurrentTab.
        SuspendedTab suspendedTab =
                SuspendedTab.from(mCurrentTab, assertNonNull(mTabContentManagerSupplier));
        // We don't need to do anything in situations where the current state matches the desired;
        // i.e. either the suspended tab is already showing with the correct fqdn, or the suspended
        // tab is hidden and should be hidden.
        if (isNewlySuspended && fqdn.equals(suspendedTab.getFqdn())) return false;
        if (!isNewlySuspended && !suspendedTab.isShowing()) return false;

        if (isNewlySuspended) {
            suspendedTab.show(fqdn);
            return true;
        } else {
            suspendedTab.removeIfPresent();
            if (!mCurrentTab.isLoading() && !SadTab.isShowing(mCurrentTab)) {
                mCurrentTab.reload();
            }
        }
        return false;
    }

    private void reportStop() {
        mEventTracker.addWebsiteEvent(
                new WebsiteEvent(
                        System.currentTimeMillis(),
                        assertNonNull(mLastFqdn),
                        WebsiteEvent.EventType.STOP));
        reportToPlatformIfDomainIsTracked("reportUsageStop", mLastFqdn);
        mLastFqdn = null;
    }

    private void activeTabChanged(@Nullable Tab tab) {
        mCurrentTab = tab;
        if (mCurrentTab == null) {
            updateUrl(null);
        } else if (mCurrentTab.isIncognito()) {
            updateUrl(null);
            mCurrentTab.removeObserver(this);
        } else if (!mCurrentTab.isHidden()) {
            // If the newly active tab is hidden, we don't want to check its URL yet; we'll wait
            // until the onShown event fires.
            updateUrl(mCurrentTab.getUrl());
        }
    }

    private void reportToPlatformIfDomainIsTracked(String reportMethodName, @Nullable String fqdn) {
        mTokenTracker
                .getTokenForFqdn(fqdn)
                .then(
                        (token) -> {
                            if (token == null) return;
                            // The Promise we attached to is held by the long-lived per-Profile
                            // TokenTracker, so this callback may fire after the owning Activity
                            // has been destroyed and mActivity has been cleared.
                            Activity activity = mActivity;
                            if (activity == null) return;
                            try (TraceEvent te =
                                    TraceEvent.scoped(
                                            "PageViewObserver.reportToPlatformIfDomainIsTracked")) {
                                UsageStatsManager instance =
                                        (UsageStatsManager)
                                                activity.getSystemService(
                                                        Context.USAGE_STATS_SERVICE);
                                Method reportMethod =
                                        UsageStatsManager.class.getDeclaredMethod(
                                                reportMethodName, Activity.class, String.class);

                                reportMethod.invoke(instance, activity, token);
                            } catch (InvocationTargetException
                                    | NoSuchMethodException
                                    | IllegalAccessException e) {
                                Log.e(TAG, "Failed to report to platform API", e);
                            }
                        });
    }

    private static String getValidFqdnOrEmptyString(@Nullable GURL url) {
        if (GURL.isEmptyOrInvalid(url)) return "";
        return url.getHost();
    }
}
