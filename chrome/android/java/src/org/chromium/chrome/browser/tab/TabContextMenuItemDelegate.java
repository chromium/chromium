// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab;

import android.app.Activity;
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
import org.chromium.chrome.browser.offlinepages.OfflinePageBridge;
import org.chromium.chrome.browser.offlinepages.RequestCoordinatorBridge;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.state.CriticalPersistedTabData;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.document.TabDelegate;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.components.embedder_support.util.UrlUtilities;
import org.chromium.components.feature_engagement.EventConstants;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.common.Referrer;
import org.chromium.ui.base.Clipboard;
import org.chromium.ui.base.PageTransition;
import org.chromium.url.GURL;

/**
 * A default {@link ContextMenuItemDelegate} that supports the context menu functionality in Tab.
 */
public class TabContextMenuItemDelegate implements ContextMenuItemDelegate {
    private final TabImpl mTab;
    private final TabModelSelector mTabModelSelector;
    private final Supplier<EphemeralTabCoordinator> mEphemeralTabCoordinatorSupplier;
    private final Runnable mContextMenuCopyLinkObserver;
    private final Supplier<SnackbarManager> mSnackbarManager;

    /**
     * Builds a {@link TabContextMenuItemDelegate} instance.
     */
    public TabContextMenuItemDelegate(Tab tab, TabModelSelector tabModelSelector,
            Supplier<EphemeralTabCoordinator> ephemeralTabCoordinatorSupplier,
            Runnable contextMenuCopyLinkObserver, Supplier<SnackbarManager> snackbarManager) {
        mTab = (TabImpl) tab;
        mTabModelSelector = tabModelSelector;
        mEphemeralTabCoordinatorSupplier = ephemeralTabCoordinatorSupplier;
        mContextMenuCopyLinkObserver = contextMenuCopyLinkObserver;
        mSnackbarManager = snackbarManager;
    }

    @Override
    public void onDestroy() {}

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
    public boolean canEnterMultiWindowMode() {
        return MultiWindowUtils.getInstance().canEnterMultiWindowMode(TabUtils.getActivity(mTab));
    }

