// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp;

import android.content.Context;
import android.graphics.Canvas;
import android.graphics.Point;
import android.graphics.Rect;
import android.graphics.drawable.Drawable;
import android.support.v4.view.ViewCompat;
import android.support.v7.widget.RecyclerView;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;

import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.base.DiscardableReferencePool;
import org.chromium.base.Log;
import org.chromium.base.TimeUtils;
import org.chromium.base.TraceEvent;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ActivityTabProvider;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.GlobalDiscardableReferencePool;
import org.chromium.chrome.browser.compositor.layouts.content.InvalidationAwareThumbnailProvider;
import org.chromium.chrome.browser.download.DownloadManagerService;
import org.chromium.chrome.browser.fullscreen.ChromeFullscreenManager;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.lifecycle.LifecycleObserver;
import org.chromium.chrome.browser.lifecycle.PauseResumeWithNativeObserver;
import org.chromium.chrome.browser.native_page.NativePage;
import org.chromium.chrome.browser.native_page.NativePageHost;
import org.chromium.chrome.browser.ntp.NewTabPageView.NewTabPageManager;
import org.chromium.chrome.browser.ntp.cards.ItemViewType;
import org.chromium.chrome.browser.ntp.cards.NewTabPageAdapter;
import org.chromium.chrome.browser.ntp.snippets.SuggestionsSource;
import org.chromium.chrome.browser.omnibox.LocationBar.OmniboxFocusReason;
import org.chromium.chrome.browser.omnibox.LocationBarVoiceRecognitionHandler;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.search_engines.TemplateUrlServiceFactory;
import org.chromium.chrome.browser.snackbar.SnackbarManager;
import org.chromium.chrome.browser.suggestions.SuggestionsDependencyFactory;
import org.chromium.chrome.browser.suggestions.SuggestionsEventReporter;
import org.chromium.chrome.browser.suggestions.SuggestionsMetrics;
import org.chromium.chrome.browser.suggestions.SuggestionsNavigationDelegate;
import org.chromium.chrome.browser.suggestions.SuggestionsUiDelegateImpl;
import org.chromium.chrome.browser.suggestions.tile.Tile;
import org.chromium.chrome.browser.suggestions.tile.TileGroup;
import org.chromium.chrome.browser.suggestions.tile.TileGroupDelegateImpl;
import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.Tab.TabHidingType;
import org.chromium.chrome.browser.tab.TabBrowserControlsState;
import org.chromium.chrome.browser.tab.TabObserver;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabSelectionType;
import org.chromium.chrome.browser.util.UrlConstants;
import org.chromium.chrome.browser.util.UrlUtilities;
import org.chromium.chrome.browser.vr.VrModuleProvider;
import org.chromium.components.search_engines.TemplateUrlService.TemplateUrlServiceObserver;
import org.chromium.content_public.browser.NavigationController;
import org.chromium.content_public.browser.NavigationEntry;
import org.chromium.content_public.common.BrowserControlsState;
import org.chromium.ui.mojom.WindowOpenDisposition;

import java.net.URI;
import java.net.URISyntaxException;
import java.util.List;

/**
 * Provides functionality when the user interacts with the NTP.
 */
