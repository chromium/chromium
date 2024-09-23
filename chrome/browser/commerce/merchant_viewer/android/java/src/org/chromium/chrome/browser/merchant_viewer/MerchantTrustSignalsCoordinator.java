// Copyright 2021 The Chromium Authors
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

import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.chrome.browser.feature_engagement.TrackerFactory;
import org.chromium.chrome.browser.merchant_viewer.MerchantTrustMetrics.BottomSheetOpenedSource;
import org.chromium.chrome.browser.merchant_viewer.MerchantTrustMetrics.MessageClearReason;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.tab_ui.R;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.commerce.core.ShoppingService.MerchantInfo;
import org.chromium.components.feature_engagement.EventConstants;
import org.chromium.components.feature_engagement.Tracker;
import org.chromium.components.messages.DismissReason;
import org.chromium.components.messages.MessageDispatcher;
import org.chromium.components.prefs.PrefService;
import org.chromium.components.security_state.ConnectionSecurityLevel;
import org.chromium.components.security_state.SecurityStateModel;
import org.chromium.components.site_engagement.SiteEngagementService;
import org.chromium.components.user_prefs.UserPrefs;
import org.chromium.content_public.browser.NavigationHandle;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.IntentRequestTracker;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.url.GURL;

/** Coordinator for managing merchant trust signals experience. */
public class MerchantTrustSignalsCoordinator
        implements PageInfoStoreInfoController.StoreInfoActionHandler,
                MerchantTrustMessageViewModel.MessageActionsHandler {
    /** Interface to control the omnibox store icon and the related IPH. */
    public interface OmniboxIconController {
        /**
         * Show the store icon in omnibox.
         * @param window The {@link WindowAndroid} of the app.
         * @param url The url associated with the just showing {@link MerchantTrustMessageContext}.
         * @param drawable The store icon drawable.
         * @param stringId Resource id of the IPH string.
         * @param canShowIph Whether it is eligible to show IPH when showing the store icon. For
         *         example, when user swipes the message, we don't want to show the IPH which may be
         *         annoying.
         */
        void showStoreIcon(
                WindowAndroid window,
                String url,
                Drawable drawable,
                @StringRes int stringId,
                boolean canShowIph);
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
    private final ObservableSupplier<Tab> mTabSupplier;
    private OmniboxIconController mOmniboxIconController;

    /** Creates a new instance. */
    public MerchantTrustSignalsCoordinator(
            Context context,
            WindowAndroid windowAndroid,
            BottomSheetController bottomSheetController,
            View layoutView,
            MessageDispatcher messageDispatcher,
            ObservableSupplier<Tab> tabSupplier,
            ObservableSupplier<Profile> profileSupplier,
            MerchantTrustMetrics metrics,
            IntentRequestTracker intentRequestTracker) {
        this(
                context,
                windowAndroid,
                new MerchantTrustMessageScheduler(messageDispatcher, metrics, tabSupplier),
                tabSupplier,
                new MerchantTrustSignalsDataProvider(),
                profileSupplier,
                metrics,
                new MerchantTrustBottomSheetCoordinator(
                        context,
                        windowAndroid,
                        bottomSheetController,
                        tabSupplier,
                        layoutView,
                        metrics,
                        intentRequestTracker,
                        profileSupplier),
                new MerchantTrustSignalsStorageFactory(profileSupplier));
    }

    @VisibleForTesting
    MerchantTrustSignalsCoordinator(
            Context context,
            WindowAndroid windowAndroid,
            MerchantTrustMessageScheduler messageScheduler,
            ObservableSupplier<Tab> tabSupplier,
            MerchantTrustSignalsDataProvider dataProvider,
            ObservableSupplier<Profile> profileSupplier,
            MerchantTrustMetrics metrics,
            MerchantTrustBottomSheetCoordinator detailsTabCoordinator,
            MerchantTrustSignalsStorageFactory storageFactory) {
        mContext = context;
        mDataProvider = dataProvider;
        mMetrics = metrics;
        mStorageFactory = storageFactory;
        mProfileSupplier = profileSupplier;
        mWindowAndroid = windowAndroid;
        mTabSupplier = tabSupplier;

        mMediator =
                new MerchantTrustSignalsMediator(
                        tabSupplier, this::onFinishEligibleNavigation, metrics);
        mMessageScheduler = messageScheduler;
        mDetailsTabCoordinator = detailsTabCoordinator;
    }

    /** Cleans up internal state. */
    public void destroy() {
        mMediator.destroy();
        mStorageFactory.destroy();
    }

    /** Set the {@link OmniboxIconController} to manage the store icon in omnibox. */
    public void setOmniboxIconController(@Nullable OmniboxIconController omniboxIconController) {
        mOmniboxIconController = omniboxIconController;
    }

    @VisibleForTesting
    void onFinishEligibleNavigation(MerchantTrustMessageContext item) {
        NavigationHandle navigationHandle = item.getNavigationHandle();
        if (navigationHandle == null || navigationHandle.getUrl() == null) {
            return;
        }
        boolean shouldExpediteMessage;
        MerchantTrustMessageContext scheduledMessage =
                mMessageScheduler.getScheduledMessageContext();
        if (scheduledMessage != null
                && scheduledMessage.getHostName() != null
                && scheduledMessage.getHostName().equals(item.getHostName())
                && !scheduledMessage.getUrl().equals(item.getUrl())) {
            // When user enters PageInfo, we fetch data based on url which is less reliable than the
            // navigation-based one. Thus, to make sure the "Store info" row is visible, we still
            // need to fetch data based on this navigation before showing the message, rather than
            // using the scheduled message directly.
            mMessageScheduler.clear(MessageClearReason.NAVIGATE_TO_SAME_DOMAIN);
            shouldExpediteMessage = true;
        } else {
            mMessageScheduler.clear(MessageClearReason.NAVIGATE_TO_DIFFERENT_DOMAIN);
            shouldExpediteMessage = false;
        }
        mDataProvider.getDataForUrl(
                mProfileSupplier.get(),
                navigationHandle.getUrl(),
                (merchantInfo) -> maybeDisplayMessage(merchantInfo, item, shouldExpediteMessage));
    }

    @VisibleForTesting
    void maybeDisplayMessage(
            MerchantInfo merchantInfo,
            MerchantTrustMessageContext item,
            boolean shouldExpediteMessage) {
        if (merchantInfo == null) return;
        mMetrics.recordUkmOnDataAvailable(item.getWebContents());
        NavigationHandle navigationHandle = item.getNavigationHandle();
        MerchantTrustSignalsEventStorage storage = mStorageFactory.getForLastUsedProfile();
        if (navigationHandle == null
                || navigationHandle.getUrl() == null
                || storage == null
                || MerchantViewerConfig.isTrustSignalsMessageDisabled()
                || merchantInfo.proactiveMessageDisabled
                || isMerchantRatingBelowThreshold(merchantInfo)
                || isNonPersonalizedFamiliarMerchant(merchantInfo)
                || isFamiliarMerchant(navigationHandle.getUrl().getSpec())
                || hasReachedMaxAllowedMessageNumberInGivenTime()
                || !isOnSecureWebsite(item.getWebContents())) {
            return;
        }

        storage.load(
                navigationHandle.getUrl().getHost(),
                (event) -> {
                    if (event == null) {
                        scheduleMessage(merchantInfo, item, shouldExpediteMessage);
                    } else if (System.currentTimeMillis() - event.getTimestamp()
                            > MerchantViewerConfig
                                    .getTrustSignalsMessageWindowDurationMilliSeconds()) {
                        storage.delete(event);
                        scheduleMessage(merchantInfo, item, shouldExpediteMessage);
                    }
                });
    }

    private void scheduleMessage(
            MerchantInfo merchantInfo,
            MerchantTrustMessageContext item,
            boolean shouldExpediteMessage) {
        assert (merchantInfo != null) && (item != null);
        mMessageScheduler.schedule(
                MerchantTrustMessageViewModel.create(mContext, merchantInfo, item.getUrl(), this),
                merchantInfo.starRating,
                item,
                shouldExpediteMessage
                        ? MerchantTrustMessageScheduler.MESSAGE_ENQUEUE_NO_DELAY
                        : MerchantViewerConfig.getDefaultTrustSignalsMessageDelay(),
                this::onMessageEnqueued);
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
    boolean isOnSecureWebsite(WebContents webContents) {
        return SecurityStateModel.getSecurityLevelForWebContents(webContents)
                == ConnectionSecurityLevel.SECURE;
    }

    private boolean isMerchantRatingBelowThreshold(MerchantInfo merchantInfo) {
        return merchantInfo.starRating
                < MerchantViewerConfig.getTrustSignalsMessageRatingThreshold();
    }

    private boolean isNonPersonalizedFamiliarMerchant(MerchantInfo merchantInfo) {
        return merchantInfo.nonPersonalizedFamiliarityScore
                > MerchantViewerConfig.getTrustSignalsNonPersonalizedFamiliarityScoreThreshold();
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

        mMetrics.recordUkmOnMessageSeen(messageContext.getWebContents());
        updateShownMessagesTimestamp();
        storage.save(
                new MerchantTrustSignalsEvent(
                        messageContext.getHostName(), System.currentTimeMillis()));
    }

    // MerchantTrustMessageViewModel.MessageActionsHandler implementations.
    @Override
    public void onMessageDismissed(@DismissReason int dismissReason, String messageAssociatedUrl) {
        mMetrics.recordMetricsForMessageDismissed(dismissReason);
        if (dismissReason == DismissReason.TIMER || dismissReason == DismissReason.GESTURE) {
            maybeShowStoreIcon(messageAssociatedUrl, dismissReason == DismissReason.TIMER);
        }
    }

    @Override
    public void onMessagePrimaryAction(MerchantInfo merchantInfo, String messageAssociatedUrl) {
        mMetrics.recordMetricsForMessageTapped();

        // TODO(crbug.com/40216480): Pass webContents directly to this method instead of using
        // mTabSupplier.
        if (mTabSupplier.hasValue()) {
            mMetrics.recordUkmOnMessageClicked(mTabSupplier.get().getWebContents());
        }
        launchDetailsPage(
                merchantInfo.detailsPageUrl,
                BottomSheetOpenedSource.FROM_MESSAGE,
                messageAssociatedUrl);
    }

    // PageInfoStoreInfoController.StoreInfoActionHandler implementation.
    @Override
    public void onStoreInfoClicked(MerchantInfo merchantInfo) {
        launchDetailsPage(
                merchantInfo.detailsPageUrl, BottomSheetOpenedSource.FROM_PAGE_INFO, null);
        // If user has clicked the "Store info" row, send a signal to disable {@link
        // FeatureConstants.PAGE_INFO_STORE_INFO_FEATURE}.
        final Tracker tracker = TrackerFactory.getTrackerForProfile(mProfileSupplier.get());
        tracker.notifyEvent(EventConstants.PAGE_INFO_STORE_INFO_ROW_CLICKED);
    }

    private void launchDetailsPage(
            GURL detailsPageUrl,
            @BottomSheetOpenedSource int openSource,
            @Nullable String messageAssociatedUrl) {
        mMetrics.recordMetricsForBottomSheetOpenedSource(openSource);
        mDetailsTabCoordinator.requestOpenSheet(
                detailsPageUrl,
                mContext.getResources().getString(R.string.merchant_viewer_preview_sheet_title),
                () -> onBottomSheetDismissed(openSource, messageAssociatedUrl));
    }

    private void onBottomSheetDismissed(
            @BottomSheetOpenedSource int openSource, @Nullable String messageAssociatedUrl) {
        if (openSource == BottomSheetOpenedSource.FROM_MESSAGE) {
            maybeShowStoreIcon(messageAssociatedUrl, true);
        }
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

    /** Every time showing a message, we need to update the serialized timestamps. */
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
                prefService.setString(
                        Pref.COMMERCE_MERCHANT_VIEWER_MESSAGES_SHOWN_TIME,
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
    void maybeShowStoreIcon(@Nullable String messageAssociatedUrl, boolean canShowIph) {
        if (mOmniboxIconController != null && messageAssociatedUrl != null) {
            mOmniboxIconController.showStoreIcon(
                    mWindowAndroid,
                    messageAssociatedUrl,
                    getStoreIconDrawable(),
                    R.string.merchant_viewer_omnibox_icon_iph,
                    canShowIph);
        }
    }

    @VisibleForTesting
    Drawable getStoreIconDrawable() {
        return ResourcesCompat.getDrawable(
                mContext.getResources(), R.drawable.ic_storefront_blue, mContext.getTheme());
    }
}