    @Override
    public boolean startDownload(GURL url, boolean isLink) {
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
    public void onCall(GURL url) {
        Intent intent = new Intent(Intent.ACTION_VIEW);
        intent.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        intent.setData(Uri.parse(url.getSpec()));
        IntentUtils.safeStartActivity(mTab.getContext(), intent);
    }

    @Override
    public boolean supportsSendEmailMessage() {
        Intent intent = new Intent(Intent.ACTION_VIEW);
        intent.setData(Uri.parse("mailto:test@example.com"));
        return mTab.getWindowAndroid().canResolveActivity(intent);
    }

    @Override
    public void onSendEmailMessage(GURL url) {
        Intent intent = new Intent(Intent.ACTION_VIEW);
        intent.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        intent.setData(Uri.parse(url.getSpec()));
        IntentUtils.safeStartActivity(mTab.getContext(), intent);
    }

    @Override
    public boolean supportsSendTextMessage() {
        Intent intent = new Intent(Intent.ACTION_VIEW);
        intent.setData(Uri.parse("sms:"));
        return mTab.getWindowAndroid().canResolveActivity(intent);
    }

    @Override
    public void onSendTextMessage(GURL url) {
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
    public void onAddToContacts(GURL url) {
        Intent intent = new Intent(Intent.ACTION_INSERT);
        intent.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        intent.setType(ContactsContract.Contacts.CONTENT_TYPE);
        if (MailTo.isMailTo(url.getSpec())) {
            intent.putExtra(ContactsContract.Intents.Insert.EMAIL,
                    MailTo.parse(url.getSpec()).getTo().split(",")[0]);
        } else if (UrlUtilities.isTelScheme(url)) {
            intent.putExtra(ContactsContract.Intents.Insert.PHONE, UrlUtilities.getTelNumber(url));
        }
        IntentUtils.safeStartActivity(mTab.getContext(), intent);
    }

    @Override
    public void onOpenInOtherWindow(GURL url, Referrer referrer) {
        TabDelegate tabDelegate = new TabDelegate(mTab.isIncognito());
        LoadUrlParams loadUrlParams = new LoadUrlParams(url.getSpec());
        loadUrlParams.setReferrer(referrer);
        Activity activity = TabUtils.getActivity(mTab);
        tabDelegate.createTabInOtherWindow(loadUrlParams, activity,
                CriticalPersistedTabData.from(mTab).getParentId(),
                MultiWindowUtils.getAdjacentWindowActivity(activity));
    }

    @Override
    public void onOpenInNewTab(GURL url, Referrer referrer, boolean navigateToTab) {
        RecordUserAction.record("MobileNewTabOpened");
        RecordUserAction.record("LinkOpenedInNewTab");
        LoadUrlParams loadUrlParams = new LoadUrlParams(url.getSpec());
        loadUrlParams.setReferrer(referrer);
        mTabModelSelector.openNewTab(loadUrlParams,
                navigateToTab ? TabLaunchType.FROM_LONGPRESS_FOREGROUND
                              : TabLaunchType.FROM_LONGPRESS_BACKGROUND,
                mTab, isIncognito());
    }

    @Override
    public void onOpenInNewTabInGroup(GURL url, Referrer referrer) {
        RecordUserAction.record("MobileNewTabOpened");
        RecordUserAction.record("LinkOpenedInNewTab");
        LoadUrlParams loadUrlParams = new LoadUrlParams(url.getSpec());
        loadUrlParams.setReferrer(referrer);
        mTabModelSelector.openNewTab(loadUrlParams,
                TabLaunchType.FROM_LONGPRESS_BACKGROUND_IN_GROUP, mTab, isIncognito());
    }

    @Override
    public void onOpenInNewIncognitoTab(GURL url) {
        RecordUserAction.record("MobileNewTabOpened");
        mTabModelSelector.openNewTab(new LoadUrlParams(url.getSpec()),
                TabLaunchType.FROM_LONGPRESS_FOREGROUND, mTab, true);
    }

    @Override
    public GURL getPageUrl() {
        return mTab.getUrl();
    }

    @Override
    public void onOpenImageUrl(GURL url, Referrer referrer) {
        LoadUrlParams loadUrlParams = new LoadUrlParams(url.getSpec());
        loadUrlParams.setTransitionType(PageTransition.LINK);
        loadUrlParams.setReferrer(referrer);
        mTab.loadUrl(loadUrlParams);
    }

    @Override
    public void onOpenImageInNewTab(GURL url, Referrer referrer) {
        LoadUrlParams loadUrlParams = new LoadUrlParams(url.getSpec());
        loadUrlParams.setReferrer(referrer);
        mTabModelSelector.openNewTab(
                loadUrlParams, TabLaunchType.FROM_LONGPRESS_BACKGROUND, mTab, isIncognito());
    }

    @Override
    public void onOpenInEphemeralTab(GURL url, String title) {
        if (mEphemeralTabCoordinatorSupplier == null
                || mEphemeralTabCoordinatorSupplier.get() == null) {
            return;
        }
        mEphemeralTabCoordinatorSupplier.get().requestOpenSheet(url, title, mTab.isIncognito());
    }

    @Override
    public void onReadLater(GURL url, String title) {
        if (url == null || url.isEmpty()) return;
        assert url.isValid();

        BookmarkModel bookmarkModel =
                BookmarkModel.getForProfile(Profile.getLastUsedRegularProfile());
        bookmarkModel.finishLoadingBookmarkModel(() -> {
            // Add to reading list.
            BookmarkUtils.addToReadingList(
                    url, title, mSnackbarManager.get(), bookmarkModel, mTab.getContext());
            TrackerFactory.getTrackerForProfile(Profile.getLastUsedRegularProfile())
                    .notifyEvent(EventConstants.READ_LATER_CONTEXT_MENU_TAPPED);

            // Add to offline pages.
            RequestCoordinatorBridge.getForProfile(Profile.getLastUsedRegularProfile())
                    .savePageLater(url.getSpec(), OfflinePageBridge.BOOKMARK_NAMESPACE,
                            /*userRequested*/ true);
        });
    }

    @Override
    public void onOpenInChrome(GURL linkUrl, GURL pageUrl) {
        Context applicationContext = ContextUtils.getApplicationContext();
        Intent chromeIntent = new Intent(Intent.ACTION_VIEW, Uri.parse(linkUrl.getSpec()));
        chromeIntent.setPackage(applicationContext.getPackageName());
        chromeIntent.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK);

        if (!PackageManagerUtils.canResolveActivity(chromeIntent)) {
            // If Chrome can't handle intent fallback to using any other VIEW handlers.
            chromeIntent.setPackage(null);

            // Query again without the package name set and if there are still no handlers for the
            // URI fail gracefully, and do nothing, since this will still cause a crash if launched.
            if (!PackageManagerUtils.canResolveActivity(chromeIntent)) return;
        }

        boolean activityStarted = false;
        if (pageUrl != null) {
            if (UrlUtilities.isInternalScheme(pageUrl)) {
                IntentHandler.startChromeLauncherActivityForTrustedIntent(chromeIntent);
                activityStarted = true;
            }
        }

        if (!activityStarted) {
            mTab.getContext().startActivity(chromeIntent);
            activityStarted = true;
        }
    }

    @Override
    public void onOpenInNewChromeTabFromCCT(GURL linkUrl, boolean isIncognito) {
        Intent intent = new Intent(Intent.ACTION_VIEW, Uri.parse(linkUrl.getSpec()));
        intent.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        intent.setClass(ContextUtils.getApplicationContext(), ChromeLauncherActivity.class);
        if (isIncognito) {
            intent.putExtra(IntentHandler.EXTRA_OPEN_NEW_INCOGNITO_TAB, true);
            intent.putExtra(Browser.EXTRA_APPLICATION_ID,
                    ContextUtils.getApplicationContext().getPackageName());
            IntentUtils.addTrustedIntentExtras(intent);
            IntentHandler.setTabLaunchType(intent, TabLaunchType.FROM_EXTERNAL_APP);
        }
        IntentUtils.safeStartActivity(mTab.getContext(), intent);
    }

    @Override
    public String getTitleForOpenTabInExternalApp() {
        return DefaultBrowserInfo.getTitleOpenInDefaultBrowser(false);
    }

    @Override
    public void onOpenInDefaultBrowser(GURL url) {
        Intent intent = new Intent(Intent.ACTION_VIEW, Uri.parse(url.getSpec()));
        CustomTabsIntent.setAlwaysUseBrowserUI(intent);
        IntentUtils.safeStartActivity(mTab.getContext(), intent);
    }
}
