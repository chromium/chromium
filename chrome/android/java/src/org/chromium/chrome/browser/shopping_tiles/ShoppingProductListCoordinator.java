package org.chromium.chrome.browser.shopping_tiles;

import android.content.Context;
import android.graphics.Bitmap;
import android.view.LayoutInflater;
import android.view.View;
import android.view.View.OnAttachStateChangeListener;
import android.view.ViewGroup;
import android.view.ViewGroup.LayoutParams;
import android.widget.FrameLayout;
import android.widget.ImageView;
import android.widget.TextView;

import androidx.annotation.IntDef;
import androidx.annotation.Nullable;
import androidx.recyclerview.widget.GridLayoutManager;
import androidx.recyclerview.widget.LinearLayoutManager;
import androidx.recyclerview.widget.RecyclerView;
import androidx.recyclerview.widget.StaggeredGridLayoutManager;

import org.chromium.base.Callback;
import org.chromium.base.Log;
import org.chromium.base.ObserverList;
import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.compositor.bottombar.ephemeraltab.EphemeralTabCoordinator;
import org.chromium.chrome.browser.image_fetcher.ImageFetcher;
import org.chromium.chrome.browser.native_page.ContextMenuManager;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.shopping.front_door.FrontDoorFeedProvider;
import org.chromium.chrome.browser.shopping.front_door.FrontDoorOnBoardingCoordinator;
import org.chromium.chrome.browser.shopping.front_door.ProductLineInfo;
import org.chromium.chrome.browser.shopping.front_door.ScrimableImageContainer;
import org.chromium.chrome.browser.shopping_tiles.ShoppingProductListMediator.ContextMenuDelegate;
import org.chromium.components.query_tiles.QueryTileConstants;
import org.chromium.ui.UiUtils;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modelutil.MVCListAdapter;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModel.ReadableObjectPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableObjectPropertyKey;
import org.chromium.ui.modelutil.SimpleRecyclerViewAdapter;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.text.NumberFormat;
import java.util.List;

