// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.searchwidget;

import android.app.Activity;
import android.app.SearchManager;
import android.content.Intent;
import android.graphics.Rect;
import android.graphics.drawable.ColorDrawable;
import android.graphics.drawable.Drawable;
import android.net.Uri;
import android.text.TextUtils;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;

import androidx.annotation.ColorInt;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;
import androidx.core.app.ActivityOptionsCompat;

import org.chromium.base.BuildInfo;
import org.chromium.base.Callback;
import org.chromium.base.IntentUtils;
import org.chromium.base.Log;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.supplier.OneshotSupplierImpl;
import org.chromium.base.supplier.UnownedUserDataSupplier;
import org.chromium.blink.mojom.DisplayMode;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.IntentHandler;
import org.chromium.chrome.browser.WebContentsFactory;
import org.chromium.chrome.browser.app.omnibox.ActionChipsDelegateImpl;
import org.chromium.chrome.browser.app.tabmodel.TabWindowManagerSingleton;
import org.chromium.chrome.browser.back_press.BackPressManager;
import org.chromium.chrome.browser.contextmenu.ContextMenuPopulatorFactory;
import org.chromium.chrome.browser.crash.ChromePureJavaExceptionReporter;
import org.chromium.chrome.browser.customtabs.CustomTabsConnection;
import org.chromium.chrome.browser.document.ChromeLauncherActivity;
import org.chromium.chrome.browser.init.AsyncInitializationActivity;
import org.chromium.chrome.browser.init.SingleWindowKeyboardVisibilityDelegate;
import org.chromium.chrome.browser.locale.LocaleManager;
import org.chromium.chrome.browser.omnibox.BackKeyBehaviorDelegate;
import org.chromium.chrome.browser.omnibox.LocationBarCoordinator;
import org.chromium.chrome.browser.omnibox.OmniboxFeatures;
import org.chromium.chrome.browser.omnibox.OverrideUrlLoadingDelegate;
import org.chromium.chrome.browser.omnibox.SearchEngineLogoUtils;
import org.chromium.chrome.browser.omnibox.UrlFocusChangeListener;
import org.chromium.chrome.browser.omnibox.suggestions.OmniboxSuggestionsDropdownScrollListener;
import org.chromium.chrome.browser.omnibox.suggestions.base.HistoryClustersProcessor.OpenHistoryClustersDelegate;
import org.chromium.chrome.browser.omnibox.voice.VoiceRecognitionHandler;
import org.chromium.chrome.browser.privacy.settings.PrivacyPreferencesManagerImpl;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabBuilder;
import org.chromium.chrome.browser.tab.TabDelegateFactory;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tab.TabWebContentsDelegateAndroid;
import org.chromium.chrome.browser.toolbar.VoiceToolbarButtonController;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager.SnackbarManageable;
import org.chromium.chrome.browser.ui.native_page.NativePage;
import org.chromium.chrome.browser.ui.searchactivityutils.SearchActivityConstants;
import org.chromium.chrome.browser.ui.system.StatusBarColorController;
import org.chromium.components.browser_ui.modaldialog.AppModalPresenter;
import org.chromium.components.browser_ui.styles.ChromeColors;
import org.chromium.components.browser_ui.util.BrowserControlsVisibilityDelegate;
import org.chromium.components.browser_ui.widget.InsetObserverView;
import org.chromium.components.browser_ui.widget.InsetObserverViewSupplier;
import org.chromium.components.external_intents.ExternalNavigationHandler;
import org.chromium.components.url_formatter.UrlFormatter;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.common.ContentUrlConstants;
import org.chromium.ui.base.ActivityWindowAndroid;
import org.chromium.ui.base.PageTransition;
import org.chromium.ui.base.WindowDelegate;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.url.GURL;

import java.lang.ref.WeakReference;

