// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.merchant_viewer;

import android.content.Context;
import android.graphics.drawable.Drawable;
import android.text.TextUtils;
import android.view.View;

import androidx.annotation.Nullable;
import androidx.annotation.StringRes;
import androidx.annotation.VisibleForTesting;
import androidx.core.content.res.ResourcesCompat;

import org.chromium.base.Callback;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.chrome.browser.feature_engagement.TrackerFactory;
import org.chromium.chrome.browser.merchant_viewer.MerchantTrustMetrics.MessageClearReason;
import org.chromium.chrome.browser.merchant_viewer.proto.MerchantTrustSignalsOuterClass.MerchantTrustSignals;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.tab_ui.R;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.feature_engagement.EventConstants;
import org.chromium.components.feature_engagement.Tracker;
import org.chromium.components.messages.DismissReason;
import org.chromium.components.messages.MessageDispatcher;
import org.chromium.components.page_info.PageInfoFeatures;
import org.chromium.components.prefs.PrefService;
import org.chromium.components.site_engagement.SiteEngagementService;
import org.chromium.components.user_prefs.UserPrefs;
import org.chromium.content_public.browser.NavigationHandle;
import org.chromium.ui.base.IntentRequestTracker;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.url.GURL;

import java.util.concurrent.TimeUnit;

/**
 * Coordinator for managing merchant trust signals experience.
 */
