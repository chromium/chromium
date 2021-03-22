// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.share.share_sheet;

import android.app.Activity;
import android.content.res.Configuration;
import android.text.TextUtils;
import android.view.View;

import androidx.annotation.VisibleForTesting;
import androidx.appcompat.content.res.AppCompatResources;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.base.Callback;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.lifecycle.ConfigurationChangedObserver;
import org.chromium.chrome.browser.share.ChromeShareExtras;
import org.chromium.chrome.browser.share.ShareHelper;
import org.chromium.chrome.browser.share.link_to_text.LinkToTextCoordinator;
import org.chromium.chrome.browser.share.link_to_text.LinkToTextCoordinator.LinkGeneration;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.modules.image_editor.ImageEditorModuleProvider;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetContent;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetObserver;
import org.chromium.components.browser_ui.bottomsheet.EmptyBottomSheetObserver;
import org.chromium.components.browser_ui.settings.SettingsLauncher;
import org.chromium.components.browser_ui.share.ShareParams;
import org.chromium.components.favicon.LargeIconBridge;
import org.chromium.components.feature_engagement.Tracker;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.base.WindowAndroid.ActivityStateObserver;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.ArrayList;
import java.util.List;
import java.util.Set;

/**
 * Coordinator for displaying the share sheet.
 */
