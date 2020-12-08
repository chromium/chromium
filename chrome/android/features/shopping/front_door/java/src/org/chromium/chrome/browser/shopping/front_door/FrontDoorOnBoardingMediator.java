package org.chromium.chrome.browser.shopping.front_door;

import static org.chromium.chrome.browser.shopping.front_door.ChipProperties.EDIT_CHIP_ID;

import android.content.Context;
import android.content.SharedPreferences;
import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.graphics.drawable.Drawable;
import android.text.TextUtils;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.ImageView;
import android.widget.TextView;

import androidx.annotation.IntDef;
import androidx.annotation.Nullable;

import org.chromium.base.Callback;
import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.base.ObserverList;
import org.chromium.chrome.browser.image_fetcher.ImageFetcher;
import org.chromium.chrome.browser.image_fetcher.ImageFetcherConfig;
import org.chromium.chrome.browser.image_fetcher.ImageFetcherFactory;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.shopping.front_door.ChipProperties.ToggleHandler;
import org.chromium.chrome.browser.shopping.front_door.FrontDoorOnBoardingCoordinator.OnboardingObserver;
import org.chromium.chrome.browser.shopping.front_door.Picker.ListType;
import org.chromium.chrome.browser.shopping.front_door.Picker.PickerItemPropertyModel;
import org.chromium.chrome.browser.shopping.front_door.ShoppingFeedFetcher.CountryCodeProvider;
import org.chromium.chrome.browser.ui.favicon.FaviconHelper;
import org.chromium.chrome.browser.ui.favicon.FaviconHelper.FaviconImageCallback;
import org.chromium.chrome.browser.ui.favicon.FaviconUtils;
import org.chromium.components.browser_ui.util.GlobalDiscardableReferencePool;
import org.chromium.components.favicon.LargeIconBridge;
import org.chromium.ui.modaldialog.DialogDismissalCause;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogManager.ModalDialogType;
import org.chromium.ui.modaldialog.ModalDialogProperties;
import org.chromium.ui.modelutil.ListModel;
import org.chromium.ui.modelutil.MVCListAdapter;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModel.WritableBooleanPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableObjectPropertyKey;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.HashMap;
import java.util.HashSet;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Locale;
import java.util.Map;
import java.util.Set;

class FrontDoorOnBoardingMediator {
    public static class OnBoardCategoryItemProperties {
        public static final PropertyModel.WritableObjectPropertyKey<String> ID =
                new PropertyModel.WritableObjectPropertyKey<>();
        public static final PropertyModel.WritableObjectPropertyKey<Bitmap> IMAGE_BITMAP =
                new PropertyModel.WritableObjectPropertyKey<>();
        public static final PropertyModel.WritableObjectPropertyKey<String> NAME =
                new PropertyModel.WritableObjectPropertyKey<>();
        public static final PropertyModel
                .WritableObjectPropertyKey<PickActionListener> ON_CLICK_CALLBACK =
                new PropertyModel.WritableObjectPropertyKey<>();

        public static final PropertyKey[] ALL_KEYS = new PropertyKey[] {ID, IMAGE_BITMAP, NAME,
                ON_CLICK_CALLBACK, PickerItemPropertyModel.IS_PICKABLE,
                PickerItemPropertyModel.PICKER_EFFECT_CALLBACK, PickerItemPropertyModel.IS_PICKED};
    }

    public static class CategoryItemViewBinder {
        public static void bind(
                PropertyModel model, ViewGroup view, @Nullable PropertyKey propertyKey) {
            if (propertyKey == OnBoardCategoryItemProperties.IMAGE_BITMAP) {
                ((ImageView) view.findViewById(R.id.image))
                        .setImageBitmap(model.get(OnBoardCategoryItemProperties.IMAGE_BITMAP));
            } else if (propertyKey == OnBoardCategoryItemProperties.NAME) {
                View textScrim = view.findViewById(R.id.scrim);
                ((TextView) textScrim.findViewById(R.id.name))
                        .setText(model.get(OnBoardCategoryItemProperties.NAME));
            } else if (propertyKey == OnBoardCategoryItemProperties.ON_CLICK_CALLBACK) {
                view.setOnClickListener((v) -> {
                    String id = model.get(OnBoardCategoryItemProperties.ID);
                    String name = model.get(OnBoardCategoryItemProperties.NAME);
                    boolean isPicked = model.get(PickerItemPropertyModel.IS_PICKED);

                    model.set(PickerItemPropertyModel.IS_PICKED, !isPicked);

                    model.get(OnBoardCategoryItemProperties.ON_CLICK_CALLBACK)
                            .run(id, name, !isPicked);
                });
            }
        }
    }

    @IntDef({BrandPickerItemType.HEADER, BrandPickerItemType.PICKABLE_ITEM,
            BrandPickerItemType.DIVIDER})
    @Retention(RetentionPolicy.SOURCE)
    public @interface BrandPickerItemType {
        int HEADER = 0;
        int PICKABLE_ITEM = 1;
        int DIVIDER = 2;
    }

    public static class OnBoardBrandItemProperties {
        public static final PropertyModel.WritableObjectPropertyKey<String> ID =
                new PropertyModel.WritableObjectPropertyKey<>();

        public static final PropertyModel.WritableObjectPropertyKey<String> NAME =
                new PropertyModel.WritableObjectPropertyKey<>();

