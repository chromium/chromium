// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.page_info;

import static org.chromium.components.browser_ui.site_settings.AllSiteSettings.EXTRA_SEARCH;

import android.app.Activity;
import android.content.Context;
import android.content.res.Resources;
import android.graphics.drawable.BitmapDrawable;
import android.graphics.drawable.Drawable;
import android.os.Bundle;
import android.text.TextUtils;
import android.view.ViewGroup;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.fragment.app.FragmentActivity;
import androidx.fragment.app.FragmentManager;

import org.chromium.base.Callback;
import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ephemeraltab.EphemeralTabCoordinator;
import org.chromium.chrome.browser.feature_engagement.TrackerFactory;
import org.chromium.chrome.browser.feedback.HelpAndFeedbackLauncherImpl;
import org.chromium.chrome.browser.merchant_viewer.PageInfoStoreInfoController;
import org.chromium.chrome.browser.merchant_viewer.PageInfoStoreInfoController.StoreInfoActionHandler;
import org.chromium.chrome.browser.offlinepages.OfflinePageItem;
import org.chromium.chrome.browser.offlinepages.OfflinePageUtils;
import org.chromium.chrome.browser.offlinepages.OfflinePageUtils.OfflinePageLoadUrlDelegate;
import org.chromium.chrome.browser.omnibox.ChromeAutocompleteSchemeClassifier;
import org.chromium.chrome.browser.paint_preview.TabbedPaintPreview;
import org.chromium.chrome.browser.pdf.PdfUtils;
import org.chromium.chrome.browser.pdf.PdfUtils.PdfPageType;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.privacy_sandbox.PrivacySandboxReferrer;
import org.chromium.chrome.browser.privacy_sandbox.PrivacySandboxSettingsBaseFragment;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.settings.SettingsNavigationFactory;
import org.chromium.chrome.browser.site_settings.ChromeSiteSettingsDelegate;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabUtils;
import org.chromium.chrome.browser.tabmodel.TabCreator;
import org.chromium.chrome.browser.ui.favicon.FaviconHelper;
import org.chromium.chrome.browser.util.ChromeAccessibilityUtil;
import org.chromium.components.browser_ui.settings.SettingsNavigation;
import org.chromium.components.browser_ui.site_settings.AllSiteSettings;
import org.chromium.components.browser_ui.site_settings.SiteSettingsCategory;
import org.chromium.components.browser_ui.site_settings.SiteSettingsDelegate;
import org.chromium.components.browser_ui.widget.TintedDrawable;
import org.chromium.components.content_settings.CookieControlsBridge;
import org.chromium.components.content_settings.CookieControlsObserver;
import org.chromium.components.embedder_support.util.UrlUtilities;
import org.chromium.components.feature_engagement.EventConstants;
import org.chromium.components.page_info.PageInfoAdPersonalizationController;
import org.chromium.components.page_info.PageInfoControllerDelegate;
import org.chromium.components.page_info.PageInfoMainController;
import org.chromium.components.page_info.PageInfoRowView;
import org.chromium.components.page_info.PageInfoSubpageController;
import org.chromium.components.page_info.PageInfoView;
import org.chromium.components.privacy_sandbox.TrackingProtectionSettings;
import org.chromium.components.user_prefs.UserPrefs;
import org.chromium.content_public.browser.BrowserContextHandle;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.url.GURL;

import java.text.DateFormat;
import java.util.ArrayList;
import java.util.Collection;
import java.util.Date;
import java.util.function.Consumer;

/**
 * Chrome's customization of PageInfoControllerDelegate. This class provides Chrome-specific info to
 * PageInfoController. It also contains logic for Chrome-specific features, like {@link
 * TabbedPaintPreview}
 */
public class ChromePageInfoControllerDelegate extends PageInfoControllerDelegate {
    private final WebContents mWebContents;
    private final Supplier<ModalDialogManager> mModalDialogManagerSupplier;
    private final Supplier<EphemeralTabCoordinator> mEphemeralTabCoordinatorSupplier;
    private final Context mContext;
    private final Profile mProfile;
    private final Supplier<StoreInfoActionHandler> mStoreInfoActionHandlerSupplier;
    private final ChromePageInfoHighlight mPageInfoHighlight;
    private final OfflinePageLoadUrlDelegate mOfflinePageLoadUrlDelegate;
    private String mOfflinePageCreationDate;
    private final TabCreator mTabCreator;

