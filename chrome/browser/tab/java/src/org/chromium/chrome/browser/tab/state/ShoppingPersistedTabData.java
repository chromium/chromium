// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab.state;

import androidx.annotation.IntDef;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import com.google.common.primitives.UnsignedLongs;
import com.google.protobuf.InvalidProtocolBufferException;

import org.chromium.base.Callback;
import org.chromium.base.FeatureList;
import org.chromium.base.Log;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.build.annotations.DoNotClassMerge;
import org.chromium.chrome.browser.commerce.PriceUtils;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.optimization_guide.OptimizationGuideBridge;
import org.chromium.chrome.browser.optimization_guide.OptimizationGuideBridgeFactory;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileManager;
import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.proto.ShoppingPersistedTabData.ShoppingPersistedTabDataProto;
import org.chromium.components.commerce.PriceTracking.BuyableProduct;
import org.chromium.components.commerce.PriceTracking.PriceTrackingData;
import org.chromium.components.commerce.PriceTracking.ProductPriceUpdate;
import org.chromium.components.optimization_guide.OptimizationGuideDecision;
import org.chromium.components.optimization_guide.proto.HintsProto;
import org.chromium.components.payments.CurrencyFormatter;
import org.chromium.content_public.browser.NavigationHandle;
import org.chromium.ui.base.PageTransition;
import org.chromium.url.GURL;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.nio.ByteBuffer;
import java.util.ArrayDeque;
import java.util.Arrays;
import java.util.HashMap;
import java.util.List;
import java.util.Locale;
import java.util.Map;
import java.util.Queue;
import java.util.concurrent.TimeUnit;

/**
 * {@link PersistedTabData} for Shopping related websites
 *
 * This class should not be merged because it is being used as a key in a Map
 * in PersistedTabDataConfiguration.java.
 */
@DoNotClassMerge
public class ShoppingPersistedTabData extends PersistedTabData {
    private static final String TAG = "SPTD";
    private static final String STALE_TAB_THRESHOLD_SECONDS_PARAM =
            "price_tracking_stale_tab_threshold_seconds";
    private static final String TIME_TO_LIVE_MS_PARAM = "price_tracking_time_to_live_ms";
    private static final String DISPLAY_TIME_MS_PARAM = "price_tracking_display_time_ms";
    private static final String RETURN_EMPTY_PRICE_DROPS_UNTIL_INIT_PARAM =
            "return_empty_price_drops_until_init";
    private static final String CHECK_IF_PRICE_DROP_IS_SEEN_PARAM = "check_if_price_drop_is_seen";
    private static final String METRICS_IDENTIFIER_PREFIX = "NavigationComplete";

    private static final Class<ShoppingPersistedTabData> USER_DATA_KEY =
            ShoppingPersistedTabData.class;
    private static final int MICROS_TO_UNITS = 1000000;
    private static final long TWO_UNITS = 2 * MICROS_TO_UNITS;
    private static final int MINIMUM_DROP_PERCENTAGE = 10;
    private static final int ONE_WEEK_MS = (int) TimeUnit.DAYS.toMillis(7);

    @VisibleForTesting public static final long ONE_HOUR_MS = TimeUnit.HOURS.toMillis(1);

    private static final int NINETY_DAYS_SECONDS = (int) TimeUnit.DAYS.toSeconds(90);

    @VisibleForTesting public static final long NO_TRANSITIONS_OCCURRED = -1;

    @VisibleForTesting public static final long NO_PRICE_KNOWN = -1;

    private static boolean sPriceTrackingWithOptimizationGuideForTesting;

    private static Queue<ShoppingDataRequest> sShoppingDataRequests = new ArrayDeque<>();
    private static boolean sDelayedInitFinished;

    public long mLastPriceChangeTimeMs = NO_TRANSITIONS_OCCURRED;

    private PriceDropData mPriceDropData = new PriceDropData();
    private PriceDropMetricsLogger mPriceDropMetricsLogger;
    private Map<String, CurrencyFormatter> mCurrencyFormatterMap = new HashMap<>();

    @VisibleForTesting
    protected ObservableSupplierImpl<Boolean> mIsTabSaveEnabledSupplier =
            new ObservableSupplierImpl<>();

    @VisibleForTesting protected EmptyTabObserver mUrlUpdatedObserver;

    static {
        PersistedTabData.addSupportedMaintenanceClass(USER_DATA_KEY);
    }

    // Lazy initialization of OptimizationGuideBridgeFactory
    private static class OptimizationGuideBridgeHolder {
        private static final OptimizationGuideBridge sOptimizationGuideBridge;

        static {
            List<HintsProto.OptimizationType> optimizationTypes;
            Profile profile = ProfileManager.getLastUsedRegularProfile();
            sOptimizationGuideBridge = OptimizationGuideBridgeFactory.getForProfile(profile);
            if (sOptimizationGuideBridge != null) {
                sOptimizationGuideBridge.registerOptimizationTypes(
                        Arrays.asList(
                                HintsProto.OptimizationType.SHOPPING_PAGE_PREDICTOR,
                                HintsProto.OptimizationType.PRICE_TRACKING));
            }
        }
    }

    /**
     * Provides a deep copy of a previous {@link ShoppingPersistedTabData} for client side price
     * drop tracking. Only fields required for price drop identification are retained.
     */
    private static class PriceDataSnapshot {
        public long priceMicros;
        public long previousPriceMicros;
        public long lastPriceChangeTimeMs;

