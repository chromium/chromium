// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tips;

import android.content.Context;
import android.content.Intent;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.layouts.LayoutManager;
import org.chromium.chrome.browser.lens.LensController;
import org.chromium.chrome.browser.lens.LensEntryPoint;
import org.chromium.chrome.browser.lens.LensIntentParams;
import org.chromium.chrome.browser.lens.LensMetrics;
import org.chromium.chrome.browser.quick_delete.QuickDeleteController;
import org.chromium.chrome.browser.safe_browsing.metrics.SettingsAccessPoint;
import org.chromium.chrome.browser.safe_browsing.settings.SafeBrowsingSettingsFragment;
import org.chromium.chrome.browser.settings.SettingsNavigationFactory;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tab_ui.TabSwitcherUtils;
import org.chromium.chrome.browser.tabmodel.TabCreator;
import org.chromium.chrome.browser.tips.TipsPromoProperties.FeatureTipPromoData;
import org.chromium.chrome.browser.toolbar.settings.AddressBarSettingsFragment;
import org.chromium.chrome.browser.toolbar.settings.AddressBarSettingsFragment.HighlightedOption;
import org.chromium.chrome.browser.ui.signin.BottomSheetSigninAndHistorySyncCoordinator;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.ui.base.PageTransition;
import org.chromium.ui.base.WindowAndroid;

import java.util.function.Supplier;

/**
 * Fallback implementation of {@link TipsPromoHandler} providing legacy behavior for feature tips
 * that have not migrated to self-service, or when self-service is disabled.
 */
@NullMarked
class LegacyTipsPromoHandler implements TipsPromoHandler {
    private final @TipsNotificationsFeatureType int mFeatureType;
    private final WindowAndroid mWindowAndroid;
    private final boolean mIsIncognito;
    private final boolean mIsUserSignedIn;
    private final Supplier<QuickDeleteController> mQuickDeleteControllerCreator;
    private final BottomSheetSigninAndHistorySyncCoordinator mSigninCoordinator;
    private final TabCreator mRegularTabCreator;
    private final Supplier<LayoutManager> mLayoutManagerSupplier;
    private final Supplier<LensController> mLensControllerSupplier;

    /**
     * Constructs a {@link LegacyTipsPromoHandler}.
     *
     * @param featureType The {@link TipsNotificationsFeatureType} for the promo.
     * @param windowAndroid The {@link WindowAndroid} for UI actions.
     * @param isIncognito Whether the current tab model is incognito.
     * @param isUserSignedIn Whether the user is signed in to Chrome.
     * @param quickDeleteControllerCreator Supplier for {@link QuickDeleteController}.
     * @param signinCoordinator Coordinator for sign-in flows.
     * @param regularTabCreator Tab creator for launching new regular tabs.
     * @param layoutManagerSupplier Supplier for {@link LayoutManager}.
     * @param lensControllerSupplier Supplier for {@link LensController}.
     */
    LegacyTipsPromoHandler(
            @TipsNotificationsFeatureType int featureType,
            WindowAndroid windowAndroid,
            boolean isIncognito,
            boolean isUserSignedIn,
            Supplier<QuickDeleteController> quickDeleteControllerCreator,
            BottomSheetSigninAndHistorySyncCoordinator signinCoordinator,
            TabCreator regularTabCreator,
            Supplier<LayoutManager> layoutManagerSupplier,
            Supplier<LensController> lensControllerSupplier) {
        mFeatureType = featureType;
        mWindowAndroid = windowAndroid;
        mIsIncognito = isIncognito;
        mIsUserSignedIn = isUserSignedIn;
        mQuickDeleteControllerCreator = quickDeleteControllerCreator;
        mSigninCoordinator = signinCoordinator;
        mRegularTabCreator = regularTabCreator;
        mLayoutManagerSupplier = layoutManagerSupplier;
        mLensControllerSupplier = lensControllerSupplier;
    }

