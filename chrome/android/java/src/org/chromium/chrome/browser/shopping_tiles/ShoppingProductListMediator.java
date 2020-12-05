package org.chromium.chrome.browser.shopping_tiles;

import android.content.Context;
import android.view.ContextMenu;
import android.view.ContextMenu.ContextMenuInfo;
import android.view.LayoutInflater;
import android.view.View;
import android.view.View.OnCreateContextMenuListener;

import org.chromium.base.Callback;
import org.chromium.base.Log;
import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.bookmarks.BookmarkModel;
import org.chromium.chrome.browser.bookmarks.BookmarkUtils;
import org.chromium.chrome.browser.compositor.bottombar.ephemeraltab.EphemeralTabCoordinator;
import org.chromium.chrome.browser.image_fetcher.ImageFetcher;
import org.chromium.chrome.browser.image_fetcher.ImageFetcherConfig;
import org.chromium.chrome.browser.image_fetcher.ImageFetcherFactory;
import org.chromium.chrome.browser.native_page.ContextMenuManager;
import org.chromium.chrome.browser.native_page.ContextMenuManager.ContextMenuItemId;
import org.chromium.chrome.browser.native_page.ContextMenuManager.EmptyDelegate;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.shopping.front_door.FilterChipListCoordinator;
import org.chromium.chrome.browser.shopping.front_door.FrontDoorFeedProvider;
import org.chromium.chrome.browser.shopping.front_door.FrontDoorOnBoardingCoordinator;
import org.chromium.chrome.browser.shopping.front_door.FrontDoorOnBoardingCoordinator.OnboardingObserver;
import org.chromium.chrome.browser.shopping.front_door.ProductLineInfo;
import org.chromium.chrome.browser.shopping_tiles.ShoppingProductListCoordinator.FullSpanSuppliedViewProperties;
import org.chromium.chrome.browser.shopping_tiles.ShoppingProductListCoordinator.HeaderProperties;
import org.chromium.chrome.browser.shopping_tiles.ShoppingProductListCoordinator.ItemType;
import org.chromium.chrome.browser.shopping_tiles.ShoppingProductListCoordinator.OpenUrlHelper;
import org.chromium.chrome.browser.shopping_tiles.ShoppingProductListCoordinator.ProductLineProperties;
import org.chromium.chrome.browser.shopping_tiles.ShoppingProductListCoordinator.ShareUrlHelper;
import org.chromium.components.browser_ui.util.GlobalDiscardableReferencePool;
import org.chromium.ui.modelutil.MVCListAdapter;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.SimpleRecyclerViewAdapter;
import org.chromium.url.GURL;

import java.util.ArrayDeque;
import java.util.ArrayList;
import java.util.HashSet;
import java.util.List;
import java.util.Queue;
import java.util.Set;

class ShoppingProductListMediator {
    private static final int INVALID_INDEX = -1;
    private static final int INITIAL_SIZE = 1000;
    private static final int VIEW_MORE_SIZE = 4;
    private static final int OFFER_ITEM_LIMIT = 8;
    private MVCListAdapter.ModelList mModel;
    private Queue<ProductInfo> mAvailableProducts = new ArrayDeque<>();
    private final Profile mProfile;
    private final Supplier<EphemeralTabCoordinator> mEphemeralTabCoordinatorSupplier;
    private final Context mContext;

    private final FrontDoorOnBoardingCoordinator mOnboardingViewProvider;
    private SimpleRecyclerViewAdapter.ListItem mOnboardingViewItem;
    private final FilterChipListCoordinator mFilterChipListViewProvider;

    private int mProductLineItemStartedIndex = INVALID_INDEX;
    private SimpleRecyclerViewAdapter.ListItem mFirstProductLineListItem;

    private final Set<String> mSelectedCategoryFilterIds = new HashSet<>();
    private final Set<String> mSelectedBrandFilterIds = new HashSet<>();

    private final FrontDoorFeedProvider mFeedProvider;
    // Get data for offer section
    DummyProductProvider mDummyProductProvider;

    private Supplier<ContextMenuManager> mContextMenuManagerSupplier;
    private OpenUrlHelper mOpenUrlHelper;
    private ShareUrlHelper mShareUrlHelper;