        PriceDataSnapshot(ShoppingPersistedTabData shoppingPersistedTabData) {
            this.priceMicros = shoppingPersistedTabData.getPriceMicros();
            this.previousPriceMicros = shoppingPersistedTabData.getPreviousPriceMicros();
            this.lastPriceChangeTimeMs = shoppingPersistedTabData.getLastPriceChangeTimeMs();
        }
    }

    /**
     * Used to defer initialization/acquisition of {@link ShoppingPersistedTabData}
     * until DeferredStartup.
     */
    private static class ShoppingDataRequest {
        public Tab tab;
        public Callback<ShoppingPersistedTabData> callback;

        /**
         * @param tab {@link Tab} {@link ShoppingPersistedTabData} is being acquired for
         * @param callback {@link Callback} {@link ShoppingPersistedTabData} is passed back in
         */
        ShoppingDataRequest(Tab tab, Callback<ShoppingPersistedTabData> callback) {
            this.tab = tab;
            this.callback = callback;
        }
    }

    /**
     * Raw price drop data acquired from backend service. This is converted to formatted
     * price strings in the public API getPriceDrop().
     */
    @VisibleForTesting
    protected static class PriceDropData {
        public long priceMicros;
        public long previousPriceMicros;
        public String currencyCode;
        public String offerId;
        public GURL gurl;
        public boolean isCurrentPriceDropSeen;
        public String productTitle;
        public GURL productImageUrl;

        PriceDropData() {
            this.priceMicros = NO_PRICE_KNOWN;
            this.previousPriceMicros = NO_PRICE_KNOWN;
        }

        /**
         * @param priceMicros the price in micros
         * @param previousPriceMicros previous price in micros
         * @param currencyCode the currency code of the price
         * @param offerId the offer id associated with the price drop
         */
        PriceDropData(
                long priceMicros, long previousPriceMicros, String currencyCode, String offerId) {
            this.priceMicros = priceMicros;
            this.previousPriceMicros = previousPriceMicros;
            this.currencyCode = currencyCode;
            this.offerId = offerId;
        }
    }

    /**
     * A price drop for the offer {@link ShoppingPersistedTabData}
     * refers to
     */
    public static class PriceDrop {
        public final String price;
        public final String previousPrice;

        /**
         * @param price {@link String} representation of the price
         * @param previousPrice {@link String} representation of the previous price
         */
        public PriceDrop(String price, String previousPrice) {
            this.price = price;
            this.previousPrice = previousPrice;
        }

        @Override
        public boolean equals(Object object) {
            if (!(object instanceof PriceDrop)) return false;
            PriceDrop priceDrop = (PriceDrop) object;
            return this.price.equals(priceDrop.price)
                    && this.previousPrice.equals(priceDrop.previousPrice);
        }

        @Override
        public int hashCode() {
            int result = 17;
            result = 31 * result + (price == null ? 0 : price.hashCode());
            result = 31 * result + (previousPrice == null ? 0 : previousPrice.hashCode());
            return result;
        }
    }

    @VisibleForTesting(otherwise = VisibleForTesting.PROTECTED)
    public ShoppingPersistedTabData(Tab tab) {
        super(
                tab,
                PersistedTabDataConfiguration.get(ShoppingPersistedTabData.class, tab.isIncognito())
                        .getStorage(),
                PersistedTabDataConfiguration.get(ShoppingPersistedTabData.class, tab.isIncognito())
                        .getId());
        setupPersistence(tab);
        mPriceDropMetricsLogger = new PriceDropMetricsLogger(this);
    }

    private void prefetchOnNewNavigation(Tab tab, NavigationHandle navigationHandle) {
        prefetchOnNewNavigation(tab, navigationHandle, null);
    }

    /**
     * @param tab {@link Tab} associated with {@link ShoppingPersistedTabData}
     * @param navigationHandle the {@link NavigationHandle} associated with the new navigation
     * @param onCompleteForTesting runnable which indicates when the callback has completed for test
     *         synchronization
     */
    @VisibleForTesting
    protected void prefetchOnNewNavigation(
            Tab tab, NavigationHandle navigationHandle, Runnable onCompleteForTesting) {
        if (!navigationHandle.isInPrimaryMainFrame()) {
            return;
        }
        if (OptimizationGuideBridgeHolder.sOptimizationGuideBridge == null) return;
        ShoppingPersistedTabDataService service =
                ShoppingPersistedTabDataService.getForProfile(tab.getProfile());
        OptimizationGuideBridge.OptimizationGuideCallback optimizationCallback =
                (decision, metadata) -> {
                    if (!tab.isInitialized()) {
                        if (onCompleteForTesting != null) {
                            onCompleteForTesting.run();
                        }
                        return;
                    }
                    if (!tab.getUrl().equals(navigationHandle.getUrl())
                            || decision != OptimizationGuideDecision.TRUE) {
                        if (onCompleteForTesting != null) {
                            onCompleteForTesting.run();
                        }
                        mPriceDropMetricsLogger.logPriceDropMetrics(
                                METRICS_IDENTIFIER_PREFIX, getTimeSinceTabLastOpenedMs(tab));
                        if (ChromeFeatureList.isEnabled(ChromeFeatureList.PRICE_CHANGE_MODULE)) {
                            service.notifyPriceDropStatus(tab, false);
                        }
                        return;
                    }
                    try {
                        PriceTrackingData priceTrackingDataProto =
                                PriceTrackingData.parseFrom(metadata.getValue());
                        parsePriceTrackingDataProto(tab, priceTrackingDataProto, null);
                        setLastUpdatedMs(System.currentTimeMillis());
                        mPriceDropMetricsLogger.logPriceDropMetrics(
                                METRICS_IDENTIFIER_PREFIX, getTimeSinceTabLastOpenedMs(tab));
                        if (ChromeFeatureList.isEnabled(ChromeFeatureList.PRICE_CHANGE_MODULE)) {
                            service.notifyPriceDropStatus(
                                    tab,
                                    ShoppingPersistedTabDataService.isDataEligibleForPriceDrop(
                                            this));
                        }
                    } catch (InvalidProtocolBufferException e) {
                        Log.i(TAG, "Problem parsing PriceTrackingDataProto", e);
                    }
                    if (onCompleteForTesting != null) {
                        onCompleteForTesting.run();
                    }
                };
        OptimizationGuideBridgeHolder.sOptimizationGuideBridge.canApplyOptimization(
                navigationHandle.getUrl(),
                HintsProto.OptimizationType.PRICE_TRACKING,
                optimizationCallback);
    }