// TODO(crbug/1022172): Should be package-protected once modularization is complete.
public class ShareSheetCoordinator implements ActivityStateObserver, ChromeOptionShareCallback,
                                              ConfigurationChangedObserver,
                                              View.OnLayoutChangeListener {
    private final BottomSheetController mBottomSheetController;
    private final Supplier<Tab> mTabProvider;
    private final ShareSheetPropertyModelBuilder mPropertyModelBuilder;
    private final Callback<Tab> mPrintTabCallback;
    private final SettingsLauncher mSettingsLauncher;
    private final boolean mIsSyncEnabled;
    private final ImageEditorModuleProvider mImageEditorModuleProvider;
    private long mShareStartTime;
    private boolean mExcludeFirstParty;
    private boolean mIsMultiWindow;
    private Set<Integer> mContentTypes;
    private Activity mActivity;
    private ActivityLifecycleDispatcher mLifecycleDispatcher;
    private ChromeProvidedSharingOptionsProvider mChromeProvidedSharingOptionsProvider;
    private ShareParams mShareParams;
    private ShareSheetBottomSheetContent mBottomSheet;
    private WindowAndroid mWindowAndroid;
    private ChromeShareExtras mChromeShareExtras;
    private LinkToTextCoordinator mLinkToTextCoordinator;
    private final BottomSheetObserver mBottomSheetObserver;
    private final LargeIconBridge mIconBridge;
    private final Tracker mFeatureEngagementTracker;
    private String mShareDetailsForMetrics;

    /**
     * Constructs a new ShareSheetCoordinator.
     *
     * @param controller The {@link BottomSheetController} for the current activity.
     * @param lifecycleDispatcher Dispatcher for activity lifecycle events, e.g. configuration
     * changes.
     * @param tabProvider Supplier for the current activity tab.
     * @param modelBuilder The {@link ShareSheetPropertyModelBuilder} for the share sheet.
     * @param imageEditorModuleProvider Image Editor module entry point if present in the APK.
     */
    // TODO(crbug/1022172): Should be package-protected once modularization is complete.
    public ShareSheetCoordinator(BottomSheetController controller,
            ActivityLifecycleDispatcher lifecycleDispatcher, Supplier<Tab> tabProvider,
            ShareSheetPropertyModelBuilder modelBuilder, Callback<Tab> printTab,
            LargeIconBridge iconBridge, SettingsLauncher settingsLauncher, boolean isSyncEnabled,
            ImageEditorModuleProvider imageEditorModuleProvider, Tracker featureEngagementTracker) {
        mBottomSheetController = controller;
        mLifecycleDispatcher = lifecycleDispatcher;
        mLifecycleDispatcher.register(this);
        mTabProvider = tabProvider;
        mPropertyModelBuilder = modelBuilder;
        mPrintTabCallback = printTab;
        mSettingsLauncher = settingsLauncher;
        mIsSyncEnabled = isSyncEnabled;
        mImageEditorModuleProvider = imageEditorModuleProvider;
        mBottomSheetObserver = new EmptyBottomSheetObserver() {
            @Override
            public void onSheetContentChanged(BottomSheetContent bottomSheet) {
                super.onSheetContentChanged(bottomSheet);
                if (bottomSheet == mBottomSheet) {
                    mBottomSheet.getContentView().addOnLayoutChangeListener(
                            ShareSheetCoordinator.this::onLayoutChange);
                } else {
                    mBottomSheet.getContentView().removeOnLayoutChangeListener(
                            ShareSheetCoordinator.this::onLayoutChange);
                }
            }
        };
        mBottomSheetController.addObserver(mBottomSheetObserver);
        mIconBridge = iconBridge;
        mFeatureEngagementTracker = featureEngagementTracker;
    }

    protected void destroy() {
        if (mShareParams != null) {
            ShareParams.TargetChosenCallback callback = mShareParams.getCallback();
            if (callback != null) {
                callback.onCancel();
            }
        }
        if (mWindowAndroid != null) {
            mWindowAndroid.removeActivityStateObserver(this);
            mWindowAndroid = null;
        }
        if (mLifecycleDispatcher != null) {
            mLifecycleDispatcher.unregister(this);
            mLifecycleDispatcher = null;
        }
    }

    // TODO(crbug/1022172): Should be package-protected once modularization is complete.
    @Override
    public void showShareSheet(
            ShareParams params, ChromeShareExtras chromeShareExtras, long shareStartTime) {
        mShareParams = params;
        mChromeShareExtras = chromeShareExtras;
        mActivity = params.getWindow().getActivity().get();
        if (mActivity == null) return;

        if (mWindowAndroid == null) {
            mWindowAndroid = params.getWindow();
            if (mWindowAndroid != null) {
                mWindowAndroid.addActivityStateObserver(this);
            }
        }

        mBottomSheet = new ShareSheetBottomSheetContent(mActivity, mIconBridge, this, params);

        mShareStartTime = shareStartTime;
        if (ChromeFeatureList.isEnabled(ChromeFeatureList.PREEMPTIVE_LINK_TO_TEXT_GENERATION)) {
            setShareMetrics(mBottomSheet.getLinkGenerationState());
        }
        updateShareSheet();

        boolean shown = mBottomSheetController.requestShowContent(mBottomSheet, true);
        if (shown) {
            long delta = System.currentTimeMillis() - shareStartTime;
            RecordHistogram.recordMediumTimesHistogram(
                    "Sharing.SharingHubAndroid.TimeToShowShareSheet", delta);
        }
    }

    /**
     * Updates {@code mShareParams} from the {@link LinkGeneration} state.
     * Called when toggling between LinkToText options
     *
     * @param state The state from {@link LinkGeneration} to which ShareParams should be updated.
     */
    void updateShareSheetForLinkToText(@LinkGeneration int state) {
        if (!ChromeFeatureList.isEnabled(ChromeFeatureList.PREEMPTIVE_LINK_TO_TEXT_GENERATION)
                || mLinkToTextCoordinator == null) {
            return;
        }

        mShareParams = mLinkToTextCoordinator.getShareParams(state);
        mBottomSheet.updateShareParams(mShareParams);
        setShareMetrics(state);
        updateShareSheet();
    }

    private void setShareMetrics(@LinkGeneration int state) {
        switch (state) {
            case LinkGeneration.LINK:
                mShareDetailsForMetrics =
                        "SharingHubAndroid.LinkGeneration.Success.LinkToTextShared";
                break;
            case LinkGeneration.TEXT:
                mShareDetailsForMetrics = "SharingHubAndroid.LinkGeneration.Success.TextShared";
                break;
            case LinkGeneration.FAILURE:
                mShareDetailsForMetrics = "SharingHubAndroid.LinkGeneration.Failure.TextShared";
                break;
            default:
                break;
        }
    }

    private void updateShareSheet() {
        mContentTypes =
                ShareSheetPropertyModelBuilder.getContentTypes(mShareParams, mChromeShareExtras);
        List<PropertyModel> firstPartyApps = createFirstPartyPropertyModels(
                mActivity, mShareParams, mChromeShareExtras, mContentTypes);
        List<PropertyModel> thirdPartyApps = createThirdPartyPropertyModels(
                mActivity, mShareParams, mContentTypes, mChromeShareExtras.saveLastUsed());

        mBottomSheet.createRecyclerViews(
                firstPartyApps, thirdPartyApps, mContentTypes, mShareParams.getFileContentType());
    }

    /**
     * If preemptive link to text generation is enable, create LinkTotextCoordinator
     * which will generate link to text, create a new share and show share sheet.
     * Otherwise show share sheet with the current share.
     *
     * @param params The {@link ShareParams} for the current share.
     * @param chromeShareExtras The {@link ChromeShareExtras} for the current share.
     * @param shareStartTime The start time of the current share.
     */
    public void showInitialShareSheet(
            ShareParams params, ChromeShareExtras chromeShareExtras, long shareStartTime) {
        if (ChromeFeatureList.isEnabled(ChromeFeatureList.PREEMPTIVE_LINK_TO_TEXT_GENERATION)
                && chromeShareExtras.isUserHighlightedText()) {
            String tabUrl =
                    mTabProvider.get().isInitialized() ? mTabProvider.get().getUrl().getSpec() : "";
            mLinkToTextCoordinator =
                    new LinkToTextCoordinator(params, mTabProvider.get(), this, chromeShareExtras,
                            shareStartTime, getUrlToShare(params, chromeShareExtras, tabUrl));
            return;
        }

        showShareSheet(params, chromeShareExtras, shareStartTime);
    }

    // Used by first party features to share with only non-chrome apps.
    @Override
    public void showThirdPartyShareSheet(
            ShareParams params, ChromeShareExtras chromeShareExtras, long shareStartTime) {
        mExcludeFirstParty = true;
        showShareSheet(params, chromeShareExtras, shareStartTime);
    }

    List<PropertyModel> createFirstPartyPropertyModels(Activity activity, ShareParams shareParams,
            ChromeShareExtras chromeShareExtras, Set<Integer> contentTypes) {
        if (mExcludeFirstParty) {
            return new ArrayList<>();
        }
        mChromeProvidedSharingOptionsProvider = new ChromeProvidedSharingOptionsProvider(activity,
                mTabProvider, mBottomSheetController, mBottomSheet, shareParams, chromeShareExtras,
                mPrintTabCallback, mSettingsLauncher, mIsSyncEnabled, mShareStartTime, this,
                mImageEditorModuleProvider, mFeatureEngagementTracker,
                getUrlToShare(shareParams, chromeShareExtras,
                        mTabProvider.get().isInitialized() ? mTabProvider.get().getUrl().getSpec()
                                                           : ""),
                mShareDetailsForMetrics);
        mIsMultiWindow = ApiCompatibilityUtils.isInMultiWindowMode(activity);

        return mChromeProvidedSharingOptionsProvider.getPropertyModels(
                contentTypes, mIsMultiWindow);
    }

    @VisibleForTesting
    List<PropertyModel> createThirdPartyPropertyModels(Activity activity, ShareParams params,
            Set<Integer> contentTypes, boolean saveLastUsed) {
        if (params == null) return null;
        List<PropertyModel> models = mPropertyModelBuilder.selectThirdPartyApps(mBottomSheet,
                contentTypes, params, saveLastUsed, params.getWindow(), mShareStartTime);
        // More...
        PropertyModel morePropertyModel = ShareSheetPropertyModelBuilder.createPropertyModel(
                AppCompatResources.getDrawable(activity, R.drawable.sharing_more),
                activity.getResources().getString(R.string.sharing_more_icon_label),
                (shareParams)
                        -> {
                    RecordUserAction.record("SharingHubAndroid.MoreSelected");
                    if (ChromeFeatureList.isEnabled(
                                ChromeFeatureList.PREEMPTIVE_LINK_TO_TEXT_GENERATION)
                            && mShareDetailsForMetrics != null) {
                        RecordUserAction.record(mShareDetailsForMetrics);
                    }
                    mBottomSheetController.hideContent(mBottomSheet, true);
                    ShareHelper.showDefaultShareUi(params, saveLastUsed);
                },
                /*displayNew*/ false);
        models.add(morePropertyModel);

        return models;
    }

    @VisibleForTesting
    protected void disableFirstPartyFeaturesForTesting() {
        mExcludeFirstParty = true;
    }

    /**
     * Returns the url to share.
     *
     * <p>This prioritizes the URL in {@link ShareParams}, but if it does not exist, we look for an
     * image source URL from {@link ChromeShareExtras}. The image source URL is not contained in
     * {@link ShareParams#getUrl()} because we do not want to share the image URL with the image
     * file in third-party app shares. If both are empty then current tab URL is used. This is
     * useful for {@link LinkToTextCoordinator} that needs URL but it cannot be provided through
     * {@link ShareParams}.
     */
    private String getUrlToShare(
            ShareParams shareParams, ChromeShareExtras chromeShareExtras, String tabUrl) {
        if (!TextUtils.isEmpty(shareParams.getUrl())) {
            return shareParams.getUrl();
        } else if (!chromeShareExtras.getImageSrcUrl().isEmpty()) {
            return chromeShareExtras.getImageSrcUrl().getSpec();
        }
        return tabUrl;
    }

    // ActivityStateObserver
    @Override
    public void onActivityResumed() {}

    @Override
    public void onActivityPaused() {
        if (mBottomSheet != null) {
            mBottomSheetController.hideContent(mBottomSheet, true);
        }
    }

    @Override
    public void onActivityDestroyed() {}

    // ConfigurationChangedObserver
    @Override
    public void onConfigurationChanged(Configuration newConfig) {
        if (mActivity == null) {
            return;
        }
        boolean isMultiWindow = ApiCompatibilityUtils.isInMultiWindowMode(mActivity);
        // mContentTypes is null if Chrome features should not be shown.
        if (mIsMultiWindow == isMultiWindow || mContentTypes == null) {
            return;
        }

        mIsMultiWindow = isMultiWindow;
        mBottomSheet.createFirstPartyRecyclerViews(
                mChromeProvidedSharingOptionsProvider.getPropertyModels(
                        mContentTypes, mIsMultiWindow));
        mBottomSheetController.requestShowContent(mBottomSheet, /*animate=*/false);
    }

    // View.OnLayoutChangeListener
    @Override
    public void onLayoutChange(View v, int left, int top, int right, int bottom, int oldLeft,
            int oldTop, int oldRight, int oldBottom) {
        if ((oldRight - oldLeft) == (right - left)) {
            return;
        }
        mBottomSheet.getFirstPartyView().invalidate();
        mBottomSheet.getFirstPartyView().requestLayout();
        mBottomSheet.getThirdPartyView().invalidate();
        mBottomSheet.getThirdPartyView().requestLayout();
    }

}