// Creating a list of shopping products
public class ShoppingProductListCoordinator
        implements Supplier<View>, ShoppingTasksSection.ViewMoreHandler {
    public static class HeaderProperties {
        public static final ReadableObjectPropertyKey<String> HEADER_STRING =
                new ReadableObjectPropertyKey<>();
        public static final PropertyKey[] ALL_KEYS = new PropertyKey[] {HEADER_STRING};
    }

    public static class ProductViewBinder {
        public static void bindProductItem(
                PropertyModel model, ViewGroup view, @Nullable PropertyKey propertyKey) {
            Log.d("Meil", "Bind Product");
            if (propertyKey == ShoppingProductProperties.PRODUCT_NAME) {
                TextView productName = view.findViewById(R.id.name);
                productName.setText(model.get(ShoppingProductProperties.PRODUCT_NAME));
            } else if (propertyKey == ShoppingProductProperties.PRICE) {
                TextView price = view.findViewById(R.id.price);
                NumberFormat priceFormat = NumberFormat.getCurrencyInstance();
                price.setText(priceFormat.format(model.get(ShoppingProductProperties.PRICE)));
            } else if (propertyKey == ShoppingProductProperties.PRICE_STR) {
                TextView price = view.findViewById(R.id.price);
                price.setText(model.get(ShoppingProductProperties.PRICE_STR));
            } else if (propertyKey == ShoppingProductProperties.URL) {
            } else if (propertyKey == ShoppingProductProperties.PRODUCT_IMAGE) {
            } else if (propertyKey == ShoppingProductProperties.IMAGE_FETCHER) {
                int width;
                Callback<Bitmap> callback;
                if (view.findViewById(R.id.image_container) == null) {
                    ImageView imageView = (ImageView) view.findViewById(R.id.image);

                    callback = result -> {
                        if (result == null) {
                            Log.d("Meil", "Fetcher return null bitmap");
                        } else {
                            imageView.setImageBitmap(result);
                        }
                    };
                    width = imageView.getWidth();

                } else {
                    ScrimableImageContainer imageContainer =
                            (ScrimableImageContainer) view.findViewById(R.id.image_container);

                    callback = result -> {
                        if (result == null) {
                            Log.d("Meil", "Fetcher return null bitmap");
                        } else {
                            imageContainer.setImageBitmap(result);
                        }
                    };
                    width = imageContainer.getWidth();
                }

                String imageUrl = model.get(ShoppingProductProperties.IMAGE_URL);
                ImageFetcher.Params params = ImageFetcher.Params.createWithExpirationInterval(
                        imageUrl, ImageFetcher.SHOPPING_TILE_UMA_CLIENT_NAME, width, width,
                        QueryTileConstants.IMAGE_EXPIRATION_INTERVAL_MINUTES);

                model.get(ShoppingProductProperties.IMAGE_FETCHER).fetchImage(params, callback);
            } else if (propertyKey == ShoppingProductProperties.ON_CLICK_CALLBACK) {
                String url = model.get(ShoppingProductProperties.URL);
                view.setOnClickListener((v) -> {
                    model.get(ShoppingProductProperties.ON_CLICK_CALLBACK).onResult(url);
                });
            }  else if (propertyKey == ShoppingProductProperties.BOOKMARK_CLICK_CALLBACK) {
                String url = model.get(ShoppingProductProperties.URL);
                view.findViewById(R.id.bookmark_product).setOnClickListener((v) -> {
                    model.get(ShoppingProductProperties.BOOKMARK_CLICK_CALLBACK).onResult(url);
                });
            } else if (propertyKey == ShoppingProductProperties.IS_BOOKMARKED) {
                if (model.get(ShoppingProductProperties.IS_BOOKMARKED)) {
                    ((ImageView) view.findViewById(R.id.bookmark_product)).setImageResource(R.drawable.btn_star_filled);
                } else {
                    ((ImageView) view.findViewById(R.id.bookmark_product)).setImageResource(R.drawable.btn_star);
                }
            } else if (propertyKey == ShoppingProductProperties.ITEM_CONTEXT_MENU_DELEGATE) {
                model.get(ShoppingProductProperties.ITEM_CONTEXT_MENU_DELEGATE)
                        .setItemPropertyModel(model, true);
                view.setOnCreateContextMenuListener(
                        model.get(ShoppingProductProperties.ITEM_CONTEXT_MENU_DELEGATE));
            } else if (propertyKey == ShoppingProductProperties.IS_RECENTLY_VIEWED) {
                boolean isRecentlyView = model.get(ShoppingProductProperties.IS_RECENTLY_VIEWED);
                ((TextView) view.findViewById(R.id.description))
                        .setText(isRecentlyView ? R.string.product_label_recently_view
                                                : R.string.product_label_recommended);
            }
        }
    }

    public static class FullSpanSuppliedViewProperties {
        public static WritableObjectPropertyKey<Supplier<View>> VIEW_SUPPLIER =
                new WritableObjectPropertyKey<>();

        public static final PropertyKey[] ALL_KEYS = new PropertyKey[] {VIEW_SUPPLIER};
    }

    public static class FullSpanSuppliedViewBinder {
        public static void bind(
                PropertyModel model, ViewGroup view, @Nullable PropertyKey propertyKey) {
            if (propertyKey == FullSpanSuppliedViewProperties.VIEW_SUPPLIER) {
                assert view instanceof FrameLayout;

                if (view.getChildCount() > 0) {
                    for (int i = 0; i < view.getChildCount(); i++) {
                        View child = view.getChildAt(i);
                        UiUtils.removeViewFromParent(child);
                    }
                    view.removeAllViews();
                }

                view.addView(model.get(FullSpanSuppliedViewProperties.VIEW_SUPPLIER).get(),
                        new ViewGroup.LayoutParams(
                                ViewGroup.LayoutParams.MATCH_PARENT, LayoutParams.WRAP_CONTENT));
            }
        }
    }

    public static class ProductLineProperties {
        public static final PropertyModel.WritableObjectPropertyKey<String> PRODUCT_NAME =
                new PropertyModel.WritableObjectPropertyKey<>();
        public static final PropertyModel.WritableObjectPropertyKey<String> SRP_URL =
                new PropertyModel.WritableObjectPropertyKey<>();
        public static final PropertyModel.WritableObjectPropertyKey<String> BRAND_NAME =
                new PropertyModel.WritableObjectPropertyKey<>();
        public static final PropertyModel.WritableObjectPropertyKey<String> IMAGE_URL =
                new PropertyModel.WritableObjectPropertyKey<>();
        public static final PropertyModel.WritableObjectPropertyKey<ImageFetcher> IMAGE_FETCHER =
                new PropertyModel.WritableObjectPropertyKey<>();
        public static final PropertyModel
                .WritableObjectPropertyKey<Callback<String>> ON_CLICK_CALLBACK =
                new PropertyModel.WritableObjectPropertyKey<>();
        public static final PropertyModel
                .WritableObjectPropertyKey<ContextMenuDelegate> ITEM_CONTEXT_MENU_DELEGATE =
                new WritableObjectPropertyKey<>();
        public static final PropertyModel
                .WritableObjectPropertyKey<Callback<String>> BOOKMARK_CLICK_CALLBACK =
                new PropertyModel.WritableObjectPropertyKey<>();
        public static final PropertyModel.WritableBooleanPropertyKey IS_BOOKMARKED =
                new PropertyModel.WritableBooleanPropertyKey();

        public static final PropertyKey[] ALL_KEYS = new PropertyKey[] {PRODUCT_NAME, SRP_URL,
                BRAND_NAME, IMAGE_FETCHER, IMAGE_URL, ON_CLICK_CALLBACK, ITEM_CONTEXT_MENU_DELEGATE,
                BOOKMARK_CLICK_CALLBACK, IS_BOOKMARKED};
    }

    public interface Observer {
        void onUpdated();
    }

    private final Context mContext;
    private final Supplier<EphemeralTabCoordinator> mEphemeralTabCoordinatorSupplier;

    private final RecyclerView mProductRecyclerView;
    private final MVCListAdapter.ModelList mProductListModel;
    private final SimpleRecyclerViewAdapter mAdapter;

    private final ShoppingProductListMediator mMediator;
    private final ObserverList<Observer> mObservers = new ObserverList<>();

    // Onboarding module.
    private final FrontDoorOnBoardingCoordinator mOnboardingViewProvider;

    @IntDef({ListType.GRID, ListType.LINEAR_HORIZONTAL, ListType.LINEAR_VERTICAL,
            ListType.STAGGERED})
    @Retention(RetentionPolicy.SOURCE)
    public @interface ListType {
        int GRID = 0;
        int LINEAR_HORIZONTAL = 1;
        int LINEAR_VERTICAL = 2;
        int STAGGERED = 3;
    }

    @IntDef({ItemType.OFFER_ITEM, ItemType.PRODUCT_LINE_ITEM, ItemType.SECTION_HEADER,
            ItemType.DIVIDER, ItemType.FILTER_HEADER, ItemType.BRAND_ITEM, ItemType.END_OF_FEED,
            ItemType.FULL_SPAN_SUPPLIED_VIEW})
    @Retention(RetentionPolicy.SOURCE)
    public @interface ItemType {
        int OFFER_ITEM = 0;
        int PRODUCT_LINE_ITEM = 1;
        int SECTION_HEADER = 2;
        int DIVIDER = 3;
        int FILTER_HEADER = 4;
        int BRAND_ITEM = 5;
        int END_OF_FEED = 6;
        int FULL_SPAN_SUPPLIED_VIEW = 7;
    }

    public interface OpenUrlHelper {
        void openUrl(String url, int disposition);
    }

    public interface ShareUrlHelper {
        void shareUrl(String url, String title);
    }

    private final FrontDoorFeedProvider mFeedProvider = new FrontDoorFeedProvider();
    // Get data for offer section
    DummyProductProvider mDummyProductProvider = new DummyProductProvider(-1);

    // TODO(meiliang): Add more to allow show all product items or partial items.
    public ShoppingProductListCoordinator(Context context,
            Supplier<EphemeralTabCoordinator> ephemeralTabCoordinatorSupplier, Profile profile,
            @ListType int listType, ModalDialogManager modalDialogManager,
            Supplier<ContextMenuManager> contextMenuManagerSupplier, OpenUrlHelper openUrlHelper,
            ShareUrlHelper shareUrlHelper) {
        mContext = context;
        mEphemeralTabCoordinatorSupplier = ephemeralTabCoordinatorSupplier;

        mOnboardingViewProvider = new FrontDoorOnBoardingCoordinator(
                context, modalDialogManager, this::handleChipToggle, mFeedProvider);

        mProductListModel = new MVCListAdapter.ModelList();
        mAdapter = new SimpleRecyclerViewAdapter(mProductListModel);

        mProductRecyclerView = new NestedRecyclerView(mContext);
        mProductRecyclerView.setId(R.id.product_list);
        mProductRecyclerView.setHasFixedSize(true);

        switch (listType) {
            case ListType.GRID:
                mProductRecyclerView.setLayoutManager(new GridLayoutManager(mContext, 2));
                break;
            case ListType.LINEAR_HORIZONTAL:
                mProductRecyclerView.setLayoutManager(
                        new LinearLayoutManager(mContext, LinearLayoutManager.HORIZONTAL, false));
                mProductRecyclerView.setLayoutParams(new LayoutParams(LayoutParams.MATCH_PARENT,
                        context.getResources().getDimensionPixelSize(
                                R.dimen.resume_recycler_view_height)));
                break;
            case ListType.LINEAR_VERTICAL:
                mProductRecyclerView.setLayoutManager(
                        new LinearLayoutManager(mContext, LinearLayoutManager.VERTICAL, false));
                break;
            case ListType.STAGGERED:
                mProductRecyclerView.setLayoutManager(
                        new StaggeredGridLayoutManager(2, StaggeredGridLayoutManager.VERTICAL));
        }

        mProductRecyclerView.setAdapter(mAdapter);

        RecyclerView.LayoutParams params = new RecyclerView.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT);
        mProductRecyclerView.setLayoutParams(params);

        // TODO(meiliang): Add recycler listen to verify view is being recycle. Not triggered since
        //  all product has been loaded to RecyclerView at once
        RecyclerView.RecyclerListener recyclerListener = (holder) -> {
            Log.d("Meil", "on recycle");
        };
        mProductRecyclerView.setRecyclerListener(recyclerListener);

        mAdapter.registerType(ItemType.PRODUCT_LINE_ITEM, parent -> {
            ViewGroup group = (ViewGroup) LayoutInflater.from(context).inflate(
                    R.layout.product_line_item_view, parent, false);

            if (listType == ListType.LINEAR_HORIZONTAL) {
                group.getLayoutParams().width =
                        context.getResources().getDimensionPixelSize(R.dimen.resume_item_width);
            }

            return group;
        }, ProductLineItemViewBinder::bind);

        mAdapter.registerType(ItemType.OFFER_ITEM, parent -> {
            ViewGroup group = (ViewGroup) LayoutInflater.from(context).inflate(
                    R.layout.offer_item_view, parent, false);

            return group;
        }, ProductViewBinder::bindProductItem);

        mAdapter.registerType(ItemType.DIVIDER, parent -> {
            View divier =
                    LayoutInflater.from(context).inflate(R.layout.divider_preference, null, false);

            if (mProductRecyclerView.getLayoutManager() instanceof StaggeredGridLayoutManager) {
                StaggeredGridLayoutManager.LayoutParams lp =
                        (StaggeredGridLayoutManager.LayoutParams) mProductRecyclerView
                                .getLayoutManager()
                                .generateDefaultLayoutParams();
                lp.setFullSpan(true);
                divier.setLayoutParams(lp);
            }

            return divier;
        }, (m, v, k) -> {});

        mAdapter.registerType(ItemType.SECTION_HEADER,
                parent
                -> {
                    ViewGroup header = (ViewGroup) LayoutInflater.from(context).inflate(
                            R.layout.shopping_section_header_view, null, false);

                    if (mProductRecyclerView.getLayoutManager()
                                    instanceof StaggeredGridLayoutManager) {
                        StaggeredGridLayoutManager.LayoutParams lp =
                                (StaggeredGridLayoutManager.LayoutParams) mProductRecyclerView
                                        .getLayoutManager()
                                        .generateDefaultLayoutParams();
                        lp.setFullSpan(true);
                        header.setLayoutParams(lp);
                    }

                    return header;
                },
                (m, v, k) -> {
                    ((TextView) v.findViewById(R.id.header_title))
                            .setText(m.get(HeaderProperties.HEADER_STRING));
                });

        mAdapter.registerType(ItemType.FULL_SPAN_SUPPLIED_VIEW, parent -> {
            FrameLayout layout = new FrameLayout(context);

            if (mProductRecyclerView.getLayoutManager() instanceof StaggeredGridLayoutManager) {
                StaggeredGridLayoutManager.LayoutParams lp =
                        (StaggeredGridLayoutManager.LayoutParams) mProductRecyclerView
                                .getLayoutManager()
                                .generateDefaultLayoutParams();
                lp.setFullSpan(true);
                layout.setLayoutParams(lp);
            }
            return layout;
        }, FullSpanSuppliedViewBinder::bind);

        mMediator = new ShoppingProductListMediator(mContext, mProductListModel, profile,
                ephemeralTabCoordinatorSupplier, mOnboardingViewProvider, mFeedProvider,
                mDummyProductProvider, contextMenuManagerSupplier, openUrlHelper, shareUrlHelper);

        mProductRecyclerView.addOnAttachStateChangeListener(new OnAttachStateChangeListener() {
            @Override
            public void onViewAttachedToWindow(View view) {
                assert view instanceof NestedRecyclerView;
                assert view.getParent() instanceof FrameLayout;
                assert view.getParent().getParent() instanceof RecyclerView;

                ((NestedRecyclerView) view)
                        .setParentRecycler((RecyclerView) view.getParent().getParent());
            }

            @Override
            public void onViewDetachedFromWindow(View view) {
                assert view instanceof NestedRecyclerView;

                ((NestedRecyclerView) view).setParentRecycler(null);
            }
        });
    }

    public void setProductList(List<ProductInfo> productList) {
        Log.e("Meil", "Set ProductList size: " + productList.size());
        mMediator.resetProductList(productList);
    }

    public void addOfferItem(List<ProductInfo> productList) {
        Log.e("Meil", "add offer item size: " + productList.size());

        mMediator.addOfferItem(productList);
    }

    public void addProductLineItem(List<ProductLineInfo> productList) {
        Log.e("Meil", "add prodcut line item size: " + productList.size());

        mMediator.addProductLineItem(productList);
    }

    public void addProduct(ProductInfo product) {
        mMediator.addProduct(product, 0, -1);
    }

    public void addObserver(Observer observer) {
        mObservers.addObserver(observer);
    }

    @Override
    public View get() {
        return mProductRecyclerView;
    }

    @Override
    public boolean hasMore() {
        return mMediator.hasMoreProduct();
    }

    @Override
    public void viewMore() {
        mMediator.viewMore();
        for (Observer observer : mObservers) {
            observer.onUpdated();
        }
    }

    private void handleChipToggle(String chipId, boolean isCategoryChip, boolean isChecked) {
        mMediator.chipToggleHandler(chipId, isCategoryChip, isChecked);
    }
}