    class ContextMenuDelegate extends EmptyDelegate implements OnCreateContextMenuListener {
        String mUrl;
        PropertyModel mItemProperty;
        Boolean mIsOfferItem;
        // EmptyDelegate implmenations.
        @Override
        public void openItem(int windowDisposition) {
            mOpenUrlHelper.openUrl(getUrl(), windowDisposition);
        }

        @Override
        public String getUrl() {
            if (mIsOfferItem) {
                return mItemProperty.get(ShoppingProductProperties.URL);
            } else {
                return mItemProperty.get(ProductLineProperties.SRP_URL);
            }
        }

        public void setItemPropertyModel(PropertyModel model, boolean isOfferItem) {
            mItemProperty = model;
            mIsOfferItem = isOfferItem;
        }

        @Override
        public boolean isItemSupported(int menuItemId) {
            switch (menuItemId) {
                case ContextMenuItemId.SEARCH:
                case ContextMenuItemId.OPEN_IN_NEW_WINDOW:
                case ContextMenuItemId.SAVE_FOR_OFFLINE:
                case ContextMenuItemId.REMOVE:
                case ContextMenuItemId.LEARN_MORE:
                case ContextMenuItemId.ADD_TO_MY_APPS:
                    return false;
                default:
                    return true;
            }
        }

        // OnCreateContextMenuListener implementations.
        @Override
        public void onCreateContextMenu(
                ContextMenu contextMenu, View view, ContextMenuInfo contextMenuInfo) {
            assert mContextMenuManagerSupplier.get()
                    != null : "Context menu manager should not be null";
            mContextMenuManagerSupplier.get().createContextMenu(contextMenu, view, this);
        }

        @Override
        public boolean handleMenuItemClick(int menuItemId) {
            switch (menuItemId) {
                // OPEN_IN_NEW_TAB and OPEN_IN_INCOGNITO_TAB handle in openItem.
                case ContextMenuItemId.SHARE:
                    Log.e("Meil_menu", "Share");
                    String url;
                    String title;
                    if (mIsOfferItem) {
                        url = mItemProperty.get(ShoppingProductProperties.URL);
                        title = mItemProperty.get(ShoppingProductProperties.PRODUCT_NAME);
                    } else {
                        url = mItemProperty.get(ProductLineProperties.SRP_URL);
                        title = mItemProperty.get(ProductLineProperties.PRODUCT_NAME);
                    }
                    mShareUrlHelper.shareUrl(url, title);
                    return true;
                case ContextMenuItemId.BOOKMARK:
                    Log.e("Meil_menu", "bookmark");
                    if (mIsOfferItem) {
                        mItemProperty.get(ShoppingProductProperties.BOOKMARK_CLICK_CALLBACK)
                                .onResult(getUrl());
                    } else {
                        mItemProperty.get(ProductLineProperties.BOOKMARK_CLICK_CALLBACK)
                                .onResult(getUrl());
                    }
                    return true;
                case ContextMenuItemId.NOT_INTERESTED:
                    Log.e("Meil_menu", "Not interested");
                    return true;
            }
            return false;
        }
    }

    ShoppingProductListMediator(Context context, MVCListAdapter.ModelList model, Profile profile,
            Supplier<EphemeralTabCoordinator> ephemeralTabCoordinatorSupplier,
            FrontDoorOnBoardingCoordinator onboardingViewProvider,
            FrontDoorFeedProvider feedProvider, DummyProductProvider dmmyProductProvider,
            Supplier<ContextMenuManager> contextMenuManager, OpenUrlHelper openUrlHelper,
            ShareUrlHelper shareUrlHelper) {
        mContext = context;
        mModel = model;
        mProfile = profile;
        mEphemeralTabCoordinatorSupplier = ephemeralTabCoordinatorSupplier;
        mFeedProvider = feedProvider;
        mDummyProductProvider = dmmyProductProvider;
        mContextMenuManagerSupplier = contextMenuManager;
        mOpenUrlHelper = openUrlHelper;
        mShareUrlHelper = shareUrlHelper;

        mOnboardingViewProvider = onboardingViewProvider;

        mOnboardingViewProvider.addObserver(new OnboardingObserver() {
            @Override
            public void onDone(List<String> pickedBrandIds) {
                mFeedProvider.getProductLinesWithCallback(
                        pickedBrandIds, (itemLists) -> { resetProductLineItem(itemLists); });
            }
        });

        mFilterChipListViewProvider =
                new FilterChipListCoordinator(mContext, onboardingViewProvider);

        if (!mOnboardingViewProvider.hasDoneOnboarding()) {
            mOnboardingViewItem = new SimpleRecyclerViewAdapter.ListItem(
                    ItemType.FULL_SPAN_SUPPLIED_VIEW,
                    new PropertyModel.Builder(FullSpanSuppliedViewProperties.ALL_KEYS).build());
            mOnboardingViewItem.model.set(FullSpanSuppliedViewProperties.VIEW_SUPPLIER,
                    () -> mOnboardingViewProvider.getOnboardPromoCard(this::removeOnboardingItem));
            mModel.add(mOnboardingViewItem);
        }

        mDummyProductProvider.getProductWithCallback(this::addOfferItem);
        mFeedProvider.getProductLinesWithCallback(
                mOnboardingViewProvider.getInterestedBrands(), this::addProductLineItem);
    }