    /**
     * Log price drop metrics, if we have price drop data
     *
     * @param locationIdentifier where in the user experience the metrics were called from.
     */
    public void logPriceDropMetrics(String locationIdentifier) {
        if (mPriceDropMetricsLogger != null) {
            mPriceDropMetricsLogger.logPriceDropMetrics(
                    locationIdentifier, getTimeSinceTabLastOpenedMs(mTab));
        }
    }

    protected PriceDropMetricsLogger getPriceDropMetricsLoggerForTesting() {
        return mPriceDropMetricsLogger;
    }

    @VisibleForTesting
    protected ShoppingPersistedTabData(
            Tab tab, PersistedTabDataStorage storage, String persistedTabDataId) {
        super(tab, storage, persistedTabDataId);
        setupPersistence(tab);
        mPriceDropMetricsLogger = new PriceDropMetricsLogger(this);
    }

    private void setupPersistence(Tab tab) {
        // ShoppingPersistedTabData is not saved by default - only when its fields are populated
        // (after a successful endpoint repsonse)
        disableSaving();
        registerIsTabSaveEnabledSupplier(mIsTabSaveEnabledSupplier);
        // In the below, resetting price data is decoupled from prefetching because
        // when we couple the two together, we sometimes delete too aggressively. The example is -
        // after a restart the active Tab is reloaded. If the page had a price drop which was
        // persisted across restarts, the reload of the active Tab results in resetting the price
        // data but at that point, OptimizationGuide is not returning results yet - so we
        // essentially can't persisted any price drops of the active Tab across restarts.
        mUrlUpdatedObserver =
                new EmptyTabObserver() {
                    @Override
                    public void onDidStartNavigationInPrimaryMainFrame(
                            Tab tab, NavigationHandle navigationHandle) {
                        if (navigationHandle.isSameDocument()) {
                            return;
                        }
                        // User is navigating to a different page - as detected by a change in URL
                        if (!tab.getUrl().equals(navigationHandle.getUrl())) {
                            resetPriceData();
                        }
                    }

                    @Override
                    public void onDidFinishNavigationInPrimaryMainFrame(
                            Tab tab, NavigationHandle navigationHandle) {
                        if (navigationHandle.isSameDocument() || !navigationHandle.hasCommitted()) {
                            return;
                        }

                        // User navigating to a different page, as detected by a search or typing
                        // something into the address bar.
                        boolean fromAddressBar =
                                (navigationHandle.pageTransition()
                                                & PageTransition.FROM_ADDRESS_BAR)
                                        != 0;
                        if (navigationHandle.isValidSearchFormUrl() || fromAddressBar) {
                            resetPriceData();
                        }

                        prefetchOnNewNavigation(tab, navigationHandle);
                    }
                };
        tab.addObserver(mUrlUpdatedObserver);
    }

    protected void resetPriceData() {
        delete();
        mPriceDropData = new PriceDropData();
    }

    public EmptyTabObserver getUrlUpdatedObserverForTesting() {
        return mUrlUpdatedObserver;
    }

    public PriceDropData getPriceDropDataForTesting() {
        return mPriceDropData;
    }

    /**
     * Initializes {@link ShoppingPersistedTabData} for a {@link Tab}. This results in
     * a {@link ShoppingPersistedTabData} being acquired from storage, via a network call
     * or a blank one being created. In any case, a {@link ShoppingPersistedTabData} object will be
     * created which enables pricing data to be prefetched on each new navigation. The only scenario
     * where no {@link ShoppingPersistedTabData} will be returned is if the {@link Tab} was
     * destroyed shortly after calling this method.
     * @param tab {@link Tab} for which {@link ShoppingPersistedTabData} is initialized.
     */
    public static void initialize(Tab tab) {
        Callback<ShoppingPersistedTabData> callback =
                (res) -> {
                    if (res == null) {
                        // If there is no ShoppingPersistedTabData found from storage, we create
                        // an empty ShoppingPersistedTabData so the pricing data can be prefetched
                        // on each new navigation. We gate this with an isDestroyed() check to
                        // protect against the Tab being destroyed in the meantime.
                        if (!tab.isDestroyed()) {
                            ShoppingPersistedTabData.from(tab);
                        }
                    }
                    if (ChromeFeatureList.isEnabled(ChromeFeatureList.PRICE_CHANGE_MODULE)) {
                        ShoppingPersistedTabDataService service =
                                ShoppingPersistedTabDataService.getForProfile(tab.getProfile());
                        service.notifyPriceDropStatus(
                                tab,
                                !tab.isDestroyed()
                                        && ShoppingPersistedTabDataService
                                                .isDataEligibleForPriceDrop(res));
                    }
                };
        if (sDelayedInitFinished) {
            ShoppingPersistedTabData.from(tab, callback);
        } else {
            sShoppingDataRequests.add(new ShoppingDataRequest(tab, callback));
        }
    }