/** Queries the user's default search engine and shows autocomplete suggestions. */
public class SearchActivity extends AsyncInitializationActivity
        implements SnackbarManageable, BackKeyBehaviorDelegate, UrlFocusChangeListener,
                   OmniboxSuggestionsDropdownScrollListener {
    // Shared with other org.chromium.chrome.browser.searchwidget classes.
    protected static final String TAG = "searchwidget";

    public static final String EXTRA_FROM_SEARCH_ACTIVITY =
            "org.chromium.chrome.browser.searchwidget.FROM_SEARCH_ACTIVITY";

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
    private View mAnchorView;

    /** Whether the user is now allowed to perform searches. */
    private boolean mIsActivityUsable;

    /** Input submitted before before the native library was loaded. */
    private String mQueuedUrl;
    private @PageTransition int mQueuedTransition;
    private String mQueuedPostDataType;
    private byte[] mQueuedPostData;

    /** The View that represents the search box. */
    private SearchActivityLocationBarLayout mSearchBox;
    LocationBarCoordinator mLocationBarCoordinator;

    private SnackbarManager mSnackbarManager;
    private SearchBoxDataProvider mSearchBoxDataProvider;
    private Tab mTab;
    private ObservableSupplierImpl<Profile> mProfileSupplier = new ObservableSupplierImpl<>();
    protected final UnownedUserDataSupplier<InsetObserverView> mInsetObserverViewSupplier =
            new InsetObserverViewSupplier();

    @Override
    protected boolean isStartedUpCorrectly(Intent intent) {
        if (getActivityDelegate().isActivityDisabledForTests()) return false;
        return super.isStartedUpCorrectly(intent);
    }

    @Override
    protected boolean shouldDelayBrowserStartup() {
        return true;
    }

    @Override
    protected ActivityWindowAndroid createWindowAndroid() {
        return new ActivityWindowAndroid(this, /* listenToActivityState= */ true,
                new SingleWindowKeyboardVisibilityDelegate(new WeakReference(this)),
                getIntentRequestTracker()) {
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
        enableHardwareAcceleration();
        mSnackbarManager = new SnackbarManager(this, findViewById(android.R.id.content), null);
        mSearchBoxDataProvider = new SearchBoxDataProvider(this);
        mSearchBoxDataProvider.setIsFromQuickActionSearchWidget(isFromQuickActionSearchWidget());

        ViewGroup rootView = (ViewGroup) getWindow().getDecorView().getRootView();
        // Setting fitsSystemWindows to false ensures that the root view doesn't consume the
        // insets.
        rootView.setFitsSystemWindows(false);
        // Add a custom view right after the root view that stores the insets to access later.
        // WebContents needs the insets to determine the portion of the screen obscured by
        // non-content displaying things such as the OSK.
        mInsetObserverViewSupplier.attach(getWindowAndroid().getUnownedUserDataHost());
        mInsetObserverViewSupplier.set(InsetObserverView.create(this));
        rootView.addView(mInsetObserverViewSupplier.get(), 0);

        mContentView = createContentView();
        setContentView(mContentView);

        // Build the search box.
        mSearchBox = (SearchActivityLocationBarLayout) mContentView.findViewById(
                R.id.search_location_bar);
        mAnchorView = mContentView.findViewById(R.id.toolbar);
        updateAnchorViewLayout();

        // Create status bar color controller and assign to search activity.
        if (OmniboxFeatures.shouldMatchToolbarAndStatusBarColor()) {
            // Update the status bar's color based on the toolbar color.
            Drawable anchorViewBackground = mAnchorView.getBackground();
            if (anchorViewBackground instanceof ColorDrawable) {
                int anchorViewColor = ((ColorDrawable) anchorViewBackground).getColor();
                StatusBarColorController.setStatusBarColor(this.getWindow(), anchorViewColor);
            }
        }

        OverrideUrlLoadingDelegate overrideUrlLoadingDelegate =
                (String url, @PageTransition int transition, long inputStart, String postDataType,
                        byte[] postData, boolean incognito) -> {
            loadUrl(url, transition, postDataType, postData);
            return true;
        };

        BackPressManager backPressManager = null;
        if (BackPressManager.isEnabled() || BuildInfo.isAtLeastT()) {
            backPressManager = new BackPressManager();
            getOnBackPressedDispatcher().addCallback(this, backPressManager.getCallback());
        }
        // clang-format off
        mLocationBarCoordinator = new LocationBarCoordinator(mSearchBox, mAnchorView,
            mProfileSupplier, PrivacyPreferencesManagerImpl.getInstance(),
            mSearchBoxDataProvider, null, new WindowDelegate(getWindow()), getWindowAndroid(),
            /*activityTabSupplier=*/() -> null, getModalDialogManagerSupplier(),
            /*shareDelegateSupplier=*/null, /*incognitoStateProvider=*/null,
            getLifecycleDispatcher(), overrideUrlLoadingDelegate, /*backKeyBehavior=*/this,
            SearchEngineLogoUtils.getInstance(),
            /*pageInfoAction=*/(tab, pageInfoHighlight) -> {},
            IntentHandler::bringTabToFront,
            /*saveOfflineButtonState=*/(tab) -> false,
            /*omniboxUma*/(url, transition, isNtp) -> {},
            TabWindowManagerSingleton::getInstance, /*bookmarkState=*/(url) -> false,
            VoiceToolbarButtonController::isToolbarMicEnabled,
            /*merchantTrustSignalsCoordinatorSupplier=*/null,
            new ActionChipsDelegateImpl(this, new OneshotSupplierImpl<>(),
                getModalDialogManagerSupplier(), () -> null), null,
            ChromePureJavaExceptionReporter::reportJavaException, backPressManager,
            /*OmniboxSuggestionsDropdownScrollListener=*/this,
            new OpenHistoryClustersDelegate() {
                @Override
                public void openHistoryClustersUi(String query) {}
            });
        // clang-format on
        mLocationBarCoordinator.setUrlBarFocusable(true);
        mLocationBarCoordinator.setShouldShowMicButtonWhenUnfocused(true);
        mLocationBarCoordinator.getOmniboxStub().addUrlFocusChangeListener(this);

        // Kick off everything needed for the user to type into the box.
        beginQuery();

        // Kick off loading of the native library.
        if (!getActivityDelegate().shouldDelayNativeInitialization()) {
            mHandler.post(this::startDelayedNativeInitialization);
        }

        onInitialLayoutInflationComplete();
    }

    @Override
    public void finishNativeInitialization() {
        Profile profile = Profile.getLastUsedRegularProfile();
        mProfileSupplier.set(profile);

        super.finishNativeInitialization();

        TabDelegateFactory factory = new TabDelegateFactory() {
            @Override
            public TabWebContentsDelegateAndroid createWebContentsDelegate(Tab tab) {
                return new TabWebContentsDelegateAndroid() {
                    @Override
                    public int getDisplayMode() {
                        return DisplayMode.BROWSER;
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
            public ContextMenuPopulatorFactory createContextMenuPopulatorFactory(Tab tab) {
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

        WebContents webContents = WebContentsFactory.createWebContents(profile, false, false);
        mTab = new TabBuilder()
                       .setWindow(getWindowAndroid())
                       .setLaunchType(TabLaunchType.FROM_EXTERNAL_APP)
                       .setWebContents(webContents)
                       .setDelegateFactory(factory)
                       .build();
        mTab.loadUrl(new LoadUrlParams(ContentUrlConstants.ABOUT_BLANK_DISPLAY_URL));

        mSearchBoxDataProvider.onNativeLibraryReady(mTab);

        // Force the user to choose a search engine if they have to.
        final Callback<Boolean> onSearchEngineFinalizedCallback = (result) -> {
            if (isActivityFinishingOrDestroyed()) return;

            if (result == null || !result.booleanValue()) {
                Log.e(TAG, "User failed to select a default search engine.");
                finish();
                return;
            }

            mHandler.post(this::finishDeferredInitialization);
        };
        getActivityDelegate().showSearchEngineDialogIfNeeded(
                SearchActivity.this, onSearchEngineFinalizedCallback);
    }

    // OverrideBackKeyBehaviorDelegate implementation.
    @Override
    public boolean handleBackKeyPressed() {
        cancelSearch();
        return true;
    }

    private void finishDeferredInitialization() {
        assert !mIsActivityUsable
                : "finishDeferredInitialization() incorrectly called multiple times";
        mIsActivityUsable = true;
        if (mQueuedUrl != null) {
            loadUrl(mQueuedUrl, mQueuedTransition, mQueuedPostDataType, mQueuedPostData);
        }

        // TODO(tedchoc): Warmup triggers the CustomTab layout to be inflated, but this widget
        //                will navigate to Tabbed mode.  Investigate whether this can inflate
        //                the tabbed mode layout in the background instead of CCTs.
        CustomTabsConnection.getInstance().warmup(0);
        VoiceRecognitionHandler voiceRecognitionHandler =
                mLocationBarCoordinator.getVoiceRecognitionHandler();
        @SearchType
        int searchType = getSearchType(getIntent().getAction());
        if (isFromQuickActionSearchWidget()) {
            recordQuickActionSearchType(searchType);
        }
        mSearchBox.onDeferredStartup(searchType, voiceRecognitionHandler, getWindowAndroid());
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
        mSearchBoxDataProvider.setIsFromQuickActionSearchWidget(isFromQuickActionSearchWidget());
        beginQuery();
    }

    @Override
    public void onPause() {
        super.onPause();
        // Make sure that re-entering the SearchActivity from different widgets shows appropriate
        // suggestion types.
        mLocationBarCoordinator.clearOmniboxFocus();
    }

    @Override
    public void onResume() {
        super.onResume();
        mSearchBox.focusTextBox();
    }

    @Override
    public SnackbarManager getSnackbarManager() {
        return mSnackbarManager;
    }

    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    static @SearchType int getSearchType(String action) {
        if (TextUtils.equals(action, SearchActivityConstants.ACTION_START_VOICE_SEARCH)
                || TextUtils.equals(
                        action, SearchActivityConstants.ACTION_START_EXTENDED_VOICE_SEARCH)) {
            return SearchType.VOICE;
        } else if (TextUtils.equals(action, SearchActivityConstants.ACTION_START_LENS_SEARCH)) {
            return SearchType.LENS;
        } else {
            return SearchType.TEXT;
        }
    }

    private boolean isFromSearchWidget() {
        return IntentUtils.safeGetBooleanExtra(
                getIntent(), SearchWidgetProvider.EXTRA_FROM_SEARCH_WIDGET, false);
    }

    private boolean isFromQuickActionSearchWidget() {
        return IntentUtils.safeGetBooleanExtra(getIntent(),
                SearchActivityConstants.EXTRA_BOOLEAN_FROM_QUICK_ACTION_SEARCH_WIDGET, false);
    }

    private String getOptionalIntentQuery() {
        return IntentUtils.safeGetStringExtra(getIntent(), SearchManager.QUERY);
    }

    private void beginQuery() {
        @SearchType
        int searchType = getSearchType(getIntent().getAction());
        if (isFromQuickActionSearchWidget()) {
            recordQuickActionSearchType(searchType);
        }
        mSearchBox.beginQuery(searchType, getOptionalIntentQuery(),
                mLocationBarCoordinator.getVoiceRecognitionHandler(), getWindowAndroid());
    }

    @Override
    protected void onDestroy() {
        if (mTab != null && mTab.isInitialized()) mTab.destroy();
        if (mLocationBarCoordinator != null && mLocationBarCoordinator.getOmniboxStub() != null) {
            mLocationBarCoordinator.getOmniboxStub().removeUrlFocusChangeListener(this);
            mLocationBarCoordinator.destroy();
            mLocationBarCoordinator = null;
        }
        mHandler.removeCallbacksAndMessages(null);
        super.onDestroy();
    }

    @Override
    public boolean shouldStartGpuProcess() {
        return true;
    }

    @Override
    public void onUrlFocusChange(boolean hasFocus) {
        if (hasFocus) {
            mLocationBarCoordinator.setUrlFocusChangeInProgress(false);
        }
    }

    /* package */ void loadUrl(String url, @PageTransition int transition,
            @Nullable String postDataType, @Nullable byte[] postData) {
        // Wait until native has loaded.
        if (!mIsActivityUsable) {
            mQueuedUrl = url;
            mQueuedTransition = transition;
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
        LocaleManager.getInstance().recordLocaleBasedSearchMetrics(true, url, transition);
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
        if (isFromSearchWidget()) {
            intent.putExtra(SearchWidgetProvider.EXTRA_FROM_SEARCH_WIDGET, true);
        }
        intent.putExtra(EXTRA_FROM_SEARCH_ACTIVITY, true);
        IntentUtils.addTrustedIntentExtras(intent);

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

        if (OmniboxFeatures.shouldShowModernizeVisualUpdate(this)) {
            View toolbarView = contentView.findViewById(R.id.toolbar);
            final int edgePadding =
                    getResources().getDimensionPixelOffset(R.dimen.toolbar_edge_padding_modern);
            toolbarView.setPaddingRelative(edgePadding, toolbarView.getPaddingTop(), edgePadding,
                    toolbarView.getPaddingBottom());
            toolbarView.setBackground(new ColorDrawable(ChromeColors.getSurfaceColor(
                    this, R.dimen.omnibox_suggestion_dropdown_bg_elevation)));
        }
        return contentView;
    }

    private void cancelSearch() {
        finish();
        overridePendingTransition(0, R.anim.activity_close_exit);
    }

    private static void recordQuickActionSearchType(@SearchType int searchType) {
        if (searchType == SearchType.VOICE) {
            RecordUserAction.record("QuickActionSearchWidget.VoiceQuery");
        } else if (searchType == SearchType.LENS) {
            RecordUserAction.record("QuickActionSearchWidget.LensQuery");
        } else if (searchType == SearchType.TEXT) {
            RecordUserAction.record("QuickActionSearchWidget.TextQuery");
        }
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

    @VisibleForTesting
    public View getAnchorViewForTesting() {
        return mAnchorView;
    }

    LocationBarCoordinator getLocationBarCoordinatorForTesting() {
        return mLocationBarCoordinator;
    }

    /**
     * Increase the toolbar vertical height and bottom padding if the omnibox phase 2 feature is
     * enabled.
     */
    private void updateAnchorViewLayout() {
        if (!OmniboxFeatures.shouldShowModernizeVisualUpdate(mAnchorView.getContext())) {
            return;
        }

        var layoutParams = mAnchorView.getLayoutParams();
        int heightIncrease = getResources().getDimensionPixelSize(
                OmniboxFeatures.shouldShowActiveColorOnOmnibox()
                        ? R.dimen.toolbar_url_focus_height_increase_active_color
                        : R.dimen.toolbar_url_focus_height_increase_no_active_color);
        layoutParams.height = getResources().getDimensionPixelSize(R.dimen.toolbar_height_no_shadow)
                + heightIncrease;
        mAnchorView.setLayoutParams(layoutParams);

        // Apply extra bottom padding for no active-color treatments.
        if (!OmniboxFeatures.shouldShowActiveColorOnOmnibox()) {
            int bottomPadding =
                    getResources().getDimensionPixelSize(R.dimen.toolbar_url_focus_bottom_padding);
            mAnchorView.setPaddingRelative(mAnchorView.getPaddingStart(),
                    mAnchorView.getPaddingTop(), mAnchorView.getPaddingEnd(), bottomPadding);
        }
    }

    /**
     * Apply the color to locationbar's and toolbar's background.
     */
    private void applyColor(@ColorInt int color) {
        if (!OmniboxFeatures.shouldShowModernizeVisualUpdate(SearchActivity.this)
                || OmniboxFeatures.shouldShowActiveColorOnOmnibox()) {
            return;
        }

        Drawable locationbarBackground =
                mContentView.findViewById(R.id.search_location_bar).getBackground();
        Drawable toolbarBackground = mContentView.findViewById(R.id.toolbar).getBackground();
        locationbarBackground.setTint(color);
        toolbarBackground.setTint(color);

        if (OmniboxFeatures.shouldMatchToolbarAndStatusBarColor()) {
            StatusBarColorController.setStatusBarColor(this.getWindow(), color);
        }
    }

    @Override
    public void onSuggestionDropdownScroll() {
        applyColor(ChromeColors.getSurfaceColor(
                SearchActivity.this, R.dimen.toolbar_text_box_elevation));
    }

    @Override
    public void onSuggestionDropdownOverscrolledToTop() {
        applyColor(ChromeColors.getSurfaceColor(
                SearchActivity.this, R.dimen.omnibox_suggestion_dropdown_bg_elevation));
    }
}