        public static final PropertyModel.WritableObjectPropertyKey<String> DOMAIN_URL =
                new PropertyModel.WritableObjectPropertyKey<>();

        public static final PropertyModel.WritableObjectPropertyKey<String> LOGO_URL =
                new WritableObjectPropertyKey<>();

        public static final PropertyModel.WritableObjectPropertyKey<ImageFetcher> IMAGE_FETCHER =
                new PropertyModel.WritableObjectPropertyKey<>();

        public static final PropertyModel
                .WritableObjectPropertyKey<List<String>> REPRESENTATIVE_IMAGE_URLS =
                new PropertyModel.WritableObjectPropertyKey<>();

        public static final PropertyModel
                .WritableObjectPropertyKey<PickActionListener> ON_CLICK_CALLBACK =
                new PropertyModel.WritableObjectPropertyKey<>();

        public static final PropertyKey[] ALL_KEYS = new PropertyKey[] {
                PickerItemPropertyModel.IS_PICKABLE, PickerItemPropertyModel.IS_PICKED,
                PickerItemPropertyModel.PICKER_EFFECT_CALLBACK, ID, NAME, DOMAIN_URL, LOGO_URL,
                ON_CLICK_CALLBACK, IMAGE_FETCHER, REPRESENTATIVE_IMAGE_URLS};
    }

    public interface HeaderClickingHandler {
        void run(String id, boolean shouldExpand);
    }

    public static class OnBoardBrandPickerCategoryHeaderProperties {
        public static final PropertyModel.WritableObjectPropertyKey<String> ID =
                new WritableObjectPropertyKey<>();
        public static final PropertyModel.WritableObjectPropertyKey<String> NAME =
                new PropertyModel.WritableObjectPropertyKey<>();
        public static final PropertyModel.WritableBooleanPropertyKey IS_EXPANDED =
                new WritableBooleanPropertyKey();
        public static final PropertyModel
                .WritableObjectPropertyKey<HeaderClickingHandler> CLICK_HANDLER =
                new WritableObjectPropertyKey<>();

        public static final PropertyKey[] ALL_KEYS = new PropertyKey[] {
                PickerItemPropertyModel.IS_PICKABLE, ID, NAME, IS_EXPANDED, CLICK_HANDLER};
    }

    public static class BrandItemViewBinder {
        public static final int IMAGE_EXPIRATION_INTERVAL_MINUTES = 10;

        public static void bind(
                PropertyModel model, ViewGroup view, @Nullable PropertyKey propertyKey) {
            if (propertyKey == OnBoardBrandItemProperties.NAME) {
                ((TextView) view.findViewById(R.id.title))
                        .setText(model.get(OnBoardBrandItemProperties.NAME));
            } else if (propertyKey == OnBoardBrandItemProperties.DOMAIN_URL) {
                ((TextView) view.findViewById(R.id.description))
                        .setText(model.get(OnBoardBrandItemProperties.DOMAIN_URL));
            } else if (propertyKey == OnBoardBrandItemProperties.ON_CLICK_CALLBACK) {
                view.setOnClickListener((v) -> {
                    String brandId = model.get(OnBoardBrandItemProperties.ID);
                    String brandName = model.get(OnBoardBrandItemProperties.NAME);
                    boolean isPicked = model.get(PickerItemPropertyModel.IS_PICKED);

                    model.get(OnBoardBrandItemProperties.ON_CLICK_CALLBACK)
                            .run(brandId, brandName, !isPicked);
                    model.set(PickerItemPropertyModel.IS_PICKED, !isPicked);
                });
            } else if (propertyKey == OnBoardBrandItemProperties.IMAGE_FETCHER) {
                updateImages(view.findViewById(R.id.images_container),
                        ((ImageView) view.findViewById(R.id.favicon)), model);
            }
        }

        private static void updateImages(View imageViews, ImageView logoView, PropertyModel model) {
            assert imageViews != null;

            ImageFetcher fetcher = model.get(OnBoardBrandItemProperties.IMAGE_FETCHER);
            List<String> imageUrls =
                    model.get(OnBoardBrandItemProperties.REPRESENTATIVE_IMAGE_URLS);

            assert imageUrls.size() == 3;

            String logo = model.get(OnBoardBrandItemProperties.LOGO_URL);
            if (!TextUtils.isEmpty(logo)) {
                logoView.setVisibility(View.VISIBLE);
                fetchAndUpdateImage(logoView, fetcher, logo);
            } else {
                logoView.setVisibility(View.GONE);
            }

            ImageView startImageView = (ImageView) imageViews.findViewById(R.id.start_image);
            fetchAndUpdateImage(startImageView, fetcher, imageUrls.get(0));

            ImageView endTopImageView = (ImageView) imageViews.findViewById(R.id.end_top_image);
            fetchAndUpdateImage(endTopImageView, fetcher, imageUrls.get(1));

            ImageView endBottomImageView =
                    (ImageView) imageViews.findViewById(R.id.end_bottom_image);
            fetchAndUpdateImage(endBottomImageView, fetcher, imageUrls.get(2));
        }