    /**
     * Acquire {@link ShoppingPersistedTabData} for a {@link Tab}
     * @param tab {@link Tab} ShoppingPersistedTabData is acquired for
     * @param callback {@link Callback} receiving the Tab's {@link ShoppingPersistedTabData}
     * The result in the callback wil be null for a:
     * - Custom Tab
     * - Incognito Tab
     * - Tab greater than 90 days old
     * - Tab with a non-shopping related page currently navigated to
     * - Tab with a shopping related page for which no shopping related data was found
     * - Uninitialized Tab
     */
    public static void from(Tab tab, Callback<ShoppingPersistedTabData> callback) {
        if (tab == null || tab.isDestroyed()) {
            PostTask.runOrPostTask(
                    TaskTraits.UI_DEFAULT,
                    () -> {
                        callback.onResult(null);
                    });
            return;
        }
        if (sDelayedInitFinished) {
            fromWithoutDelayedInit(tab, callback);
        } else {
            @DelayedInitMethod int delayedInitMethod = getDelayedInitMethod();
            if (delayedInitMethod == DelayedInitMethod.EMPTY_RESPONSES_UNTIL_INIT) {
                PostTask.postTask(
                        TaskTraits.UI_DEFAULT,
                        () -> {
                            callback.onResult(null);
                        });
            } else if (delayedInitMethod == DelayedInitMethod.DELAY_RESPONSES_UNTIL_INIT) {
                sShoppingDataRequests.add(new ShoppingDataRequest(tab, callback));
            } else {
                assert false : "Unknown DelayedInitMethod: " + delayedInitMethod;
            }
        }
    }

    /**
     * Acquire {@link ShoppingPersistedTabData} for a {@link Tab}, with an option to skip delayed
     * initialization and initialize immediately.
     * @param tab {@link Tab} ShoppingPersistedTabData is acquired for
     * @param callback {@link Callback} receiving the Tab's {@link ShoppingPersistedTabData}
     * @param skipDelayedInit whether to skip the delayed initialization of {@link
     * ShoppingPersistedTabData} and initialize immediately
     * The result in the callback wil be null for a:
     * - Custom Tab
     * - Incognito Tab
     * - Tab greater than 90 days old
     * - Tab with a non-shopping related page currently navigated to
     * - Tab with a shopping related page for which no shopping related data was found
     * - Uninitialized Tab
     */
    static void from(
            Tab tab, Callback<ShoppingPersistedTabData> callback, boolean skipDelayedInit) {
        if (skipDelayedInit) {
            fromWithoutDelayedInit(tab, callback);
        } else {
            from(tab, callback);
        }
    }

    private static void fromWithoutDelayedInit(
            Tab tab, Callback<ShoppingPersistedTabData> callback) {
        // Shopping related data is not available for incognito, Custom or Destroyed Tabs. For
        // example, for incognito Tabs it is not possible to call a backend service with the user's
        // URL.
        if (tab == null || tab.isDestroyed() || tab.isIncognito() || tab.isCustomTab()) {
            PostTask.postTask(
                    TaskTraits.UI_DEFAULT,
                    () -> {
                        callback.onResult(null);
                    });
            return;
        }
        PersistedTabData.from(
                tab,
                (data, storage, id, factoryCallback) -> {
                    PostTask.postTask(
                            TaskTraits.UI_DEFAULT,
                            () -> {
                                ShoppingPersistedTabData shoppingPersistedTabData =
                                        tab.isDestroyed()
                                                ? null
                                                : ShoppingPersistedTabData.from(tab);
                                PostTask.postTask(
                                        TaskTraits.USER_BLOCKING_MAY_BLOCK,
                                        () -> {
                                            if (shoppingPersistedTabData != null) {
                                                shoppingPersistedTabData.deserializeAndLog(data);
                                            }
                                            PostTask.postTask(
                                                    TaskTraits.UI_DEFAULT,
                                                    () -> {
                                                        factoryCallback.onResult(
                                                                shoppingPersistedTabData);
                                                    });
                                        });
                            });
                },
                (supplierCallback) -> {
                    if (tab.isDestroyed()
                            || OptimizationGuideBridgeHolder.sOptimizationGuideBridge == null
                            || getTimeSinceTabLastOpenedMs(tab)
                                    > TimeUnit.SECONDS.toMillis(getStaleTabThresholdSeconds())) {
                        supplierCallback.onResult(null);
                        return;
                    }
                    PriceDataSnapshot previous =
                            PersistedTabData.from(tab, USER_DATA_KEY) == null
                                    ? null
                                    : new PriceDataSnapshot(
                                            PersistedTabData.from(tab, USER_DATA_KEY));
                    OptimizationGuideBridge.OptimizationGuideCallback optimizationCallback =
                            (decision, metadata) -> {
                                if (tab.isDestroyed()) {
                                    supplierCallback.onResult(null);
                                    return;
                                }
                                if (decision != OptimizationGuideDecision.TRUE) {
                                    ShoppingPersistedTabData res =
                                            getEmptyShoppingPersistedTabData(tab);
                                    res.logPriceDropMetrics(METRICS_IDENTIFIER_PREFIX);
                                    supplierCallback.onResult(res);
                                    return;
                                }
                                try {
                                    PriceTrackingData priceTrackingDataProto =
                                            PriceTrackingData.parseFrom(metadata.getValue());
                                    ShoppingPersistedTabData sptd =
                                            ShoppingPersistedTabData.from(tab);
                                    sptd.parsePriceTrackingDataProto(
                                            tab, priceTrackingDataProto, previous);
                                    sptd.logPriceDropMetrics(METRICS_IDENTIFIER_PREFIX);
                                    supplierCallback.onResult(sptd);
                                } catch (InvalidProtocolBufferException e) {
                                    Log.i(TAG, "Problem with PriceTrackingDataProto.Details", e);
                                    supplierCallback.onResult(null);
                                }
                            };
                    OptimizationGuideBridgeHolder.sOptimizationGuideBridge.canApplyOptimization(
                            tab.getUrl(),
                            HintsProto.OptimizationType.PRICE_TRACKING,
                            optimizationCallback);
                },
                ShoppingPersistedTabData.class,
                callback);
    }

