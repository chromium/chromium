// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.share.android_share_sheet;

import android.app.Activity;
import android.os.Parcelable;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.base.Callback;
import org.chromium.base.Log;
import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.browser.feature_engagement.TrackerFactory;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.share.ChromeShareExtras;
import org.chromium.chrome.browser.share.ShareHelper;
import org.chromium.chrome.browser.share.share_sheet.ChromeOptionShareCallback;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.share.ShareParams;

import java.util.List;

/**
 * Share sheet controller used to display Android share sheet.
 */
public class AndroidShareSheetController implements ChromeOptionShareCallback {
    private static final String TAG = "AndroidShare";

    private final BottomSheetController mController;
    private final Supplier<Tab> mTabProvider;
    private final Supplier<TabModelSelector> mTabModelSelectorSupplier;
    private final Supplier<Profile> mProfileSupplier;
    private final Callback<Tab> mPrintCallback;

    /**
     * Construct the controller used to display Android share sheet, and show the share sheet.
     *
     * @param params The share parameters.
     * @param chromeShareExtras The extras not contained in {@code params}.
     * @param controller The {@link BottomSheetController} for the current activity.
     * @param tabProvider Supplier for the current activity tab.
     * @param tabModelSelectorSupplier Supplier for the {@link TabModelSelector}. Used to determine
     * whether incognito mode is selected or not.
     * @param profileSupplier Supplier of the current profile of the User.
     * @param printCallback The callback used to trigger print action.
     */
    public static void showShareSheet(ShareParams params, ChromeShareExtras chromeShareExtras,
            BottomSheetController controller, Supplier<Tab> tabProvider,
            Supplier<TabModelSelector> tabModelSelectorSupplier, Supplier<Profile> profileSupplier,
            Callback<Tab> printCallback) {
        new AndroidShareSheetController(
                controller, tabProvider, tabModelSelectorSupplier, profileSupplier, printCallback)
                .showShareSheet(params, chromeShareExtras, true);
    }

    /**
     * Construct the controller used to display Android share sheet.
     *
     * @param controller The {@link BottomSheetController} for the current activity.
     * @param tabProvider Supplier for the current activity tab.
     * @param tabModelSelectorSupplier Supplier for the {@link TabModelSelector}. Used to determine
     * whether incognito mode is selected or not.
     * @param profileSupplier Supplier of the current profile of the User.
     * @param printCallback The callback used to trigger print action.
     */
    @VisibleForTesting
    AndroidShareSheetController(BottomSheetController controller, Supplier<Tab> tabProvider,
            Supplier<TabModelSelector> tabModelSelectorSupplier, Supplier<Profile> profileSupplier,
            Callback<Tab> printCallback) {
        mController = controller;
        mTabProvider = tabProvider;
        mTabModelSelectorSupplier = tabModelSelectorSupplier;
        mProfileSupplier = profileSupplier;
        mPrintCallback = printCallback;
    }

    @Override
    public void showThirdPartyShareSheet(
            ShareParams params, ChromeShareExtras chromeShareExtras, long shareStartTime) {
        showShareSheet(params, chromeShareExtras, false);
    }

    @Override
    public void showShareSheet(
            ShareParams params, ChromeShareExtras chromeShareExtras, long shareStartTime) {
        showShareSheet(params, chromeShareExtras, true);
    }

    private void showShareSheet(
            ShareParams params, ChromeShareExtras chromeShareExtras, boolean showCustomActions) {
        Profile profile = mProfileSupplier.get();
        boolean isIncognito = mTabModelSelectorSupplier.hasValue()
                && mTabModelSelectorSupplier.get().isIncognitoSelected();
        List<Parcelable> customActions = null;
        if (showCustomActions) {
            var customActionHandler = new AndroidCustomActionProvider(
                    params.getWindow().getActivity().get(), params.getWindow(), mTabProvider,
                    mController, params, mPrintCallback, isIncognito, this,
                    TrackerFactory.getTrackerForProfile(profile), params.getUrl(), profile);

            Activity activity = params.getWindow().getActivity().get();
            boolean isInMultiWindow = ApiCompatibilityUtils.isInMultiWindowMode(activity);
            customActions = customActionHandler.createCustomActions(
                    params, chromeShareExtras, isInMultiWindow);
        }

        if (customActions == null || customActions.size() == 0) {
            Log.i(TAG, "No custom actions provided.");
        }
        ShareHelper.shareWithSystemShareSheetUi(
                params, profile, chromeShareExtras.saveLastUsed(), customActions);
    }

    @VisibleForTesting
    public static void resetForTesting() {
        AndroidCustomActionProvider.unregisterBroadcastReceiver();
    }
}