        private static void fetchAndUpdateImage(
                ImageView imageView, ImageFetcher fetcher, String imageUrl) {
            Callback<Bitmap> callback = bitmap -> {
                if (bitmap == null) {
                    Log.e("Meil", "Fetcher return null bitmap for start image");
                } else {
                    imageView.setImageBitmap(bitmap);
                }
            };

            ImageFetcher.Params params = ImageFetcher.Params.createWithExpirationInterval(imageUrl,
                    ImageFetcher.SHOPPING_TILE_UMA_CLIENT_NAME, imageView.getWidth(),
                    imageView.getHeight(), IMAGE_EXPIRATION_INTERVAL_MINUTES);

            fetcher.fetchImage(params, callback);
        }
    }

    public static class BrandHeaderItemViewBinder {
        public static void bind(PropertyModel model, View view, @Nullable PropertyKey propertyKey) {
            if (propertyKey == OnBoardBrandPickerCategoryHeaderProperties.NAME) {
                TextView nameView = (TextView) view.findViewById(R.id.header);
                nameView.setText(model.get(OnBoardBrandPickerCategoryHeaderProperties.NAME));
            } else if (propertyKey == OnBoardBrandPickerCategoryHeaderProperties.CLICK_HANDLER) {
                view.setOnClickListener((v) -> {
                    model.get(OnBoardBrandPickerCategoryHeaderProperties.CLICK_HANDLER)
                            .run(model.get(OnBoardBrandPickerCategoryHeaderProperties.ID),
                                    !model.get(OnBoardBrandPickerCategoryHeaderProperties
                                                       .IS_EXPANDED));
                });
            } else if (propertyKey == OnBoardBrandPickerCategoryHeaderProperties.IS_EXPANDED) {
                boolean isExpanded =
                        model.get(OnBoardBrandPickerCategoryHeaderProperties.IS_EXPANDED);
                ImageView iconView = (ImageView) view.findViewById(R.id.icon);
                if (isExpanded) {
                    iconView.setImageResource(R.drawable.ic_expand_less_black_24dp);
                } else {
                    iconView.setImageResource(R.drawable.ic_expand_more_black_24dp);
                }
            }
        }
    }

    @IntDef({OnboardingStage.GET_STARTED, OnboardingStage.PICK_CATEGORIES,
            OnboardingStage.PICK_BRANDS, OnboardingStage.DONE})
    @Retention(RetentionPolicy.SOURCE)
    @interface OnboardingStage {
        int GET_STARTED = 0;
        int PICK_CATEGORIES = 1;
        int PICK_BRANDS = 3;
        int DONE = 4;
    }

    public interface PickActionListener {
        void run(String id, String name, boolean isPicked);
    }

    private static final boolean BRAND_LIST_PICKED_ZOOM = false;

    private static final boolean USING_BRAND_CARD = true;

    private static final int MINIMUM_CATEGORY_PICKING_COUNT = 3;
    private static final int MINIMUM_BRAND_PICKING_COUNT = 6;

    private static final String PREFERENCES_NAME = "frontdoor_shopping_cache";
    private static SharedPreferences sPref;
    private static SharedPreferences getSharedPreferences() {
        if (sPref == null) {
            sPref = ContextUtils.getApplicationContext().getSharedPreferences(
                    PREFERENCES_NAME, Context.MODE_PRIVATE);
        }
        return sPref;
    }

    private final Context mContext;

    @OnboardingStage
    private int mStage = OnboardingStage.GET_STARTED;

    private View mOnboardingPromoCard;
    private Runnable mDismissOnboardPromoCardHandler;

    private final ModalDialogManager mModalDialogManager;
    private final PropertyModel mDialogModel;

    private final Picker mCategoriesPicker;
    private final MVCListAdapter.ModelList mCategoriesModel;

    private final Picker mBrandsPicker;
    private final MVCListAdapter.ModelList mBrandsModel;

    private final List<String> mInterestedCategories = new ArrayList<>();
    private final Map<String, String> mInterestedCategoryIdToNameMap = new LinkedHashMap<>();

    private final List<String> mInterestedBrands = new ArrayList<>();
    private final Map<String, String> mInterestedBrandIdToNameMap = new LinkedHashMap<>();

    private final Map<String, CategoryBrandAdapterPositionInfo> mCategoryToBrandAdapterInfoMap =
            new HashMap<>();

    private final List<Integer> mCategoryThumbnailResourceIds;
    private final List<Integer> mCategoryNameResourceIds;
    private final List<String> mCategoryKeys;

    private final ListModel<PropertyModel> mInterestedCategoriesAndBrandsChipModel =
            new ListModel<>();

    private final ToggleHandler mChipToggleHandler;
    private final OnboardCategoryAndBrandProvider mDataProvider;

    private final FaviconHelper mFaviconHelper = new FaviconHelper();
    private final LargeIconBridge mServerFaviconHelper;

    private final ObserverList<OnboardingObserver> mObservers = new ObserverList<>();
    private final CountryCodeProvider mCountryCodeProvider;