    static final String FEEDBACK_REPORT_TYPE =
            "com.google.chrome.browser.page_info.USER_INITIATED_FEEDBACK_REPORT";

    public ChromePageInfoControllerDelegate(
            Context context,
            WebContents webContents,
            Supplier<ModalDialogManager> modalDialogManagerSupplier,
            OfflinePageLoadUrlDelegate offlinePageLoadUrlDelegate,
            @Nullable Supplier<StoreInfoActionHandler> storeInfoActionHandlerSupplier,
            Supplier<EphemeralTabCoordinator> ephemeralTabCoordinatorSupplier,
            ChromePageInfoHighlight pageInfoHighlight,
            TabCreator tabCreator) {
        super(
                new ChromeAutocompleteSchemeClassifier(Profile.fromWebContents(webContents)),
                /* isSiteSettingsAvailable= */ SiteSettingsHelper.isSiteSettingsAvailable(
                        webContents),
                /* cookieControlsShown= */ CookieControlsBridge.isCookieControlsEnabled(
                        Profile.fromWebContents(webContents)));
        mContext = context;
        mWebContents = webContents;
        mModalDialogManagerSupplier = modalDialogManagerSupplier;
        mEphemeralTabCoordinatorSupplier = ephemeralTabCoordinatorSupplier;
        mProfile = Profile.fromWebContents(mWebContents);
        mStoreInfoActionHandlerSupplier = storeInfoActionHandlerSupplier;
        mPageInfoHighlight = pageInfoHighlight;
        mTabCreator = tabCreator;

        initOfflinePageParams();
        mOfflinePageLoadUrlDelegate = offlinePageLoadUrlDelegate;

        TrackerFactory.getTrackerForProfile(mProfile).notifyEvent(EventConstants.PAGE_INFO_OPENED);
    }

    private void initOfflinePageParams() {
        mOfflinePageState = OfflinePageState.NOT_OFFLINE_PAGE;
        OfflinePageItem offlinePage = OfflinePageUtils.getOfflinePage(mWebContents);
        if (offlinePage != null) {
            mOfflinePageUrl = offlinePage.getUrl();
            if (OfflinePageUtils.isShowingTrustedOfflinePage(mWebContents)) {
                mOfflinePageState = OfflinePageState.TRUSTED_OFFLINE_PAGE;
            } else {
                mOfflinePageState = OfflinePageState.UNTRUSTED_OFFLINE_PAGE;
            }
            // Get formatted creation date of the offline page. If the page was shared (so the
            // creation date cannot be acquired), make date an empty string and there will be
            // specific processing for showing different string in UI.
            long pageCreationTimeMs = offlinePage.getCreationTimeMs();
            if (pageCreationTimeMs != 0) {
                Date creationDate = new Date(offlinePage.getCreationTimeMs());
                DateFormat df = DateFormat.getDateInstance(DateFormat.MEDIUM);
                mOfflinePageCreationDate = df.format(creationDate);
            }
        }
    }

    /** {@inheritDoc} */
    @Override
    public ModalDialogManager getModalDialogManager() {
        return mModalDialogManagerSupplier.get();
    }

    /** {@inheritDoc} */
    @Override
    public void initOfflinePageUiParams(
            PageInfoView.Params viewParams, Consumer<Runnable> runAfterDismiss) {
        if (isShowingOfflinePage() && OfflinePageUtils.isConnected()) {
            viewParams.openOnlineButtonClickCallback =
                    () -> {
                        runAfterDismiss.accept(
                                () -> {
                                    // Attempt to reload to an online version of the viewed offline
                                    // web page.
                                    // This attempt might fail if the user is offline, in which case
                                    // an offline copy will be reloaded.
                                    OfflinePageUtils.reload(
                                            mWebContents, mOfflinePageLoadUrlDelegate);
                                });
                    };
        } else {
            viewParams.openOnlineButtonShown = false;
        }
    }

