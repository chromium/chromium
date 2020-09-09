// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.searchwidget;

import android.app.Activity;
import android.app.SearchManager;
import android.content.Intent;
import android.graphics.Rect;
import android.net.Uri;
import android.text.TextUtils;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;

import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;
import androidx.core.app.ActivityOptionsCompat;

import org.chromium.base.Callback;
import org.chromium.base.IntentUtils;
import org.chromium.base.Log;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.IntentHandler;
import org.chromium.chrome.browser.WebContentsFactory;
import org.chromium.chrome.browser.WindowDelegate;
import org.chromium.chrome.browser.contextmenu.ContextMenuPopulator;
import org.chromium.chrome.browser.customtabs.CustomTabsConnection;
import org.chromium.chrome.browser.document.ChromeLauncherActivity;
import org.chromium.chrome.browser.init.AsyncInitializationActivity;
import org.chromium.chrome.browser.init.SingleWindowKeyboardVisibilityDelegate;
import org.chromium.chrome.browser.locale.LocaleManager;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabBuilder;
import org.chromium.chrome.browser.tab.TabDelegateFactory;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tab.TabWebContentsDelegateAndroid;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager.SnackbarManageable;
import org.chromium.chrome.browser.ui.native_page.NativePage;
import org.chromium.chrome.browser.webapps.WebDisplayMode;
import org.chromium.components.browser_ui.modaldialog.AppModalPresenter;
import org.chromium.components.browser_ui.util.BrowserControlsVisibilityDelegate;
import org.chromium.components.external_intents.ExternalNavigationHandler;
import org.chromium.components.url_formatter.UrlFormatter;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.common.ContentUrlConstants;
import org.chromium.ui.base.ActivityKeyboardVisibilityDelegate;
import org.chromium.ui.base.ActivityWindowAndroid;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.url.GURL;

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
    private String mQueuedPostDataType;
    private byte[] mQueuedPostData;

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
            @Override
            public ModalDialogManager getModalDialogManager() {
                return SearchActivity.this.getModalDialogManager();
            }
        };
    }

    @Override
    protected ModalDialogManager createModalDialogManager() {
        return new ModalDialogManager(
                new AppModalPresenter(this), ModalDialogManager.ModalDialogType.APP);
    }

    @Override
    protected void triggerLayoutInflation() {
        mSnackbarManager = new SnackbarManager(this, findViewById(android.R.id.content), null);
        mSearchBoxDataProvider = new SearchBoxDataProvider(getResources());

        mContentView = createContentView();
        setContentView(mContentView);

        // Build the search box.
        mSearchBox = (SearchActivityLocationBarLayout) mContentView.findViewById(
                R.id.search_location_bar);
        mSearchBox.setDelegate(this);
        mSearchBox.setToolbarDataProvider(mSearchBoxDataProvider);
        mSearchBox.initializeControls(
                new WindowDelegate(getWindow()), getWindowAndroid(), null, null, null, null);

        // Kick off everything needed for the user to type into the box.
        beginQuery();

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

        TabDelegateFactory factory = new TabDelegateFactory() {
            @Override
            public TabWebContentsDelegateAndroid createWebContentsDelegate(Tab tab) {
                return new TabWebContentsDelegateAndroid() {
                    @Override
                    protected int getDisplayMode() {
                        return WebDisplayMode.BROWSER;
                    }

                    @Override
                    protected boolean shouldResumeRequestsForCreatedWindow() {
                        return false;
                    }

                    @Override
                    protected boolean addNewContents(WebContents sourceWebContents,
                            WebContents webContents, int disposition, Rect initialPosition,
                            boolean userGesture) {
                        return false;
                    }

                    @Override
                    protected void setOverlayMode(boolean useOverlayMode) {}

                    @Override
                    public boolean canShowAppBanners() {
                        return false;
                    }
                };
            }

            @Override
            public ExternalNavigationHandler createExternalNavigationHandler(Tab tab) {
                return null;
            }

            @Override
            public ContextMenuPopulator createContextMenuPopulator(Tab tab) {
                return null;
            }

            @Override
            public BrowserControlsVisibilityDelegate createBrowserControlsVisibilityDelegate(
                    Tab tab) {
                return null;
            }

            @Override
            public NativePage createNativePage(String url, NativePage candidatePage, Tab tab) {
                // SearchActivity does not create native pages.
                return null;
            }
        };
        mTab = new TabBuilder()
                       .setWindow(getWindowAndroid())
                       .setLaunchType(TabLaunchType.FROM_EXTERNAL_APP)
                       .setWebContents(WebContentsFactory.createWebContents(false, false))
                       .setDelegateFactory(factory)
                       .build();
        mTab.loadUrl(new LoadUrlParams(ContentUrlConstants.ABOUT_BLANK_DISPLAY_URL));

        mSearchBoxDataProvider.onNativeLibraryReady(mTab);
        mSearchBox.onNativeLibraryReady();

        // Force the user to choose a search engine if they have to.
        final Callback<Boolean> onSearchEngineFinalizedCallback = new Callback<Boolean>() {
            @Override
            public void onResult(Boolean result) {
                if (isActivityFinishingOrDestroyed()) return;

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
        if (mQueuedUrl != null) loadUrl(mQueuedUrl, mQueuedPostDataType, mQueuedPostData);

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
    public void loadUrl(String url, @Nullable String postDataType, @Nullable byte[] postData) {
        // Wait until native has loaded.
        if (!mIsActivityUsable) {
            mQueuedUrl = url;
            mQueuedPostDataType = postDataType;
            mQueuedPostData = postData;
            return;
        }

        Intent intent = createIntentForStartActivity(url, postDataType, postData);
        if (intent == null) return;

        IntentUtils.safeStartActivity(this, intent,
                ActivityOptionsCompat
                        .makeCustomAnimation(this, android.R.anim.fade_in, android.R.anim.fade_out)
                        .toBundle());
        RecordUserAction.record("SearchWidget.SearchMade");
        finish();
    }

    /**
     * Creates an intent that will be used to launch Chrome.
     *
     * @param url The URL to be loaded.
     * @param postDataType   postData type.
     * @param postData       Post-data to include in the tab URL's request body, ex. bitmap when
     *         image search.
     * @return the intent will be passed to ChromeLauncherActivity, null if input was emprty.
     */
    private Intent createIntentForStartActivity(
            String url, @Nullable String postDataType, @Nullable byte[] postData) {
        // Don't do anything if the input was empty. This is done after the native check to prevent
        // resending a queued query after the user deleted it.
        if (TextUtils.isEmpty(url)) return null;

        // Fix up the URL and send it to the full browser.
        GURL fixedUrl = UrlFormatter.fixupUrl(url);
        Intent intent = new Intent(Intent.ACTION_VIEW, Uri.parse(fixedUrl.getValidSpecOrEmpty()));
        intent.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK | Intent.FLAG_ACTIVITY_NEW_DOCUMENT);
        intent.setClass(this, ChromeLauncherActivity.class);
        if (!TextUtils.isEmpty(postDataType) && postData != null && postData.length != 0) {
            intent.putExtra(IntentHandler.EXTRA_POST_DATA_TYPE, postDataType);
            intent.putExtra(IntentHandler.EXTRA_POST_DATA, postData);
        }
        IntentHandler.addTrustedIntentExtras(intent);

        return intent;
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