    FrontDoorOnBoardingMediator(Context context, ModalDialogManager modalDialogManager,
            ToggleHandler chipToggleHandler, OnboardCategoryAndBrandProvider dataProvider,
            CountryCodeProvider countryCodeProvider) {
        mContext = context;
        mModalDialogManager = modalDialogManager;
        mChipToggleHandler = chipToggleHandler;
        mDataProvider = dataProvider;
        mServerFaviconHelper = new LargeIconBridge(Profile.getLastUsedRegularProfile());
        mCountryCodeProvider = countryCodeProvider;

        mCategoryThumbnailResourceIds = new ArrayList<>(
                Arrays.asList(R.drawable.Thumbnail_Electronics, R.drawable.Thumbnail_Beauty,
                        R.drawable.Thumbnail_Home_and_Garden, R.drawable.Thumbnail_Apparel,
                        R.drawable.Thumbnail_Home_Improvement_and_Tools,
                        R.drawable.Thumbnail_Toys_and_Games, R.drawable.Thumbnail_Baby_and_Kids,
                        R.drawable.Thumbnail_Sports_and_Outfoors,
                        R.drawable.Thumbnail_Travel_Luggage_and_Bags));

        mCategoryNameResourceIds = new ArrayList<>(Arrays.asList(R.string.category_electronics,
                R.string.category_health_and_beauty, R.string.category_home_and_garden,
                R.string.category_apparel, R.string.category_home_and_tools,
                R.string.category_toys_and_games, R.string.category_baby_and_kids,
                R.string.category_sports_and_outdoors, R.string.category_travel_and_bags));

        mCategoryKeys = new ArrayList<>(
                Arrays.asList("electronics", "health_beauty", "home_garden", "apparel",
                        "home_tools", "toys_games", "baby_kids", "sports_outdoors", "travel_bags"));

        ModalDialogProperties.Controller onBoardDialogController =
                new ModalDialogProperties.Controller() {
                    @Override
                    public void onClick(PropertyModel model, int buttonType) {
                        assert mStage == OnboardingStage.PICK_CATEGORIES
                                || mStage == OnboardingStage.PICK_BRANDS;

                        if (buttonType == ModalDialogProperties.ButtonType.POSITIVE) {
                            if (mStage == OnboardingStage.PICK_CATEGORIES) {
                                // Finish picking categories, show the brands next.
                                dataProvider.getBrandsForCategoriesWithCallback(
                                        mInterestedCategories,
                                        (map) -> setUpBrands(map), mCountryCodeProvider);
                            } else {
                                // Finish picking brands, finished the onboarding process.
                                doneOnboarding();
                                modalDialogManager.dismissDialog(
                                        model, DialogDismissalCause.POSITIVE_BUTTON_CLICKED);
                            }
                        } else {
                            if (mStage == OnboardingStage.PICK_CATEGORIES) {
                                // Back out the onboarding process.
                                modalDialogManager.dismissDialog(
                                        model, DialogDismissalCause.NEGATIVE_BUTTON_CLICKED);
                            } else {
                                // Go back to picking categories.
                                setUpCategories();
                            }
                        }
                    }

                    @Override
                    public void onDismiss(PropertyModel model, int dismissalCause) {
                        mStage = OnboardingStage.GET_STARTED;
                        resetAllPickerData();
                    }
                };

        mDialogModel = new PropertyModel.Builder(ModalDialogProperties.ALL_KEYS)
                               .with(ModalDialogProperties.PRIMARY_BUTTON_FILLED, true)
                               .with(ModalDialogProperties.CONTROLLER, onBoardDialogController)
                               .with(ModalDialogProperties.POSITIVE_BUTTON_DISABLED, true)
                               .build();

        mCategoriesModel = new MVCListAdapter.ModelList();
        mCategoriesPicker = new Picker(context, mCategoriesModel, ListType.GRID, true);
        mCategoriesPicker.registerItemView(0, parent -> {
            ViewGroup group = (ViewGroup) LayoutInflater.from(context).inflate(
                    R.layout.onboard_category_item_view, parent, false);
            ScrimableImageContainer imageContainer = group.findViewById(R.id.item);
            imageContainer.scrimViewVisibility(View.VISIBLE);
            View nameView = LayoutInflater.from(context).inflate(
                    R.layout.category_name_text_view, parent, false);
            imageContainer.insertToScrimView(nameView);
            return group;
        }, CategoryItemViewBinder::bind);

        mBrandsModel = new MVCListAdapter.ModelList();

        if (USING_BRAND_CARD) {
            mBrandsPicker = new Picker(context, mBrandsModel, ListType.GRID, false);

            mBrandsPicker.registerItemView(BrandPickerItemType.PICKABLE_ITEM, parent -> {
                ViewGroup group = (ViewGroup) LayoutInflater.from(context).inflate(
                        R.layout.brand_card_item_view, parent, false);

                return group;
            }, BrandItemViewBinder::bind, 1);

        } else {
            mBrandsPicker = new Picker(context, mBrandsModel, ListType.LINEAR_VERTICAL, false);

            mBrandsPicker.registerItemView(BrandPickerItemType.PICKABLE_ITEM, parent -> {
                ViewGroup group;

                if (BRAND_LIST_PICKED_ZOOM) {
                    group = (ViewGroup) LayoutInflater.from(context).inflate(
                            R.layout.brand_picker_brand_item, parent, false);
                } else {
                    group = (ViewGroup) LayoutInflater.from(context).inflate(
                            R.layout.brand_picker_brand_checkbox_item, parent, false);
                }

                return group;
            }, BrandItemViewBinder::bind);

            mBrandsPicker.registerItemView(BrandPickerItemType.DIVIDER, parent -> {
                return LayoutInflater.from(context).inflate(
                        R.layout.divider_preference, null, false);
            }, (model, view, propertyKey) -> {});
        }

        mBrandsPicker.registerItemView(BrandPickerItemType.HEADER, parent -> {
            View group = LayoutInflater.from(context).inflate(
                    R.layout.brand_picker_category_header, parent, false);

            return group;
        }, BrandHeaderItemViewBinder::bind, 3);

        restoreAllCategoryAndBrands();
    }

