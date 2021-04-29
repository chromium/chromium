// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.merchant_viewer;

import android.content.Context;
import android.graphics.Typeface;
import android.text.Spannable;
import android.text.SpannableStringBuilder;
import android.text.style.StyleSpan;
import android.view.View;

import androidx.annotation.VisibleForTesting;
import androidx.core.content.res.ResourcesCompat;

import org.chromium.base.Callback;
import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.browser.merchant_viewer.MerchantTrustMetrics.MessageClearReason;
import org.chromium.chrome.browser.merchant_viewer.proto.MerchantTrustSignalsOuterClass.MerchantTrustSignals;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.tab_ui.R;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.messages.DismissReason;
import org.chromium.components.messages.MessageBannerProperties;
import org.chromium.components.messages.MessageDispatcher;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.url.GURL;

import java.text.NumberFormat;
import java.util.concurrent.TimeUnit;

/**
 * Coordinator for managing merchant trust signals experience.
 */
public class MerchantTrustSignalsCoordinator {
    private static final int BASELINE_RATING = 5;

    private final MerchantTrustSignalsMediator mMediator;
    private final MerchantTrustMessageScheduler mMessageScheduler;
    private final MerchantTrustDetailsTabCoordinator mDetailsTabCoordinator;
    private final Context mContext;
    private final WindowAndroid mWindowAndroid;
    private final BottomSheetController mBottomSheetController;
    private final View mLayoutView;
    private final MerchantTrustMetrics mMetrics;
    private final MerchantTrustSignalsDataProvider mDataProvider;
    private final MerchantTrustSignalsEventStorage mStorage;

    /** Creates a new instance. */
    public MerchantTrustSignalsCoordinator(Context context, WindowAndroid windowAndroid,
            BottomSheetController bottomSheetController, View layoutView,
            TabModelSelector tabModelSelector, MessageDispatcher messageDispatcher,
            Supplier<Tab> tabSupplier, Supplier<Profile> profileSupplier,
            MerchantTrustMetrics metrics) {
        this(context, windowAndroid, bottomSheetController, layoutView, tabModelSelector,
                new MerchantTrustMessageScheduler(messageDispatcher, metrics), tabSupplier,
                new MerchantTrustSignalsDataProvider(),
                new MerchantTrustSignalsEventStorage(profileSupplier.get()), metrics);
    }

    @VisibleForTesting
    MerchantTrustSignalsCoordinator(Context context, WindowAndroid windowAndroid,
            BottomSheetController bottomSheetController, View layoutView,
            TabModelSelector tabModelSelector, MerchantTrustMessageScheduler messageScheduler,
            Supplier<Tab> tabSupplier, MerchantTrustSignalsDataProvider dataProvider,
            MerchantTrustSignalsEventStorage storage, MerchantTrustMetrics metrics) {
        mContext = context;
        mWindowAndroid = windowAndroid;
        mBottomSheetController = bottomSheetController;
        mLayoutView = layoutView;
        mDataProvider = dataProvider;
        mStorage = storage;
        mMetrics = metrics;

        mMediator = new MerchantTrustSignalsMediator(tabModelSelector, this::maybeDisplayMessage);
        mMessageScheduler = messageScheduler;
        mDetailsTabCoordinator = new MerchantTrustDetailsTabCoordinator(
                context, windowAndroid, bottomSheetController, tabSupplier, layoutView, mMetrics);
    }

    /** Cleans up internal state. */
    public void destroy() {
        mMediator.destroy();
    }

    @VisibleForTesting
    void maybeDisplayMessage(MerchantTrustMessageContext item) {
        MerchantTrustMessageContext scheduledMessage =
                mMessageScheduler.getScheduledMessageContext();
        if (scheduledMessage != null && scheduledMessage.getHostName() != null
                && scheduledMessage.getHostName().equals(item.getHostName())) {
            mMessageScheduler.expedite(this::onMessageEnqueued);
        } else {
            mMessageScheduler.clear(MessageClearReason.NAVIGATE_TO_DIFFERENT_DOMAIN);

            getDataForUnfamiliarMerchant(item.getUrl(), (trustSignals) -> {
                if (trustSignals == null) {
                    return;
                }

                mMessageScheduler.schedule(getMessagePropertyModel(item, trustSignals), item,
                        MerchantViewerConfig.DEFAULT_TRUST_SIGNALS_MESSAGE_DELAY.getValue(),
                        this::onMessageEnqueued);
            });
        }
    }

