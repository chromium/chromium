// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.explore_sites;

import android.app.Activity;
import android.content.Context;
import android.os.Parcel;
import android.os.Parcelable;
import android.text.TextUtils;
import android.text.format.DateUtils;
import android.util.Base64;
import android.view.ViewGroup;

import androidx.annotation.IntDef;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;
import androidx.recyclerview.widget.RecyclerView;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.native_page.ContextMenuManager;
import org.chromium.chrome.browser.native_page.NativePageNavigationDelegate;
import org.chromium.chrome.browser.native_page.NativePageNavigationDelegateImpl;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabObserver;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.ui.native_page.BasicNativePage;
import org.chromium.chrome.browser.ui.native_page.NativePageHost;
import org.chromium.components.browser_ui.widget.RoundedIconGenerator;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.content_public.browser.NavigationController;
import org.chromium.content_public.browser.NavigationEntry;
import org.chromium.ui.modelutil.ListModel;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.RecyclerViewAdapter;
import org.chromium.url.GURL;
import org.chromium.url.URI;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.net.URISyntaxException;
import java.util.List;

/**
 * Provides functionality when the user interacts with the explore sites page.
 */
public class ExploreSitesPage extends BasicNativePage {
    private static final long BACK_NAVIGATION_TIMEOUT_FOR_UMA = DateUtils.SECOND_IN_MILLIS * 30;
    private static final String CONTEXT_MENU_USER_ACTION_PREFIX = "ExploreSites";
    private static final int INITIAL_SCROLL_POSITION = 0;
    private static final String NAVIGATION_ENTRY_PAGE_STATE_KEY = "ExploreSitesPageState";
    // Constants that dictate sizes of rows and columns
    private static final int MAX_COLUMNS_DENSE_TITLE_BOTTOM = 5;
    private static final int MAX_COLUMNS_DENSE_TITLE_RIGHT = 3;
    private static final int MAX_COLUMNS_ORIGINAL = 4;

    private static final int MAX_ROWS_DENSE_TITLE_BOTTOM = 2;
    private static final int MAX_ROWS_DENSE_TITLE_RIGHT = 3;
    private static final int MAX_ROWS_ORIGINAL = 2;
    public static final int MAX_TILE_COUNT_ALL_VARIATIONS =
            Math.max(Math.max(MAX_COLUMNS_DENSE_TITLE_BOTTOM * MAX_ROWS_DENSE_TITLE_BOTTOM,
                             MAX_COLUMNS_DENSE_TITLE_RIGHT* MAX_ROWS_DENSE_TITLE_RIGHT),
                    MAX_COLUMNS_ORIGINAL* MAX_ROWS_ORIGINAL);

    static final PropertyModel.WritableIntPropertyKey STATUS_KEY =
            new PropertyModel.WritableIntPropertyKey();
    static final PropertyModel.WritableIntPropertyKey SCROLL_TO_CATEGORY_KEY =
            new PropertyModel.WritableIntPropertyKey();
    static final PropertyModel
            .ReadableObjectPropertyKey<ListModel<ExploreSitesCategory>> CATEGORY_LIST_KEY =
            new PropertyModel.ReadableObjectPropertyKey<>();
    static final PropertyModel.ReadableIntPropertyKey DENSE_VARIATION_KEY =
            new PropertyModel.ReadableIntPropertyKey();
    static final PropertyModel.ReadableIntPropertyKey MAX_COLUMNS_KEY =
            new PropertyModel.ReadableIntPropertyKey();
    static final PropertyModel.ReadableIntPropertyKey MAX_ROWS_KEY =
            new PropertyModel.ReadableIntPropertyKey();
    private static final int UNKNOWN_NAV_CATEGORY = -1;

    @IntDef({CatalogLoadingState.LOADING, CatalogLoadingState.SUCCESS, CatalogLoadingState.ERROR,
            CatalogLoadingState.LOADING_NET})
    @Retention(RetentionPolicy.SOURCE)
    public @interface CatalogLoadingState {
        int LOADING = 1; // Loading catalog info from disk.
        int SUCCESS = 2;
        int ERROR = 3; // Error retrieving catalog resources from internet.
        int LOADING_NET = 4; // Retrieving catalog resources from internet.
    }

    private NativePageHost mHost;
    private Tab mTab;
    private TabObserver mTabObserver;
    private Profile mProfile;
    private ViewGroup mView;
    private RecyclerView mRecyclerView;
    private StableScrollLayoutManager mLayoutManager;
    private String mTitle;
    private PropertyModel mModel;
    private ContextMenuManager mContextMenuManager;
    private int mNavigateToCategory;
    private boolean mIsLoaded;
    private int mInitialScrollPosition;
    private boolean mScrollUserActionReported;
    @DenseVariation
    private int mDenseVariation;
    private int mMaxColumns;

