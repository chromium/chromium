// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui;

import android.content.Context;

import androidx.annotation.Nullable;
import androidx.appcompat.content.res.AppCompatResources;

import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ActivityTabProvider;
import org.chromium.chrome.browser.ai.AiAssistantService;
import org.chromium.chrome.browser.ai.PageSummaryButtonController;
import org.chromium.chrome.browser.bookmarks.BookmarkModel;
import org.chromium.chrome.browser.bookmarks.TabBookmarker;
import org.chromium.chrome.browser.commerce.CommerceBottomSheetContentController;
import org.chromium.chrome.browser.commerce.CommerceBottomSheetContentCoordinator;
import org.chromium.chrome.browser.commerce.CommerceBottomSheetContentProvider;
import org.chromium.chrome.browser.commerce.ShoppingServiceFactory;
import org.chromium.chrome.browser.commerce.coupons.DiscountsBottomSheetContentCoordinator;
import org.chromium.chrome.browser.commerce.coupons.DiscountsButtonController;
import org.chromium.chrome.browser.dom_distiller.ReaderModeToolbarButtonController;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.identity_disc.IdentityDiscController;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.omnibox.voice.VoiceRecognitionHandler;
import org.chromium.chrome.browser.omnibox.voice.VoiceRecognitionHandler.VoiceInteractionSource;
import org.chromium.chrome.browser.price_history.PriceHistoryBottomSheetContentCoordinator;
import org.chromium.chrome.browser.price_insights.PriceInsightsButtonController;
import org.chromium.chrome.browser.price_tracking.CurrentTabPriceTrackingStateSupplier;
import org.chromium.chrome.browser.price_tracking.PriceTrackingBottomSheetContentCoordinator;
import org.chromium.chrome.browser.price_tracking.PriceTrackingButtonController;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.readaloud.ReadAloudController;
import org.chromium.chrome.browser.readaloud.ReadAloudToolbarButtonController;
import org.chromium.chrome.browser.segmentation_platform.ContextualPageActionController;
import org.chromium.chrome.browser.share.ShareButtonController;
import org.chromium.chrome.browser.share.ShareDelegate;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.toolbar.VoiceToolbarButtonController;
import org.chromium.chrome.browser.toolbar.adaptive.AdaptiveButtonActionMenuCoordinator;
import org.chromium.chrome.browser.toolbar.adaptive.AdaptiveToolbarBehavior;
import org.chromium.chrome.browser.toolbar.adaptive.AdaptiveToolbarButtonController;
import org.chromium.chrome.browser.toolbar.adaptive.AdaptiveToolbarButtonVariant;
import org.chromium.chrome.browser.toolbar.adaptive.TranslateToolbarButtonController;
import org.chromium.chrome.browser.toolbar.optional_button.ButtonDataProvider;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.widget.scrim.ScrimManager;
import org.chromium.components.commerce.core.CommerceFeatureUtils;
import org.chromium.components.feature_engagement.Tracker;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modaldialog.ModalDialogManager;

import java.util.ArrayList;
import java.util.List;

/**
 * Acts as a bridge between {@link RootUiCoordinator} and {@link AdaptiveToolbarButtonController}.
 */
public class AdaptiveToolbarUiCoordinator {
    private final Context mContext;
    private final ActivityTabProvider mActivityTabProvider;
    private final Supplier<ModalDialogManager> mModalDialogManagerSupplier;

    private List<ButtonDataProvider> mButtonDataProviders;
    private CurrentTabPriceTrackingStateSupplier mCurrentTabPriceTrackingStateSupplier;
    private ContextualPageActionController mContextualPageActionController;
    private AdaptiveToolbarButtonController mAdaptiveToolbarButtonController;
    private VoiceToolbarButtonController mVoiceToolbarButtonController;
    private BottomSheetController mBottomSheetController;
    private ObservableSupplier<Profile> mProfileSupplier;
    private Supplier<ScrimManager> mScrimSupplier;
    private CommerceBottomSheetContentCoordinator mCommerceBottomSheetContentCoordinator;
    private Supplier<TabModelSelector> mTabModelSelectorSupplier;

