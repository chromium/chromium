// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.share.share_sheet;

import android.app.Activity;
import android.content.res.Configuration;
import android.view.View;

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
import org.chromium.chrome.browser.settings.SettingsLauncher;
import org.chromium.chrome.browser.share.ChromeShareExtras;
import org.chromium.chrome.browser.share.ShareHelper;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.ui.favicon.LargeIconBridge;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetContent;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetObserver;
import org.chromium.components.browser_ui.bottomsheet.EmptyBottomSheetObserver;
import org.chromium.components.browser_ui.share.ShareParams;
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
    private final BottomSheetObserver mBottomSheetObserver;
    private final LargeIconBridge mIconBridge;

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
            LargeIconBridge iconBridge, SettingsLauncher settingsLauncher, boolean isSyncEnabled) {
        mBottomSheetController = controller;
        mLifecycleDispatcher = lifecycleDispatcher;
        mLifecycleDispatcher.register(this);
        mTabProvider = tabProvider;
        mPropertyModelBuilder = modelBuilder;
        mPrintTabCallback = printTab;
        mSettingsLauncher = settingsLauncher;
        mIsSyncEnabled = isSyncEnabled;
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
    public void showShareSheet(
            ShareParams params, ChromeShareExtras chromeShareExtras, long shareStartTime) {
        mShareParams = params;
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
        mContentTypes = ShareSheetPropertyModelBuilder.getContentTypes(params, chromeShareExtras);
        List<PropertyModel> firstPartyApps =
                createFirstPartyPropertyModels(mActivity, params, chromeShareExtras, mContentTypes);
        List<PropertyModel> thirdPartyApps = createThirdPartyPropertyModels(
                mActivity, params, mContentTypes, chromeShareExtras.saveLastUsed());

        mBottomSheet.createRecyclerViews(
                firstPartyApps, thirdPartyApps, mContentTypes, params.getFileContentType());

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

    List<PropertyModel> createFirstPartyPropertyModels(Activity activity, ShareParams shareParams,
            ChromeShareExtras chromeShareExtras, Set<Integer> contentTypes) {
        if (mExcludeFirstParty) {
            return new ArrayList<>();
        }
        mChromeProvidedSharingOptionsProvider = new ChromeProvidedSharingOptionsProvider(activity,
                mTabProvider, mBottomSheetController, mBottomSheet, shareParams, chromeShareExtras,
                mPrintTabCallback, mSettingsLauncher, mIsSyncEnabled, mShareStartTime, this);
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

}