    /**
     * @return {@link ShoppingPersistedTabData} from {@link UserDataHost}
     */
    public static ShoppingPersistedTabData from(Tab tab) {
        ShoppingPersistedTabData shoppingPersistedTabData =
                PersistedTabData.from(tab, USER_DATA_KEY);
        if (shoppingPersistedTabData == null) {
            shoppingPersistedTabData =
                    tab.getUserDataHost()
                            .setUserData(USER_DATA_KEY, new ShoppingPersistedTabData(tab));
        }
        return shoppingPersistedTabData;
    }

    /**
     * @param tab {@link Tab} of interest.
     * @return true if the {@link Tab} has a price drop associated with it.
     */
    public static boolean hasPriceDrop(Tab tab) {
        ShoppingPersistedTabData shoppingPersistedTabData =
                PersistedTabData.from(tab, USER_DATA_KEY);
        if (shoppingPersistedTabData == null) {
            return false;
        }
        return shoppingPersistedTabData.getPriceDrop() != null;
    }

    /** Whether a BuyableProductAnnotation was found or not */
    @IntDef({
        FoundBuyableProduct.NOT_FOUND,
        FoundBuyableProduct.FOUND,
        FoundBuyableProduct.FOUND_WITH_PRICE_UPDATE
    })
    @Retention(RetentionPolicy.SOURCE)
    @interface FoundBuyableProduct {
        int NOT_FOUND = 0;
        int FOUND = 1;
        int FOUND_WITH_PRICE_UPDATE = 2;
        int NUM_ENTRIES = 3;
    }

    @IntDef({
        FoundBuyableProductAnnotation.NOT_FOUND,
        FoundBuyableProductAnnotation.FOUND,
        FoundBuyableProductAnnotation.FOUND_WITH_PRICE_UPDATE
    })
    @Retention(RetentionPolicy.SOURCE)
    @interface FoundBuyableProductAnnotation {
        int NOT_FOUND = 0;
        int FOUND = 1;
        int FOUND_WITH_PRICE_UPDATE = 2;
        int NUM_ENTRIES = 3;
    }

    @IntDef({
        DelayedInitMethod.DELAY_RESPONSES_UNTIL_INIT,
        DelayedInitMethod.EMPTY_RESPONSES_UNTIL_INIT
    })
    @Retention(RetentionPolicy.SOURCE)
    @interface DelayedInitMethod {
        /**
         * Responses from public API ShoppingPersistedTabData.from are delayed until {@link
         * ShoppingPersistedTabData} is initialized. This is achieved by onDeferredStartup() being
         * called and the Queue<ShoppingDataRequest> being flushed.
         */
        int DELAY_RESPONSES_UNTIL_INIT = 0;

        /**
         * Responses from public API ShoppingPersistedTabData.from are returned as empty until
         * {@link ShoppingPersistedTabData} is initialized. This is achieved by onDeferredStartup()
         * being called and the Queue<ShoppingDataRequest> being flushed.
         */
        int EMPTY_RESPONSES_UNTIL_INIT = 1;
    }

    /** Enable saving of {@link ShoppingPersistedTabData} */
    protected void enableSaving() {
        mIsTabSaveEnabledSupplier.set(true);
    }

    /**
     * Disable saving of {@link ShoppingPersistedTabData}. Deletes previously saved {@link
     * ShoppingPersistedTabData} as well.
     */
    public void disableSaving() {
        mIsTabSaveEnabledSupplier.set(false);
    }

    @VisibleForTesting
    protected void parsePriceTrackingDataProto(
            Tab tab, PriceTrackingData priceTrackingData, PriceDataSnapshot previousPricingData) {
        @FoundBuyableProduct int foundBuyableProduct = FoundBuyableProduct.NOT_FOUND;

        ProductPriceUpdate productUpdate = priceTrackingData.getProductUpdate();
        BuyableProduct buyableProduct = priceTrackingData.getBuyableProduct();

        if (hasPriceUpdate(priceTrackingData)) {
            if (hasPriceChange(productUpdate)) {
                setIsCurrentPriceDropSeen(false);
            }
            setPriceMicros(productUpdate.getNewPrice().getAmountMicros());
            setPreviousPriceMicros(productUpdate.getOldPrice().getAmountMicros());
            setCurrencyCode(productUpdate.getOldPrice().getCurrencyCode());
            setLastUpdatedMs(System.currentTimeMillis());
            // Use UnsignedLongs to convert OfferId to avoid overflow.
            setMainOfferId(UnsignedLongs.toString(buyableProduct.getOfferId()));
            setPriceDropGurl(tab.getUrl());
            setProductTitle(buyableProduct.getTitle());
            setProductImageUrl(new GURL(buyableProduct.getImageUrl()));
            foundBuyableProduct = FoundBuyableProduct.FOUND_WITH_PRICE_UPDATE;
        }

        RecordHistogram.recordEnumeratedHistogram(
                "Tabs.ShoppingPersistedTabData.FoundBuyableProduct",
                foundBuyableProduct,
                FoundBuyableProduct.NUM_ENTRIES);
        // Only persist this ShoppingPersistedTabData if it was correctly populated from the
        // response
        if (foundBuyableProduct == FoundBuyableProduct.FOUND
                || foundBuyableProduct == FoundBuyableProduct.FOUND_WITH_PRICE_UPDATE) {
            enableSaving();
        }
    }