    @Override
    public FeatureTipPromoData getPromoData(Context context) {
        return TipsUtils.getFeatureTipPromoDataForType(context, mFeatureType, mIsUserSignedIn);
    }

    @Override
    public void onPromoAccepted(Context context) {
        switch (mFeatureType) {
            case TipsNotificationsFeatureType.ENHANCED_SAFE_BROWSING:
                Intent intent =
                        SettingsNavigationFactory.createSettingsNavigation()
                                .createSettingsIntent(
                                        context,
                                        SafeBrowsingSettingsFragment.class,
                                        SafeBrowsingSettingsFragment.createArguments(
                                                SettingsAccessPoint.TIPS_NOTIFICATIONS_PROMO));
                context.startActivity(intent);
                break;
            case TipsNotificationsFeatureType.QUICK_DELETE:
                mQuickDeleteControllerCreator.get().showDialog();
                break;
            case TipsNotificationsFeatureType.GOOGLE_LENS:
                LensMetrics.recordClicked(LensEntryPoint.TIPS_NOTIFICATIONS);
                mLensControllerSupplier
                        .get()
                        .startLens(
                                mWindowAndroid,
                                new LensIntentParams.Builder(
                                                LensEntryPoint.TIPS_NOTIFICATIONS, mIsIncognito)
                                        .build());
                break;
            case TipsNotificationsFeatureType.BOTTOM_OMNIBOX:
                SettingsNavigationFactory.createSettingsNavigation()
                        .startSettings(
                                context,
                                AddressBarSettingsFragment.class,
                                AddressBarSettingsFragment.createArguments(
                                        HighlightedOption.BOTTOM_TOOLBAR));
                break;
            case TipsNotificationsFeatureType.PASSWORD_AUTOFILL:
                // No-op since there is no page to travel to.
                break;
            case TipsNotificationsFeatureType.SIGNIN:
                // The user must be signed out in order to see this flow.
                if (!mIsUserSignedIn) {
                    mSigninCoordinator.startSigninFlow(
                            TipsUtils.getAccountPickerBottomSheetConfig(context));
                }
                break;
            case TipsNotificationsFeatureType.CREATE_TAB_GROUPS:
                TabSwitcherUtils.navigateToTabSwitcher(
                        mLayoutManagerSupplier.get(),
                        /* animate= */ true,
                        /* onNavigationFinished= */ null);
                break;
            case TipsNotificationsFeatureType.CUSTOMIZE_MVT:
                // No-op since there is no page to travel to.
                break;
            case TipsNotificationsFeatureType.RECENT_TABS:
                LoadUrlParams params =
                        new LoadUrlParams(
                                UrlConstants.RECENT_TABS_URL, PageTransition.AUTO_BOOKMARK);
                mRegularTabCreator.createNewTab(
                        params, TabLaunchType.FROM_CHROME_UI, /* parent= */ null);
                break;
            default:
                assert false : "Invalid feature type: " + mFeatureType;
        }
    }

    @Override
    public void onPromoShown(TipsPromoCustomizer customizer) {
        switch (mFeatureType) {
            case TipsNotificationsFeatureType.QUICK_DELETE:
                customizer.setLogoAnimation(R.raw.tips_notifications_quick_delete_logo_anim);
                break;
            case TipsNotificationsFeatureType.GOOGLE_LENS:
                LensMetrics.recordShown(LensEntryPoint.TIPS_NOTIFICATIONS, /* isShown= */ true);
                break;
            case TipsNotificationsFeatureType.SIGNIN:
                if (mIsUserSignedIn) {
                    customizer.setDetailsButtonVisibility(false);
                    customizer.setDescriptionVisibility(false);
                }
                break;
            case TipsNotificationsFeatureType.RECENT_TABS:
                customizer.setLogoTopPadding(
                        R.dimen.tips_notifications_bottom_sheet_vertical_margin);
                break;
            default:
                break;
        }
    }
}