    private void onMessageEnqueued(MerchantTrustMessageContext messageContext) {
        if (messageContext == null) {
            return;
        }
        mStorage.save(new MerchantTrustSignalsEvent(
                messageContext.getHostName(), System.currentTimeMillis()));
    }

    private void getDataForUnfamiliarMerchant(GURL url, Callback<MerchantTrustSignals> callback) {
        mStorage.load(url.getHost(), (event) -> {
            if (event == null) {
                mDataProvider.getDataForUrl(url, callback);
            } else if (System.currentTimeMillis() - event.getTimestamp()
                    > TimeUnit.SECONDS.toMillis(
                            MerchantViewerConfig.TRUST_SIGNALS_MESSAGE_WINDOW_DURATION_SECONDS
                                    .getValue())) {
                mStorage.delete(event);
                mDataProvider.getDataForUrl(url, callback);
            } else {
                callback.onResult(null);
            }
        });
    }

    private void launchDetailsPage(GURL url) {
        mDetailsTabCoordinator.requestOpenSheet(url,
                mContext.getResources().getString(R.string.merchant_viewer_preview_sheet_title));
    }

    private PropertyModel getMessagePropertyModel(
            MerchantTrustMessageContext messageContext, MerchantTrustSignals trustSignals) {
        return new PropertyModel.Builder(MessageBannerProperties.ALL_KEYS)
                .with(MessageBannerProperties.ICON,
                        ResourcesCompat.getDrawable(mContext.getResources(),
                                R.drawable.ic_logo_googleg_24dp, mContext.getTheme()))
                .with(MessageBannerProperties.ICON_TINT_COLOR, MessageBannerProperties.TINT_NONE)
                .with(MessageBannerProperties.TITLE,
                        mContext.getResources().getString(R.string.merchant_viewer_message_title))
                .with(MessageBannerProperties.DESCRIPTION, getMessageDescription(trustSignals))
                .with(MessageBannerProperties.PRIMARY_BUTTON_TEXT,
                        mContext.getResources().getString(R.string.merchant_viewer_message_action))
                .with(MessageBannerProperties.ON_DISMISSED, this::onMessageDismissed)
                .with(MessageBannerProperties.ON_PRIMARY_ACTION,
                        () -> {
                            mMetrics.recordMetricsForMessageTapped();
                            launchDetailsPage(new GURL(trustSignals.getMerchantDetailsPageUrl()));
                        })
                .build();
    }

    @VisibleForTesting
    void onMessageDismissed(@DismissReason int dismissReason) {
        mMetrics.recordMetricsForMessageDismissed(dismissReason);
    }

    private Spannable getMessageDescription(MerchantTrustSignals trustSignals) {
        SpannableStringBuilder builder = new SpannableStringBuilder();
        NumberFormat numberFormatter = NumberFormat.getIntegerInstance();
        numberFormatter.setMaximumFractionDigits(1);
        builder.append(mContext.getResources().getString(
                R.string.merchant_viewer_message_description_rating,
                numberFormatter.format(trustSignals.getMerchantStarRating()),
                numberFormatter.format(BASELINE_RATING)));
        builder.append(" ");
        builder.setSpan(new StyleSpan(Typeface.BOLD), 0, builder.length(),
                Spannable.SPAN_INCLUSIVE_EXCLUSIVE);
        builder.append(mContext.getResources().getQuantityString(
                R.plurals.merchant_viewer_message_description_reviews,
                trustSignals.getMerchantCountRating(), trustSignals.getMerchantCountRating()));
        return builder;
    }
}