public class NewTabPage implements NativePage, InvalidationAwareThumbnailProvider,
                                   TemplateUrlServiceObserver,
                                   ChromeFullscreenManager.FullscreenListener {
    private static final String TAG = "NewTabPage";

    // Key for the scroll position data that may be stored in a navigation entry.
    private static final String NAVIGATION_ENTRY_SCROLL_POSITION_KEY = "NewTabPageScrollPosition";
    public static final String CONTEXT_MENU_USER_ACTION_PREFIX = "Suggestions";

    protected final Tab mTab;
    private final ActivityTabProvider mActivityTabProvider;
    private final ActivityLifecycleDispatcher mActivityLifecycleDispatcher;

    private final String mTitle;
    private final int mBackgroundColor;
    protected final NewTabPageManagerImpl mNewTabPageManager;
    protected final TileGroup.Delegate mTileGroupDelegate;
    private final boolean mIsTablet;
    private final ChromeFullscreenManager mFullscreenManager;

    /**
     * The {@link NewTabPageView} shown in this NewTabPageLayout. This may be null in sub-classes.
     */
    private @Nullable NewTabPageView mNewTabPageView;
    protected NewTabPageLayout mNewTabPageLayout;
    private TabObserver mTabObserver;
    private LifecycleObserver mLifecycleObserver;
    protected boolean mSearchProviderHasLogo;

    protected FakeboxDelegate mFakeboxDelegate;
    private LocationBarVoiceRecognitionHandler mVoiceRecognitionHandler;

    // The timestamp at which the constructor was called.
    protected final long mConstructedTimeNs;

    // The timestamp at which this NTP was last shown to the user.
    private long mLastShownTimeNs;

    private boolean mIsLoaded;

    // Whether destroy() has been called.
    private boolean mIsDestroyed;

    @Override
    public void onContentOffsetChanged(int offset) {}

    @Override
    public void onControlsOffsetChanged(int topOffset, int bottomOffset, boolean needsAnimate) {}

    @Override
    public void onToggleOverlayVideoMode(boolean enabled) {}

    @Override
    public void onBottomControlsHeightChanged(int bottomControlsHeight) {
        updateMargins();
    }

    /**
     * Allows clients to listen for updates to the scroll changes of the search box on the
     * NTP.
     */
    public interface OnSearchBoxScrollListener {
        /**
         * Callback to be notified when the scroll position of the search box on the NTP has
         * changed.  A scroll percentage of 0, means the search box has no scroll applied and
         * is in it's natural resting position.  A value of 1 means the search box is scrolled
         * entirely to the top of the screen viewport.
         *
         * @param scrollPercentage The percentage the search box has been scrolled off the page.
         */
        void onNtpScrollChanged(float scrollPercentage);
    }

    /**
     * @param url The URL to check whether it is for the NTP.
     * @return Whether the passed in URL is used to render the NTP.
     */
    public static boolean isNTPUrl(String url) {
        // Also handle the legacy chrome://newtab and about:newtab URLs since they will redirect to
        // chrome-native://newtab natively.
        if (url == null) return false;
        try {
            // URL().getProtocol() throws MalformedURLException if the scheme is "invalid",
            // including common ones like "about:", so it's not usable for isInternalScheme().
            URI uri = new URI(url);
            if (!UrlUtilities.isInternalScheme(uri)) return false;

            String host = uri.getHost();
            if (host == null) {
                // "about:newtab" would lead to null host.
                uri = new URI(uri.getScheme() + "://" + uri.getSchemeSpecificPart());
                host = uri.getHost();
            }
            return UrlConstants.NTP_HOST.equals(host);
        } catch (URISyntaxException e) {
            return false;
        }
    }

    protected class NewTabPageManagerImpl
            extends SuggestionsUiDelegateImpl implements NewTabPageManager {
        public NewTabPageManagerImpl(SuggestionsSource suggestionsSource,
                SuggestionsEventReporter eventReporter,
                SuggestionsNavigationDelegate navigationDelegate, Profile profile,
                NativePageHost nativePageHost, DiscardableReferencePool referencePool,
                SnackbarManager snackbarManager) {
            super(suggestionsSource, eventReporter, navigationDelegate, profile, nativePageHost,
                    referencePool, snackbarManager);
        }

        @Override
        public boolean isLocationBarShownInNTP() {
            if (mIsDestroyed) return false;
            return isInSingleUrlBarMode() && !mNewTabPageLayout.urlFocusAnimationsDisabled();
        }

        @Override
        public boolean isVoiceSearchEnabled() {
            return mVoiceRecognitionHandler != null
                    && mVoiceRecognitionHandler.isVoiceSearchEnabled();
        }

        @Override
        public void focusSearchBox(boolean beginVoiceSearch, String pastedText) {
            if (mIsDestroyed) return;
            if (VrModuleProvider.getDelegate().isInVr()) return;
            if (mVoiceRecognitionHandler != null && beginVoiceSearch) {
                mVoiceRecognitionHandler.startVoiceRecognition(
                        LocationBarVoiceRecognitionHandler.VoiceInteractionSource.NTP);
            } else if (mFakeboxDelegate != null) {
                mFakeboxDelegate.setUrlBarFocus(true, pastedText,
                        pastedText == null ? OmniboxFocusReason.FAKE_BOX_TAP
                                           : OmniboxFocusReason.FAKE_BOX_LONG_PRESS);
            }
        }

        @Override
        public boolean isCurrentPage() {
            if (mIsDestroyed) return false;
            if (mFakeboxDelegate == null) return false;
            return mFakeboxDelegate.isCurrentPage(NewTabPage.this);
        }

        @Override
        public void onLoadingComplete() {
            if (mIsDestroyed) return;

            long loadTimeMs = (System.nanoTime() - mConstructedTimeNs) / 1000000;
            RecordHistogram.recordTimesHistogram("Tab.NewTabOnload", loadTimeMs);
            mIsLoaded = true;
            NewTabPageUma.recordNTPImpression(NewTabPageUma.NTP_IMPRESSION_REGULAR);
            // If not visible when loading completes, wait until onShown is received.
            if (!mTab.isHidden()) recordNTPShown();
        }
    }

    /**
     * Extends {@link TileGroupDelegateImpl} to add metrics logging that is specific to
     * {@link NewTabPage}.
     */
    private class NewTabPageTileGroupDelegate extends TileGroupDelegateImpl {
        private NewTabPageTileGroupDelegate(ChromeActivity activity, Profile profile,
                SuggestionsNavigationDelegate navigationDelegate) {
            super(activity, profile, navigationDelegate, activity.getSnackbarManager());
        }

        @Override
        public void onLoadingComplete(List<Tile> tiles) {
            if (mIsDestroyed) return;

            super.onLoadingComplete(tiles);
            mNewTabPageLayout.onTilesLoaded();
        }

        @Override
        public void openMostVisitedItem(int windowDisposition, Tile tile) {
            if (mIsDestroyed) return;

            super.openMostVisitedItem(windowDisposition, tile);

            if (windowDisposition != WindowOpenDisposition.NEW_WINDOW) {
                RecordHistogram.recordMediumTimesHistogram("NewTabPage.MostVisitedTime",
                        (System.nanoTime() - mLastShownTimeNs)
                                / TimeUtils.NANOSECONDS_PER_MILLISECOND);
            }
        }
    }

    /**
     * Constructs a NewTabPage.
     * @param activity The activity used for context to create the new tab page's View.
     * @param nativePageHost The host that is showing this new tab page.
     * @param tabModelSelector The TabModelSelector used to open tabs.
     * @param activityTabProvider Allows us to check if we are the current tab.
     * @param activityLifecycleDispatcher Allows us to subscribe to backgrounding events.
     */
    public NewTabPage(ChromeActivity activity, NativePageHost nativePageHost,
            TabModelSelector tabModelSelector, ActivityTabProvider activityTabProvider,
            ActivityLifecycleDispatcher activityLifecycleDispatcher) {
        mConstructedTimeNs = System.nanoTime();
        TraceEvent.begin(TAG);

        mActivityTabProvider = activityTabProvider;
        mActivityLifecycleDispatcher = activityLifecycleDispatcher;
        mTab = nativePageHost.getActiveTab();
        Profile profile = mTab.getProfile();

        SuggestionsDependencyFactory depsFactory = SuggestionsDependencyFactory.getInstance();
        SuggestionsSource suggestionsSource = depsFactory.createSuggestionSource(profile);
        SuggestionsEventReporter eventReporter = depsFactory.createEventReporter();

        SuggestionsNavigationDelegate navigationDelegate = new SuggestionsNavigationDelegate(
                activity, profile, nativePageHost, tabModelSelector);
        mNewTabPageManager = new NewTabPageManagerImpl(suggestionsSource, eventReporter,
                navigationDelegate, profile, nativePageHost,
                GlobalDiscardableReferencePool.getReferencePool(), activity.getSnackbarManager());
        mTileGroupDelegate = new NewTabPageTileGroupDelegate(activity, profile, navigationDelegate);

        mTitle = activity.getResources().getString(R.string.button_new_tab);
        mBackgroundColor = ApiCompatibilityUtils.getColor(
                activity.getResources(), R.color.modern_primary_color);
        mIsTablet = activity.isTablet();
        TemplateUrlServiceFactory.get().addObserver(this);

        mTabObserver = new EmptyTabObserver() {
            @Override
            public void onShown(Tab tab, @TabSelectionType int type) {
                // Showing the NTP is only meaningful when the page has been loaded already.
                if (mIsLoaded) recordNTPShown();

                mNewTabPageLayout.getTileGroup().onSwitchToForeground(/* trackLoadTask = */ false);
            }

            @Override
            public void onHidden(Tab tab, @TabHidingType int type) {
                if (mIsLoaded) recordNTPHidden();
            }

            @Override
            public void onPageLoadStarted(Tab tab, String url) {
                saveLastScrollPosition();
            }

            @Override
            public void onBrowserControlsConstraintsUpdated(Tab tab, int constraints) {
                updateMargins();
            }
        };
        mTab.addObserver(mTabObserver);

        mLifecycleObserver = new PauseResumeWithNativeObserver() {
            @Override
            public void onResumeWithNative() {}

            @Override
            public void onPauseWithNative() {
                // Only record when this tab is the current tab.
                if (mActivityTabProvider.get() == mTab) {
                    RecordUserAction.record("MobileNTPPaused");
                }
            }
        };
        mActivityLifecycleDispatcher.register(mLifecycleObserver);

        updateSearchProviderHasLogo();
        initializeMainView(activity, nativePageHost);

        mFullscreenManager = activity.getFullscreenManager();
        getView().addOnAttachStateChangeListener(new View.OnAttachStateChangeListener() {
            @Override
            public void onViewAttachedToWindow(View view) {
                updateMargins();
                getView().removeOnAttachStateChangeListener(this);
            }

            @Override
            public void onViewDetachedFromWindow(View view) {}
        });
        mFullscreenManager.addListener(this);

        eventReporter.onSurfaceOpened();

        DownloadManagerService.getDownloadManagerService().checkForExternallyRemovedDownloads(
                /*isOffTheRecord=*/false);

        NewTabPageUma.recordIsUserOnline();
        NewTabPageUma.recordLoadType(activity);
        NewTabPageUma.recordContentSuggestionsDisplayStatus();
        TraceEvent.end(TAG);
    }

    /**
     * Create and initialize the main view contained in this NewTabPage.
     * @param context The context used to inflate the view.
     * @param host NativePageHost used for initialization.
     */
    protected void initializeMainView(Context context, NativePageHost host) {
        LayoutInflater inflater = LayoutInflater.from(context);
        mNewTabPageView = (NewTabPageView) inflater.inflate(R.layout.new_tab_page_view, null);
        mNewTabPageLayout = mNewTabPageView.getNewTabPageLayout();

        mNewTabPageView.initialize(mNewTabPageManager, mTab, mTileGroupDelegate,
                mSearchProviderHasLogo,
                TemplateUrlServiceFactory.get().isDefaultSearchEngineGoogle(),
                getScrollPositionFromNavigationEntry(NAVIGATION_ENTRY_SCROLL_POSITION_KEY, mTab),
                mConstructedTimeNs);
    }

    /**
     * Save the last scroll position stored in the navigation entry if necessary.
     */
    protected void saveLastScrollPosition() {
        int scrollPosition = mNewTabPageView.getScrollPosition();
        if (scrollPosition == RecyclerView.NO_POSITION) return;
        saveStringToNavigationEntry(
                mTab, NAVIGATION_ENTRY_SCROLL_POSITION_KEY, Integer.toString(scrollPosition));
    }

    /**
     * Saves a single string under a given key to the navigation entry. It is up to the caller to
     * extract and interpret later.
     * @param tab A tab that is used to access the NavigationController and the NavigationEntry
     *            extras.
     * @param key The key to store the data under, will need to be used to access later.
     * @param value The payload to persist.
     *
     * TODO(https://crbug.com/941581): Refactor this to be reusable across NativePage components.
     */
    public static void saveStringToNavigationEntry(Tab tab, String key, String value) {
        if (tab.getWebContents() == null) return;
        NavigationController controller = tab.getWebContents().getNavigationController();
        int index = controller.getLastCommittedEntryIndex();
        NavigationEntry entry = controller.getEntryAtIndex(index);
        if (entry == null) return;

        // At least under test conditions this method may be called initially for the load of the
        // NTP itself, at which point the last committed entry is not for the NTP yet. This method
        // will then be called a second time when the user navigates away, at which point the last
        // committed entry is for the NTP. The extra data must only be set in the latter case.
        if (!isNTPUrl(entry.getUrl())) return;

        controller.setEntryExtraData(index, key, value);
    }

    /**
     * Update the margins for the content when browser controls constraints or bottom control
     *  height are changed.
     */
    private void updateMargins() {
        // TODO(mdjones): can this be merged with BasicNativePage's updateMargins?

        View view = getView();
        ViewGroup.MarginLayoutParams layoutParams =
                ((ViewGroup.MarginLayoutParams) view.getLayoutParams());
        if (layoutParams == null) return;

        final @BrowserControlsState int constraints = TabBrowserControlsState.getConstraints(mTab);
        layoutParams.bottomMargin = (constraints != BrowserControlsState.HIDDEN)
                ? mFullscreenManager.getBottomControlsHeight()
                : 0;

        view.setLayoutParams(layoutParams);

        // Apply negative margin to the top of the N logo (which would otherwise be the height of
        // the top toolbar) when Duet is enabled to remove some of the empty space.
        mNewTabPageLayout.setSearchProviderTopMargin((layoutParams.bottomMargin == 0)
                        ? view.getResources().getDimensionPixelSize(R.dimen.ntp_logo_margin_top)
                        : -view.getResources().getDimensionPixelSize(
                                R.dimen.duet_ntp_logo_top_margin));
    }

    /** @return The view container for the new tab page. */
    @VisibleForTesting
    public NewTabPageView getNewTabPageView() {
        return mNewTabPageView;
    }

    /** @return The view container for the new tab layout. */
    @VisibleForTesting
    public NewTabPageLayout getNewTabPageLayout() {
        return mNewTabPageLayout;
    }

    /**
     * Updates whether the NewTabPage should animate on URL focus changes.
     * @param disable Whether to disable the animations.
     */
    public void setUrlFocusAnimationsDisabled(boolean disable) {
        mNewTabPageLayout.setUrlFocusAnimationsDisabled(disable);
    }

    private boolean isInSingleUrlBarMode() {
        return !mIsTablet && mSearchProviderHasLogo;
    }

    private void updateSearchProviderHasLogo() {
        mSearchProviderHasLogo = TemplateUrlServiceFactory.doesDefaultSearchEngineHaveLogo();
    }

    private void onSearchEngineUpdated() {
        updateSearchProviderHasLogo();
        setSearchProviderInfoOnView(mSearchProviderHasLogo,
                TemplateUrlServiceFactory.get().isDefaultSearchEngineGoogle());
        mNewTabPageLayout.loadSearchProviderLogo();
    }

    /**
     * Set the search provider info on the main child view, so that it can change layouts if
     * needed.
     * @param hasLogo Whether the search provider has a logo.
     * @param isGoogle Whether the search provider is Google.
     */
    private void setSearchProviderInfoOnView(boolean hasLogo, boolean isGoogle) {
        mNewTabPageLayout.setSearchProviderInfo(hasLogo, isGoogle);
    }

    /**
     * Specifies the percentage the URL is focused during an animation.  1.0 specifies that the URL
     * bar has focus and has completed the focus animation.  0 is when the URL bar is does not have
     * any focus.
     *
     * @param percent The percentage of the URL bar focus animation.
     */
    public void setUrlFocusChangeAnimationPercent(float percent) {
        mNewTabPageLayout.setUrlFocusChangeAnimationPercent(percent);
    }

    /**
     * Get the bounds of the search box in relation to the top level NewTabPage view.
     *
     * @param bounds The current drawing location of the search box.
     * @param translation The translation applied to the search box by the parent view hierarchy up
     *                    to the NewTabPage view.
     */
    public void getSearchBoxBounds(Rect bounds, Point translation) {
        mNewTabPageLayout.getSearchBoxBounds(bounds, translation, getView());
    }

    /**
     * Updates the opacity of the search box when scrolling.
     *
     * @param alpha opacity (alpha) value to use.
     */
    public void setSearchBoxAlpha(float alpha) {
        mNewTabPageLayout.setSearchBoxAlpha(alpha);
    }

    /**
     * Updates the opacity of the search provider logo when scrolling.
     *
     * @param alpha opacity (alpha) value to use.
     */
    public void setSearchProviderLogoAlpha(float alpha) {
        mNewTabPageLayout.setSearchProviderLogoAlpha(alpha);
    }

    /**
     * Set the search box background drawable.
     *
     * @param drawable The search box background.
     */
    public void setSearchBoxBackground(Drawable drawable) {
        mNewTabPageLayout.setSearchBoxBackground(drawable);
    }

    /**
     * @return Whether the location bar is shown in the NTP.
     */
    public boolean isLocationBarShownInNTP() {
        return mNewTabPageManager.isLocationBarShownInNTP();
    }

    /**
     * @return Whether the location bar has been scrolled to top in the NTP.
     */
    public boolean isLocationBarScrolledToTopInNtp() {
        return mNewTabPageLayout.getToolbarTransitionPercentage() == 1;
    }

    /**
     * Sets the listener for search box scroll changes.
     * @param listener The listener to be notified on changes.
     */
    public void setSearchBoxScrollListener(OnSearchBoxScrollListener listener) {
        mNewTabPageLayout.setSearchBoxScrollListener(listener);
    }

    /**
     * Sets the FakeboxDelegate that this pages interacts with.
     */
    public void setFakeboxDelegate(FakeboxDelegate fakeboxDelegate) {
        mFakeboxDelegate = fakeboxDelegate;
        if (mNewTabPageView != null) mNewTabPageView.setFakeboxDelegate(fakeboxDelegate);
        if (mFakeboxDelegate != null) {
            // The toolbar can't get the reference to the native page until its initialization is
            // finished, so we can't cache it here and transfer it to the view later. We pull that
            // state from the location bar when we get a reference to it as a workaround.
            mNewTabPageLayout.setUrlFocusChangeAnimationPercent(
                    fakeboxDelegate.isUrlBarFocused() ? 1f : 0f);
        }

        mVoiceRecognitionHandler = mFakeboxDelegate.getLocationBarVoiceRecognitionHandler();
        if (mVoiceRecognitionHandler != null) {
            mNewTabPageLayout.updateVoiceSearchButtonVisibility();
        }
    }

    /**
     * Records UMA for the NTP being shown. This includes a fresh page load or being brought to the
     * foreground.
     */
    private void recordNTPShown() {
        mLastShownTimeNs = System.nanoTime();
        RecordUserAction.record("MobileNTPShown");
        SuggestionsMetrics.recordSurfaceVisible();
    }

    /** Records UMA for the NTP being hidden and the time spent on it. */
    private void recordNTPHidden() {
        RecordHistogram.recordMediumTimesHistogram("NewTabPage.TimeSpent",
                (System.nanoTime() - mLastShownTimeNs) / TimeUtils.NANOSECONDS_PER_MILLISECOND);
        SuggestionsMetrics.recordSurfaceHidden();
    }

    /**
     * Returns the value of the adapter scroll position that was stored in the last committed
     * navigation entry. Returns {@code RecyclerView.NO_POSITION} if there is no last committed
     * navigation entry, or if no data is found.
     * @param scrollPositionKey The key under which the scroll position has been stored in the
     *                          NavigationEntryExtraData.
     * @param tab A tab that is used to access the NavigationController and the NavigationEntry
     *            extras.
     * @return The adapter scroll position.
     */
    public static int getScrollPositionFromNavigationEntry(String scrollPositionKey, Tab tab) {
        return getIntFromNavigationEntry(scrollPositionKey, tab, RecyclerView.NO_POSITION);
    }

    /**
     * Returns an arbitrary int value stored in the last committed navigation entry. If some step
     * fails then the default is returned instead.
     * @param key The string previously used to tag this piece of data.
     * @param tab A tab that is used to access the NavigationController and the NavigationEntry
     *            extras.
     * @param defaultValue The value to return if lookup or parsing is unsuccessful.
     * @return The value for the given key.
     *
     * TODO(https://crbug.com/941581): Refactor this to be reusable across NativePage components.
     */
    private static int getIntFromNavigationEntry(String key, Tab tab, int defaultValue) {
        if (tab.getWebContents() == null) return defaultValue;

        String stringValue = getStringFromNavigationEntry(tab, key);
        if (stringValue == null || stringValue.isEmpty()) {
            return RecyclerView.NO_POSITION;
        }

        try {
            return Integer.parseInt(stringValue);
        } catch (NumberFormatException e) {
            Log.w(TAG, "Bad data found for %s : %s", key, stringValue, e);
            return RecyclerView.NO_POSITION;
        }
    }

    /**
     * Returns an arbitrary string value stored in the last committed navigation entry. If the look
     * up fails, an empty string is returned.
     * @param tab A tab that is used to access the NavigationController and the NavigationEntry
     *            extras.
     * @param key The string previously used to tag this piece of data.
     * @return The value previously stored with the given key.
     *
     * TODO(https://crbug.com/941581): Refactor this to be reusable across NativePage components.
     */
    public static String getStringFromNavigationEntry(Tab tab, String key) {
        if (tab.getWebContents() == null) return "";
        NavigationController controller = tab.getWebContents().getNavigationController();
        int index = controller.getLastCommittedEntryIndex();
        return controller.getEntryExtraData(index, key);
    }

    /**
     * @return Whether the NTP has finished loaded.
     */
    @VisibleForTesting
    public boolean isLoadedForTests() {
        return mIsLoaded;
    }

    // TemplateUrlServiceObserver overrides

    @Override
    public void onTemplateURLServiceChanged() {
        onSearchEngineUpdated();
    }

    // NativePage overrides

    @Override
    public void destroy() {
        assert !mIsDestroyed;
        assert !ViewCompat
                .isAttachedToWindow(getView()) : "Destroy called before removed from window";
        if (mIsLoaded && !mTab.isHidden()) recordNTPHidden();

        mNewTabPageManager.onDestroy();
        mTileGroupDelegate.destroy();
        TemplateUrlServiceFactory.get().removeObserver(this);
        mTab.removeObserver(mTabObserver);
        mTabObserver = null;
        mActivityLifecycleDispatcher.unregister(mLifecycleObserver);
        mLifecycleObserver = null;
        mFullscreenManager.removeListener(this);
        mIsDestroyed = true;
    }

    @Override
    public String getUrl() {
        return UrlConstants.NTP_URL;
    }

    @Override
    public String getTitle() {
        return mTitle;
    }

    @Override
    public int getBackgroundColor() {
        return mBackgroundColor;
    }

    @Override
    public boolean needsToolbarShadow() {
        return !mSearchProviderHasLogo;
    }

    @Override
    public View getView() {
        return mNewTabPageView;
    }

    @Override
    public String getHost() {
        return UrlConstants.NTP_HOST;
    }

    @Override
    public void updateForUrl(String url) {
    }

    // InvalidationAwareThumbnailProvider

    @Override
    public boolean shouldCaptureThumbnail() {
        return mNewTabPageView.shouldCaptureThumbnail();
    }

    @Override
    public void captureThumbnail(Canvas canvas) {
        mNewTabPageView.captureThumbnail(canvas);
    }

    @VisibleForTesting
    public NewTabPageManager getManagerForTesting() {
        return mNewTabPageManager;
    }

    @VisibleForTesting
    public View getSignInPromoViewForTesting() {
        RecyclerView recyclerView = mNewTabPageView.getRecyclerView();
        NewTabPageAdapter adapter = (NewTabPageAdapter) recyclerView.getAdapter();
        return recyclerView
                .findViewHolderForAdapterPosition(
                        adapter.getFirstPositionForType(ItemViewType.PROMO))
                .itemView;
    }

    @VisibleForTesting
    public View getSectionHeaderViewForTesting() {
        RecyclerView recyclerView = mNewTabPageView.getRecyclerView();
        NewTabPageAdapter adapter = (NewTabPageAdapter) recyclerView.getAdapter();
        return recyclerView.findViewHolderForAdapterPosition(adapter.getFirstHeaderPosition())
                .itemView;
    }
}
