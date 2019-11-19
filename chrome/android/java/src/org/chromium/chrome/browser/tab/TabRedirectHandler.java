// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab;

import android.content.ComponentName;
import android.content.Intent;
import android.content.pm.ResolveInfo;
import android.os.SystemClock;
import android.provider.Browser;
import android.text.TextUtils;

import androidx.annotation.Nullable;

import org.chromium.base.ContextUtils;
import org.chromium.base.PackageManagerUtils;
import org.chromium.base.UserData;
import org.chromium.base.UserDataHost;
import org.chromium.chrome.browser.ChromeFeatureList;
import org.chromium.chrome.browser.LaunchIntentDispatcher;
import org.chromium.chrome.browser.customtabs.CustomTabIntentDataProvider;
import org.chromium.chrome.browser.tab.Tab.TabHidingType;
import org.chromium.chrome.browser.util.IntentUtils;
import org.chromium.ui.base.PageTransition;

import java.util.ArrayList;
import java.util.HashSet;
import java.util.List;

/**
 * This class contains the logic to determine effective navigation/redirect.
 */
public class TabRedirectHandler extends EmptyTabObserver implements UserData {
    private static final Class<TabRedirectHandler> USER_DATA_KEY = TabRedirectHandler.class;
    /**
     * An invalid entry index.
     */
    public static final int INVALID_ENTRY_INDEX = -1;
    private static final long INVALID_TIME = -1;

    private static final int NAVIGATION_TYPE_NONE = 0;
    private static final int NAVIGATION_TYPE_FROM_INTENT = 1;
    private static final int NAVIGATION_TYPE_FROM_USER_TYPING = 2;
    private static final int NAVIGATION_TYPE_FROM_LINK_WITHOUT_USER_GESTURE = 3;
    private static final int NAVIGATION_TYPE_FROM_RELOAD = 4;
    private static final int NAVIGATION_TYPE_OTHER = 5;

    private Intent mInitialIntent;
    // A resolver list which includes all resolvers of |mInitialIntent|.
    private final HashSet<ComponentName> mCachedResolvers = new HashSet<ComponentName>();
    private boolean mIsInitialIntentHeadingToChrome;
    private boolean mIsCustomTabIntent;

    private long mLastNewUrlLoadingTime = INVALID_TIME;
    private boolean mIsOnEffectiveRedirectChain;
    private int mInitialNavigationType;
    private int mLastCommittedEntryIndexBeforeStartingNavigation;

    private boolean mShouldNotOverrideUrlLoadingUntilNewUrlLoading;

    /**
     * Returns {@link TabRedirectHandler} that hangs on to a given {@link Tab}.
     * If not present, creates a new instance and associate it with the {@link UserDataHost}
     * that the {@link Tab} manages.
     * @param tab Tab instance that the TabRedirectHandler hangs on to.
     * @return TabRedirectHandler for a given Tab.
     */
    public static TabRedirectHandler from(Tab tab) {
        UserDataHost host = tab.getUserDataHost();
        TabRedirectHandler handler = host.getUserData(USER_DATA_KEY);
        if (handler == null) {
            handler = new TabRedirectHandler();
            host.setUserData(USER_DATA_KEY, handler);
            tab.addObserver(handler);
        }
        return handler;
    }

    /**
     * @return {@link TabRedirectHandler} hanging to the given {@link Tab},
     *     or {@code null} if there is no instance available.
     */
    @Nullable
    public static TabRedirectHandler get(Tab tab) {
        return tab.getUserDataHost().getUserData(USER_DATA_KEY);
    }

    /**
     * Replace {@link TabRedirectHandler} instance for the Tab with the new one.
     * @return Old {@link TabRedirectHandler} associated with the Tab. Could be {@code null}.
     */
    public static TabRedirectHandler swapFor(Tab tab, @Nullable TabRedirectHandler newHandler) {
        UserDataHost host = tab.getUserDataHost();
        TabRedirectHandler oldHandler = host.getUserData(USER_DATA_KEY);
        if (newHandler != null) {
            host.setUserData(USER_DATA_KEY, newHandler);
        } else {
            host.removeUserData(USER_DATA_KEY);
        }
        return oldHandler;
    }

    public static TabRedirectHandler create() {
        return new TabRedirectHandler();
    }

    protected TabRedirectHandler() {}

    @Override
    public void onHidden(Tab tab, @TabHidingType int type) {
        clear();
    }

