// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor.bottombar.ephemeraltab;

import static org.chromium.chrome.browser.dependency_injection.ChromeCommonQualifiers.ACTIVITY_CONTEXT;
import static org.chromium.chrome.browser.dependency_injection.ChromeCommonQualifiers.DECOR_VIEW;
import static org.chromium.chrome.browser.dependency_injection.ChromeCommonQualifiers.IS_PROMOTABLE_TO_TAB_BOOLEAN;

import android.content.Context;
import android.graphics.drawable.Drawable;
import android.view.View;

import org.chromium.base.Callback;
import org.chromium.base.SysUtils;
import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ActivityTabProvider;
import org.chromium.chrome.browser.ChromeVersionInfo;
import org.chromium.chrome.browser.WebContentsFactory;
import org.chromium.chrome.browser.content.ContentUtils;
import org.chromium.chrome.browser.dependency_injection.ActivityScope;
import org.chromium.chrome.browser.feature_engagement.TrackerFactory;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tabmodel.TabCreatorManager.TabCreator;
import org.chromium.chrome.browser.ui.favicon.FaviconHelper;
import org.chromium.chrome.browser.ui.favicon.FaviconUtils;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetContent;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.SheetState;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.StateChangeReason;
import org.chromium.components.browser_ui.bottomsheet.EmptyBottomSheetObserver;
import org.chromium.components.browser_ui.widget.RoundedIconGenerator;
import org.chromium.components.embedder_support.view.ContentView;
import org.chromium.components.feature_engagement.EventConstants;
import org.chromium.components.feature_engagement.Tracker;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.UiUtils;
import org.chromium.ui.base.ActivityWindowAndroid;
import org.chromium.ui.base.PageTransition;
import org.chromium.ui.base.ViewAndroidDelegate;

import javax.inject.Inject;
import javax.inject.Named;

/**
 * Central class for ephemeral tab, responsible for spinning off other classes necessary to display
 * the preview tab UI.
 */
@ActivityScope
public class EphemeralTabCoordinator implements View.OnLayoutChangeListener {
    private final Context mContext;
    private final ActivityWindowAndroid mWindow;
    private final View mLayoutView;
    private final ActivityTabProvider mTabProvider;
    private final Supplier<TabCreator> mTabCreator;
    private final BottomSheetController mBottomSheetController;
    private final EphemeralTabMetrics mMetrics = new EphemeralTabMetrics();
    private final boolean mCanPromoteToNewTab;

    private EphemeralTabMediator mMediator;

    private WebContents mWebContents;
    private ContentView mContentView;
    private EphemeralTabSheetContent mSheetContent;
    private EmptyBottomSheetObserver mSheetObserver;

    private String mUrl;
    private int mCurrentMaxSheetHeight;
    private boolean mPeeked;
    private boolean mViewed; // Moved up from peek state by user
    private boolean mFullyOpened;

    /**
     * Constructor.
     * @param context The associated {@link Context}.
     * @param window The associated {@link WindowAndroid}.
     * @param layoutView The {@link View} to listen layout change on.
     * @param tabProvider Provider of the current activity tab.
     * @param tabCreator Supplier for {@link TabCreator} handling a new tab creation.
     * @param bottomSheetController {@link BottomSheetController} as the container of the tab.
     * @param canPromoteToNewTab Whether the tab can be promoted to a normal tab.
     */
    @Inject
    public EphemeralTabCoordinator(@Named(ACTIVITY_CONTEXT) Context context,
            ActivityWindowAndroid window, @Named(DECOR_VIEW) View layoutView,
            ActivityTabProvider tabProvider, Supplier<TabCreator> tabCreator,
            BottomSheetController bottomSheetController,
            @Named(IS_PROMOTABLE_TO_TAB_BOOLEAN) boolean canPromoteToNewTab) {
        mContext = context;
        mWindow = window;
        mLayoutView = layoutView;
        mTabProvider = tabProvider;
        mTabCreator = tabCreator;
        mBottomSheetController = bottomSheetController;
        mCanPromoteToNewTab = canPromoteToNewTab;
    }

