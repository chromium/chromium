// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab;

import android.content.Context;
import android.content.Intent;
import android.net.MailTo;
import android.net.Uri;
import android.provider.Browser;
import android.provider.ContactsContract;

import androidx.browser.customtabs.CustomTabsIntent;

import org.chromium.base.ContextUtils;
import org.chromium.base.IntentUtils;
import org.chromium.base.PackageManagerUtils;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.browser.DefaultBrowserInfo;
import org.chromium.chrome.browser.IntentHandler;
import org.chromium.chrome.browser.bookmarks.BookmarkModel;
import org.chromium.chrome.browser.bookmarks.BookmarkUtils;
import org.chromium.chrome.browser.compositor.bottombar.ephemeraltab.EphemeralTabCoordinator;
import org.chromium.chrome.browser.contextmenu.ContextMenuItemDelegate;
import org.chromium.chrome.browser.document.ChromeLauncherActivity;
import org.chromium.chrome.browser.download.ChromeDownloadDelegate;
import org.chromium.chrome.browser.feature_engagement.TrackerFactory;
import org.chromium.chrome.browser.incognito.IncognitoUtils;
import org.chromium.chrome.browser.multiwindow.MultiWindowUtils;
import org.chromium.chrome.browser.net.spdyproxy.DataReductionProxySettings;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.state.CriticalPersistedTabData;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.document.TabDelegate;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.components.embedder_support.util.UrlUtilities;
import org.chromium.components.feature_engagement.EventConstants;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.common.Referrer;
import org.chromium.ui.base.Clipboard;
import org.chromium.ui.base.PageTransition;
import org.chromium.url.URI;

import java.util.Locale;

/**
 * A default {@link ContextMenuItemDelegate} that supports the context menu functionality in Tab.
 */
public class TabContextMenuItemDelegate implements ContextMenuItemDelegate {
    private final TabImpl mTab;
    private final TabModelSelector mTabModelSelector;
    private boolean mLoadOriginalImageRequestedForPageLoad;
    private EmptyTabObserver mDataReductionProxyContextMenuTabObserver;
    private final Supplier<EphemeralTabCoordinator> mEphemeralTabCoordinatorSupplier;
    private final Runnable mContextMenuCopyLinkObserver;
    private final Supplier<SnackbarManager> mSnackbarManagerSupplier;

    /**
     * Builds a {@link TabContextMenuItemDelegate} instance.
     */
    public TabContextMenuItemDelegate(Tab tab, TabModelSelector tabModelSelector,
            Supplier<EphemeralTabCoordinator> ephemeralTabCoordinatorSupplier,
            Runnable contextMenuCopyLinkObserver,
            Supplier<SnackbarManager> snackbarManagerSupplier) {
        mTab = (TabImpl) tab;
        mTabModelSelector = tabModelSelector;
        mEphemeralTabCoordinatorSupplier = ephemeralTabCoordinatorSupplier;
        mContextMenuCopyLinkObserver = contextMenuCopyLinkObserver;
        mSnackbarManagerSupplier = snackbarManagerSupplier;

        mDataReductionProxyContextMenuTabObserver = new EmptyTabObserver() {
            @Override
            public void onPageLoadStarted(Tab tab, String url) {
                mLoadOriginalImageRequestedForPageLoad = false;
            }
        };
        mTab.addObserver(mDataReductionProxyContextMenuTabObserver);
    }

    @Override
    public void onDestroy() {
        mTab.removeObserver(mDataReductionProxyContextMenuTabObserver);
    }

    @Override
    public String getPageTitle() {
        return mTab.getTitle();
    }

    @Override
    public WebContents getWebContents() {
        return mTab.getWebContents();
    }

    @Override
    public boolean isIncognito() {
        return mTab.isIncognito();
    }

    @Override
    public boolean isIncognitoSupported() {
        return IncognitoUtils.isIncognitoModeEnabled();
    }

    @Override
    public boolean isOpenInOtherWindowSupported() {
        return MultiWindowUtils.getInstance().isOpenInOtherWindowSupported(
                TabUtils.getActivity(mTab));
    }

    @Override
    public boolean isDataReductionProxyEnabledForURL(String url) {
        return isSpdyProxyEnabledForUrl(url);
    }

    @Override
    public boolean startDownload(String url, boolean isLink) {
        return !isLink
                || !ChromeDownloadDelegate.from(mTab).shouldInterceptContextMenuDownload(url);
    }

    @Override
    public void onSaveToClipboard(String text, int clipboardType) {
        Clipboard.getInstance().setText(text);
        if (clipboardType == ClipboardType.LINK_URL) {
            // TODO(crbug/1150090): Find a better way of passing event for IPH.
            mContextMenuCopyLinkObserver.run();
        }
    }