    /**
     * Updates |mIntentHistory| and |mLastIntentUpdatedTime|. If |intent| comes from chrome and
     * currently |mIsOnEffectiveIntentRedirectChain| is true, that means |intent| was sent from
     * this tab because only the front tab or a new tab can receive an intent from chrome. In that
     * case, |intent| is added to |mIntentHistory|.
     * Otherwise, |mIntentHistory| and |mPreviousResolvers| are cleared, and then |intent| is put
     * into |mIntentHistory|.
     */
    public void updateIntent(Intent intent) {
        clear();

        if (intent == null || !Intent.ACTION_VIEW.equals(intent.getAction())) {
            return;
        }

        mIsCustomTabIntent = LaunchIntentDispatcher.isCustomTabIntent(intent);
        boolean checkIsToChrome = true;
        // All custom tabs VIEW intents are by design explicit intents, so the presence of package
        // name doesn't imply they have to be handled by Chrome explicitly. Check if external apps
        // should be checked for handling the initial redirect chain.
        if (mIsCustomTabIntent) {
            boolean sendToExternalApps = IntentUtils.safeGetBooleanExtra(intent,
                    CustomTabIntentDataProvider.EXTRA_SEND_TO_EXTERNAL_DEFAULT_HANDLER, false);
            checkIsToChrome = !(sendToExternalApps
                    && ChromeFeatureList.isEnabled(ChromeFeatureList.CCT_EXTERNAL_LINK_HANDLING));
        }

        if (checkIsToChrome) mIsInitialIntentHeadingToChrome = isIntentToChrome(intent);

        // A copy of the intent with component cleared to find resolvers.
        mInitialIntent = new Intent(intent).setComponent(null);
        Intent selector = mInitialIntent.getSelector();
        if (selector != null) selector.setComponent(null);
    }

    private static boolean isIntentToChrome(Intent intent) {
        String chromePackageName = ContextUtils.getApplicationContext().getPackageName();
        return TextUtils.equals(chromePackageName, intent.getPackage())
                || TextUtils.equals(chromePackageName, IntentUtils.safeGetStringExtra(intent,
                        Browser.EXTRA_APPLICATION_ID));
    }

    private void clearIntentHistory() {
        mIsInitialIntentHeadingToChrome = false;
        mIsCustomTabIntent = false;
        mInitialIntent = null;
        mCachedResolvers.clear();
    }

    /**
     * Resets all variables except timestamps.
     */
    public void clear() {
        clearIntentHistory();
        mInitialNavigationType = NAVIGATION_TYPE_NONE;
        mIsOnEffectiveRedirectChain = false;
        mLastCommittedEntryIndexBeforeStartingNavigation = 0;
        mShouldNotOverrideUrlLoadingUntilNewUrlLoading = false;
    }

    public void setShouldNotOverrideUrlLoadingUntilNewUrlLoading() {
        mShouldNotOverrideUrlLoadingUntilNewUrlLoading = true;
    }

    /**
     * Updates new url loading information to trace navigation.
     * A time based heuristic is used to determine if this loading is an effective redirect or not
     * if core of |pageTransType| is LINK.
     *
     * http://crbug.com/322567 : Trace navigation started from an external app.
     * http://crbug.com/331571 : Trace navigation started from user typing to do not override such
     * navigation.
     * http://crbug.com/426679 : Trace every navigation and the last committed entry index right
     * before starting the navigation.
     *
     * @param pageTransType page transition type of this loading.
     * @param isRedirect whether this loading is http redirect or not.
     * @param hasUserGesture whether this loading is started by a user gesture.
     * @param lastUserInteractionTime time when the last user interaction was made.
     * @param lastCommittedEntryIndex the last committed entry index right before this loading.
     */
    public void updateNewUrlLoading(int pageTransType, boolean isRedirect, boolean hasUserGesture,
            long lastUserInteractionTime, int lastCommittedEntryIndex) {
        long prevNewUrlLoadingTime = mLastNewUrlLoadingTime;
        mLastNewUrlLoadingTime = SystemClock.elapsedRealtime();

        int pageTransitionCore = pageTransType & PageTransition.CORE_MASK;

        boolean isNewLoadingStartedByUser = false;
        boolean isFromIntent = pageTransitionCore == PageTransition.LINK
                && (pageTransType & PageTransition.FROM_API) != 0;
        if (!isRedirect) {
            if ((pageTransType & PageTransition.FORWARD_BACK) != 0) {
                isNewLoadingStartedByUser = true;
            } else if (pageTransitionCore != PageTransition.LINK
                    && pageTransitionCore != PageTransition.FORM_SUBMIT) {
                isNewLoadingStartedByUser = true;
            } else if (prevNewUrlLoadingTime == INVALID_TIME || isFromIntent
                    || lastUserInteractionTime > prevNewUrlLoadingTime) {
                isNewLoadingStartedByUser = true;
            }
        }

        if (isNewLoadingStartedByUser) {
            // Updates mInitialNavigationType for a new loading started by a user's gesture.
            if (isFromIntent && mInitialIntent != null) {
                mInitialNavigationType = NAVIGATION_TYPE_FROM_INTENT;
            } else {
                clearIntentHistory();
                if (pageTransitionCore == PageTransition.TYPED) {
                    mInitialNavigationType = NAVIGATION_TYPE_FROM_USER_TYPING;
                } else if (pageTransitionCore == PageTransition.RELOAD
                        || (pageTransType & PageTransition.FORWARD_BACK) != 0) {
                    mInitialNavigationType = NAVIGATION_TYPE_FROM_RELOAD;
                } else if (pageTransitionCore == PageTransition.LINK && !hasUserGesture) {
                    mInitialNavigationType = NAVIGATION_TYPE_FROM_LINK_WITHOUT_USER_GESTURE;
                } else {
                    mInitialNavigationType = NAVIGATION_TYPE_OTHER;
                }
            }
            mIsOnEffectiveRedirectChain = false;
            mLastCommittedEntryIndexBeforeStartingNavigation = lastCommittedEntryIndex;
            mShouldNotOverrideUrlLoadingUntilNewUrlLoading = false;
        } else if (mInitialNavigationType != NAVIGATION_TYPE_NONE) {
            // Redirect chain starts from the second url loading.
            mIsOnEffectiveRedirectChain = true;
        }
    }