    /**
     * Constructor.
     *
     * @param context {@link Context} object.
     * @param activityTabProvider {@link ActivityTabProvider} instance.
     * @param modalDialogManagerSupplier Provides access to the modal dialog manager.
     */
    public AdaptiveToolbarUiCoordinator(
            Context context,
            ActivityTabProvider activityTabProvider,
            Supplier<ModalDialogManager> modalDialogManagerSupplier) {
        mContext = context;
        mActivityTabProvider = activityTabProvider;
        mModalDialogManagerSupplier = modalDialogManagerSupplier;
        mButtonDataProviders = List.of();
    }

    void initialize(
            AdaptiveToolbarBehavior toolbarBehavior,
            ActivityLifecycleDispatcher activityLifecycleDispatcher,
            Supplier<TabModelSelector> tabModelSelectorSupplier,
            BottomSheetController bottomSheetController,
            Supplier<SnackbarManager> snackbarManagerSupplier,
            Supplier<TabBookmarker> tabBookmarkerSupplier,
            ObservableSupplier<Profile> profileSupplier,
            ObservableSupplier<BookmarkModel> bookmarkModelSupplier,
            Supplier<ReadAloudController> readAloudControllerSupplier,
            ObservableSupplier<ShareDelegate> shareDelegateSupplier,
            Runnable onShareRunnable,
            WindowAndroid windowAndroid,
            Supplier<Tracker> trackerSupplier,
            Supplier<ScrimManager> scrimSupplier) {
        if (!toolbarBehavior.shouldInitialize()) return;
        mBottomSheetController = bottomSheetController;
        mProfileSupplier = profileSupplier;
        mScrimSupplier = scrimSupplier;
        mTabModelSelectorSupplier = tabModelSelectorSupplier;
        IdentityDiscController identityDiscController =
                new IdentityDiscController(mContext, activityLifecycleDispatcher, profileSupplier);
        mCurrentTabPriceTrackingStateSupplier =
                new CurrentTabPriceTrackingStateSupplier(mActivityTabProvider, profileSupplier);

        PriceInsightsButtonController priceInsightsButtonController =
                new PriceInsightsButtonController(
                        mContext,
                        mActivityTabProvider,
                        tabModelSelectorSupplier,
                        () -> ShoppingServiceFactory.getForProfile(profileSupplier.get()),
                        mModalDialogManagerSupplier.get(),
                        bottomSheetController,
                        snackbarManagerSupplier.get(),
                        new PriceInsightsDelegateImpl(
                                mContext, mCurrentTabPriceTrackingStateSupplier),
                        AppCompatResources.getDrawable(mContext, R.drawable.ic_trending_down_24dp),
                        this::getCommerceBottomSheetContentController);
        PriceTrackingButtonController priceTrackingButtonController =
                new PriceTrackingButtonController(
                        mContext,
                        mActivityTabProvider,
                        mModalDialogManagerSupplier.get(),
                        bottomSheetController,
                        snackbarManagerSupplier.get(),
                        tabBookmarkerSupplier,
                        profileSupplier,
                        bookmarkModelSupplier,
                        mCurrentTabPriceTrackingStateSupplier);
        ReaderModeToolbarButtonController readerModeToolbarButtonController =
                new ReaderModeToolbarButtonController(
                        mContext,
                        mActivityTabProvider,
                        mModalDialogManagerSupplier.get(),
                        AppCompatResources.getDrawable(mContext, R.drawable.ic_mobile_friendly));
        ReadAloudToolbarButtonController readAloudButtonController =
                new ReadAloudToolbarButtonController(
                        mContext,
                        mActivityTabProvider,
                        AppCompatResources.getDrawable(mContext, R.drawable.ic_play_circle),
                        readAloudControllerSupplier,
                        trackerSupplier);

        ShareButtonController shareButtonController =
                new ShareButtonController(
                        mContext,
                        AppCompatResources.getDrawable(
                                mContext, R.drawable.ic_toolbar_share_offset_24dp),
                        mActivityTabProvider,
                        shareDelegateSupplier,
                        trackerSupplier,
                        mModalDialogManagerSupplier.get(),
                        onShareRunnable);
        TranslateToolbarButtonController translateToolbarButtonController =
                new TranslateToolbarButtonController(
                        mActivityTabProvider,
                        AppCompatResources.getDrawable(mContext, R.drawable.ic_translate),
                        mContext.getString(R.string.menu_translate),
                        trackerSupplier);
        AdaptiveToolbarButtonController adaptiveToolbarButtonController =
                new AdaptiveToolbarButtonController(
                        mContext,
                        activityLifecycleDispatcher,
                        profileSupplier,
                        new AdaptiveButtonActionMenuCoordinator(toolbarBehavior.canShowSettings()),
                        toolbarBehavior,
                        windowAndroid);
        PageSummaryButtonController pageSummaryButtonController =
                new PageSummaryButtonController(
                        mContext,
                        mModalDialogManagerSupplier.get(),
                        mActivityTabProvider,
                        AiAssistantService.getInstance(),
                        trackerSupplier);

        if (ChromeFeatureList.sEnableDiscountInfoApi.isEnabled()) {
            DiscountsButtonController discountsButtonController =
                    new DiscountsButtonController(
                            mContext,
                            mActivityTabProvider,
                            mModalDialogManagerSupplier.get(),
                            mBottomSheetController,
                            this::getCommerceBottomSheetContentController);
            adaptiveToolbarButtonController.addButtonVariant(
                    AdaptiveToolbarButtonVariant.DISCOUNTS, discountsButtonController);
        }

        adaptiveToolbarButtonController.addButtonVariant(
                AdaptiveToolbarButtonVariant.SHARE, shareButtonController);
        adaptiveToolbarButtonController.addButtonVariant(
                AdaptiveToolbarButtonVariant.TRANSLATE, translateToolbarButtonController);
        adaptiveToolbarButtonController.addButtonVariant(
                AdaptiveToolbarButtonVariant.PRICE_INSIGHTS, priceInsightsButtonController);
        adaptiveToolbarButtonController.addButtonVariant(
                AdaptiveToolbarButtonVariant.PRICE_TRACKING, priceTrackingButtonController);
        adaptiveToolbarButtonController.addButtonVariant(
                AdaptiveToolbarButtonVariant.READER_MODE, readerModeToolbarButtonController);
        adaptiveToolbarButtonController.addButtonVariant(
                AdaptiveToolbarButtonVariant.READ_ALOUD, readAloudButtonController);
        adaptiveToolbarButtonController.addButtonVariant(
                AdaptiveToolbarButtonVariant.PAGE_SUMMARY, pageSummaryButtonController);
        mContextualPageActionController =
                new ContextualPageActionController(
                        profileSupplier,
                        mActivityTabProvider,
                        adaptiveToolbarButtonController,
                        () -> ShoppingServiceFactory.getForProfile(profileSupplier.get()),
                        bookmarkModelSupplier);
        mAdaptiveToolbarButtonController = adaptiveToolbarButtonController;
        toolbarBehavior.registerPerSurfaceButtons(adaptiveToolbarButtonController, trackerSupplier);
        mButtonDataProviders = List.of(identityDiscController, adaptiveToolbarButtonController);
    }