    private static boolean hasPriceUpdate(PriceTrackingData priceTrackingDataProto) {
        if (!priceTrackingDataProto.hasBuyableProduct()
                || !priceTrackingDataProto.hasProductUpdate()) {
            return false;
        }
        ProductPriceUpdate productUpdateProto = priceTrackingDataProto.getProductUpdate();
        if (!productUpdateProto.hasNewPrice() || !productUpdateProto.hasOldPrice()) {
            return false;
        }
        if (!productUpdateProto.getNewPrice().hasCurrencyCode()
                || !productUpdateProto.getOldPrice().hasCurrencyCode()) {
            return false;
        }
        if (!productUpdateProto
                .getNewPrice()
                .getCurrencyCode()
                .equals(productUpdateProto.getOldPrice().getCurrencyCode())) {
            return false;
        }
        return true;
    }

    private static boolean hasPrice(PriceTrackingData priceTrackingDataProto) {
        if (!priceTrackingDataProto.hasBuyableProduct()) {
            return false;
        }
        if (!priceTrackingDataProto.getBuyableProduct().hasCurrentPrice()) {
            return false;
        }
        if (!priceTrackingDataProto.getBuyableProduct().getCurrentPrice().hasAmountMicros()
                || !priceTrackingDataProto
                        .getBuyableProduct()
                        .getCurrentPrice()
                        .hasCurrencyCode()) {
            return false;
        }
        return true;
    }

    @VisibleForTesting(otherwise = VisibleForTesting.PROTECTED)
    public void setPriceMicros(long priceMicros) {
        mPriceDropData.priceMicros = priceMicros;
        save();
    }

    protected void setCurrencyCode(String currencyCode) {
        mPriceDropData.currencyCode = currencyCode;
        save();
    }

    @VisibleForTesting
    protected void setPriceDropGurl(GURL gurl) {
        mPriceDropData.gurl = gurl;
        save();
    }

    @VisibleForTesting
    protected void setProductTitle(String productTitle) {
        mPriceDropData.productTitle = productTitle;
        save();
    }

    @VisibleForTesting
    public String getProductTitle() {
        return mPriceDropData.productTitle;
    }

    @VisibleForTesting
    protected void setProductImageUrl(GURL imageUrl) {
        mPriceDropData.productImageUrl = imageUrl;
        save();
    }

    @VisibleForTesting
    public GURL getProductImageUrl() {
        return mPriceDropData.productImageUrl;
    }

    @VisibleForTesting
    protected String getCurrencyCode() {
        return mPriceDropData.currencyCode;
    }

    @VisibleForTesting
    protected void setPreviousPriceMicros(long previousPriceMicros) {
        mPriceDropData.previousPriceMicros = previousPriceMicros;
        save();
    }

    @VisibleForTesting(otherwise = VisibleForTesting.PROTECTED)
    public long getPriceMicros() {
        return mPriceDropData.priceMicros;
    }

    /**
     * @return true if there is a price
     */
    protected boolean hasPriceMicros() {
        return mPriceDropData.priceMicros != NO_PRICE_KNOWN;
    }

    @VisibleForTesting(otherwise = VisibleForTesting.PROTECTED)
    public long getPreviousPriceMicros() {
        return mPriceDropData.previousPriceMicros;
    }

    /**
     * @return true if there is a previous price
     */
    protected boolean hasPreviousPriceMicros() {
        return mPriceDropData.previousPriceMicros != NO_PRICE_KNOWN;
    }

    public void setPriceMicrosForTesting(long priceMicros) {
        mPriceDropData.priceMicros = priceMicros;
    }

    public void setPreviousPriceMicrosForTesting(long previousPriceMicros) {
        mPriceDropData.previousPriceMicros = previousPriceMicros;
    }

    public void setMainOfferId(String offerId) {
        mPriceDropData.offerId = offerId;
        save();
    }

    public String getMainOfferId() {
        return mPriceDropData.offerId;
    }

    /**
     * @return {@link PriceDrop} relating to the main offer in the page.
     */
    public PriceDrop getPriceDrop() {
        if (!isValidPriceDropUpdate()
                || isPriceChangeStale()
                || !mTab.getUrl().equals(mPriceDropData.gurl)) {
            return null;
        }
        return createPriceDrop(mPriceDropData.priceMicros, mPriceDropData.previousPriceMicros);
    }

    private boolean isValidPriceDropUpdate() {
        return mPriceDropData.priceMicros != NO_PRICE_KNOWN
                && mPriceDropData.previousPriceMicros != NO_PRICE_KNOWN
                && mPriceDropData.priceMicros < mPriceDropData.previousPriceMicros;
    }

    private PriceDrop createPriceDrop(long priceMicros, long previousPriceMicros) {
        String formattedPrice = formatPrice(priceMicros);
        String formattedPreviousPrice = formatPrice(previousPriceMicros);
        if (formattedPrice.equals(formattedPreviousPrice)) {
            return null;
        }
        return new PriceDrop(formattedPrice, formattedPreviousPrice);
    }

    private boolean isPriceChangeStale() {
        return mLastPriceChangeTimeMs != NO_TRANSITIONS_OCCURRED
                && System.currentTimeMillis() - mLastPriceChangeTimeMs > getDisplayTimeMs();
    }

