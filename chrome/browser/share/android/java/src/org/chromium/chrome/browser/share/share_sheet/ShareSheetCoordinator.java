// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.share.share_sheet;

import android.app.Activity;
import android.content.res.Configuration;
import android.content.res.Resources;
import android.graphics.Bitmap;
import android.view.View;

import androidx.annotation.ColorInt;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;
import androidx.appcompat.content.res.AppCompatResources;

import org.chromium.base.Callback;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.lifecycle.ConfigurationChangedObserver;
import org.chromium.chrome.browser.multiwindow.MultiWindowUtils;
import org.chromium.chrome.browser.share.ChromeShareExtras;
import org.chromium.chrome.browser.share.ShareHelper;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.ui.favicon.IconType;
import org.chromium.chrome.browser.ui.favicon.LargeIconBridge;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetContent;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetObserver;
import org.chromium.components.browser_ui.bottomsheet.EmptyBottomSheetObserver;
import org.chromium.components.browser_ui.share.ShareParams;
import org.chromium.components.browser_ui.widget.RoundedIconGenerator;
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
    private static final String NO_SHARE_SHEET_MESSAGE = "";
    private final BottomSheetController mBottomSheetController;
    private final Supplier<Tab> mTabProvider;
    private final ShareSheetPropertyModelBuilder mPropertyModelBuilder;
    private final Callback<Tab> mPrintTabCallback;
    private long mShareStartTime;
    private boolean mExcludeFirstParty;
    private boolean mIsMultiWindow;
    private Set<Integer> mContentTypes;
    private Activity mActivity;
    private ActivityLifecycleDispatcher mLifecycleDispatcher;
    private ChromeProvidedSharingOptionsProvider mChromeProvidedSharingOptionsProvider;
    private ShareSheetBottomSheetContent mBottomSheet;
    private WindowAndroid mWindowAndroid;
    private final BottomSheetObserver mBottomSheetObserver;
    private final LargeIconBridge mIconBridge;
    private String mUrl;
    private Bitmap mIconForPreview;

    /**
     * Constructs a new ShareSheetCoordinator.
     *
     * @param controller The {@link BottomSheetController} for the current activity.
     * @param lifecycleDispatcher Dispatcher for activity lifecycle events, e.g. configuration
     * changes.
     * @param tabProvider Supplier for the current activity tab.
     * @param modelBuilder The {@link ShareSheetPropertyModelBuilder} for the share sheet.
     */
    // TODO(crbug/1022172): Should be package-protected once modularization is complete.
    public ShareSheetCoordinator(BottomSheetController controller,
            ActivityLifecycleDispatcher lifecycleDispatcher, Supplier<Tab> tabProvider,
            ShareSheetPropertyModelBuilder modelBuilder, Callback<Tab> printTab,
            LargeIconBridge iconBridge) {
        mBottomSheetController = controller;
        mLifecycleDispatcher = lifecycleDispatcher;
        mLifecycleDispatcher.register(this);
        mTabProvider = tabProvider;
        mPropertyModelBuilder = modelBuilder;
        mPrintTabCallback = printTab;
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
    }

    protected void destroy() {
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
    public void showShareSheet(
            ShareParams params, ChromeShareExtras chromeShareExtras, long shareStartTime) {
        showShareSheetWithMessage(
                NO_SHARE_SHEET_MESSAGE, params, chromeShareExtras, shareStartTime);
    }

    void showShareSheetWithMessage(String message, ShareParams params,
            ChromeShareExtras chromeShareExtras, long shareStartTime) {
        mActivity = params.getWindow().getActivity().get();
        if (mActivity == null) return;

        if (mWindowAndroid == null) {
            mWindowAndroid = params.getWindow();
            if (mWindowAndroid != null) {
                mWindowAndroid.addActivityStateObserver(this);
            }
        }

        mBottomSheet = new ShareSheetBottomSheetContent(mActivity, this, params);
        fetchFavicon(mActivity, params.getUrl());

        mShareStartTime = shareStartTime;
        mContentTypes = ShareSheetPropertyModelBuilder.getContentTypes(params, chromeShareExtras);
        List<PropertyModel> firstPartyApps =
                createFirstPartyPropertyModels(mActivity, params, chromeShareExtras, mContentTypes);
        List<PropertyModel> thirdPartyApps = createThirdPartyPropertyModels(
                mActivity, params, mContentTypes, chromeShareExtras.saveLastUsed());

        mBottomSheet.createRecyclerViews(firstPartyApps, thirdPartyApps, message);

        boolean shown = mBottomSheetController.requestShowContent(mBottomSheet, true);
        if (shown) {
            long delta = System.currentTimeMillis() - shareStartTime;
            RecordHistogram.recordMediumTimesHistogram(
                    "Sharing.SharingHubAndroid.TimeToShowShareSheet", delta);
        }
    }

    // Used by first party features to share with only non-chrome apps.
    @Override
    public void showThirdPartyShareSheet(
            ShareParams params, ChromeShareExtras chromeShareExtras, long shareStartTime) {
        mExcludeFirstParty = true;
        showShareSheet(params, chromeShareExtras, shareStartTime);
    }

    // Used by first party features to share with only non-chrome apps along with a message.
    @Override
    public void showThirdPartyShareSheetWithMessage(String message, ShareParams params,
            ChromeShareExtras chromeShareExtras, long shareStartTime) {
        mExcludeFirstParty = true;
        showShareSheetWithMessage(message, params, chromeShareExtras, shareStartTime);
    }

    List<PropertyModel> createFirstPartyPropertyModels(Activity activity, ShareParams shareParams,
            ChromeShareExtras chromeShareExtras, Set<Integer> contentTypes) {
        if (mExcludeFirstParty) {
            return new ArrayList<>();
        }
        mChromeProvidedSharingOptionsProvider = new ChromeProvidedSharingOptionsProvider(activity,
                mTabProvider, mBottomSheetController, mBottomSheet, shareParams, chromeShareExtras,
                mPrintTabCallback, mShareStartTime, this);
        mIsMultiWindow = MultiWindowUtils.getInstance().isInMultiWindowMode(activity);

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
                (shareParams) -> {
                    RecordUserAction.record("SharingHubAndroid.MoreSelected");
                    mBottomSheetController.hideContent(mBottomSheet, true);
                    ShareHelper.showDefaultShareUi(params, saveLastUsed);
                });
        models.add(morePropertyModel);

        return models;
    }

    @VisibleForTesting
    protected void disableFirstPartyFeaturesForTesting() {
        mExcludeFirstParty = true;
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

    // ConfigurationChangedObserver
    @Override
    public void onConfigurationChanged(Configuration newConfig) {
        if (mActivity == null) {
            return;
        }
        boolean isMultiWindow = MultiWindowUtils.getInstance().isInMultiWindowMode(mActivity);
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

    /** Fetches the favicon for the given url. **/
    void fetchFavicon(Activity activity, String url) {
        if (!url.isEmpty()) {
            // Update mActivity so it's non-null in onFaviconAvailable in tests.
            mActivity = activity;
            mUrl = url;
            mIconBridge.getLargeIconForStringUrl(url,
                    activity.getResources().getDimensionPixelSize(R.dimen.default_favicon_min_size),
                    this::onFaviconAvailable);
        }
    }

    /**
     * Passed as the callback to {@link LargeIconBridge#getLargeIconForStringUrl}
     * by showShareSheetWithMessage.
     */
    void onFaviconAvailable(@Nullable Bitmap icon, @ColorInt int fallbackColor,
            boolean isColorDefault, @IconType int iconType) {
        // If we didn't get a favicon, generate a monogram instead
        if (icon == null) {
            RoundedIconGenerator iconGenerator = createRoundedIconGenerator(fallbackColor);
            icon = iconGenerator.generateIconForUrl(mUrl);
            // generateIconForUrl might return null if the URL is empty or the domain cannot be
            // resolved. See https://crbug.com/987101
            // TODO(1120093): Handle the case where generating an icon fails.
            if (icon == null) {
                return;
            }
        }

        int size = mActivity.getResources().getDimensionPixelSize(
                R.dimen.sharing_hub_preview_monogram_size);

        mIconForPreview = Bitmap.createScaledBitmap(icon, size, size, true);

        if (mBottomSheet != null) {
            mBottomSheet.setFaviconForPreview(mIconForPreview);
        }
    }

    private RoundedIconGenerator createRoundedIconGenerator(@ColorInt int iconColor) {
        Resources resources = mActivity.getResources();
        int iconSize = resources.getDimensionPixelSize(R.dimen.sharing_hub_preview_monogram_size);
        int cornerRadius = iconSize / 2;
        int textSize =
                resources.getDimensionPixelSize(R.dimen.sharing_hub_preview_monogram_text_size);

        return new RoundedIconGenerator(iconSize, iconSize, cornerRadius, iconColor, textSize);
    }

    @VisibleForTesting
    Bitmap getIconForPreview() {
        return mIconForPreview;
    }
}