    /**
     * Add voice search action button.
     *
     * @param Supplies {@link VoiceRecognitionHandler} object.
     * @param Supplies {@link Tracker} object.
     */
    public void addVoiceSearchAdaptiveButton(
            Supplier<VoiceRecognitionHandler> voiceRecognitionHandler,
            Supplier<Tracker> trackerSupplier) {
        var voiceSearchDelegate =
                new VoiceToolbarButtonController.VoiceSearchDelegate() {
                    @Override
                    public boolean isVoiceSearchEnabled() {
                        if (voiceRecognitionHandler.get() == null) return false;
                        return voiceRecognitionHandler.get().isVoiceSearchEnabled();
                    }

                    @Override
                    public void startVoiceRecognition() {
                        if (voiceRecognitionHandler.get() == null) return;
                        voiceRecognitionHandler
                                .get()
                                .startVoiceRecognition(VoiceInteractionSource.TOOLBAR);
                    }
                };
        mVoiceToolbarButtonController =
                new VoiceToolbarButtonController(
                        mContext,
                        AppCompatResources.getDrawable(mContext, R.drawable.ic_mic_white_24dp),
                        mActivityTabProvider,
                        trackerSupplier,
                        mModalDialogManagerSupplier.get(),
                        voiceSearchDelegate);

        assert mAdaptiveToolbarButtonController != null;
        mAdaptiveToolbarButtonController.addButtonVariant(
                AdaptiveToolbarButtonVariant.VOICE, mVoiceToolbarButtonController);
    }