    @VisibleForTesting
    static class PageState implements Parcelable {
        private Parcelable mStableLayoutManagerState;
        private Long mLastTimestamp;

        public static final Creator<PageState> CREATOR = new Creator<PageState>() {
            @Override
            public PageState createFromParcel(Parcel in) {
                return new PageState(in);
            }

            @Override
            public PageState[] newArray(int size) {
                return new PageState[size];
            }
        };

        PageState(Parcel parcel) {
            mLastTimestamp = parcel.readLong();
            mStableLayoutManagerState = parcel.readParcelable(getClass().getClassLoader());
        }

        PageState(Long lastTimestamp, Parcelable stableScrollLayoutManagerState) {
            mLastTimestamp = lastTimestamp;
            mStableLayoutManagerState = stableScrollLayoutManagerState;
        }

        @Override
        public int describeContents() {
            return 0;
        }

        @Override
        public void writeToParcel(Parcel dest, int flags) {
            dest.writeLong(mLastTimestamp);
            dest.writeParcelable(mStableLayoutManagerState, flags);
        }

        public long getLastTimestamp() {
            return mLastTimestamp;
        }

        public Parcelable getStableLayoutManagerState() {
            return mStableLayoutManagerState;
        }
    }

    /**
     * Returns whether the given host is the ExploreSitesPage's host. Does not check the
     * scheme, which is required to fully validate a URL.
     * @param host The host to check
     * @return Whether this host is the ExploreSitesPage host.
     */
    public static boolean isExploreSitesHost(String host) {
        return UrlConstants.EXPLORE_HOST.equals(host);
    }

    /**
     * Create a new instance of the explore sites page.
     */
    public ExploreSitesPage(
            Activity activity, NativePageHost host, Tab tab, TabModelSelector tabModelSelector) {
        super(host);

        mHost = host;
        mTab = tab;

        mTitle = activity.getString(R.string.explore_sites_title);
        mView = (ViewGroup) activity.getLayoutInflater().inflate(
                R.layout.explore_sites_page_layout, null);
        mProfile = Profile.fromWebContents(mTab.getWebContents());

        mDenseVariation = ExploreSitesBridge.getDenseVariation();
        int maxRows;
        switch (mDenseVariation) {
            case DenseVariation.DENSE_TITLE_BOTTOM:
                maxRows = MAX_ROWS_DENSE_TITLE_BOTTOM;
                mMaxColumns = MAX_COLUMNS_DENSE_TITLE_BOTTOM;
                break;
            case DenseVariation.DENSE_TITLE_RIGHT:
                maxRows = MAX_ROWS_DENSE_TITLE_RIGHT;
                mMaxColumns = MAX_COLUMNS_DENSE_TITLE_RIGHT;
                break;
            default:
                maxRows = MAX_ROWS_ORIGINAL;
                mMaxColumns = MAX_COLUMNS_ORIGINAL;
        }
        mModel = new PropertyModel
                         .Builder(STATUS_KEY, SCROLL_TO_CATEGORY_KEY, CATEGORY_LIST_KEY,
                                 MAX_COLUMNS_KEY, MAX_ROWS_KEY, DENSE_VARIATION_KEY)
                         .with(CATEGORY_LIST_KEY, new ListModel<>())
                         .with(STATUS_KEY, CatalogLoadingState.LOADING)
                         .with(MAX_COLUMNS_KEY, mMaxColumns)
                         .with(MAX_ROWS_KEY, maxRows)
                         .with(DENSE_VARIATION_KEY, mDenseVariation)
                         .build();

        Context context = mView.getContext();
        mLayoutManager = new StableScrollLayoutManager(context);

        // Set dimensions for iconGenerator
        int iconSizePx;
        int textSizeDimensionResource;
        int iconRadius;
        boolean isDense = ExploreSitesBridge.isDense(mDenseVariation);
        if (isDense) {
            iconSizePx = context.getResources().getDimensionPixelSize(
                    R.dimen.explore_sites_dense_icon_size);
            iconRadius = context.getResources().getDimensionPixelSize(
                    R.dimen.explore_sites_dense_icon_corner_radius);
            textSizeDimensionResource = R.dimen.explore_sites_dense_icon_text_size;
        } else {
            iconSizePx = context.getResources().getDimensionPixelSize(R.dimen.tile_view_icon_size);
            textSizeDimensionResource = R.dimen.tile_view_icon_text_size;
            iconRadius = iconSizePx / 2;
        }

        RoundedIconGenerator iconGenerator = new RoundedIconGenerator(iconSizePx, iconSizePx,
                iconRadius, context.getColor(R.color.default_favicon_background_color),
                context.getResources().getDimensionPixelSize(textSizeDimensionResource));

        NativePageNavigationDelegateImpl navDelegate = new NativePageNavigationDelegateImpl(
                activity, mProfile, host, tabModelSelector, mTab);

        // ExploreSitePage is recreated upon reparenting. Safe to use |activity|.
        Runnable closeContextMenuCallback = activity::closeContextMenu;

        mContextMenuManager = createContextMenuManager(
                navDelegate, closeContextMenuCallback, CONTEXT_MENU_USER_ACTION_PREFIX);

        mTab.getWindowAndroid().addContextMenuCloseListener(mContextMenuManager);

        CategoryCardAdapter adapterDelegate = new CategoryCardAdapter(
                mModel, mLayoutManager, iconGenerator, mContextMenuManager, navDelegate, mProfile);

        mRecyclerView = mView.findViewById(R.id.explore_sites_category_recycler);

        CategoryCardViewHolderFactory factory = createCategoryCardViewHolderFactory();
        RecyclerViewAdapter<CategoryCardViewHolderFactory.CategoryCardViewHolder, Void> adapter =
                new RecyclerViewAdapter<>(adapterDelegate, factory);

        mRecyclerView.setLayoutManager(mLayoutManager);
        mRecyclerView.setAdapter(adapter);
        mRecyclerView.addOnScrollListener(new RecyclerView.OnScrollListener() {
            @Override
            public void onScrolled(RecyclerView v, int x, int y) {
                // y=0 on initial layout, even if the initial scroll position is requested
                // that is not 0. Once user starts scrolling via touch, the onScrolled comes
                // in bunches with |y| having a dY value of every small move, positive (scroll
                // down) or negative (scroll up) number of dps for each move.
                if (!mScrollUserActionReported && (y != 0)) {
                    mScrollUserActionReported = true;
                    RecordUserAction.record("Android.ExploreSitesPage.Scrolled");
                }
            }
        });

        mInitialScrollPosition = INITIAL_SCROLL_POSITION;

        ExploreSitesBridge.getCatalog(mProfile,
                ExploreSitesCatalogUpdateRequestSource.EXPLORE_SITES_PAGE, this::translateToModel);

        initWithView(mView);

        RecordUserAction.record("Android.ExploreSitesPage.Open");
    }