    /** {@inheritDoc} */
    @Override
    public @Nullable String getOfflinePageConnectionMessage() {
        if (mOfflinePageState == OfflinePageState.TRUSTED_OFFLINE_PAGE) {
            return String.format(
                    mContext.getString(R.string.page_info_connection_offline),
                    mOfflinePageCreationDate);
        } else if (mOfflinePageState == OfflinePageState.UNTRUSTED_OFFLINE_PAGE) {
            // For untrusted pages, if there's a creation date, show it in the message.
            if (TextUtils.isEmpty(mOfflinePageCreationDate)) {
                return mContext.getString(R.string.page_info_offline_page_not_trusted_without_date);
            } else {
                return String.format(
                        mContext.getString(R.string.page_info_offline_page_not_trusted_with_date),
                        mOfflinePageCreationDate);
            }
        }
        return null;
    }

    /** {@inheritDoc} */
    @Override
    public boolean isShowingPaintPreviewPage() {
        Tab tab = TabUtils.fromWebContents(mWebContents);
        return tab != null && TabbedPaintPreview.get(tab).isShowing();
    }

    /** {@inheritDoc} */
    @Override
    public @Nullable String getPaintPreviewPageConnectionMessage() {
        if (!isShowingPaintPreviewPage()) return null;

        return mContext.getString(R.string.page_info_connection_paint_preview);
    }

    /** {@inheritDoc} */
    @Override
    public @PdfPageType int getPdfPageType() {
        Tab tab = TabUtils.fromWebContents(mWebContents);
        // TODO(shuyng): move this check to PdfUtils. Currently PdfUtils cannot depends on Tab.
        if (tab == null) {
            return PdfPageType.NONE;
        }
        return PdfUtils.getPdfPageType(tab.getNativePage());
    }

    /** {@inheritDoc} */
    @Override
    public @Nullable String getPdfPageConnectionMessage() {
        switch (getPdfPageType()) {
            case PdfPageType.TRANSIENT_SECURE:
                return mContext.getString(R.string.page_info_connection_transient_pdf);
            case PdfPageType.TRANSIENT_INSECURE:
                return mContext.getString(R.string.page_info_connection_transient_pdf_insecure);
            case PdfPageType.LOCAL:
                return mContext.getString(R.string.page_info_connection_local_pdf);
            default:
                return null;
        }
    }

    /** {@inheritDoc} */
    @Override
    public void showCookieSettings() {
        SiteSettingsHelper.showCategorySettings(
                mContext, SiteSettingsCategory.Type.THIRD_PARTY_COOKIES);
    }

    /** {@inheritDoc} */
    @Override
    public void showTrackingProtectionSettings() {
        SettingsNavigation settingsNavigation =
                SettingsNavigationFactory.createSettingsNavigation();
        settingsNavigation.startSettings(mContext, TrackingProtectionSettings.class);
    }

    /** {@inheritDoc} */
    @Override
    public void showAllSettingsForRws(String rwsOwner) {
        Bundle extras = new Bundle();
        extras.putString(EXTRA_SEARCH, rwsOwner);

        SettingsNavigation settingsNavigation =
                SettingsNavigationFactory.createSettingsNavigation();
        settingsNavigation.startSettings(mContext, AllSiteSettings.class, extras);
    }

    @Override
    public void showCookieFeedback(Activity activity) {
        Tab tab = TabUtils.fromWebContents(mWebContents);

        // FEEDBACK_REPORT_TYPE: Reports for Chrome mobile must have a contextTag of the form
        // com.chrome.feed.USER_INITIATED_FEEDBACK_REPORT, or they will be discarded for not
        // matching an allow list rule.
        HelpAndFeedbackLauncherImpl.getForProfile(mProfile)
                .showFeedback(activity, tab.getOriginalUrl().getHost(), FEEDBACK_REPORT_TYPE);
    }

    @Override
    public void showAdPersonalizationSettings() {
        PrivacySandboxSettingsBaseFragment.launchPrivacySandboxSettings(
                mContext, PrivacySandboxReferrer.PAGE_INFO_AD_PRIVACY_SECTION);
    }

