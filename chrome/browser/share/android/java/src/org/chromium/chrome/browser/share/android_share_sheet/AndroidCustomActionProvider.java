// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.share.android_share_sheet;

import android.app.Activity;
import android.graphics.drawable.Icon;
import android.os.SystemClock;
import android.text.TextUtils;

import androidx.annotation.Nullable;
import androidx.annotation.OptIn;
import androidx.core.os.BuildCompat;

import org.chromium.base.Callback;
import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.share.ChromeCustomShareAction;
import org.chromium.chrome.browser.share.ChromeProvidedSharingOptionsProviderBase;
import org.chromium.chrome.browser.share.ChromeShareExtras;
import org.chromium.chrome.browser.share.ChromeShareExtras.DetailedContentType;
import org.chromium.chrome.browser.share.ShareContentTypeHelper;
import org.chromium.chrome.browser.share.ShareContentTypeHelper.ContentType;
import org.chromium.chrome.browser.share.link_to_text.LinkToTextCoordinator;
import org.chromium.chrome.browser.share.long_screenshots.LongScreenshotsCoordinator;
import org.chromium.chrome.browser.share.share_sheet.ChromeOptionShareCallback;
import org.chromium.chrome.browser.share.share_sheet.ShareSheetLinkToggleCoordinator.LinkToggleState;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.ui.signin.DeviceLockActivityLauncher;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.share.ShareParams;
import org.chromium.components.feature_engagement.EventConstants;
import org.chromium.components.feature_engagement.Tracker;
import org.chromium.ui.base.WindowAndroid;

import java.util.ArrayList;
import java.util.List;

/**
 * Provider that constructs custom actions for Android share sheet.
 */