public class MerchantTrustSignalsCoordinator
        implements PageInfoStoreInfoController.StoreInfoActionHandler {
    /** Interface to control the omnibox store icon and the related IPH. */
    public interface OmniboxIconController {
        /**
         * Show the store icon in omnibox.
         * @param window The {@link WindowAndroid} of the app.
         * @param url The url associated with the just showing {@link MerchantTrustMessageContext}.
         * @param drawable The store icon drawable.
         * @param stringId Resource id of the IPH string.
         */
        void showStoreIcon(
                WindowAndroid window, String url, Drawable drawable, @StringRes int stringId);
    }

    private final MerchantTrustSignalsMediator mMediator;
    private final MerchantTrustMessageScheduler mMessageScheduler;
    private final MerchantTrustBottomSheetCoordinator mDetailsTabCoordinator;
    private final Context mContext;
    private final MerchantTrustMetrics mMetrics;
    private final MerchantTrustSignalsDataProvider mDataProvider;
    private final MerchantTrustSignalsStorageFactory mStorageFactory;
    private final ObservableSupplier<Profile> mProfileSupplier;
    private final WindowAndroid mWindowAndroid;
    private OmniboxIconController mOmniboxIconController;
    private MerchantTrustMessageContext mCurrentMessageContext;

    /** Creates a new instance. */
    public MerchantTrustSignalsCoordinator(Context context, WindowAndroid windowAndroid,
            BottomSheetController bottomSheetController, View layoutView,
            MessageDispatcher messageDispatcher, ObservableSupplier<Tab> tabSupplier,
            ObservableSupplier<Profile> profileSupplier, MerchantTrustMetrics metrics,
            IntentRequestTracker intentRequestTracker) {
        this(context, windowAndroid, new MerchantTrustMessageScheduler(messageDispatcher, metrics),
                tabSupplier, new MerchantTrustSignalsDataProvider(), profileSupplier, metrics,
                new MerchantTrustBottomSheetCoordinator(context, windowAndroid,
                        bottomSheetController, tabSupplier, layoutView, metrics,
                        intentRequestTracker, profileSupplier),
                new MerchantTrustSignalsStorageFactory(profileSupplier));
    }

    @VisibleForTesting
    MerchantTrustSignalsCoordinator(Context context, WindowAndroid windowAndroid,
            MerchantTrustMessageScheduler messageScheduler, ObservableSupplier<Tab> tabSupplier,
            MerchantTrustSignalsDataProvider dataProvider,
            ObservableSupplier<Profile> profileSupplier, MerchantTrustMetrics metrics,
            MerchantTrustBottomSheetCoordinator detailsTabCoordinator,
            MerchantTrustSignalsStorageFactory storageFactory) {
        mContext = context;
        mDataProvider = dataProvider;
        mMetrics = metrics;
        mStorageFactory = storageFactory;
        mProfileSupplier = profileSupplier;
        mWindowAndroid = windowAndroid;

        mMediator = new MerchantTrustSignalsMediator(tabSupplier, this::maybeDisplayMessage);
        mMessageScheduler = messageScheduler;
        mDetailsTabCoordinator = detailsTabCoordinator;
    }

    /** Cleans up internal state. */
    public void destroy() {
        mMediator.destroy();
        mStorageFactory.destroy();
    }

    /**
     * Set the {@link OmniboxIconController} to manage the store icon in omnibox.
     */
    public void setOmniboxIconController(@Nullable OmniboxIconController omniboxIconController) {
        mOmniboxIconController = omniboxIconController;
    }

    @VisibleForTesting
    void maybeDisplayMessage(MerchantTrustMessageContext item) {
        // TODO(zhiyuancai): Pass item to maybeShowStoreIcon() directly instead of using
        // mCurrentMessageContext.
        mCurrentMessageContext = item;
        MerchantTrustMessageContext scheduledMessage =
                mMessageScheduler.getScheduledMessageContext();
        if (scheduledMessage != null && scheduledMessage.getHostName() != null
                && scheduledMessage.getHostName().equals(item.getHostName())
                && !scheduledMessage.getUrl().equals(item.getUrl())) {
            mMessageScheduler.expedite(this::onMessageEnqueued);
        } else {
            mMessageScheduler.clear(MessageClearReason.NAVIGATE_TO_DIFFERENT_DOMAIN);

            getDataForUnfamiliarMerchant(item.getNavigationHandle(), (trustSignals) -> {
                if (trustSignals == null) {
                    return;
                }

                mMessageScheduler.schedule(
                        MerchantTrustMessageViewModel.create(mContext, trustSignals,
                                this::onMessageDismissed, this::onMessagePrimaryAction),
                        item, MerchantViewerConfig.getDefaultTrustSignalsMessageDelay(),
                        this::onMessageEnqueued);
            });
        }
    }

    private boolean isFamiliarMerchant(String url) {
        if (!MerchantViewerConfig.doesTrustSignalsUseSiteEngagement() || TextUtils.isEmpty(url)) {
            return false;
        }
        Profile profile = mProfileSupplier.get();
        if (profile == null || profile.isOffTheRecord()) {
            return false;
        }
        return getSiteEngagementScore(profile, url)
                > MerchantViewerConfig.getTrustSignalsSiteEngagementThreshold();
    }

    @VisibleForTesting
    double getSiteEngagementScore(Profile profile, String url) {
        return SiteEngagementService.getForBrowserContext(profile).getScore(url);
    }

    @VisibleForTesting
    void onMessageEnqueued(MerchantTrustMessageContext messageContext) {
        if (messageContext == null) {
            return;
        }

        MerchantTrustSignalsEventStorage storage = mStorageFactory.getForLastUsedProfile();
        if (storage == null) {
            return;
        }

        updateShownMessagesTimestamp();
        storage.save(new MerchantTrustSignalsEvent(
                messageContext.getHostName(), System.currentTimeMillis()));
    }

    private void getDataForUnfamiliarMerchant(
            NavigationHandle navigationHandle, Callback<MerchantTrustSignals> callback) {
        MerchantTrustSignalsEventStorage storage = mStorageFactory.getForLastUsedProfile();
        if (storage == null || navigationHandle == null || navigationHandle.getUrl() == null
                || isFamiliarMerchant(navigationHandle.getUrl().getSpec())
                || hasReachedMaxAllowedMessageNumberInGivenTime()) {
            return;
        }

        storage.load(navigationHandle.getUrl().getHost(), (event) -> {
            if (event == null) {
                mDataProvider.getDataForNavigationHandle(navigationHandle, callback);
            } else if (System.currentTimeMillis() - event.getTimestamp()
                    > TimeUnit.SECONDS.toMillis(
                            MerchantViewerConfig.getTrustSignalsMessageWindowDurationSeconds())) {
                storage.delete(event);
                mDataProvider.getDataForNavigationHandle(navigationHandle, callback);
            } else {
                callback.onResult(null);
            }
        });
    }

    @VisibleForTesting
    void onMessageDismissed(@DismissReason int dismissReason) {
        mMetrics.recordMetricsForMessageDismissed(dismissReason);
        if (dismissReason == DismissReason.TIMER || dismissReason == DismissReason.GESTURE) {
            maybeShowStoreIcon();
        }
    }

    @VisibleForTesting
    void onMessagePrimaryAction(MerchantTrustSignals trustSignals) {
        mMetrics.recordMetricsForMessageTapped();
        launchDetailsPage(new GURL(trustSignals.getMerchantDetailsPageUrl()));
    }

    @Override
    public void onStoreInfoClicked(MerchantTrustSignals trustSignals) {
        launchDetailsPage(new GURL(trustSignals.getMerchantDetailsPageUrl()));
        // If user has clicked the "Store info" row, send a signal to disable {@link
        // FeatureConstants.PAGE_INFO_STORE_INFO_FEATURE}.
        final Tracker tracker = TrackerFactory.getTrackerForProfile(mProfileSupplier.get());
        tracker.notifyEvent(EventConstants.PAGE_INFO_STORE_INFO_ROW_CLICKED);
    }

    private void launchDetailsPage(GURL url) {
        mDetailsTabCoordinator.requestOpenSheet(url,
                mContext.getResources().getString(R.string.merchant_viewer_preview_sheet_title));
    }

    /**
     * Assuming that we only allow 3 messages in 1 hour, we store the serialized timestamps of the
     * last 3 shown messages. Before showing a message, we look up the persisted data and
     * deserialize it to 3 timestamps, then check whether the earliest timestamp is more than 1 hour
     * from now. If yes, the message can be shown thus returning false.
     */
    @VisibleForTesting
    boolean hasReachedMaxAllowedMessageNumberInGivenTime() {
        PrefService prefService = getPrefService();
        if (prefService == null) return true;
        String serializedTimestamps =
                prefService.getString(Pref.COMMERCE_MERCHANT_VIEWER_MESSAGES_SHOWN_TIME);
        if (TextUtils.isEmpty(serializedTimestamps)) return false;
        String[] timestamps = serializedTimestamps.split("_");
        int maxAllowedNumber = MerchantViewerConfig.getTrustSignalsMaxAllowedNumberInGivenWindow();
        if (timestamps.length < maxAllowedNumber) return false;
        assert timestamps.length == maxAllowedNumber;
        return System.currentTimeMillis() - Long.parseLong(timestamps[0])
                < MerchantViewerConfig.getTrustSignalsNumberCheckWindowDuration();
    }

    /**
     * Every time showing a message, we need to update the serialized timestamps.
     */
    @VisibleForTesting
    void updateShownMessagesTimestamp() {
        PrefService prefService = getPrefService();
        if (prefService == null) return;
        String currentTimestamp = Long.toString(System.currentTimeMillis());
        String serializedTimestamps =
                prefService.getString(Pref.COMMERCE_MERCHANT_VIEWER_MESSAGES_SHOWN_TIME);
        if (TextUtils.isEmpty(serializedTimestamps)) {
            prefService.setString(
                    Pref.COMMERCE_MERCHANT_VIEWER_MESSAGES_SHOWN_TIME, currentTimestamp);
        } else {
            serializedTimestamps += "_" + currentTimestamp;
            String[] timestamps = serializedTimestamps.split("_");
            int maxAllowedNumber =
                    MerchantViewerConfig.getTrustSignalsMaxAllowedNumberInGivenWindow();
            if (timestamps.length <= maxAllowedNumber) {
                prefService.setString(
                        Pref.COMMERCE_MERCHANT_VIEWER_MESSAGES_SHOWN_TIME, serializedTimestamps);
            } else {
                assert timestamps.length == maxAllowedNumber + 1;
                // Remove the earliest timestamp.
                prefService.setString(Pref.COMMERCE_MERCHANT_VIEWER_MESSAGES_SHOWN_TIME,
                        serializedTimestamps.substring(timestamps[0].length() + 1));
            }
        }
    }

    @VisibleForTesting
    PrefService getPrefService() {
        Profile profile = mProfileSupplier.get();
        if (profile == null || profile.isOffTheRecord()) {
            return null;
        }
        return UserPrefs.get(profile);
    }

    @VisibleForTesting
    void maybeShowStoreIcon() {
        if (isStoreInfoFeatureEnabled() && mOmniboxIconController != null
                && mCurrentMessageContext != null && mCurrentMessageContext.getUrl() != null) {
            mOmniboxIconController.showStoreIcon(mWindowAndroid, mCurrentMessageContext.getUrl(),
                    getStoreIconDrawable(), R.string.merchant_viewer_omnibox_icon_iph);
        }
    }

    @VisibleForTesting
    boolean isStoreInfoFeatureEnabled() {
        return PageInfoFeatures.PAGE_INFO_STORE_INFO.isEnabled();
    }

    @VisibleForTesting
    Drawable getStoreIconDrawable() {
        return ResourcesCompat.getDrawable(
                mContext.getResources(), R.drawable.ic_storefront_blue, mContext.getTheme());
    }
}