    private boolean isQualifyingPriceDrop() {
        if (mPriceDropData.previousPriceMicros - mPriceDropData.priceMicros
                < getMinimumDropThresholdAbsolute()) {
            return false;
        }
        if ((100L * mPriceDropData.priceMicros) / mPriceDropData.previousPriceMicros
                > (100 - getMinimumDroppedThresholdPercentage())) {
            return false;
        }
        return true;
    }

    /**
     * Sets whether the current price drop has been viewed in the tab switcher grid.
     * @param isSeen  is true if the current price drop has been seen.
     */
    public void setIsCurrentPriceDropSeen(boolean isSeen) {
        if (isCheckIfPriceDropIsSeenEnabled()) {
            mPriceDropData.isCurrentPriceDropSeen = isSeen;
            save();
        }
    }

    /**
     * @return returns whether the current price drop has been seen.
     */
    public boolean getIsCurrentPriceDropSeen() {
        if (isCheckIfPriceDropIsSeenEnabled()) {
            return mPriceDropData.isCurrentPriceDropSeen;
        }
        return false;
    }

    // TODO(crbug.com/40158181) Make parameters finch configurable
    private static int getMinimumDroppedThresholdPercentage() {
        return MINIMUM_DROP_PERCENTAGE;
    }

    private static long getMinimumDropThresholdAbsolute() {
        return TWO_UNITS;
    }

    // TODO(crbug.com/40720561) support all currencies
    private String formatPrice(long priceMicros) {
        if (mPriceDropData.currencyCode == null) {
            return "";
        }
        CurrencyFormatter currencyFormatter = getCurrencyFormatter(mPriceDropData.currencyCode);
        return PriceUtils.formatPrice(currencyFormatter, priceMicros);
    }

    private CurrencyFormatter getCurrencyFormatter(String currencyCode) {
        if (mCurrencyFormatterMap.get(currencyCode) == null) {
            mCurrencyFormatterMap.put(
                    currencyCode,
                    new CurrencyFormatter(mPriceDropData.currencyCode, Locale.getDefault()));
        }
        return mCurrencyFormatterMap.get(currencyCode);
    }

    @Override
    public Serializer<ByteBuffer> getSerializer() {
        ShoppingPersistedTabDataProto.Builder builder =
                ShoppingPersistedTabDataProto.newBuilder()
                        .setPriceMicros(mPriceDropData.priceMicros)
                        .setPreviousPriceMicros(mPriceDropData.previousPriceMicros)
                        .setLastUpdatedMs(getLastUpdatedMs())
                        .setLastPriceChangeTimeMs(mLastPriceChangeTimeMs)
                        .setIsCurrentPriceDropSeen(mPriceDropData.isCurrentPriceDropSeen);
        if (mPriceDropData.offerId != null) {
            builder.setMainOfferId(mPriceDropData.offerId);
        }

        if (mPriceDropData.currencyCode != null) {
            builder.setPriceCurrencyCode(mPriceDropData.currencyCode);
        }

        if (mPriceDropData.gurl != null) {
            builder.setSerializedGurl(mPriceDropData.gurl.serialize());
        }

        if (mPriceDropData.productTitle != null) {
            builder.setProductTitle(mPriceDropData.productTitle);
        }

        if (mPriceDropData.productImageUrl != null) {
            builder.setProductImageUrl(mPriceDropData.productImageUrl.serialize());
        }

        return () -> {
            return builder.build().toByteString().asReadOnlyByteBuffer();
        };
    }

    @Override
    public boolean deserialize(@Nullable ByteBuffer bytes) {
        // TODO(crbug.com/40151939) add in metrics for serialize and deserialize
        // Do not attempt to deserialize if the bytes are null
        if (bytes == null || !bytes.hasRemaining()) {
            return false;
        }
        try {
            ShoppingPersistedTabDataProto shoppingPersistedTabDataProto =
                    ShoppingPersistedTabDataProto.parseFrom(bytes);
            mPriceDropData.priceMicros = shoppingPersistedTabDataProto.getPriceMicros();
            mPriceDropData.previousPriceMicros =
                    shoppingPersistedTabDataProto.getPreviousPriceMicros();
            setLastUpdatedMs(shoppingPersistedTabDataProto.getLastUpdatedMs());
            mLastPriceChangeTimeMs = shoppingPersistedTabDataProto.getLastPriceChangeTimeMs();
            mPriceDropData.offerId = shoppingPersistedTabDataProto.getMainOfferId();
            mPriceDropData.currencyCode = shoppingPersistedTabDataProto.getPriceCurrencyCode();
            mPriceDropData.gurl =
                    GURL.deserialize(shoppingPersistedTabDataProto.getSerializedGurl());
            mPriceDropData.isCurrentPriceDropSeen =
                    shoppingPersistedTabDataProto.getIsCurrentPriceDropSeen();
            mPriceDropData.productTitle = shoppingPersistedTabDataProto.getProductTitle();
            mPriceDropData.productImageUrl =
                    GURL.deserialize(shoppingPersistedTabDataProto.getProductImageUrl());
            return true;
        } catch (InvalidProtocolBufferException e) {
            Log.e(
                    TAG,
                    String.format(
                            Locale.US,
                            "There was a problem deserializing "
                                    + "ShoppingPersistedTabData. Details: %s",
                            e.getMessage()));
        }
        return false;
    }

    @Override
    public String getUmaTag() {
        return "SPTD";
    }

    @Override
    public long getTimeToLiveMs() {
        if (FeatureList.isInitialized()) {
            return ChromeFeatureList.getFieldTrialParamByFeatureAsInt(
                    ChromeFeatureList.COMMERCE_PRICE_TRACKING,
                    TIME_TO_LIVE_MS_PARAM,
                    (int) ONE_HOUR_MS);
        }
        return (int) ONE_HOUR_MS;
    }

