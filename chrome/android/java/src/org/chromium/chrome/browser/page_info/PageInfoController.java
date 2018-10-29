// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.page_info;

import android.app.Activity;
import android.content.ActivityNotFoundException;
import android.content.ClipData;
import android.content.ClipboardManager;
import android.content.Context;
import android.content.Intent;
import android.graphics.Color;
import android.net.Uri;
import android.os.Bundle;
import android.provider.Settings;
import android.support.annotation.IntDef;
import android.text.Spannable;
import android.text.SpannableString;
import android.text.SpannableStringBuilder;
import android.text.TextUtils;
import android.text.style.ForegroundColorSpan;
import android.text.style.TextAppearanceSpan;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.base.StrictModeContext;
import org.chromium.base.VisibleForTesting;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.UrlConstants;
import org.chromium.chrome.browser.feature_engagement.TrackerFactory;
import org.chromium.chrome.browser.instantapps.InstantAppsHandler;
import org.chromium.chrome.browser.modaldialog.DialogDismissalCause;
import org.chromium.chrome.browser.modaldialog.ModalDialogView;
import org.chromium.chrome.browser.modaldialog.ModalDialogView.ButtonType;
import org.chromium.chrome.browser.offlinepages.OfflinePageItem;
import org.chromium.chrome.browser.offlinepages.OfflinePageUtils;
import org.chromium.chrome.browser.omnibox.OmniboxUrlEmphasizer;
import org.chromium.chrome.browser.page_info.PageInfoView.ConnectionInfoParams;
import org.chromium.chrome.browser.page_info.PageInfoView.PageInfoViewParams;
import org.chromium.chrome.browser.preferences.Preferences;
import org.chromium.chrome.browser.preferences.PreferencesLauncher;
import org.chromium.chrome.browser.preferences.website.ContentSetting;
import org.chromium.chrome.browser.preferences.website.SingleWebsitePreferences;
import org.chromium.chrome.browser.previews.PreviewsAndroidBridge;
import org.chromium.chrome.browser.previews.PreviewsUma;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.ssl.SecurityStateModel;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.util.UrlUtilities;
import org.chromium.chrome.browser.vr.VrModuleProvider;
import org.chromium.components.feature_engagement.EventConstants;
import org.chromium.components.feature_engagement.Tracker;
import org.chromium.components.security_state.ConnectionSecurityLevel;
import org.chromium.components.url_formatter.UrlFormatter;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.WebContentsObserver;
import org.chromium.net.GURLUtils;
import org.chromium.ui.base.DeviceFormFactor;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.text.NoUnderlineClickableSpan;
import org.chromium.ui.text.SpanApplier;
import org.chromium.ui.text.SpanApplier.SpanInfo;
import org.chromium.ui.widget.Toast;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.net.URI;
import java.net.URISyntaxException;
import java.text.DateFormat;
import java.util.Date;

/**
 * Java side of Android implementation of the page info UI.
 */