    View getOnboardPromoCard(Runnable getStartedCallback) {
        assert mStage == OnboardingStage.GET_STARTED;

        if (mOnboardingPromoCard == null) {
            mOnboardingPromoCard = LayoutInflater.from(mContext).inflate(
                    R.layout.front_door_onboard_promo_view, null, false);
            mOnboardingPromoCard.findViewById(R.id.get_started).setOnClickListener((v) -> {
                showOnbardingDialog();
            });
            mDismissOnboardPromoCardHandler = getStartedCallback;
        }

        return mOnboardingPromoCard;
    }

    private void showOnbardingDialog() {
        assert mStage == OnboardingStage.GET_STARTED || mStage == OnboardingStage.DONE;

        setUpCategories();
        mModalDialogManager.showDialog(mDialogModel, ModalDialogType.APP);
    }

    private void setUpCategories() {
        assert mStage == OnboardingStage.GET_STARTED || mStage == OnboardingStage.PICK_BRANDS;

        mStage = OnboardingStage.PICK_CATEGORIES;

        String continueString = mContext.getString(R.string.onboarding_category_picker_continue);
        String laterString = mContext.getString(R.string.onboarding_category_picker_back);

        mDialogModel.set(ModalDialogProperties.TITLE, getCategoryPickerTitle());
        mDialogModel.set(ModalDialogProperties.POSITIVE_BUTTON_TEXT, continueString);
        mDialogModel.set(ModalDialogProperties.NEGATIVE_BUTTON_TEXT, laterString);
        mDialogModel.set(ModalDialogProperties.POSITIVE_BUTTON_DISABLED,
                mInterestedCategoryIdToNameMap.size() < MINIMUM_CATEGORY_PICKING_COUNT);

        setUpCategoryItemModel();

        mDialogModel.set(ModalDialogProperties.CUSTOM_VIEW, mCategoriesPicker.getView());
    }

    private String getCategoryPickerTitle() {
        return String.format(Locale.getDefault(),
                       mContext.getString(R.string.onboarding_category_picker_title),
                       String.valueOf(MINIMUM_CATEGORY_PICKING_COUNT))
                + getPickCountIndicatorString(true);
    }

    private String getPickCountIndicatorString(boolean isCategoryPicking) {
        int pickCount;
        int expectedCount;
        if (isCategoryPicking) {
            pickCount = mInterestedCategories.size();
            expectedCount = MINIMUM_CATEGORY_PICKING_COUNT;
        } else {
            pickCount = mInterestedBrands.size();
            expectedCount = MINIMUM_BRAND_PICKING_COUNT;
        }
        return " (" + pickCount + "/" + expectedCount + ")";
    }

    private void pickCategoryListener(String id, String name, boolean isPicked) {
        if (isPicked) {
            Log.d("Meil", "pick cate: " + id);
            mInterestedCategoryIdToNameMap.put(id, name);
            mInterestedCategories.add(id);
        } else {
            Log.d("Meil", "unpick cate: " + id);
            mInterestedCategoryIdToNameMap.remove(id);
            mInterestedCategories.remove(id);
        }
        mDialogModel.set(ModalDialogProperties.TITLE, getCategoryPickerTitle());
        mDialogModel.set(ModalDialogProperties.POSITIVE_BUTTON_DISABLED,
                mInterestedCategoryIdToNameMap.size() < 3);
    }

    private void setUpCategoryItemModel() {
        if (mCategoriesModel.size() != 0) return;

        for (int i = 0; i < mCategoryThumbnailResourceIds.size(); i++) {
            Bitmap thumbnail = BitmapFactory.decodeResource(
                    mContext.getResources(), mCategoryThumbnailResourceIds.get(i));
            String categoryName =
                    mContext.getResources().getString(mCategoryNameResourceIds.get(i));
            PropertyModel itemModel =
                    new PropertyModel.Builder(OnBoardCategoryItemProperties.ALL_KEYS)
                            .with(PickerItemPropertyModel.IS_PICKABLE, true)
                            .with(PickerItemPropertyModel.IS_PICKED,
                                    getCategorySet().contains(mCategoryKeys.get(i)))
                            .with(OnBoardCategoryItemProperties.ID, mCategoryKeys.get(i))
                            .with(OnBoardCategoryItemProperties.NAME, categoryName)
                            .with(OnBoardCategoryItemProperties.IMAGE_BITMAP, thumbnail)
                            .with(OnBoardCategoryItemProperties.ON_CLICK_CALLBACK,
                                    this::pickCategoryListener)
                            .build();

            mCategoriesModel.add(new MVCListAdapter.ListItem(0, itemModel));
        }
    }