    @Override
    public void onSaveImageToClipboard(Uri uri) {
        Clipboard.getInstance().setImageUri(uri);
    }

    @Override
    public boolean supportsCall() {
        Intent intent = new Intent(Intent.ACTION_VIEW);
        intent.setData(Uri.parse("tel:"));
        return mTab.getWindowAndroid().canResolveActivity(intent);
    }

    @Override
    public void onCall(String uri) {
        Intent intent = new Intent(Intent.ACTION_VIEW);
        intent.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        intent.setData(Uri.parse(uri));
        IntentUtils.safeStartActivity(mTab.getContext(), intent);
    }

    @Override
    public boolean supportsSendEmailMessage() {
        Intent intent = new Intent(Intent.ACTION_VIEW);
        intent.setData(Uri.parse("mailto:test@example.com"));
        return mTab.getWindowAndroid().canResolveActivity(intent);
    }

    @Override
    public void onSendEmailMessage(String url) {
        Intent intent = new Intent(Intent.ACTION_VIEW);
        intent.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        intent.setData(Uri.parse(url));
        IntentUtils.safeStartActivity(mTab.getContext(), intent);
    }

    @Override
    public boolean supportsSendTextMessage() {
        Intent intent = new Intent(Intent.ACTION_VIEW);
        intent.setData(Uri.parse("sms:"));
        return mTab.getWindowAndroid().canResolveActivity(intent);
    }

    @Override
    public void onSendTextMessage(String url) {
        Intent intent = new Intent(Intent.ACTION_VIEW);
        intent.setData(Uri.parse("sms:" + UrlUtilities.getTelNumber(url)));
        IntentUtils.safeStartActivity(mTab.getContext(), intent);
    }

    @Override
    public boolean supportsAddToContacts() {
        Intent intent = new Intent(Intent.ACTION_INSERT);
        intent.setType(ContactsContract.Contacts.CONTENT_TYPE);
        return mTab.getWindowAndroid().canResolveActivity(intent);
    }

    @Override
    public void onAddToContacts(String url) {
        Intent intent = new Intent(Intent.ACTION_INSERT);
        intent.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        intent.setType(ContactsContract.Contacts.CONTENT_TYPE);
        if (MailTo.isMailTo(url)) {
            intent.putExtra(
                    ContactsContract.Intents.Insert.EMAIL, MailTo.parse(url).getTo().split(",")[0]);
        } else if (UrlUtilities.isTelScheme(url)) {
            intent.putExtra(ContactsContract.Intents.Insert.PHONE, UrlUtilities.getTelNumber(url));
        }
        IntentUtils.safeStartActivity(mTab.getContext(), intent);
    }

    @Override
    public void onOpenInOtherWindow(String url, Referrer referrer) {
        TabDelegate tabDelegate = new TabDelegate(mTab.isIncognito());
        LoadUrlParams loadUrlParams = new LoadUrlParams(url);
        loadUrlParams.setReferrer(referrer);
        tabDelegate.createTabInOtherWindow(loadUrlParams, TabUtils.getActivity(mTab),
                CriticalPersistedTabData.from(mTab).getParentId());
    }

    @Override
    public void onOpenInNewTab(String url, Referrer referrer) {
        RecordUserAction.record("MobileNewTabOpened");
        RecordUserAction.record("LinkOpenedInNewTab");
        LoadUrlParams loadUrlParams = new LoadUrlParams(url);
        loadUrlParams.setReferrer(referrer);
        mTabModelSelector.openNewTab(
                loadUrlParams, TabLaunchType.FROM_LONGPRESS_BACKGROUND, mTab, isIncognito());
    }

    @Override
    public void onLoadOriginalImage() {
        mLoadOriginalImageRequestedForPageLoad = true;
        mTab.loadOriginalImage();
    }

    @Override
    public boolean wasLoadOriginalImageRequestedForPageLoad() {
        return mLoadOriginalImageRequestedForPageLoad;
    }

    @Override
    public void onOpenInNewIncognitoTab(String url) {
        RecordUserAction.record("MobileNewTabOpened");
        mTabModelSelector.openNewTab(
                new LoadUrlParams(url), TabLaunchType.FROM_LONGPRESS_FOREGROUND, mTab, true);
    }

    @Override
    public String getPageUrl() {
        return mTab.getUrlString();
    }

    @Override
    public void onOpenImageUrl(String url, Referrer referrer) {
        LoadUrlParams loadUrlParams = new LoadUrlParams(url);
        loadUrlParams.setTransitionType(PageTransition.LINK);
        loadUrlParams.setReferrer(referrer);
        mTab.loadUrl(loadUrlParams);
    }

