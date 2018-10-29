// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.searchwidget;

import android.app.Activity;
import android.app.SearchManager;
import android.content.Intent;
import android.net.Uri;
import android.support.v4.app.ActivityOptionsCompat;
import android.text.TextUtils;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;

import org.chromium.base.Callback;
import org.chromium.base.Log;
import org.chromium.base.VisibleForTesting;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.IntentHandler;
import org.chromium.chrome.browser.WebContentsFactory;
import org.chromium.chrome.browser.WindowDelegate;
import org.chromium.chrome.browser.customtabs.CustomTabsConnection;
import org.chromium.chrome.browser.document.ChromeLauncherActivity;
import org.chromium.chrome.browser.init.AsyncInitializationActivity;
import org.chromium.chrome.browser.init.SingleWindowKeyboardVisibilityDelegate;
import org.chromium.chrome.browser.locale.LocaleManager;
import org.chromium.chrome.browser.omnibox.suggestions.AutocompleteController;
import org.chromium.chrome.browser.snackbar.SnackbarManager;
import org.chromium.chrome.browser.snackbar.SnackbarManager.SnackbarManageable;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabDelegateFactory;
import org.chromium.chrome.browser.tab.TabIdManager;
import org.chromium.chrome.browser.tabmodel.TabModel.TabLaunchType;
import org.chromium.chrome.browser.util.IntentUtils;
import org.chromium.components.url_formatter.UrlFormatter;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.common.ContentUrlConstants;
import org.chromium.ui.base.ActivityKeyboardVisibilityDelegate;
import org.chromium.ui.base.ActivityWindowAndroid;