    protected ContextMenuManager createContextMenuManager(NativePageNavigationDelegate navDelegate,
            Runnable closeContextMenuCallback, String contextMenuUserActionPrefix) {
        return new ContextMenuManager(navDelegate,
                (enabled) -> {}, closeContextMenuCallback, contextMenuUserActionPrefix);
    }

    private void translateToModel(@Nullable List<ExploreSitesCategory> categoryList) {
        // If list is null or we received an empty catalog from network, show error.
        if (!ExploreSitesBridge.isValidCatalog(categoryList)) {
            mModel.set(STATUS_KEY, CatalogLoadingState.ERROR);
            mIsLoaded = true;
            return;
        }
        mModel.set(STATUS_KEY, CatalogLoadingState.SUCCESS);

        ListModel<ExploreSitesCategory> categoryListModel = mModel.get(CATEGORY_LIST_KEY);

        // Filter empty categories and categories with fewer sites originally than would fill a row.
        for (ExploreSitesCategory category : categoryList) {
            if ((category.getNumDisplayed() > 0) && (category.getMaxRows(mMaxColumns) > 0)) {
                categoryListModel.add(category);
            }
        }

        restorePageState();

        if (mTab != null) {
            // We want to observe page load start so that we can store the recycler view layout
            // state, for making "back" work correctly.
            mTabObserver = new EmptyTabObserver() {
                @Override
                public void onPageLoadStarted(Tab tab, GURL url) {
                    if (UrlConstants.CHROME_NATIVE_SCHEME.equals(url.getScheme())
                            && UrlConstants.EXPLORE_HOST.equals(url.getHost())) {
                        return;
                    }
                    savePageState();
                }
            };
            mTab.addObserver(mTabObserver);
        }

        mIsLoaded = true;
    }

    /**
     * Restore the state of the page from a Base64 Encoded String located in this page's nav entry.
     */
    private void restorePageState() {
        // Get Parcelable from the nav entry.
        PageState pageState = getPageStateFromNavigationEntry();

        if (pageState == null) {
            setInitialScrollPosition();
        } else {
            // Extract the previous timestamp.
            long previousTimestamp = pageState.getLastTimestamp();
            recordPageRevisitDelta(previousTimestamp);

            // Restore the scroll position.
            mLayoutManager.onRestoreInstanceState(pageState.getStableLayoutManagerState());
        }
    }