    /**
     * Checks if this feature (a.k.a. "Preview page/image") is supported.
     * @return {@code true} if the feature is enabled.
     */
    public static boolean isSupported() {
        return ChromeFeatureList.isEnabled(ChromeFeatureList.EPHEMERAL_TAB_USING_BOTTOM_SHEET)
                && !SysUtils.isLowEndDevice();
    }

    /**
     * Checks if the preview tab is in open (peek) state.
     */
    public boolean isOpened() {
        return mPeeked;
    }

    /**
     * Entry point for ephemeral tab flow. This will create an ephemeral tab and show it in the
     * bottom sheet.
     * @param url The URL to be shown.
     * @param title The title to be shown.
     * @param isIncognito Whether we are currently in incognito mode.
     */
    public void requestOpenSheet(String url, String title, boolean isIncognito) {
        mUrl = url;
        Profile profile = isIncognito ? Profile.getLastUsedRegularProfile().getOffTheRecordProfile()
                                      : Profile.getLastUsedRegularProfile();

        if (mMediator == null) {
            float topControlsHeight =
                    mContext.getResources().getDimensionPixelSize(R.dimen.toolbar_height_no_shadow)
                    / mWindow.getDisplay().getDipScale();
            mMediator = new EphemeralTabMediator(mBottomSheetController,
                    new FaviconLoader(mContext), mMetrics, (int) topControlsHeight);
        }
        if (mWebContents == null) {
            assert mSheetContent == null;
            createWebContents(isIncognito);
            mSheetObserver = new EmptyBottomSheetObserver() {
                private int mCloseReason;

                @Override
                public void onSheetContentChanged(BottomSheetContent newContent) {
                    if (newContent != mSheetContent) {
                        mMetrics.recordMetricsForClosed(mCloseReason);
                        mPeeked = false;
                        destroyWebContents();
                    }
                }

                @Override
                public void onSheetOpened(@StateChangeReason int reason) {
                    if (!mViewed) {
                        mMetrics.recordMetricsForViewed();
                        mViewed = true;
                    }
                }

                @Override
                public void onSheetStateChanged(int newState) {
                    if (mSheetContent == null) return;
                    switch (newState) {
                        case SheetState.PEEK:
                            if (!mPeeked) {
                                mMetrics.recordMetricsForPeeked();
                                mPeeked = true;
                            }
                            break;
                        case SheetState.FULL:
                            if (!mFullyOpened) {
                                mMetrics.recordMetricsForOpened();
                                mFullyOpened = true;
                            }
                            break;
                    }
                }

                @Override
                public void onSheetClosed(int reason) {
                    // "Closed" actually means "Peek" for bottom sheet. Save the reason to
                    // log when the sheet goes to hidden state. See http://crbug.com/986310.
                    mCloseReason = reason;
                }

                @Override
                public void onSheetOffsetChanged(float heightFraction, float offsetPx) {
                    if (mSheetContent == null) return;
                    if (mCanPromoteToNewTab) mSheetContent.showOpenInNewTabButton(heightFraction);
                }
            };
            mBottomSheetController.addObserver(mSheetObserver);
            mSheetContent = new EphemeralTabSheetContent(mContext, this::openInNewTab,
                    this::onToolbarClick, this::close, getMaxSheetHeight());
            mMediator.init(mWebContents, mContentView, mSheetContent, profile);
            mLayoutView.addOnLayoutChangeListener(this);
        }

        mPeeked = false;
        mViewed = false;
        mFullyOpened = false;
        mMediator.requestShowContent(url, title);

        Tracker tracker = TrackerFactory.getTrackerForProfile(profile);
        if (tracker.isInitialized()) tracker.notifyEvent(EventConstants.EPHEMERAL_TAB_USED);
    }

    private void createWebContents(boolean incognito) {
        assert mWebContents == null;

        // Creates an initially hidden WebContents which gets shown when the panel is opened.
        mWebContents = WebContentsFactory.createWebContents(incognito, true);

        mContentView = ContentView.createContentView(
                mContext, null /* eventOffsetHandler */, mWebContents);

        mWebContents.initialize(ChromeVersionInfo.getProductVersion(),
                ViewAndroidDelegate.createBasicDelegate(mContentView), mContentView, mWindow,
                WebContents.createDefaultInternalsHolder());
        ContentUtils.setUserAgentOverride(mWebContents);
    }

