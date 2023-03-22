// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.share.android_share_sheet;

import android.app.Activity;
import android.graphics.drawable.Icon;

import androidx.annotation.Nullable;
import androidx.annotation.OptIn;
import androidx.core.os.BuildCompat;

import org.chromium.base.Callback;
import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.share.ChromeCustomShareAction;
import org.chromium.chrome.browser.share.ChromeProvidedSharingOptionsProviderBase;
import org.chromium.chrome.browser.share.ChromeShareExtras;
import org.chromium.chrome.browser.share.ShareContentTypeHelper;
import org.chromium.chrome.browser.share.share_sheet.ChromeOptionShareCallback;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.share.ShareParams;
import org.chromium.components.feature_engagement.Tracker;
import org.chromium.ui.base.WindowAndroid;

import java.util.ArrayList;
import java.util.List;

/**
 * Provider that constructs custom actions for Android share sheet.
 */
class AndroidCustomActionProvider extends ChromeProvidedSharingOptionsProviderBase
        implements ChromeCustomShareAction.Provider {
    private final List<ChromeCustomShareAction> mCustomActions = new ArrayList<>();

    /**
     * Constructs a new {@link AndroidCustomActionProvider}.
     *
     * @param activity The current {@link Activity}.
     * @param windowAndroid The current window.
     * @param tabProvider Supplier for the current activity tab.
     * @param bottomSheetController The {@link BottomSheetController} for the current activity.
     * @param shareParams The {@link ShareParams} for the current share.
     * @param printTab A {@link Callback} that will print a given Tab.
     * @param isIncognito Whether incognito mode is enabled.
     * @param chromeOptionShareCallback A ChromeOptionShareCallback that can be used by
     * Chrome-provided sharing options.
     * @param featureEngagementTracker feature engagement tracker.
     * @param url Url to share.
     * @param profile The current profile of the User.
     * @param chromeShareExtras The {@link ChromeShareExtras} for the current share, if exists.
     * @param isMultiWindow Whether the current activity is in multi-window mode.
     */
    AndroidCustomActionProvider(Activity activity, WindowAndroid windowAndroid,
            Supplier<Tab> tabProvider, BottomSheetController bottomSheetController,
            ShareParams shareParams, Callback<Tab> printTab, boolean isIncognito,
            ChromeOptionShareCallback chromeOptionShareCallback, Tracker featureEngagementTracker,
            String url, Profile profile, ChromeShareExtras chromeShareExtras,
            boolean isMultiWindow) {
        super(activity, windowAndroid, tabProvider, bottomSheetController, shareParams, printTab,
                isIncognito, chromeOptionShareCallback, featureEngagementTracker, url, profile);

        initCustomActions(shareParams, chromeShareExtras, isMultiWindow);
    }

    /**
     * Create the list of Parcelable used as custom actions for Android share sheet.
     *
     * @param params The {@link ShareParams} for the current share.
     * @param chromeShareExtras The {@link ChromeShareExtras} for the current share, if exists.
     * @param isMultiWindow Whether the current activity is in multi-window mode.
     * @return List of custom action used for Android share sheet.
     */
    @OptIn(markerClass = androidx.core.os.BuildCompat.PrereleaseSdkCheck.class)
    private void initCustomActions(
            ShareParams params, ChromeShareExtras chromeShareExtras, boolean isMultiWindow) {
        if (!BuildCompat.isAtLeastU()) {
            return;
        }

        List<FirstPartyOption> options = getFirstPartyOptions(
                ShareContentTypeHelper.getContentTypes(params, chromeShareExtras),
                chromeShareExtras.getDetailedContentType(), isMultiWindow);

        for (var option : options) {
            mCustomActions.add(new ChromeCustomShareAction(option.featureNameForMetrics,
                    Icon.createWithResource(mActivity, option.icon),
                    mActivity.getResources().getString(option.iconLabel),
                    option.onClickCallback.bind(null)));
        }
    }

    @Override
    public List<ChromeCustomShareAction> getCustomActions() {
        return mCustomActions;
    }

    //  extends ChromeProvidedSharingOptionsProviderBase:

    @Nullable
    @Override
    protected FirstPartyOption createScreenshotFirstPartyOption() {
        return null;
    }

    // TODO(https://crbug/1410201): Support long screenshot.
    @Nullable
    @Override
    protected FirstPartyOption createLongScreenshotsFirstPartyOption() {
        return null;
    }
}
