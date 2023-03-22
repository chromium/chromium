// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.share.android_share_sheet;

import android.app.Activity;
import android.content.Context;
import android.graphics.Bitmap;
import android.net.Uri;
import android.os.SystemClock;
import android.text.TextUtils;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.base.Callback;
import org.chromium.base.Log;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.feature_engagement.TrackerFactory;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.share.ChromeCustomShareAction;
import org.chromium.chrome.browser.share.ChromeShareExtras;
import org.chromium.chrome.browser.share.ShareContentTypeHelper;
import org.chromium.chrome.browser.share.ShareContentTypeHelper.ContentType;
import org.chromium.chrome.browser.share.ShareHelper;
import org.chromium.chrome.browser.share.share_sheet.ChromeOptionShareCallback;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.ui.favicon.FaviconHelper;
import org.chromium.chrome.browser.ui.favicon.FaviconUtils;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.share.ShareImageFileUtils;
import org.chromium.components.browser_ui.share.ShareParams;
import org.chromium.url.GURL;

import java.util.Set;

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
        Activity activity = params.getWindow().getActivity().get();
        ChromeCustomShareAction.Provider provider = null;

        if (showCustomActions) {
            boolean isInMultiWindow = ApiCompatibilityUtils.isInMultiWindowMode(activity);
            var actionProvider =
                    new AndroidCustomActionProvider(params.getWindow().getActivity().get(),
                            params.getWindow(), mTabProvider, mController, params, mPrintCallback,
                            isIncognito, this, TrackerFactory.getTrackerForProfile(profile),
                            params.getUrl(), profile, chromeShareExtras, isInMultiWindow);
            if (actionProvider.getCustomActions().size() > 0) {
                provider = actionProvider;
            }
        }

        // TODO(https://crbug.com/1421783): Maybe fallback to Chrome's share sheet properly.
        if (provider == null || provider.getCustomActions().size() == 0) {
            Log.i(TAG, "No custom actions provided.");
        }

        // Update the image being shared into ShareParams's url based on the information from
        // chromeShareExtras.
        if (chromeShareExtras.isImage()) {
            String imageUrlToShare = getImageUrlToShare(params, chromeShareExtras);
            params.setUrl(imageUrlToShare);
        }
        if (!isLinkSharing(params, chromeShareExtras)) {
            ShareHelper.shareWithSystemShareSheetUi(
                    params, profile, chromeShareExtras.saveLastUsed(), provider);
            return;
        }

        long iconPrepStartTime = SystemClock.elapsedRealtime();
        ChromeCustomShareAction.Provider finalProvider = provider;
        preparePreviewFavicon(activity, profile, params.getUrl(), (uri) -> {
            RecordHistogram.recordTimesHistogram("Sharing.PreparePreviewFaviconDuration",
                    SystemClock.elapsedRealtime() - iconPrepStartTime);
            params.setPreviewImageUri(uri);
            ShareHelper.shareWithSystemShareSheetUi(
                    params, profile, chromeShareExtras.saveLastUsed(), finalProvider);
        });
    }

    private static void preparePreviewFavicon(
            Context context, Profile profile, String pageUrl, Callback<Uri> onUriReady) {
        int size = context.getResources().getDimensionPixelSize(R.dimen.share_preview_favicon_size);
        FaviconHelper faviconHelper = new FaviconHelper();
        faviconHelper.getLocalFaviconImageForURL(
                profile, new GURL(pageUrl), size, (Bitmap icon, GURL iconUrl) -> {
                    onFaviconRetrieved(context, icon, size, onUriReady);
                });
    }

    private static void onFaviconRetrieved(
            Context context, Bitmap bitmap, int size, Callback<Uri> onImageUriAvailable) {
        // If bitmap is not provided, fallback to the globe placeholder icon.
        if (bitmap == null) {
            bitmap = FaviconUtils.createGenericFaviconBitmap(context, size);
        }
        String fileName = String.valueOf(System.currentTimeMillis());
        ShareImageFileUtils.generateTemporaryUriFromBitmap(fileName, bitmap, onImageUriAvailable);
    }

    private static boolean isLinkSharing(ShareParams params, ChromeShareExtras chromeShareExtras) {
        @ContentType
        Set<Integer> contents = ShareContentTypeHelper.getContentTypes(params, chromeShareExtras);
        return contents.contains(ContentType.LINK_PAGE_VISIBLE)
                || contents.contains(ContentType.LINK_PAGE_NOT_VISIBLE)
                || contents.contains(ContentType.LINK_AND_TEXT);
    }

    private static String getImageUrlToShare(
            ShareParams shareParams, ChromeShareExtras chromeShareExtras) {
        if (!TextUtils.isEmpty(shareParams.getUrl())) {
            return shareParams.getUrl();
        }
        if (!chromeShareExtras.getImageSrcUrl().isEmpty()) {
            return chromeShareExtras.getImageSrcUrl().getSpec();
        }
        return chromeShareExtras.getContentUrl().getSpec();
    }
}