    @VisibleForTesting
    public long getLastPriceChangeTimeMs() {
        return mLastPriceChangeTimeMs;
    }

    public void setLastPriceChangeTimeMsForTesting(long lastPriceChangeTimeMs) {
        mLastPriceChangeTimeMs = lastPriceChangeTimeMs;
    }

    @Override
    protected boolean needsUpdate() {
        if (mPriceDropData.gurl != null && !mTab.getUrl().equals(mPriceDropData.gurl)) {
            return true;
        }
        return super.needsUpdate();
    }

    @Override
    public void destroy() {
        mTab.removeObserver(mUrlUpdatedObserver);
        for (CurrencyFormatter currencyFormatter : mCurrencyFormatterMap.values()) {
            assert currencyFormatter != null;
            currencyFormatter.destroy();
        }
        mCurrencyFormatterMap.clear();
        mPriceDropMetricsLogger.destroy();
        mPriceDropMetricsLogger = null;
        super.destroy();
    }

    /**
     * @return the threshold in seconds at which a {@link Tab} is considered stale
     * (and we no longer acquire shopping data for stale tabs).
     */
    public static int getStaleTabThresholdSeconds() {
        if (FeatureList.isInitialized()) {
            return ChromeFeatureList.getFieldTrialParamByFeatureAsInt(
                    ChromeFeatureList.COMMERCE_PRICE_TRACKING,
                    STALE_TAB_THRESHOLD_SECONDS_PARAM,
                    NINETY_DAYS_SECONDS);
        }
        return NINETY_DAYS_SECONDS;
    }

    /**
     * @return true if checking if a price drop is spotted is enabled.
     */
    public static boolean isCheckIfPriceDropIsSeenEnabled() {
        if (FeatureList.isInitialized()) {
            return ChromeFeatureList.getFieldTrialParamByFeatureAsBoolean(
                    ChromeFeatureList.COMMERCE_PRICE_TRACKING,
                    CHECK_IF_PRICE_DROP_IS_SEEN_PARAM,
                    false);
        }
        return false;
    }

    private static @DelayedInitMethod int getDelayedInitMethod() {
        if (FeatureList.isInitialized()
                && ChromeFeatureList.getFieldTrialParamByFeatureAsBoolean(
                        ChromeFeatureList.COMMERCE_PRICE_TRACKING,
                        RETURN_EMPTY_PRICE_DROPS_UNTIL_INIT_PARAM,
                        false)) {
            return DelayedInitMethod.EMPTY_RESPONSES_UNTIL_INIT;
        }
        return DelayedInitMethod.DELAY_RESPONSES_UNTIL_INIT;
    }

    public static void enablePriceTrackingWithOptimizationGuideForTesting() {
        sPriceTrackingWithOptimizationGuideForTesting = true;
    }

    private static int getDisplayTimeMs() {
        if (FeatureList.isInitialized()) {
            return ChromeFeatureList.getFieldTrialParamByFeatureAsInt(
                    ChromeFeatureList.COMMERCE_PRICE_TRACKING, DISPLAY_TIME_MS_PARAM, ONE_WEEK_MS);
        }
        return ONE_WEEK_MS;
    }

    private static long getTimeSinceTabLastOpenedMs(Tab tab) {
        return System.currentTimeMillis() - tab.getTimestampMillis();
    }

    /**
     * Returns true if there is an incoming change for the price.
     *
     * @param productPriceUpdate incoming price update data.
     */
    private boolean hasPriceChange(ProductPriceUpdate productPriceUpdate) {
        if (productPriceUpdate.getNewPrice().getAmountMicros() != mPriceDropData.priceMicros) {
            return true;
        }
        if (!productPriceUpdate
                .getNewPrice()
                .getCurrencyCode()
                .equals(mPriceDropData.currencyCode)) {
            return true;
        }
        return false;
    }

    /**
     * Called when it is appropriate to initialize and acquire {@link ShoppingPersistedTabData}.
     * Initialization and acquisition are delayed to avoid consuming system resources when
     * the system is busy with other urgent tasks on startup.
     */
    public static void onDeferredStartup() {
        processNextItemOnQueue();
    }

    private static void processNextItemOnQueue() {
        if (sDelayedInitFinished) {
            assert sShoppingDataRequests.isEmpty();
            return;
        }
        if (sShoppingDataRequests.isEmpty()) {
            sDelayedInitFinished = true;
            return;
        }
        ShoppingDataRequest shoppingDataRequest = sShoppingDataRequests.poll();
        if (shoppingDataRequest.tab.isDestroyed()) {
            // If Tab was destroyed we should just return null and not try and
            // create and associate {@link ShoppingPersistedTabData} with a
            // destroyed {@link Tab}.
            PostTask.postTask(
                    TaskTraits.UI_DEFAULT,
                    () -> {
                        shoppingDataRequest.callback.onResult(null);
                    });
            processNextItemOnQueue();
            return;
        }
        ShoppingPersistedTabData.fromWithoutDelayedInit(
                shoppingDataRequest.tab,
                (res) -> {
                    shoppingDataRequest.callback.onResult(res);
                    processNextItemOnQueue();
                });
    }

    private static ShoppingPersistedTabData getEmptyShoppingPersistedTabData(Tab tab) {
        ShoppingPersistedTabData shoppingPersistedTabData = ShoppingPersistedTabData.from(tab);
        shoppingPersistedTabData.resetPriceData();
        return shoppingPersistedTabData;
    }
}