    /**
     * Returns the list of {@link ButtonDataProvider}. The order in which the providers determines
     * which one will be shown first.
     */
    public List<ButtonDataProvider> getButtonDataProviders() {
        return mButtonDataProviders;
    }

    /** Returns {@link ContextualPageActionController} used for adaptive toolbar button. */
    public ContextualPageActionController getContextualPageActionController() {
        return mContextualPageActionController;
    }

    /** Returns {@link VoiceToolbarButtonController} used for voice search button. */
    public VoiceToolbarButtonController getVoiceToolbarButtonController() {
        return mVoiceToolbarButtonController;
    }

    /** Invokes Price Insights UI. */
    public void runPriceInsightsAction() {
        mAdaptiveToolbarButtonController.runPriceInsightsAction();
    }

    /** Destroy internally used objects. */
    public void destroy() {
        if (mCurrentTabPriceTrackingStateSupplier != null) {
            mCurrentTabPriceTrackingStateSupplier.destroy();
            mCurrentTabPriceTrackingStateSupplier = null;
        }

        if (mContextualPageActionController != null) {
            mContextualPageActionController.destroy();
            mContextualPageActionController = null;
        }

        if (mButtonDataProviders != null) {
            for (ButtonDataProvider provider : mButtonDataProviders) provider.destroy();
            mButtonDataProviders = null;
        }
    }

    private PriceTrackingBottomSheetContentCoordinator createPriceTrackingContentProvider() {
        return new PriceTrackingBottomSheetContentCoordinator(
                mContext,
                mActivityTabProvider,
                new PriceInsightsDelegateImpl(mContext, mCurrentTabPriceTrackingStateSupplier));
    }

    private DiscountsBottomSheetContentCoordinator createDiscountsContentProvider() {
        return new DiscountsBottomSheetContentCoordinator(mContext, mActivityTabProvider);
    }

    private PriceHistoryBottomSheetContentCoordinator createPriceHistoryContentProvider() {
        return new PriceHistoryBottomSheetContentCoordinator(
                mContext,
                mActivityTabProvider,
                mTabModelSelectorSupplier,
                new PriceInsightsDelegateImpl(mContext, mCurrentTabPriceTrackingStateSupplier));
    }

    @Nullable
    private CommerceBottomSheetContentController getCommerceBottomSheetContentController() {
        // This flag is for discounts and commerce bottom sheet as a feature together.
        if (mCommerceBottomSheetContentCoordinator == null
                && CommerceFeatureUtils.isDiscountInfoApiEnabled(
                        ShoppingServiceFactory.getForProfile(mProfileSupplier.get()))) {

            List<Supplier<CommerceBottomSheetContentProvider>> contentProviderSuppliers =
                    new ArrayList<>();
            contentProviderSuppliers.add(this::createPriceTrackingContentProvider);
            contentProviderSuppliers.add(this::createDiscountsContentProvider);
            contentProviderSuppliers.add(this::createPriceHistoryContentProvider);

            mCommerceBottomSheetContentCoordinator =
                    new CommerceBottomSheetContentCoordinator(
                            mContext,
                            mBottomSheetController,
                            mScrimSupplier,
                            contentProviderSuppliers);
        }

        return mCommerceBottomSheetContentCoordinator;
    }
}