    /**
     * @return whether on effective intent redirect chain or not.
     */
    public boolean isOnEffectiveIntentRedirectChain() {
        return mInitialNavigationType == NAVIGATION_TYPE_FROM_INTENT && mIsOnEffectiveRedirectChain;
    }

    /**
     * @param hasExternalProtocol whether the destination URI has an external protocol or not.
     * @return whether we should stay in Chrome or not.
     */
    public boolean shouldStayInChrome(boolean hasExternalProtocol) {
        return shouldStayInChrome(hasExternalProtocol, false);
    }

    /**
     * @param hasExternalProtocol whether the destination URI has an external protocol or not.
     * @param isForTrustedCallingApp whether the app we would launch to is trusted and what launched
     *                               Chrome.
     * @return whether we should stay in Chrome or not.
     */
    public boolean shouldStayInChrome(boolean hasExternalProtocol,
            boolean isForTrustedCallingApp) {
        // http://crbug/424029 : Need to stay in Chrome for an intent heading explicitly to Chrome.
        // http://crbug/881740 : Relax stay in Chrome restriction for Custom Tabs.
        return (mIsInitialIntentHeadingToChrome && !hasExternalProtocol)
                || shouldNavigationTypeStayInChrome(isForTrustedCallingApp);
    }

    /**
     * @return Whether the current navigation is of the type that should always stay in Chrome.
     */
    public boolean shouldNavigationTypeStayInChrome() {
        return shouldNavigationTypeStayInChrome(false);
    }

    private boolean shouldNavigationTypeStayInChrome(boolean isForTrustedCallingApp) {
        // http://crbug.com/162106: Never leave Chrome from a refresh.
        if (mInitialNavigationType == NAVIGATION_TYPE_FROM_RELOAD) return true;

        // If the app we would navigate to is trusted and what launched Chrome, allow the
        // navigation.
        if (isForTrustedCallingApp) return false;

        // Otherwise allow navigation out of the app only with a user gesture.
        return mInitialNavigationType == NAVIGATION_TYPE_FROM_LINK_WITHOUT_USER_GESTURE;
    }

    /**
     * @return Whether this navigation is initiated by a Custom Tabs {@link Intent}.
     */
    public boolean isFromCustomTabIntent() {
        return mIsCustomTabIntent;
    }

    /**
     * @return whether navigation is from a user's typing or not.
     */
    public boolean isNavigationFromUserTyping() {
        return mInitialNavigationType == NAVIGATION_TYPE_FROM_USER_TYPING;
    }

    /**
     * @return whether we should stay in Chrome or not.
     */
    public boolean shouldNotOverrideUrlLoading() {
        return mShouldNotOverrideUrlLoadingUntilNewUrlLoading;
    }

    /**
     * @return whether on navigation or not.
     */
    public boolean isOnNavigation() {
        return mInitialNavigationType != NAVIGATION_TYPE_NONE;
    }

    /**
     * @return the last committed entry index which was saved before starting this navigation.
     */
    public int getLastCommittedEntryIndexBeforeStartingNavigation() {
        return mLastCommittedEntryIndexBeforeStartingNavigation;
    }

    private static List<ComponentName> getIntentHandlers(Intent intent) {
        List<ResolveInfo> list = PackageManagerUtils.queryIntentActivities(intent, 0);
        List<ComponentName> nameList = new ArrayList<ComponentName>();
        for (ResolveInfo r : list) {
            nameList.add(new ComponentName(r.activityInfo.packageName, r.activityInfo.name));
        }
        return nameList;
    }

    /**
     * @return whether |intent| has a new resolver against |mIntentHistory| or not.
     */
    public boolean hasNewResolver(Intent intent) {
        if (mInitialIntent == null) {
            return intent != null;
        } else if (intent == null) {
            return false;
        }

        List<ComponentName> newList = getIntentHandlers(intent);
        if (mCachedResolvers.isEmpty()) {
            mCachedResolvers.addAll(getIntentHandlers(mInitialIntent));
        }
        for (ComponentName name : newList) {
            if (!mCachedResolvers.contains(name)) {
                return true;
            }
        }
        return false;
    }

    /**
     * @return The initial intent of a redirect chain, if available.
     */
    public Intent getInitialIntent() {
        return mInitialIntent;
    }
}