    @Override
    public void onOpenImageInNewTab(String url, Referrer referrer) {
        LoadUrlParams loadUrlParams = new LoadUrlParams(url);
        loadUrlParams.setReferrer(referrer);
        mTabModelSelector.openNewTab(
                loadUrlParams, TabLaunchType.FROM_LONGPRESS_BACKGROUND, mTab, isIncognito());
    }

    @Override
    public void onOpenInEphemeralTab(String url, String title) {
        if (mEphemeralTabCoordinatorSupplier == null
                || mEphemeralTabCoordinatorSupplier.get() == null) {
            return;
        }
        mEphemeralTabCoordinatorSupplier.get().requestOpenSheet(url, title, mTab.isIncognito());
    }

    @Override
    public void onReadLater(String url, String title) {
        BookmarkModel bookmarkModel = new BookmarkModel();
        bookmarkModel.finishLoadingBookmarkModel(() -> {
            BookmarkUtils.addToReadingList(
                    url, title, mSnackbarManagerSupplier.get(), bookmarkModel, mTab.getContext());
            TrackerFactory.getTrackerForProfile(Profile.getLastUsedRegularProfile())
                    .notifyEvent(EventConstants.READ_LATER_CONTEXT_MENU_TAPPED);
            bookmarkModel.destroy();
        });
    }

    @Override
    public void onOpenInChrome(String linkUrl, String pageUrl) {
        Context applicationContext = ContextUtils.getApplicationContext();
        Intent chromeIntent = new Intent(Intent.ACTION_VIEW, Uri.parse(linkUrl));
        chromeIntent.setPackage(applicationContext.getPackageName());
        chromeIntent.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK);

        if (PackageManagerUtils.queryIntentActivities(chromeIntent, 0).isEmpty()) {
            // If Chrome can't handle intent fallback to using any other VIEW handlers.
            chromeIntent.setPackage(null);

            // Query again without the package name set and if there are still no handlers for the
            // URI fail gracefully, and do nothing, since this will still cause a crash if launched.
            if (PackageManagerUtils.queryIntentActivities(chromeIntent, 0).isEmpty()) return;
        }

        boolean activityStarted = false;
        if (pageUrl != null) {
            try {
                URI pageUri = URI.create(pageUrl);
                if (UrlUtilities.isInternalScheme(pageUri)) {
                    IntentHandler.startChromeLauncherActivityForTrustedIntent(chromeIntent);
                    activityStarted = true;
                }
            } catch (IllegalArgumentException ex) {
                // Ignore the exception for creating the URI and launch the intent
                // without the trusted intent extras.
            }
        }

        if (!activityStarted) {
            mTab.getContext().startActivity(chromeIntent);
            activityStarted = true;
        }
    }

    /**
     * Returns whether the 'open in chrome' menu item should be shown. This is only called when the
     * context menu is shown in cct.
     */
    @Override
    public boolean supportsOpenInChromeFromCct() {
        return true;
    }

    @Override
    public void onOpenInNewChromeTabFromCCT(String linkUrl, boolean isIncognito) {
        Intent intent = new Intent(Intent.ACTION_VIEW, Uri.parse(linkUrl));
        intent.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        intent.setClass(ContextUtils.getApplicationContext(), ChromeLauncherActivity.class);
        if (isIncognito) {
            intent.putExtra(IntentHandler.EXTRA_OPEN_NEW_INCOGNITO_TAB, true);
            intent.putExtra(Browser.EXTRA_APPLICATION_ID,
                    ContextUtils.getApplicationContext().getPackageName());
            IntentHandler.addTrustedIntentExtras(intent);
            IntentHandler.setTabLaunchType(intent, TabLaunchType.FROM_EXTERNAL_APP);
        }
        IntentUtils.safeStartActivity(mTab.getContext(), intent);
    }

    @Override
    public String getTitleForOpenTabInExternalApp() {
        return DefaultBrowserInfo.getTitleOpenInDefaultBrowser(false);
    }

    @Override
    public void onOpenInDefaultBrowser(String url) {
        Intent intent = new Intent(Intent.ACTION_VIEW, Uri.parse(url));
        CustomTabsIntent.setAlwaysUseBrowserUI(intent);
        IntentUtils.safeStartActivity(mTab.getContext(), intent);
    }

    /**
     * Checks if spdy proxy is enabled for input url.
     * @param url Input url to check for spdy setting.
     * @return true if url is enabled for spdy proxy.
    */
    private boolean isSpdyProxyEnabledForUrl(String url) {
        if (DataReductionProxySettings.getInstance().isDataReductionProxyEnabled()
                && url != null && !url.toLowerCase(Locale.US).startsWith(
                        UrlConstants.HTTPS_URL_PREFIX)
                && !isIncognito()) {
            return true;
        }
        return false;
    }

}