    private void removeOnboardingItem() {
        assert mOnboardingViewProvider.hasDoneOnboarding() && mOnboardingViewItem != null;

        mModel.remove(mOnboardingViewItem);
    }

    void resetProductList(List<ProductInfo> productList) {
        mModel.clear();

        if (productList == null) return;

        for (int i = 0; i < productList.size(); i++) {
            ProductInfo product = productList.get(i);
            assert product != null;

            if (i < INITIAL_SIZE) {
                addProduct(productList.get(i), 0, mModel.size());
            } else {
                mAvailableProducts.add(productList.get(i));
            }
        }
    }

    void addOfferItem(List<ProductInfo> productList) {
        Log.e("Meil", "Add offer section with productList: " + productList.size());
        int headerIndex = mOnboardingViewProvider.hasDoneOnboarding() ? 0 : 1;
        String headerString = mContext.getString(R.string.section_header_resume);
        addSectionHeader(headerString, headerIndex);
        for (int i = 0; i < productList.size() && i < OFFER_ITEM_LIMIT; i++) {
            ProductInfo product = productList.get(i);
            assert product != null;

            if (i < INITIAL_SIZE) {
                addProduct(productList.get(i), ItemType.OFFER_ITEM, ++headerIndex);
            } else {
                mAvailableProducts.add(productList.get(i));
            }
        }
    }

    void addProductLineItem(List<ProductLineInfo> productList) {
        Log.e("Meil", "Add Products for you section with products: " + productList.size());
        addDivider();
        String headerString = mContext.getString(R.string.section_header_browsing);
        addSectionHeader(headerString, mModel.size());
        addFilterChipList();

        resetProductLineItem(productList);
    }

    void chipToggleHandler(String chipId, boolean isCategoryChip, boolean isChecked) {
        List<ProductLineInfo> productList = new ArrayList<>();

        if (isChecked) {
            if (isCategoryChip) {
                productList.addAll(mFeedProvider.getProductLinesForCategory(chipId));
            } else {
                productList.addAll(mFeedProvider.getProductLinesForBrand(chipId));
            }
        } else {
            if (isCategoryChip) {
                mSelectedCategoryFilterIds.remove(chipId);
            } else {
                mSelectedBrandFilterIds.remove(chipId);
            }
        }

        for (String categoryId : mSelectedCategoryFilterIds) {
            productList.addAll(mFeedProvider.getProductLinesForCategory(categoryId));
        }

        for (String brandId : mSelectedBrandFilterIds) {
            productList.addAll(mFeedProvider.getProductLinesForBrand(brandId));
        }

        if (productList.size() == 0) {
            // No chip is enabled, we should show all.
            productList = mFeedProvider.getAllProductLines();
        }

        resetProductLineItem(productList);

        // Intentionally update the map to include the new checked chip here to ensure this checked
        // chips data show first in the list.
        if (isChecked) {
            if (isCategoryChip) {
                mSelectedCategoryFilterIds.add(chipId);
            } else {
                mSelectedBrandFilterIds.add(chipId);
            }
        }
    }