/** Queries the user's default search engine and shows autocomplete suggestions. */
public class SearchActivity extends AsyncInitializationActivity
        implements SnackbarManageable, SearchActivityLocationBarLayout.Delegate {
    // Shared with other org.chromium.chrome.browser.searchwidget classes.
    protected static final String TAG = "searchwidget";

    /** Notified about events happening inside a SearchActivity. */
    public static class SearchActivityDelegate {
        /**
         * Called when {@link SearchActivity#triggerLayoutInflation} is deciding whether to continue
         * loading the native library immediately.
         * @return Whether or not native initialization should proceed immediately.
         */
        boolean shouldDelayNativeInitialization() {
            return false;
        }

        /**
         * Called to launch the search engine dialog if it's needed.
         * @param activity Activity that is launching the dialog.
         * @param onSearchEngineFinalized Called when the dialog has been dismissed.
         */
        void showSearchEngineDialogIfNeeded(
                Activity activity, Callback<Boolean> onSearchEngineFinalized) {
            LocaleManager.getInstance().showSearchEnginePromoIfNeeded(
                    activity, onSearchEngineFinalized);
        }

        /** Called when {@link SearchActivity#finishDeferredInitialization} is done. */
        void onFinishDeferredInitialization() {}

        /** Returning true causes the Activity to finish itself immediately when starting up. */
        boolean isActivityDisabledForTests() {
            return false;
        }
    }

    private static final Object DELEGATE_LOCK = new Object();

    /** Notified about events happening for the SearchActivity. */
    private static SearchActivityDelegate sDelegate;

    /** Main content view. */
    private ViewGroup mContentView;

    /** Whether the user is now allowed to perform searches. */
    private boolean mIsActivityUsable;

    /** Input submitted before before the native library was loaded. */
    private String mQueuedUrl;

    /** The View that represents the search box. */
    private SearchActivityLocationBarLayout mSearchBox;

    private SnackbarManager mSnackbarManager;
    private SearchBoxDataProvider mSearchBoxDataProvider;
    private Tab mTab;

    @Override
    protected boolean isStartedUpCorrectly(Intent intent) {
        if (getActivityDelegate().isActivityDisabledForTests()) return false;
        return super.isStartedUpCorrectly(intent);
    }

    @Override
    public void backKeyPressed() {
        cancelSearch();
    }

    @Override
    protected boolean shouldDelayBrowserStartup() {
        return true;
    }

    @Override
    protected ActivityWindowAndroid createWindowAndroid() {
        return new ActivityWindowAndroid(this) {
            @Override
            protected ActivityKeyboardVisibilityDelegate createKeyboardVisibilityDelegate() {
                return new SingleWindowKeyboardVisibilityDelegate(getActivity());
            }
        };
    }

    @Override
    protected void triggerLayoutInflation() {
        mSnackbarManager = new SnackbarManager(this, null);
        mSearchBoxDataProvider = new SearchBoxDataProvider();

        mContentView = createContentView();
        setContentView(mContentView);

        // Build the search box.
        mSearchBox = (SearchActivityLocationBarLayout) mContentView.findViewById(
                R.id.search_location_bar);
        mSearchBox.setDelegate(this);
        mSearchBox.setToolbarDataProvider(mSearchBoxDataProvider);
        mSearchBox.initializeControls(new WindowDelegate(getWindow()), getWindowAndroid());

        // Kick off everything needed for the user to type into the box.
        beginQuery();
        mSearchBox.getAutocompleteCoordinator().showCachedZeroSuggestResultsIfAvailable();

        // Kick off loading of the native library.
        if (!getActivityDelegate().shouldDelayNativeInitialization()) {
            mHandler.post(new Runnable() {
                @Override
                public void run() {
                    startDelayedNativeInitialization();
                }
            });
        }
        onInitialLayoutInflationComplete();
    }

    @Override
    public void finishNativeInitialization() {
        super.finishNativeInitialization();

        mTab = new Tab(TabIdManager.getInstance().generateValidId(Tab.INVALID_TAB_ID),
                Tab.INVALID_TAB_ID, false, getWindowAndroid(), TabLaunchType.FROM_EXTERNAL_APP,
                null, null);
        mTab.initialize(WebContentsFactory.createWebContents(false, false), null,
                new TabDelegateFactory(), false, false);
        mTab.loadUrl(new LoadUrlParams(ContentUrlConstants.ABOUT_BLANK_DISPLAY_URL));

        mSearchBoxDataProvider.onNativeLibraryReady(mTab);
        mSearchBox.onNativeLibraryReady();

        // Force the user to choose a search engine if they have to.
        final Callback<Boolean> onSearchEngineFinalizedCallback = new Callback<Boolean>() {
            @Override
            public void onResult(Boolean result) {
                if (isActivityDestroyed()) return;

                if (result == null || !result.booleanValue()) {
                    Log.e(TAG, "User failed to select a default search engine.");
                    finish();
                    return;
                }

                mHandler.post(new Runnable() {
                    @Override
                    public void run() {
                        finishDeferredInitialization();
                    }
                });
            }
        };
        getActivityDelegate().showSearchEngineDialogIfNeeded(
                SearchActivity.this, onSearchEngineFinalizedCallback);
    }

    private void finishDeferredInitialization() {
        assert !mIsActivityUsable
                : "finishDeferredInitialization() incorrectly called multiple times";
        mIsActivityUsable = true;
        if (mQueuedUrl != null) loadUrl(mQueuedUrl);

        AutocompleteController.nativePrefetchZeroSuggestResults();
        // TODO(tedchoc): Warmup triggers the CustomTab layout to be inflated, but this widget
        //                will navigate to Tabbed mode.  Investigate whether this can inflate
        //                the tabbed mode layout in the background instead of CCTs.
        CustomTabsConnection.getInstance().warmup(0);
        mSearchBox.onDeferredStartup(isVoiceSearchIntent());
        RecordUserAction.record("SearchWidget.WidgetSelected");

        getActivityDelegate().onFinishDeferredInitialization();
    }

    @Override
    protected View getViewToBeDrawnBeforeInitializingNative() {
        return mSearchBox;
    }

    @Override
    public void onNewIntent(Intent intent) {
        super.onNewIntent(intent);
        setIntent(intent);
        beginQuery();
    }

    @Override
    public SnackbarManager getSnackbarManager() {
        return mSnackbarManager;
    }

    private boolean isVoiceSearchIntent() {
        return IntentUtils.safeGetBooleanExtra(
                getIntent(), SearchWidgetProvider.EXTRA_START_VOICE_SEARCH, false);
    }

    private String getOptionalIntentQuery() {
        return IntentUtils.safeGetStringExtra(getIntent(), SearchManager.QUERY);
    }

    private void beginQuery() {
        mSearchBox.beginQuery(isVoiceSearchIntent(), getOptionalIntentQuery());
    }

    @Override
    protected void onDestroy() {
        if (mTab != null && mTab.isInitialized()) mTab.destroy();
        super.onDestroy();
    }

    @Override
    public boolean shouldStartGpuProcess() {
        return true;
    }

    @Override
    public void loadUrl(String url) {
        // Wait until native has loaded.
        if (!mIsActivityUsable) {
            mQueuedUrl = url;
            return;
        }

        // Don't do anything if the input was empty. This is done after the native check to prevent
        // resending a queued query after the user deleted it.
        if (TextUtils.isEmpty(url)) return;

        // Fix up the URL and send it to the full browser.
        String fixedUrl = UrlFormatter.fixupUrl(url);
        Intent intent = new Intent(Intent.ACTION_VIEW, Uri.parse(fixedUrl));
        intent.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK | Intent.FLAG_ACTIVITY_NEW_DOCUMENT);
        intent.setClass(this, ChromeLauncherActivity.class);
        IntentHandler.addTrustedIntentExtras(intent);
        IntentUtils.safeStartActivity(this, intent,
                ActivityOptionsCompat
                        .makeCustomAnimation(this, android.R.anim.fade_in, android.R.anim.fade_out)
                        .toBundle());
        RecordUserAction.record("SearchWidget.SearchMade");
        finish();
    }

    private ViewGroup createContentView() {
        assert mContentView == null;

        ViewGroup contentView = (ViewGroup) LayoutInflater.from(this).inflate(
                R.layout.search_activity, null, false);
        contentView.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                cancelSearch();
            }
        });
        return contentView;
    }

    private void cancelSearch() {
        finish();
        overridePendingTransition(0, R.anim.activity_close_exit);
    }

    @Override
    @VisibleForTesting
    public final void startDelayedNativeInitialization() {
        super.startDelayedNativeInitialization();
    }

    private static SearchActivityDelegate getActivityDelegate() {
        synchronized (DELEGATE_LOCK) {
            if (sDelegate == null) sDelegate = new SearchActivityDelegate();
        }
        return sDelegate;
    }

    /** See {@link #sDelegate}. */
    @VisibleForTesting
    static void setDelegateForTests(SearchActivityDelegate delegate) {
        sDelegate = delegate;
    }
}