    private void setUpBrands(Map<String, List<BrandInfo>> categoryKeyToBrandInfoMap) {
        assert mStage == OnboardingStage.PICK_CATEGORIES;

        Log.e("Meil_OnboardingMediator", "result map: " + categoryKeyToBrandInfoMap.toString());

        mStage = OnboardingStage.PICK_BRANDS;

        String doneString = mContext.getString(R.string.onboarding_brand_picker_done);
        String backString = mContext.getString(R.string.onboarding_brand_picker_back);

        mDialogModel.set(ModalDialogProperties.TITLE, getBrandPickerTitle());
        mDialogModel.set(ModalDialogProperties.POSITIVE_BUTTON_TEXT, doneString);
        mDialogModel.set(ModalDialogProperties.NEGATIVE_BUTTON_TEXT, backString);
        mDialogModel.set(ModalDialogProperties.POSITIVE_BUTTON_DISABLED,
                mInterestedBrandIdToNameMap.size() < MINIMUM_BRAND_PICKING_COUNT);

        setUpBrandItemModel(categoryKeyToBrandInfoMap);

        mDialogModel.set(ModalDialogProperties.CUSTOM_VIEW, mBrandsPicker.getView());
    }

    private String getBrandPickerTitle() {
        return String.format(Locale.getDefault(),
                       mContext.getString(R.string.onboarding_brand_picker_title),
                       String.valueOf(MINIMUM_BRAND_PICKING_COUNT))
                + getPickCountIndicatorString(false);
    }

    private void setUpBrandItemModel(Map<String, List<BrandInfo>> categoryKeyToBrandInfoMap) {
        mBrandsModel.clear();

        for (Map.Entry<String, List<BrandInfo>> entry : categoryKeyToBrandInfoMap.entrySet()) {
            String categoryId = entry.getKey();
            // TODO(meiliang): get category client name based on categoryId.
            int keysIndex = mCategoryKeys.indexOf(categoryId);

            if (keysIndex == -1) {
                assert keysIndex
                        != -1 : "CategoryKey: " + categoryId + " should be in the known key set";
                Log.e("Meil_OnboardingMediator", "categoryKey: " + categoryId + " keysIndex is -1");
                continue;
            }
            String CategoryName =
                    mContext.getResources().getString(mCategoryNameResourceIds.get(keysIndex));

            PropertyModel header =
                    new PropertyModel.Builder(OnBoardBrandPickerCategoryHeaderProperties.ALL_KEYS)
                            .with(PickerItemPropertyModel.IS_PICKABLE, false)
                            .with(OnBoardBrandPickerCategoryHeaderProperties.ID, categoryId)
                            .with(OnBoardBrandPickerCategoryHeaderProperties.NAME, CategoryName)
                            .with(OnBoardBrandPickerCategoryHeaderProperties.IS_EXPANDED, true)
                            .with(OnBoardBrandPickerCategoryHeaderProperties.CLICK_HANDLER,
                                    this::headerOnClick)
                            .build();
            mBrandsModel.add(new ListItem(BrandPickerItemType.HEADER, header));

            ModelList brandModelList = createBrandModelList(entry.getValue());

            mCategoryToBrandAdapterInfoMap.put(categoryId,
                    new CategoryBrandAdapterPositionInfo(
                            mBrandsModel.size(), brandModelList.size(), true));

            mBrandsModel.addAll(brandModelList);

            if (!USING_BRAND_CARD) mBrandsModel.add(getDividerItem());
        }
    }

    private ModelList createBrandModelList(List<BrandInfo> brandInfos) {
        MVCListAdapter.ModelList modelList = new ModelList();

        for (BrandInfo info : brandInfos) {
            ImageFetcher imageFetcher = ImageFetcherFactory.createImageFetcher(
                    ImageFetcherConfig.NETWORK_ONLY, Profile.getLastUsedRegularProfile(),
                    GlobalDiscardableReferencePool.getReferencePool());

            PropertyModel item =
                    new PropertyModel.Builder(OnBoardBrandItemProperties.ALL_KEYS)
                            .with(PickerItemPropertyModel.IS_PICKABLE, true)
                            .with(PickerItemPropertyModel.IS_PICKED,
                                    mInterestedBrandIdToNameMap.containsKey(info.id))
                            .with(OnBoardBrandItemProperties.ID, info.id)
                            .with(OnBoardBrandItemProperties.NAME, info.name)
                            .with(OnBoardBrandItemProperties.DOMAIN_URL, info.url)
                            .with(OnBoardBrandItemProperties.IMAGE_FETCHER, imageFetcher)
                            .with(OnBoardBrandItemProperties.REPRESENTATIVE_IMAGE_URLS,
                                    info.imageUrls)
                            .with(OnBoardBrandItemProperties.LOGO_URL, info.logoUrl)
                            .with(OnBoardBrandItemProperties.ON_CLICK_CALLBACK,
                                    this::pickBrandListener)
                            .build();

            int size =
                    mContext.getResources().getDimensionPixelSize(R.dimen.brand_card_favicon_size);

            Callback<Bitmap> updateFavicon = (bitmap) -> {
                assert bitmap != null : "Meil Favicon bitmap should not be null";
                Drawable favicon = FaviconUtils.createRoundedBitmapDrawable(mContext.getResources(),
                        Bitmap.createScaledBitmap(bitmap, size, size, true));
                // item.set(OnBoardBrandItemProperties.FAVICON, favicon);
            };

            FaviconImageCallback faviconCallback = (bitmap, url)
                    -> {
                            // TODO(meiliang): Update after we support favicon instead of using
                            // brand logo.
                            // if (bitmap == null) {
                            //     Log.e("Meil_Favicon",
                            //             "local favicon is null for brand url: " + info.url + ";
                            //             Try server");
                            //
                            //     final GURL gurlOrigin = new GURL(info.url);
                            //     assert gurlOrigin.isValid() : "Meil Favicon gurl should be
                            //     valid";
                            //
                            //     mServerFaviconHelper.getLargeIconForUrl(gurlOrigin, size,
                            //             (icon, fallbackColor, isFallbackColorDefault, iconType)
                            //             -> {
                            //                 if (icon == null) {
                            //                     Log.e("Meil_Favicon",
                            //                             "Server favicon is null for brand url: "
                            //                             + info.url);
                            //                     return;
                            //                 }
                            //                 updateFavicon.onResult(icon);
                            //             });
                            //
                            // } else {
                            //     updateFavicon.onResult(bitmap);
                            // }
                    };

            mFaviconHelper.getLocalFaviconImageForURL(
                    Profile.getLastUsedRegularProfile(), info.url, size, faviconCallback);

            modelList.add(new ListItem(BrandPickerItemType.PICKABLE_ITEM, item));
        }

        return modelList;
    }