    void resetProductLineItem(List<ProductLineInfo> productList) {
        int firstProductLineIndex = INVALID_INDEX;

        if (mFirstProductLineListItem != null) {
            for (int i = 0; i < mModel.size(); i++) {
                SimpleRecyclerViewAdapter.ListItem item = mModel.get(i);
                if (item == mFirstProductLineListItem) {
                    firstProductLineIndex = i;
                    break;
                }
            }

            if (firstProductLineIndex != INVALID_INDEX) {
                mModel.removeRange(firstProductLineIndex, mModel.size() - firstProductLineIndex);
            }

            mFirstProductLineListItem = null;
        }

        if (productList.size() == 0) return;

        int index = firstProductLineIndex != INVALID_INDEX ? firstProductLineIndex : mModel.size();

        for (int i = 0; i < productList.size(); i++) {
            ProductLineInfo productLine = productList.get(i);
            assert productLine != null;

            ImageFetcher imageFetcher =
                    ImageFetcherFactory.createImageFetcher(ImageFetcherConfig.NETWORK_ONLY,
                            mProfile, GlobalDiscardableReferencePool.getReferencePool());
            Callback<String> productOnClickedCallback = (url) -> {
                mEphemeralTabCoordinatorSupplier.get().requestOpenSheet(
                        url, productLine.name, false, true);
            };

            BookmarkModel bookmarkModel = new BookmarkModel();
            boolean isBookmarked = bookmarkModel.isBookmarkModelLoaded()
                    && bookmarkModel.isBookmarked(new GURL(productLine.clickingUrl));
            Callback<String> bookmarkClickedCallback = (url) -> {
                boolean wasBookmarked =
                        mModel.get(index).model.get(ProductLineProperties.IS_BOOKMARKED);
                BookmarkUtils.addUrlToShoppingFolder(
                        ((ChromeTabbedActivity) mContext), url, productLine.name, wasBookmarked);
                mModel.get(index).model.set(ProductLineProperties.IS_BOOKMARKED, !wasBookmarked);
            };

            PropertyModel productModel =
                    new PropertyModel.Builder(ProductLineProperties.ALL_KEYS)
                            .with(ProductLineProperties.PRODUCT_NAME, productLine.name)
                            .with(ProductLineProperties.SRP_URL, productLine.clickingUrl)
                            .with(ProductLineProperties.IMAGE_URL, productLine.imageUrl)
                            .with(ProductLineProperties.IMAGE_FETCHER, imageFetcher)
                            .with(ProductLineProperties.BRAND_NAME, productLine.brand)
                            .with(ProductLineProperties.ON_CLICK_CALLBACK, productOnClickedCallback)
                            .with(ProductLineProperties.ITEM_CONTEXT_MENU_DELEGATE,
                                    new ContextMenuDelegate())
                            .with(ProductLineProperties.BOOKMARK_CLICK_CALLBACK,
                                    bookmarkClickedCallback)
                            .with(ProductLineProperties.IS_BOOKMARKED, isBookmarked)
                            .build();

            SimpleRecyclerViewAdapter.ListItem item = new SimpleRecyclerViewAdapter.ListItem(
                    ItemType.PRODUCT_LINE_ITEM, productModel);

            if (i == 0) mFirstProductLineListItem = item;

            mModel.add(index + i, item);
        }
        addDivider();
        addEndOfFeedItem();
    }

    void addDivider() {
        mModel.add(new SimpleRecyclerViewAdapter.ListItem(ItemType.DIVIDER, new PropertyModel()));
    }

    void addEndOfFeedItem() {
        mModel.add(new SimpleRecyclerViewAdapter.ListItem(ItemType.FULL_SPAN_SUPPLIED_VIEW,
                new PropertyModel.Builder(FullSpanSuppliedViewProperties.ALL_KEYS)
                        .with(FullSpanSuppliedViewProperties.VIEW_SUPPLIER,
                                ()
                                        -> LayoutInflater.from(mContext).inflate(
                                                R.layout.end_of_feed_view, null, false))
                        .build()));
    }