class AndroidCustomActionProvider extends ChromeProvidedSharingOptionsProviderBase
        implements ChromeCustomShareAction.Provider {
    private static final String USER_ACTION_SHARE_HIGHLIGHT_TEXT_WITH_LINK =
            "SharingHubAndroid.AndroidShareHighlightText.WithLink";
    private static final String USER_ACTION_SHARE_HIGHLIGHT_TEXT_WITHOUT_LINK =
            "SharingHubAndroid.AndroidShareHighlightText.WithoutLink";
    private static final String USER_ACTION_LONG_SCREENSHOT_NO_EDITOR_SELECTED =
            "SharingHubAndroid.LongScreenshotSelected.NoEditor";
    private static final Integer MAX_ACTION_SUPPORTED = 5;

    private final ChromeShareExtras mChromeShareExtras;
    @Nullable
    private final LinkToTextCoordinator mLinkToTextCoordinator;
    private final List<ChromeCustomShareAction> mCustomActions = new ArrayList<>();

    private @Nullable ChromeCustomShareAction mModifyAction;

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
     * @param linkToTextCoordinator Link to text generator used for this share.
     * @param deviceLockActivityLauncher The launcher to start up the device lock page.
     */
    AndroidCustomActionProvider(Activity activity, WindowAndroid windowAndroid,
            Supplier<Tab> tabProvider, BottomSheetController bottomSheetController,
            ShareParams shareParams, Callback<Tab> printTab, boolean isIncognito,
            ChromeOptionShareCallback chromeOptionShareCallback, Tracker featureEngagementTracker,
            String url, Profile profile, ChromeShareExtras chromeShareExtras, boolean isMultiWindow,
            @Nullable LinkToTextCoordinator linkToTextCoordinator,
            DeviceLockActivityLauncher deviceLockActivityLauncher) {
        super(activity, windowAndroid, tabProvider, bottomSheetController, shareParams, printTab,
                isIncognito, chromeOptionShareCallback, featureEngagementTracker, url, profile,
                deviceLockActivityLauncher);
        mChromeShareExtras = chromeShareExtras;
        mLinkToTextCoordinator = linkToTextCoordinator;
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
        assert options.size() <= MAX_ACTION_SUPPORTED;
        for (var option : options) {
            mCustomActions.add(shareActionFromFirstPartyOption(option));
        }

        // getLinkToTextSuccessful is only populated when an link is generated for share.
        if (mShareParams.getLinkToTextSuccessful() != null && mShareParams.getLinkToTextSuccessful()
                && mChromeShareExtras.getDetailedContentType()
                        == ChromeShareExtras.DetailedContentType.HIGHLIGHTED_TEXT) {
            FirstPartyOption option = TextUtils.isEmpty(mShareParams.getUrl())
                    ? createShareHighlightTextWithLink()
                    : createShareHighlightTextWithOutLink();
            mModifyAction = shareActionFromFirstPartyOption(option);
        }
    }

    @Override
    public List<ChromeCustomShareAction> getCustomActions() {
        return mCustomActions;
    }

    @Override
    public ChromeCustomShareAction getModifyShareAction() {
        return mModifyAction;
    }

    //  extends ChromeProvidedSharingOptionsProviderBase:

    @Override
    protected boolean usePolishedActionOrderedList() {
        // Always use the polished list of actions for Android share sheet.
        return true;
    }

    @Nullable
    @Override
    protected FirstPartyOption createScreenshotFirstPartyOption() {
        return null;
    }

    @Nullable
    @Override
    protected FirstPartyOption createLongScreenshotsFirstPartyOption() {
        return new FirstPartyOptionBuilder(ContentType.LINK_PAGE_VISIBLE)
                .setDetailedContentTypesToDisableFor(DetailedContentType.WEB_NOTES)
                .setIcon(R.drawable.long_screenshot, R.string.sharing_long_screenshot)
                .setFeatureNameForMetrics(USER_ACTION_LONG_SCREENSHOT_NO_EDITOR_SELECTED)
                .setDisableForMultiWindow(true)
                .setOnClickCallback((view) -> {
                    mFeatureEngagementTracker.notifyEvent(EventConstants.SHARE_SCREENSHOT_SELECTED);
                    LongScreenshotsCoordinator coordinator =
                            LongScreenshotsCoordinator.create(mActivity, mTabProvider.get(), mUrl,
                                    mChromeOptionShareCallback, mBottomSheetController, null);
                    coordinator.captureScreenshot();
                })
                .build();
    }

    @Override
    protected void maybeAddWebStyleNotesFirstPartyOption() {}

    @Override
    protected void maybeAddDownloadImageFirstPartyOption() {}

    private FirstPartyOption createShareHighlightTextWithLink() {
        return new FirstPartyOptionBuilder(ContentType.HIGHLIGHTED_TEXT)
                .setIcon(R.drawable.link, R.string.sharing_include_link)
                .setFeatureNameForMetrics(USER_ACTION_SHARE_HIGHLIGHT_TEXT_WITH_LINK)
                .setOnClickCallback((view) -> {
                    assert mLinkToTextCoordinator != null;
                    mChromeOptionShareCallback.showShareSheet(
                            mLinkToTextCoordinator.getShareParams(LinkToggleState.LINK),
                            mChromeShareExtras, SystemClock.elapsedRealtime());
                })
                .build();
    }

    private FirstPartyOption createShareHighlightTextWithOutLink() {
        return new FirstPartyOptionBuilder(ContentType.HIGHLIGHTED_TEXT)
                .setIcon(R.drawable.link_off, R.string.sharing_exclude_link)
                .setFeatureNameForMetrics(USER_ACTION_SHARE_HIGHLIGHT_TEXT_WITHOUT_LINK)
                .setOnClickCallback((view) -> {
                    assert mLinkToTextCoordinator != null;
                    mChromeOptionShareCallback.showShareSheet(
                            mLinkToTextCoordinator.getShareParams(LinkToggleState.NO_LINK),
                            mChromeShareExtras, SystemClock.elapsedRealtime());
                })
                .build();
    }

    private ChromeCustomShareAction shareActionFromFirstPartyOption(FirstPartyOption option) {
        return new ChromeCustomShareAction(option.featureNameForMetrics,
                Icon.createWithResource(mActivity, option.icon),
                mActivity.getResources().getString(option.iconLabel),
                option.onClickCallback.bind(null));
    }
}