    private void pickBrandListener(String id, String name, boolean isPicked) {
        if (isPicked) {
            Log.d("Meil", "picked add id: " + id);
            mInterestedBrandIdToNameMap.put(id, name);
            mInterestedBrands.add(id);
        } else {
            Log.d("Meil", "unpicked remove id: " + id);
            mInterestedBrandIdToNameMap.remove(id);
            mInterestedBrands.remove(id);
        }
        mDialogModel.set(ModalDialogProperties.TITLE, getBrandPickerTitle());
        mDialogModel.set(ModalDialogProperties.POSITIVE_BUTTON_DISABLED,
                mInterestedBrandIdToNameMap.size() < MINIMUM_BRAND_PICKING_COUNT);
    }

    private class CategoryBrandAdapterPositionInfo {
        public int index;
        public int size;
        public boolean isInAdapter;

        CategoryBrandAdapterPositionInfo(int index, int size, boolean isInAdapter) {
            this.index = index;
            this.size = size;
            this.isInAdapter = isInAdapter;
        }
    }

    // Values are from adapter when header collapse.
    private Map<String, List<ListItem>> mCategoryToBrandsMap = new HashMap<>();

    private void headerOnClick(String categoryId, boolean shouldExpand) {
        CategoryBrandAdapterPositionInfo info = mCategoryToBrandAdapterInfoMap.get(categoryId);

        assert shouldExpand != info.isInAdapter;

        if (shouldExpand) {
            List<ListItem> modelList = mCategoryToBrandsMap.get(categoryId);
            mBrandsModel.addAll(modelList, info.index);
            info.isInAdapter = true;
            mCategoryToBrandsMap.remove(categoryId);
        } else {
            List<ListItem> modelList = mBrandsModel.sublist(info.index, info.index + info.size);
            mCategoryToBrandsMap.put(categoryId, modelList);

            mBrandsModel.removeRange(info.index, info.size);
            info.isInAdapter = false;
        }

        updateBrandAdapterInfoMap(info.index, info.size, shouldExpand);

        mBrandsModel.get(info.index - 1)
                .model.set(OnBoardBrandPickerCategoryHeaderProperties.IS_EXPANDED, shouldExpand);
    }

    private void updateBrandAdapterInfoMap(int index, int size, boolean isAdded) {
        for (CategoryBrandAdapterPositionInfo info : mCategoryToBrandAdapterInfoMap.values()) {
            if (info.index <= index) continue;

            if (isAdded) {
                info.index += size;
            } else {
                info.index -= size;
            }
        }
    }

    private List<String> getBrandIdsForCategory(String id) {
        List<String> ids = new ArrayList<>();
        for (int i = 0; i < 10; i++) {
            ids.add(id + " " + i);
        }

        return ids;
    }

    private String getCategoryName(int id) {
        return "Cat " + id;
    }

    private ListItem getDividerItem() {
        PropertyModel dividerModel = new PropertyModel.Builder(PickerItemPropertyModel.IS_PICKABLE)
                                             .with(PickerItemPropertyModel.IS_PICKABLE, false)
                                             .build();

        return new ListItem(BrandPickerItemType.DIVIDER, dividerModel);
    }

    private void doneOnboarding() {
        assert mStage == OnboardingStage.PICK_BRANDS;

        mStage = OnboardingStage.DONE;

        if (mDismissOnboardPromoCardHandler != null) {
            mDismissOnboardPromoCardHandler.run();
            mDismissOnboardPromoCardHandler = null;
        }

        updateInterestedCategoriesAndBrands();

        cacheAllCategoryAndBrands();
        for (OnboardingObserver observer : mObservers) {
            observer.onDone(mInterestedBrands);
        }
    }