    /*
     * Retrieves the state of the page from the navigation entry and reconstitutes it into a
     * PageState using PageState.CREATOR.
     */
    private PageState getPageStateFromNavigationEntry() {
        if (mTab.getWebContents() == null) return null;

        NavigationController controller = mTab.getWebContents().getNavigationController();
        int index = controller.getLastCommittedEntryIndex();
        String pageStateFromNav =
                controller.getEntryExtraData(index, NAVIGATION_ENTRY_PAGE_STATE_KEY);
        if (TextUtils.isEmpty(pageStateFromNav)) return null;

        byte[] parcelData = Base64.decode(pageStateFromNav, 0);
        Parcel parcel = Parcel.obtain();
        parcel.unmarshall(parcelData, 0, parcelData.length);
        parcel.setDataPosition(0);
        PageState pageState = PageState.CREATOR.createFromParcel(parcel);
        parcel.recycle();

        return pageState;
    }

    private void setInitialScrollPosition() {
        int scrollPosition = mInitialScrollPosition;
        if (mNavigateToCategory != UNKNOWN_NAV_CATEGORY) {
            scrollPosition = lookupCategory();
        }
        if (scrollPosition == RecyclerView.NO_POSITION) {
            // Default to first position.
            scrollPosition = 0;
        }

        mModel.set(SCROLL_TO_CATEGORY_KEY, scrollPosition);
    }

    /** Records the time delta between page visits if it is under 30 seconds for UMA usage. */
    private static void recordPageRevisitDelta(long previousTimestamp) {
        long currentTimestamp = System.currentTimeMillis();
        long delta = currentTimestamp - previousTimestamp;

        if (delta < BACK_NAVIGATION_TIMEOUT_FOR_UMA) {
            RecordHistogram.recordCustomTimesHistogram(
                    "ExploreSites.NavBackTime", delta, 1, BACK_NAVIGATION_TIMEOUT_FOR_UMA, 50);
        }
    }

    /** Save the state of the page to a parcelable located in this page's nav entry. */
    private void savePageState() {
        if (mTab == null || mTab.getWebContents() == null) return;

        NavigationController controller = mTab.getWebContents().getNavigationController();
        int index = controller.getLastCommittedEntryIndex();
        NavigationEntry entry = controller.getEntryAtIndex(index);
        if (entry == null) return;

        PageState pageState = createPageState();
        Parcel parcel = Parcel.obtain();
        pageState.writeToParcel(parcel, 0);
        String marshalledState = Base64.encodeToString(parcel.marshall(), 0);
        parcel.recycle();

        controller.setEntryExtraData(index, NAVIGATION_ENTRY_PAGE_STATE_KEY, marshalledState);
    }

    private PageState createPageState() {
        Parcelable layoutManagerState = mLayoutManager.onSaveInstanceState();
        long timestamp = System.currentTimeMillis();

        return new PageState(timestamp, layoutManagerState);
    }

    boolean isLoadedForTests() {
        return mIsLoaded;
    }

    int initialScrollPositionForTests() {
        return mInitialScrollPosition;
    }

    @Override
    public String getHost() {
        return UrlConstants.EXPLORE_HOST;
    }

    @Override
    public String getTitle() {
        return mTitle;
    }

    @Override
    public void updateForUrl(String url) {
        super.updateForUrl(url);
        mNavigateToCategory = UNKNOWN_NAV_CATEGORY;
        try {
            mNavigateToCategory = Integer.parseInt(new URI(url).getFragment());
        } catch (URISyntaxException | NumberFormatException ignored) {
        }
        if (mModel.get(STATUS_KEY) == CatalogLoadingState.SUCCESS) {
            int category = lookupCategory();
            if (category != RecyclerView.NO_POSITION) {
                mModel.set(SCROLL_TO_CATEGORY_KEY, category);
            }
        }
    }

    @Override
    public void destroy() {
        if (mTabObserver != null) {
            mTab.removeObserver(mTabObserver);
        }
        mTab.getWindowAndroid().removeContextMenuCloseListener(mContextMenuManager);
        super.destroy();
    }

    private int lookupCategory() {
        if (mNavigateToCategory != UNKNOWN_NAV_CATEGORY) {
            ListModel<ExploreSitesCategory> categoryList = mModel.get(CATEGORY_LIST_KEY);
            for (int i = 0; i < categoryList.size(); i++) {
                if (categoryList.get(i).getId() == mNavigateToCategory) {
                    return i;
                }
            }
        }

        return RecyclerView.NO_POSITION;
    }

    protected CategoryCardViewHolderFactory createCategoryCardViewHolderFactory() {
        switch (mDenseVariation) {
            case DenseVariation.DENSE_TITLE_BOTTOM:
                return new CategoryCardViewHolderFactoryDenseTitleBottom();
            case DenseVariation.DENSE_TITLE_RIGHT:
                return new CategoryCardViewHolderFactoryDenseTitleRight();
            default:
                return new CategoryCardViewHolderFactory();
        }
    }
}