    void addSectionHeader(String headerString, int index) {
        if (index < 0 || index > mModel.size()) {
            mModel.add(new SimpleRecyclerViewAdapter.ListItem(ItemType.SECTION_HEADER,
                    new PropertyModel.Builder(HeaderProperties.ALL_KEYS)
                            .with(HeaderProperties.HEADER_STRING, headerString)
                            .build()));
        } else {
            mModel.add(index,
                    new SimpleRecyclerViewAdapter.ListItem(ItemType.SECTION_HEADER,
                            new PropertyModel.Builder(HeaderProperties.ALL_KEYS)
                                    .with(HeaderProperties.HEADER_STRING, headerString)
                                    .build()));
        }
    }

    void addFilterChipList() {
        mModel.add(new SimpleRecyclerViewAdapter.ListItem(ItemType.FULL_SPAN_SUPPLIED_VIEW,
                new PropertyModel.Builder(FullSpanSuppliedViewProperties.ALL_KEYS)
                        .with(FullSpanSuppliedViewProperties.VIEW_SUPPLIER,
                                mFilterChipListViewProvider)
                        .build()));
    }

    void addProduct(ProductInfo product, int type, int index) {
        ImageFetcher imageFetcher =
                ImageFetcherFactory.createImageFetcher(ImageFetcherConfig.NETWORK_ONLY, mProfile,
                        GlobalDiscardableReferencePool.getReferencePool());
        Callback<String> productOnClickedCallback = (url) -> {
            mEphemeralTabCoordinatorSupplier.get().requestOpenSheet(url, product.name, false, true);
            Log.e("YUSUF","URL IS "+url);
        };

        BookmarkModel bookmarkModel = new BookmarkModel();
        boolean isBookmarked = bookmarkModel.isBookmarkModelLoaded() && bookmarkModel.isBookmarked(new GURL(product.productUrl));
        Callback<String> bookmarkClickedCallback = (url) -> {
            boolean wasBookmarked = mModel.get(index).model.get(ShoppingProductProperties.IS_BOOKMARKED);
            BookmarkUtils.addUrlToShoppingFolder(((ChromeTabbedActivity)mContext), url, product.name, wasBookmarked);
            mModel.get(index).model.set(ShoppingProductProperties.IS_BOOKMARKED, !wasBookmarked);
        };

        PropertyModel productModel =
                new PropertyModel.Builder(ShoppingProductProperties.ALL_KEYS)
                        .with(ShoppingProductProperties.PRODUCT_NAME, product.name)
                        .with(ShoppingProductProperties.URL, product.productUrl)
                        .with(ShoppingProductProperties.IMAGE_URL, product.imageUrl)
                        .with(ShoppingProductProperties.IMAGE_FETCHER, imageFetcher)
                        .with(ShoppingProductProperties.ON_CLICK_CALLBACK, productOnClickedCallback)
                        .with(ShoppingProductProperties.BOOKMARK_CLICK_CALLBACK,
                                bookmarkClickedCallback)
                        .with(ShoppingProductProperties.IS_BOOKMARKED, isBookmarked)
                        .with(ShoppingProductProperties.ITEM_CONTEXT_MENU_DELEGATE,
                                new ContextMenuDelegate())
                        .with(ShoppingProductProperties.IS_RECENTLY_VIEWED, product.isRecentlyView)
                        .build();
        if (product.priceStr != null) {
            productModel.set(ShoppingProductProperties.PRICE_STR, "$" + product.priceStr);
        } else {
            productModel.set(ShoppingProductProperties.PRICE, product.price);
        }

        if (index < 0 || index > mModel.size()) {
            mModel.add(new SimpleRecyclerViewAdapter.ListItem(type, productModel));
        } else {
            mModel.add(index, new SimpleRecyclerViewAdapter.ListItem(type, productModel));
        }
    }

    void viewMore() {
        Log.d("Meil", "Add more product to show");
        int cnt = 0;
        for (int i = 0; i < VIEW_MORE_SIZE && mAvailableProducts.size() != 0; i++) {
            ProductInfo product = mAvailableProducts.poll();
            Log.d("Meil", "mAvaiable size: " + mAvailableProducts.size());
            assert product != null;
            addProduct(product, 0, mModel.size());
            cnt++;
        }
        Log.d("Meil", "Added " + cnt + " more products to show");
    }

    boolean hasMoreProduct() {
        return mAvailableProducts.size() != 0;
    }
}