    private void destroyWebContents() {
        mSheetContent = null; // Will be destroyed by BottomSheet controller.

        if (mWebContents != null) {
            mWebContents.destroy();
            mWebContents = null;
            mContentView = null;
        }

        if (mMediator != null) mMediator.destroyContent();

        mLayoutView.removeOnLayoutChangeListener(this);
        if (mSheetObserver != null) mBottomSheetController.removeObserver(mSheetObserver);
    }

    private void openInNewTab() {
        if (mCanPromoteToNewTab && mUrl != null) {
            mBottomSheetController.hideContent(
                    mSheetContent, /* animate= */ true, StateChangeReason.PROMOTE_TAB);
            mTabCreator.get().createNewTab(new LoadUrlParams(mUrl, PageTransition.LINK),
                    TabLaunchType.FROM_LINK, mTabProvider.get());
            mMetrics.recordOpenInNewTab();
        }
    }

    private void onToolbarClick() {
        int state = mBottomSheetController.getSheetState();
        if (state == SheetState.PEEK) {
            mBottomSheetController.expandSheet();
        } else if (state == SheetState.FULL) {
            mBottomSheetController.collapseSheet(true);
        }
    }

    /**
     * Close the ephemeral tab.
     */
    public void close() {
        mBottomSheetController.hideContent(mSheetContent, /* animate= */ true);
    }

    @Override
    public void onLayoutChange(View view, int left, int top, int right, int bottom, int oldLeft,
            int oldTop, int oldRight, int oldBottom) {
        if (mSheetContent == null) return;

        // It may not be possible to update the content height when the actual height changes
        // due to the current tab not being ready yet. Try it later again when the tab
        // (hence MaxSheetHeight) becomes valid.
        int maxSheetHeight = getMaxSheetHeight();
        if (maxSheetHeight == 0 || mCurrentMaxSheetHeight == maxSheetHeight) return;
        mSheetContent.updateContentHeight(maxSheetHeight);
        mCurrentMaxSheetHeight = maxSheetHeight;
    }

    private int getMaxSheetHeight() {
        Tab tab = mTabProvider.get();
        if (tab == null || tab.getView() == null) return 0;
        return (int) (tab.getView().getHeight() * 0.9f);
    }

    /**
     * Helper class to generate a favicon for a given URL and resize it to the desired dimensions
     * for displaying it on the image view.
     */
    static class FaviconLoader {
        private final Context mContext;
        private final FaviconHelper mFaviconHelper;
        private final RoundedIconGenerator mIconGenerator;
        private final int mFaviconSize;

        /** Constructor. */
        public FaviconLoader(Context context) {
            mContext = context;
            mFaviconHelper = new FaviconHelper();
            mIconGenerator = FaviconUtils.createCircularIconGenerator(mContext.getResources());
            mFaviconSize =
                    mContext.getResources().getDimensionPixelSize(R.dimen.preview_tab_favicon_size);
        }

        /**
         * Generates a favicon for a given URL. If no favicon was could be found or generated from
         * the URL, a default favicon will be shown.
         * @param url The URL for which favicon is to be generated.
         * @param callback The callback to be invoked to display the final image.
         * @param profile The profile for which favicon service is used.
         */
        public void loadFavicon(final String url, Callback<Drawable> callback, Profile profile) {
            assert profile != null;
            FaviconHelper.FaviconImageCallback imageCallback = (bitmap, iconUrl) -> {
                Drawable drawable;
                if (bitmap != null) {
                    drawable = FaviconUtils.createRoundedBitmapDrawable(
                            mContext.getResources(), bitmap);
                } else {
                    drawable = UiUtils.getTintedDrawable(mContext, R.drawable.ic_globe_24dp,
                            R.color.default_icon_color_tint_list);
                }

                callback.onResult(drawable);
            };

            mFaviconHelper.getLocalFaviconImageForURL(profile, url, mFaviconSize, imageCallback);
        }
    }
}