    private void updateInterestedCategoriesAndBrands() {
        mInterestedCategoriesAndBrandsChipModel.clear();

        for (Map.Entry<String, String> entry : mInterestedCategoryIdToNameMap.entrySet()) {
            PropertyModel model =
                    new PropertyModel.Builder(ChipProperties.ALL_KEYS)
                            .with(ChipProperties.ID, entry.getKey())
                            .with(ChipProperties.IS_CATEGORY_CHIP, true)
                            .with(ChipProperties.TEXT, entry.getValue())
                            .with(ChipProperties.TOGGLE_ACTION_HANDLER, mChipToggleHandler)
                            .build();
            mInterestedCategoriesAndBrandsChipModel.add(model);
        }

        for (Map.Entry<String, String> entry : mInterestedBrandIdToNameMap.entrySet()) {
            PropertyModel model =
                    new PropertyModel.Builder(ChipProperties.ALL_KEYS)
                            .with(ChipProperties.ID, entry.getKey())
                            .with(ChipProperties.IS_CATEGORY_CHIP, false)
                            .with(ChipProperties.TEXT, entry.getValue())
                            .with(ChipProperties.TOGGLE_ACTION_HANDLER, mChipToggleHandler)
                            .build();
            mInterestedCategoriesAndBrandsChipModel.add(model);
        }

        PropertyModel editChipModel =
                new PropertyModel.Builder(ChipProperties.ALL_KEYS)
                        .with(ChipProperties.ID, EDIT_CHIP_ID)
                        .with(ChipProperties.ICON_RESOURCE_ID, R.drawable.ic_edit_24dp)
                        .with(ChipProperties.TOGGLE_ACTION_HANDLER,
                                (a, b, c) -> { showOnbardingDialog(); })
                        .build();
        mInterestedCategoriesAndBrandsChipModel.add(editChipModel);
    }

    private void resetAllPickerData() {
        Log.d("Meil", "Reset AllPickerData");
        mCategoriesModel.clear();
        mBrandsModel.clear();
        restoreAllCategoryAndBrands();
    }

    List<String> getInterestedCategories() {
        assert mStage == OnboardingStage.DONE;

        return mInterestedCategories;
    }

    List<String> getInterestedBrands() {
        return mInterestedBrands;
    }

    boolean hasDoneOnboarding() {
        return mStage == OnboardingStage.DONE
                || (!getBrandSet().isEmpty() && !getCategorySet().isEmpty());
    }

    ListModel<PropertyModel> getInterestedCategoriesAndBrandsChipModel() {
        return mInterestedCategoriesAndBrandsChipModel;
    }

    void addObserver(OnboardingObserver observer) {
        mObservers.addObserver(observer);
    }

    private void cacheAllCategoryAndBrands() {
        cacheCategorySet(mInterestedCategoryIdToNameMap.keySet());
        cacheBrandSet(mInterestedBrandIdToNameMap.keySet());
        for (Map.Entry<String, String> entry : mInterestedCategoryIdToNameMap.entrySet()) {
            cacheCategoryName(entry.getKey(), entry.getValue());
        }
        for (Map.Entry<String, String> entry : mInterestedBrandIdToNameMap.entrySet()) {
            cacheBrandName(entry.getKey(), entry.getValue());
        }
    }

    private void restoreAllCategoryAndBrands() {
        mInterestedCategoryIdToNameMap.clear();
        mInterestedCategories.clear();

        mInterestedBrandIdToNameMap.clear();
        mInterestedBrands.clear();

        for (String categoryId : getCategorySet()) {
            mInterestedCategories.add(categoryId);
            mInterestedCategoryIdToNameMap.put(categoryId, getCategoryName(categoryId));
        }

        for (String brandId : getBrandSet()) {
            mInterestedBrands.add(brandId);
            mInterestedBrandIdToNameMap.put(brandId, getBrandName(brandId));
        }

        updateInterestedCategoriesAndBrands();

        Log.e("Meil_OnboardingMediator",
                "mInterestedCategories after restore: " + mInterestedCategories);
        Log.e("Meil_OnboardingMediator",
                "mInterestedCategoryIdToNameMap after restore: " + mInterestedCategoryIdToNameMap);
        Log.e("Meil_OnboardingMediator", "mInterestedBrands after restore: " + mInterestedBrands);
        Log.e("Meil_OnboardingMediator",
                "mInterestedBrandIdToNameMap after restore: " + mInterestedBrandIdToNameMap);
    }

    private static Set<String> getCategorySet() {
        return getSharedPreferences().getStringSet("FrontDoor_Category_List_Key", new HashSet<>());
    }

    private static void cacheCategorySet(Set<String> categories) {
        getSharedPreferences()
                .edit()
                .putStringSet("FrontDoor_Category_List_Key", categories)
                .apply();
    }

    private static Set<String> getBrandSet() {
        return getSharedPreferences().getStringSet("FrontDoor_Brand_List_Key", new HashSet<>());
    }

    private static void cacheBrandSet(Set<String> brands) {
        getSharedPreferences().edit().putStringSet("FrontDoor_Brand_List_Key", brands).apply();
    }

    private static String getBrandName(String id) {
        return getSharedPreferences().getString("Brand_id_key_" + id, "");
    }

    private static void cacheBrandName(String id, String brandName) {
        getSharedPreferences().edit().putString("Brand_id_key_" + id, brandName).apply();
    }

    private static String getCategoryName(String id) {
        return getSharedPreferences().getString("Category_id_key_" + id, "");
    }

    private static void cacheCategoryName(String id, String categoryName) {
        getSharedPreferences().edit().putString("Category_id_key_" + id, categoryName).apply();
    }
}