    @NonNull
    @Override
    public Collection<PageInfoSubpageController> createAdditionalRowViews(
            PageInfoMainController mainController, ViewGroup rowWrapper) {
        Collection<PageInfoSubpageController> controllers = new ArrayList<>();
        var adPersonalizationRow = new PageInfoRowView(rowWrapper.getContext(), null);
        adPersonalizationRow.setId(PageInfoAdPersonalizationController.ROW_ID);
        rowWrapper.addView(adPersonalizationRow);
        controllers.add(
                new PageInfoAdPersonalizationController(
                        mainController, adPersonalizationRow, this));

        // Add history row.
        final Tab tab = TabUtils.fromWebContents(mWebContents);
        var historyRow = new PageInfoRowView(rowWrapper.getContext(), null);
        historyRow.setId(PageInfoHistoryController.HISTORY_ROW_ID);
        rowWrapper.addView(historyRow);
        controllers.add(
                new PageInfoHistoryController(
                        mainController,
                        historyRow,
                        this,
                        () -> {
                            return tab;
                        }));

        if (PageInfoAboutThisSiteController.isFeatureEnabled()) {
            var aboutThisSiteRow = new PageInfoRowView(rowWrapper.getContext(), null);
            aboutThisSiteRow.setId(PageInfoAboutThisSiteController.ROW_ID);
            rowWrapper.addView(aboutThisSiteRow);
            new PageInfoAboutThisSiteController(
                    mainController,
                    mEphemeralTabCoordinatorSupplier,
                    aboutThisSiteRow,
                    this,
                    mWebContents,
                    mTabCreator);
        }
        if (!isIncognito()) {
            var storeInfoRow = new PageInfoRowView(rowWrapper.getContext(), null);
            storeInfoRow.setId(PageInfoStoreInfoController.STORE_INFO_ROW_ID);
            rowWrapper.addView(storeInfoRow);
            controllers.add(
                    new PageInfoStoreInfoController(
                            mainController,
                            storeInfoRow,
                            mStoreInfoActionHandlerSupplier,
                            mPageInfoHighlight.shouldHighlightStoreInfo(),
                            mWebContents,
                            mProfile));
        }
        return controllers;
    }

    /** {@inheritDoc} */
    @Override
    public @NonNull CookieControlsBridge createCookieControlsBridge(
            CookieControlsObserver observer) {
        return new CookieControlsBridge(
                observer,
                mWebContents,
                mProfile.isOffTheRecord() ? mProfile.getOriginalProfile() : null);
    }

    /** {@inheritDoc} */
    @Override
    public @NonNull BrowserContextHandle getBrowserContext() {
        return mProfile;
    }

    /** {@inheritDoc} */
    @Override
    public @NonNull SiteSettingsDelegate getSiteSettingsDelegate() {
        return new ChromeSiteSettingsDelegate(mContext, mProfile);
    }

    @Override
    public void getFavicon(GURL url, Callback<Drawable> callback) {
        Resources resources = mContext.getResources();
        int size = resources.getDimensionPixelSize(R.dimen.page_info_favicon_size);
        new FaviconHelper()
                .getLocalFaviconImageForURL(
                        mProfile,
                        url,
                        size,
                        (image, iconUrl) -> {
                            if (image != null) {
                                callback.onResult(new BitmapDrawable(resources, image));
                            } else if (UrlUtilities.isInternalScheme(url)) {
                                callback.onResult(
                                        TintedDrawable.constructTintedDrawable(
                                                mContext, R.drawable.chromelogo16));
                            } else {
                                callback.onResult(null);
                            }
                        });
    }

    @Override
    public boolean isAccessibilityEnabled() {
        return ChromeAccessibilityUtil.get().isAccessibilityEnabled();
    }

    @Override
    public FragmentManager getFragmentManager() {
        FragmentActivity activity = ((FragmentActivity) mContext);
        if (activity.isFinishing()) return null;
        return activity.getSupportFragmentManager();
    }

    /** {@inheritDoc} */
    @Override
    public boolean isIncognito() {
        return mProfile.isOffTheRecord();
    }

    @Override
    public boolean showTrackingProtectionUI() {
        return getSiteSettingsDelegate().shouldShowTrackingProtectionUI();
    }

    @Override
    public boolean shouldShowTrackingProtectionBrandedUI() {
        return getSiteSettingsDelegate().shouldShowTrackingProtectionBrandedUI();
    }

    @Override
    public boolean showTrackingProtectionACTFeaturesUI() {
        return getSiteSettingsDelegate().shouldShowTrackingProtectionACTFeaturesUI();
    }

    @Override
    public boolean allThirdPartyCookiesBlockedTrackingProtection() {
        return UserPrefs.get(mProfile).getBoolean(Pref.BLOCK_ALL3PC_TOGGLE_ENABLED)
                || isIncognito();
    }
}