public class PageInfoController
        implements ModalDialogView.Controller, SystemSettingsActivityRequiredListener {
    @IntDef({OpenedFromSource.MENU, OpenedFromSource.TOOLBAR, OpenedFromSource.VR})
    @Retention(RetentionPolicy.SOURCE)
    public @interface OpenedFromSource {
        int MENU = 1;
        int TOOLBAR = 2;
        int VR = 3;
    }

    @IntDef({OfflinePageState.NOT_OFFLINE_PAGE, OfflinePageState.TRUSTED_OFFLINE_PAGE,
            OfflinePageState.UNTRUSTED_OFFLINE_PAGE})
    @Retention(RetentionPolicy.SOURCE)
    public @interface OfflinePageState {
        int NOT_OFFLINE_PAGE = 1;
        int TRUSTED_OFFLINE_PAGE = 2;
        int UNTRUSTED_OFFLINE_PAGE = 3;
    }

    @IntDef({PreviewPageState.NOT_PREVIEW, PreviewPageState.SECURE_PAGE_PREVIEW,
            PreviewPageState.INSECURE_PAGE_PREVIEW})
    @Retention(RetentionPolicy.SOURCE)
    public @interface PreviewPageState {
        int NOT_PREVIEW = 1;
        int SECURE_PAGE_PREVIEW = 2;
        int INSECURE_PAGE_PREVIEW = 3;
    }

    private final Context mContext;
    private final WindowAndroid mWindowAndroid;
    private final Tab mTab;
    private final PermissionParamsListBuilder mPermissionParamsListBuilder;

    // A pointer to the C++ object for this UI.
    private long mNativePageInfoController;

    // The view inside the popup.
    private PageInfoView mView;

    // The dialog the view is placed in.
    private final PageInfoDialog mDialog;

    // The full URL from the URL bar, which is copied to the user's clipboard when they select 'Copy
    // URL'.
    private String mFullUrl;

    // The scheme of the URL of this page.
    private String mScheme;

    // Whether or not this page is an internal chrome page (e.g. the
    // chrome://settings page).
    private boolean mIsInternalPage;

    // The security level of the page (a valid ConnectionSecurityLevel).
    private int mSecurityLevel;

    // Creation date of an offline copy, if web contents contains an offline page.
    private String mOfflinePageCreationDate;

    // The state of the preview of the page (not preview, preview on a [in]secure page).
    private @PreviewPageState int mPreviewPageState;

    // The state of offline page in the web contents (not offline page, trusted/untrusted offline
    // page).
    private @OfflinePageState int mOfflinePageState;

    // The name of the content publisher, if any.
    private String mContentPublisher;

    // Observer for dismissing dialog if web contents get destroyed, navigate etc.
    private WebContentsObserver mWebContentsObserver;

    // A task that should be run once the page info popup is animated out and dismissed. Null if no
    // task is pending.
    private Runnable mPendingRunAfterDismissTask;

    /**
     * Creates the PageInfoController, but does not display it. Also initializes the corresponding
     * C++ object and saves a pointer to it.
     * @param activity                 Activity which is used for showing a popup.
     * @param tab                      Tab for which the pop up is shown.
     * @param securityLevel            The security level of the page being shown.
     * @param offlinePageUrl           URL that the offline page claims to be generated from.
     * @param offlinePageCreationDate  Date when the offline page was created.
     * @param offlinePageState         State of the tab showing offline page.
     * @param previewPageState         State of the tab showing the preview.
     * @param publisher                The name of the content publisher, if any.
     */
    protected PageInfoController(Activity activity, Tab tab, int securityLevel,
            String offlinePageUrl, String offlinePageCreationDate,
            @OfflinePageState int offlinePageState, @PreviewPageState int previewPageState,
            String publisher) {
        mContext = activity;
        mTab = tab;
        mSecurityLevel = securityLevel;
        mOfflinePageState = offlinePageState;
        mPreviewPageState = previewPageState;
        PageInfoViewParams viewParams = new PageInfoViewParams();

        if (mOfflinePageState != OfflinePageState.NOT_OFFLINE_PAGE) {
            mOfflinePageCreationDate = offlinePageCreationDate;
        }
        mWindowAndroid = mTab.getWebContents().getTopLevelNativeWindow();
        mContentPublisher = publisher;

        viewParams.urlTitleClickCallback = () -> {
            // Expand/collapse the displayed URL title.
            mView.toggleUrlTruncation();
        };
        // Long press the url text to copy it to the clipboard.
        viewParams.urlTitleLongClickCallback = () -> {
            ClipboardManager clipboard =
                    (ClipboardManager) mContext.getSystemService(Context.CLIPBOARD_SERVICE);
            ClipData clip = ClipData.newPlainText("url", mFullUrl);
            clipboard.setPrimaryClip(clip);
            Toast.makeText(mContext, R.string.url_copied, Toast.LENGTH_SHORT).show();
        };

        // Work out the URL and connection message and status visibility.
        mFullUrl = isShowingOfflinePage() ? offlinePageUrl : mTab.getOriginalUrl();

        // This can happen if an invalid chrome-distiller:// url was entered.
        if (mFullUrl == null) mFullUrl = "";

        mScheme = GURLUtils.getScheme(mFullUrl);
        try {
            mIsInternalPage = UrlUtilities.isInternalScheme(new URI(mFullUrl));
        } catch (URISyntaxException e) {
            // Ignore exception since this is for displaying some specific content on page info.
        }

        String displayUrl = UrlFormatter.formatUrlForCopy(mFullUrl);
        if (isShowingOfflinePage()) {
            displayUrl = UrlUtilities.stripScheme(mFullUrl);
        }
        SpannableStringBuilder displayUrlBuilder = new SpannableStringBuilder(displayUrl);
        OmniboxUrlEmphasizer.emphasizeUrl(displayUrlBuilder, mContext.getResources(),
                mTab.getProfile(), mSecurityLevel, mIsInternalPage, true, true);
        if (mSecurityLevel == ConnectionSecurityLevel.SECURE) {
            OmniboxUrlEmphasizer.EmphasizeComponentsResponse emphasizeResponse =
                    OmniboxUrlEmphasizer.parseForEmphasizeComponents(
                            mTab.getProfile(), displayUrlBuilder.toString());
            if (emphasizeResponse.schemeLength > 0) {
                displayUrlBuilder.setSpan(
                        new TextAppearanceSpan(mContext, R.style.RobotoMediumStyle), 0,
                        emphasizeResponse.schemeLength, Spannable.SPAN_EXCLUSIVE_INCLUSIVE);
            }
        }
        viewParams.url = displayUrlBuilder;
        viewParams.urlOriginLength = OmniboxUrlEmphasizer.getOriginEndIndex(
                displayUrlBuilder.toString(), mTab.getProfile());

        if (shouldShowSiteSettingsButton()) {
            viewParams.siteSettingsButtonClickCallback = () -> {
                // Delay while the dialog closes.
                runAfterDismiss(() -> {
                    recordAction(PageInfoAction.PAGE_INFO_SITE_SETTINGS_OPENED);
                    Bundle fragmentArguments =
                            SingleWebsitePreferences.createFragmentArgsForSite(mFullUrl);
                    Intent preferencesIntent = PreferencesLauncher.createIntentForSettingsPage(
                            mContext, SingleWebsitePreferences.class.getName());
                    preferencesIntent.putExtra(
                            Preferences.EXTRA_SHOW_FRAGMENT_ARGUMENTS, fragmentArguments);
                    // Disabling StrictMode to avoid violations (https://crbug.com/819410).
                    try (StrictModeContext unused = StrictModeContext.allowDiskReads()) {
                        mContext.startActivity(preferencesIntent);
                    }
                });
            };
        } else {
            viewParams.siteSettingsButtonShown = false;
        }

        initPreviewUiParams(viewParams);

        if (isShowingOfflinePage()) {
            boolean isConnected = OfflinePageUtils.isConnected();
            RecordHistogram.recordBooleanHistogram(
                    "OfflinePages.WebsiteSettings.OpenOnlineButtonVisible", isConnected);
            if (isConnected) {
                viewParams.openOnlineButtonClickCallback = () -> {
                    runAfterDismiss(() -> {
                        // Attempt to reload to an online version of the viewed offline web page.
                        // This attempt might fail if the user is offline, in which case an offline
                        // copy will be reloaded.
                        RecordHistogram.recordBooleanHistogram(
                                "OfflinePages.WebsiteSettings.ConnectedWhenOpenOnlineButtonClicked",
                                OfflinePageUtils.isConnected());
                        OfflinePageUtils.reload(mTab);
                    });
                };
            } else {
                viewParams.openOnlineButtonShown = false;
            }
        } else {
            viewParams.openOnlineButtonShown = false;
        }

        InstantAppsHandler instantAppsHandler = InstantAppsHandler.getInstance();
        if (!mIsInternalPage && !isShowingOfflinePage() && !isShowingPreview()
                && instantAppsHandler.isInstantAppAvailable(mFullUrl, false /* checkHoldback */,
                           false /* includeUserPrefersBrowser */)) {
            final Intent instantAppIntent = instantAppsHandler.getInstantAppIntentForUrl(mFullUrl);
            viewParams.instantAppButtonClickCallback = () -> {
                try {
                    mContext.startActivity(instantAppIntent);
                    RecordUserAction.record("Android.InstantApps.LaunchedFromWebsiteSettingsPopup");
                } catch (ActivityNotFoundException e) {
                    mView.disableInstantAppButton();
                }
            };
            RecordUserAction.record("Android.InstantApps.OpenInstantAppButtonShown");
        } else {
            viewParams.instantAppButtonShown = false;
        }

        mView = new PageInfoView(mContext, viewParams);
        if (isSheet()) mView.setBackgroundColor(Color.WHITE);
        mPermissionParamsListBuilder = new PermissionParamsListBuilder(
                mContext, mWindowAndroid, mFullUrl, this, mView::setPermissions);

        // This needs to come after other member initialization.
        mNativePageInfoController = nativeInit(this, mTab.getWebContents());
        mWebContentsObserver = new WebContentsObserver(mTab.getWebContents()) {
            @Override
            public void navigationEntryCommitted() {
                // If a navigation is committed (e.g. from in-page redirect), the data we're showing
                // is stale so dismiss the dialog.
                mDialog.dismiss(true);
            }

            @Override
            public void wasHidden() {
                // The web contents were hidden (potentially by loading another URL via an intent),
                // so dismiss the dialog).
                mDialog.dismiss(true);
            }

            @Override
            public void destroy() {
                super.destroy();
                // Force the dialog to close immediately in case the destroy was from Chrome
                // quitting.
                mDialog.dismiss(false);
            }
        };

        mDialog = new PageInfoDialog(mContext, mView, mTab.getView(), isSheet(),
                mTab.getActivity().getModalDialogManager(), this);
        mDialog.show();
    }

    /**
     * Initializes the state in viewParams with respect to showing the previews UI.
     *
     * @param viewParams The PageInfoViewParams to set state on.
     */
    private void initPreviewUiParams(PageInfoViewParams viewParams) {
        final PreviewsAndroidBridge bridge = PreviewsAndroidBridge.getInstance();
        viewParams.separatorShown = mPreviewPageState == PreviewPageState.INSECURE_PAGE_PREVIEW;
        viewParams.previewUIShown = isShowingPreview();
        if (isShowingPreview()) {
            viewParams.urlTitleShown = false;
            viewParams.connectionMessageShown = false;

            viewParams.previewShowOriginalClickCallback = () -> {
                runAfterDismiss(() -> {
                    PreviewsUma.recordOptOut(bridge.getPreviewsType(mTab.getWebContents()));
                    bridge.loadOriginal(mTab.getWebContents());
                });
            };
            final String previewOriginalHost = bridge.getOriginalHost(mTab.getWebContents());
            final String loadOriginalText = mContext.getString(
                    R.string.page_info_preview_load_original, previewOriginalHost);
            final SpannableString loadOriginalSpan = SpanApplier.applySpans(loadOriginalText,
                    new SpanInfo("<link>", "</link>",
                            // The callback given to NoUnderlineClickableSpan is overridden in
                            // PageInfoView so use previewShowOriginalClickCallback (above) instead
                            // because the entire TextView will be clickable.
                            new NoUnderlineClickableSpan((view) -> {})));
            viewParams.previewLoadOriginalMessage = loadOriginalSpan;

            viewParams.previewStaleTimestamp =
                    bridge.getStalePreviewTimestamp(mTab.getWebContents());
        }
    }

    /**
     * Whether to show a 'Details' link to the connection info popup.
     */
    private boolean isConnectionDetailsLinkVisible() {
        return mContentPublisher == null && !isShowingOfflinePage() && !isShowingPreview();
    }

    /**
     * Adds a new row for the given permission.
     *
     * @param name The title of the permission to display to the user.
     * @param type The ContentSettingsType of the permission.
     * @param currentSettingValue The ContentSetting value of the currently selected setting.
     */
    @CalledByNative
    private void addPermissionSection(String name, int type, int currentSettingValue) {
        mPermissionParamsListBuilder.addPermissionEntry(
                name, type, ContentSetting.fromInt(currentSettingValue));
    }

    /**
     * Update the permissions view based on the contents of mDisplayedPermissions.
     */
    @CalledByNative
    private void updatePermissionDisplay() {
        mView.setPermissions(mPermissionParamsListBuilder.build());
    }

    /**
     * Sets the connection security summary and detailed description strings. These strings may be
     * overridden based on the state of the Android UI.
     */
    @CalledByNative
    private void setSecurityDescription(String summary, String details) {
        ConnectionInfoParams connectionInfoParams = new ConnectionInfoParams();

        // Display the appropriate connection message.
        SpannableStringBuilder messageBuilder = new SpannableStringBuilder();
        if (mContentPublisher != null) {
            messageBuilder.append(
                    mContext.getString(R.string.page_info_domain_hidden, mContentPublisher));
        } else if (isShowingPreview()) {
            if (mPreviewPageState == PreviewPageState.INSECURE_PAGE_PREVIEW) {
                connectionInfoParams.summary = summary;
            }
        } else if (mOfflinePageState == OfflinePageState.TRUSTED_OFFLINE_PAGE) {
            messageBuilder.append(
                    String.format(mContext.getString(R.string.page_info_connection_offline),
                            mOfflinePageCreationDate));
        } else if (mOfflinePageState == OfflinePageState.UNTRUSTED_OFFLINE_PAGE) {
            // For untrusted pages, if there's a creation date, show it in the message.
            if (TextUtils.isEmpty(mOfflinePageCreationDate)) {
                messageBuilder.append(mContext.getString(
                        R.string.page_info_offline_page_not_trusted_without_date));
            } else {
                messageBuilder.append(String.format(
                        mContext.getString(R.string.page_info_offline_page_not_trusted_with_date),
                        mOfflinePageCreationDate));
            }
        } else {
            if (!TextUtils.equals(summary, details)) {
                connectionInfoParams.summary = summary;
            }
            messageBuilder.append(details);
        }

        if (isConnectionDetailsLinkVisible()) {
            messageBuilder.append(" ");
            SpannableString detailsText =
                    new SpannableString(mContext.getString(R.string.details_link));
            final ForegroundColorSpan blueSpan =
                    new ForegroundColorSpan(ApiCompatibilityUtils.getColor(
                            mContext.getResources(), R.color.default_text_color_link));
            detailsText.setSpan(
                    blueSpan, 0, detailsText.length(), Spannable.SPAN_INCLUSIVE_EXCLUSIVE);
            messageBuilder.append(detailsText);
        }

        // When a preview is being shown for a secure page, the security message is not shown. Thus,
        // messageBuilder maybe empty.
        if (messageBuilder.length() > 0) {
            connectionInfoParams.message = messageBuilder;
        }
        if (isConnectionDetailsLinkVisible()) {
            connectionInfoParams.clickCallback = () -> {
                runAfterDismiss(() -> {
                    if (!mTab.getWebContents().isDestroyed()) {
                        recordAction(PageInfoAction.PAGE_INFO_SECURITY_DETAILS_OPENED);
                        ConnectionInfoPopup.show(mContext, mTab);
                    }
                });
            };
        }
        mView.setConnectionInfo(connectionInfoParams);
    }

    @Override
    public void onSystemSettingsActivityRequired(Intent intentOverride) {
        runAfterDismiss(() -> {
            Intent settingsIntent;
            if (intentOverride != null) {
                settingsIntent = intentOverride;
            } else {
                settingsIntent = new Intent(Settings.ACTION_APPLICATION_DETAILS_SETTINGS);
                settingsIntent.setData(Uri.parse("package:" + mContext.getPackageName()));
            }
            settingsIntent.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
            mContext.startActivity(settingsIntent);
        });
    }

    /**
     * Dismiss the popup, and then run a task after the animation has completed (if there is one).
     */
    private void runAfterDismiss(Runnable task) {
        mPendingRunAfterDismissTask = task;
        mDialog.dismiss(true);
    }

    @Override
    public void onClick(@ButtonType int buttonType) {}

    @Override
    public void onDismiss(@DialogDismissalCause int dismissalCause) {
        assert mNativePageInfoController != 0;
        if (mPendingRunAfterDismissTask != null) {
            mPendingRunAfterDismissTask.run();
            mPendingRunAfterDismissTask = null;
        }
        mWebContentsObserver.destroy();
        nativeDestroy(mNativePageInfoController);
        mNativePageInfoController = 0;
    }

    private void recordAction(int action) {
        if (mNativePageInfoController != 0) {
            nativeRecordPageInfoAction(mNativePageInfoController, action);
        }
    }

    /**
     * Whether website dialog is displayed for a preview.
     */
    private boolean isShowingPreview() {
        return mPreviewPageState != PreviewPageState.NOT_PREVIEW;
    }

    /**
     * Whether website dialog is displayed for an offline page.
     */
    private boolean isShowingOfflinePage() {
        return mOfflinePageState != OfflinePageState.NOT_OFFLINE_PAGE && !isShowingPreview();
    }

    /**
     * Whether the site settings button should be displayed for the given URL.
     */
    private boolean shouldShowSiteSettingsButton() {
        return !isShowingOfflinePage() && !isShowingPreview()
                && (UrlConstants.HTTP_SCHEME.equals(mScheme)
                           || UrlConstants.HTTPS_SCHEME.equals(mScheme));
    }

    private boolean isSheet() {
        return !DeviceFormFactor.isNonMultiDisplayContextOnTablet(mContext)
                && !VrModuleProvider.getDelegate().isInVr();
    }

    @VisibleForTesting
    public PageInfoView getPageInfoViewForTesting() {
        return mView;
    }

    /**
     * Shows a PageInfo dialog for the provided Tab. The popup adds itself to the view
     * hierarchy which owns the reference while it's visible.
     *
     * @param activity Activity which is used for launching a dialog.
     * @param tab The tab hosting the web contents for which to show Website information. This
     *            information is retrieved for the visible entry.
     * @param contentPublisher The name of the publisher of the content.
     * @param source Determines the source that triggered the popup.
     */
    public static void show(final Activity activity, final Tab tab, final String contentPublisher,
            @OpenedFromSource int source) {
        if (source == OpenedFromSource.MENU) {
            RecordUserAction.record("MobileWebsiteSettingsOpenedFromMenu");
        } else if (source == OpenedFromSource.TOOLBAR) {
            RecordUserAction.record("MobileWebsiteSettingsOpenedFromToolbar");
        } else if (source == OpenedFromSource.VR) {
            RecordUserAction.record("MobileWebsiteSettingsOpenedFromVR");
        } else {
            assert false : "Invalid source passed";
        }

        final int securityLevel =
                SecurityStateModel.getSecurityLevelForWebContents(tab.getWebContents());

        @PreviewPageState
        int previewPageState = PreviewPageState.NOT_PREVIEW;
        final PreviewsAndroidBridge bridge = PreviewsAndroidBridge.getInstance();
        if (bridge.shouldShowPreviewUI(tab.getWebContents())) {
            previewPageState = securityLevel == ConnectionSecurityLevel.SECURE
                    ? PreviewPageState.SECURE_PAGE_PREVIEW
                    : PreviewPageState.INSECURE_PAGE_PREVIEW;

            PreviewsUma.recordPageInfoOpened(bridge.getPreviewsType(tab.getWebContents()));
            Tracker tracker = TrackerFactory.getTrackerForProfile(Profile.getLastUsedProfile());
            tracker.notifyEvent(EventConstants.PREVIEWS_VERBOSE_STATUS_OPENED);
        }

        String offlinePageUrl = null;
        String offlinePageCreationDate = null;
        @OfflinePageState
        int offlinePageState = OfflinePageState.NOT_OFFLINE_PAGE;

        OfflinePageItem offlinePage = OfflinePageUtils.getOfflinePage(tab);
        if (offlinePage != null) {
            offlinePageUrl = offlinePage.getUrl();
            if (OfflinePageUtils.isShowingTrustedOfflinePage(tab)) {
                offlinePageState = OfflinePageState.TRUSTED_OFFLINE_PAGE;
            } else {
                offlinePageState = OfflinePageState.UNTRUSTED_OFFLINE_PAGE;
            }
            // Get formatted creation date of the offline page. If the page was shared (so the
            // creation date cannot be acquired), make date an empty string and there will be
            // specific processing for showing different string in UI.
            long pageCreationTimeMs = offlinePage.getCreationTimeMs();
            if (pageCreationTimeMs != 0) {
                Date creationDate = new Date(offlinePage.getCreationTimeMs());
                DateFormat df = DateFormat.getDateInstance(DateFormat.MEDIUM);
                offlinePageCreationDate = df.format(creationDate);
            }
        }

        new PageInfoController(activity, tab, securityLevel, offlinePageUrl,
                offlinePageCreationDate, offlinePageState, previewPageState, contentPublisher);
    }

    private static native long nativeInit(PageInfoController controller, WebContents webContents);

    private native void nativeDestroy(long nativePageInfoControllerAndroid);

    private native void nativeRecordPageInfoAction(
            long nativePageInfoControllerAndroid, int action);
}
