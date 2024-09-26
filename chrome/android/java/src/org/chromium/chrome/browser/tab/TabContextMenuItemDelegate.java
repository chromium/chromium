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
import android.text.TextUtils;

import androidx.annotation.Nullable;
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
import org.chromium.chrome.browser.document.ChromeLauncherActivity;
import org.chromium.chrome.browser.download.ChromeDownloadDelegate;
import org.chromium.chrome.browser.ephemeraltab.EphemeralTabCoordinator;
import org.chromium.chrome.browser.feature_engagement.TrackerFactory;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.incognito.IncognitoUtils;
import org.chromium.chrome.browser.multiwindow.MultiWindowUtils;
import org.chromium.chrome.browser.offlinepages.OfflinePageBridge;
import org.chromium.chrome.browser.offlinepages.RequestCoordinatorBridge;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.document.ChromeAsyncTabLauncher;
import org.chromium.chrome.browser.tasks.tab_groups.TabGroupModelFilter;
import org.chromium.chrome.browser.tasks.tab_management.TabGroupCreationDialogManager;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.embedder_support.contextmenu.ContextMenuItemDelegate;
import org.chromium.components.embedder_support.util.UrlUtilities;
import org.chromium.components.feature_engagement.EventConstants;
import org.chromium.content_public.browser.AdditionalNavigationParams;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.common.Referrer;
import org.chromium.ui.base.Clipboard;
import org.chromium.ui.base.PageTransition;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.url.GURL;

import java.util.List;

/**
 * A default {@link ContextMenuItemDelegate} that supports the context menu functionality in Tab.
 */
public class TabContextMenuItemDelegate implements ContextMenuItemDelegate {
    private final Activity mActivity;
    private final TabImpl mTab;
    private final TabModelSelector mTabModelSelector;
    private final Supplier<EphemeralTabCoordinator> mEphemeralTabCoordinatorSupplier;
    private final Runnable mContextMenuCopyLinkObserver;
    private final Supplier<SnackbarManager> mSnackbarManagerSupplier;
    private final Supplier<BottomSheetController> mBottomSheetControllerSupplier;
    private final Supplier<ModalDialogManager> mModalDialogManagerSupplier;
    private final TabGroupCreationDialogManager mTabGroupCreationDialogManager;

    /** Builds a {@link TabContextMenuItemDelegate} instance. */
    public TabContextMenuItemDelegate(
            Activity activity,
            Tab tab,
            TabModelSelector tabModelSelector,
            Supplier<EphemeralTabCoordinator> ephemeralTabCoordinatorSupplier,
            Runnable contextMenuCopyLinkObserver,
            Supplier<SnackbarManager> snackbarManagerSupplier,
            Supplier<BottomSheetController> bottomSheetControllerSupplier,
            Supplier<ModalDialogManager> modalDialogManagerSupplier) {
        mActivity = activity;
        mTab = (TabImpl) tab;
        mTabModelSelector = tabModelSelector;
        mEphemeralTabCoordinatorSupplier = ephemeralTabCoordinatorSupplier;
        mContextMenuCopyLinkObserver = contextMenuCopyLinkObserver;
        mSnackbarManagerSupplier = snackbarManagerSupplier;
        mBottomSheetControllerSupplier = bottomSheetControllerSupplier;
        mModalDialogManagerSupplier = modalDialogManagerSupplier;
        mTabGroupCreationDialogManager =
                new TabGroupCreationDialogManager(
                        activity,
                        mModalDialogManagerSupplier.get(),
                        /* onTabGroupCreation= */ null);
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
        return IncognitoUtils.isIncognitoModeEnabled(mTab.getProfile());
    }

    /**
     * @return Whether the "Open in other window" context menu item should be shown.
     */
    public boolean isOpenInOtherWindowSupported() {
        return MultiWindowUtils.getInstance()
                .isOpenInOtherWindowSupported(TabUtils.getActivity(mTab));
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
            // TODO(crbug.com/40732234): Find a better way of passing event for IPH.
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
            intent.putExtra(
                    ContactsContract.Intents.Insert.EMAIL,
                    MailTo.parse(url.getSpec()).getTo().split(",")[0]);
        } else if (UrlUtilities.isTelScheme(url)) {
            intent.putExtra(ContactsContract.Intents.Insert.PHONE, UrlUtilities.getTelNumber(url));
        }
        IntentUtils.safeStartActivity(mTab.getContext(), intent);
    }

    /**
     * Called when the {@code url} should be opened in the other window with the same incognito
     * state as the current page.
     *
     * @param url The URL to open.
     */
    public void onOpenInOtherWindow(GURL url, Referrer referrer) {
        ChromeAsyncTabLauncher chromeAsyncTabLauncher =
                new ChromeAsyncTabLauncher(mTab.isIncognito());
        LoadUrlParams loadUrlParams = new LoadUrlParams(url.getSpec());
        loadUrlParams.setReferrer(referrer);
        Activity activity = TabUtils.getActivity(mTab);
        chromeAsyncTabLauncher.launchTabInOtherWindow(
                loadUrlParams,
                activity,
                mTab.getParentId(),
                MultiWindowUtils.getAdjacentWindowActivity(activity));
    }

    /**
     * Called when the {@code url} should be opened in a new page with the same incognito state as
     * the current page.
     *
     * @param url The URL to open.
     * @param referrer The attribution impression to associate with the navigation.
     * @param navigateToTab Whether or not to navigate to the new page.
     * @param additionalNavigationParams Additional information that needs to be passed to the
     *     navigation request.
     */
    public void onOpenInNewTab(
            GURL url,
            Referrer referrer,
            boolean navigateToTab,
            @Nullable AdditionalNavigationParams additionalNavigationParams) {
        RecordUserAction.record("MobileNewTabOpened");
        RecordUserAction.record("LinkOpenedInNewTab");
        LoadUrlParams loadUrlParams = new LoadUrlParams(url.getSpec());
        loadUrlParams.setReferrer(referrer);
        loadUrlParams.setAdditionalNavigationParams(additionalNavigationParams);
        mTabModelSelector.openNewTab(
                loadUrlParams,
                navigateToTab
                        ? TabLaunchType.FROM_LONGPRESS_FOREGROUND
                        : TabLaunchType.FROM_LONGPRESS_BACKGROUND,
                mTab,
                isIncognito());
    }

    /**
     * Called when {@code url} should be opened in a new page in the same group as the current page.
     *
     * @param url The URL to open.
     */
    public void onOpenInNewTabInGroup(GURL url, Referrer referrer) {
        RecordUserAction.record("MobileNewTabOpened");
        RecordUserAction.record("LinkOpenedInNewTab");
        LoadUrlParams loadUrlParams = new LoadUrlParams(url.getSpec());
        loadUrlParams.setReferrer(referrer);

        TabGroupModelFilter filter =
                (TabGroupModelFilter)
                        mTabModelSelector.getTabModelFilterProvider().getCurrentTabModelFilter();
        boolean willMergingCreateNewGroup = filter.willMergingCreateNewGroup(List.of(mTab));
        mTabModelSelector.openNewTab(
                loadUrlParams,
                TabLaunchType.FROM_LONGPRESS_BACKGROUND_IN_GROUP,
                mTab,
                isIncognito());

        if (ChromeFeatureList.sTabGroupParityAndroid.isEnabled()
                && willMergingCreateNewGroup
                && !TabGroupCreationDialogManager.shouldSkipGroupCreationDialog(
                        /* shouldShow= */ false)) {
            mTabGroupCreationDialogManager.showDialog(mTab.getRootId(), filter);
        }
    }

    /**
     * Called when the {@code url} should be opened in a new incognito page.
     *
     * @param url The URL to open.
     */
    public void onOpenInNewIncognitoTab(GURL url) {
        RecordUserAction.record("MobileNewTabOpened");
        mTabModelSelector.openNewTab(
                new LoadUrlParams(url.getSpec()),
                TabLaunchType.FROM_LONGPRESS_FOREGROUND,
                mTab,
                true);
    }

    @Override
    public GURL getPageUrl() {
        return mTab.getUrl();
    }

    /**
     * Called when the {@code url} is of an image and should be opened in the same page.
     *
     * @param url The image URL to open.
     */
    public void onOpenImageUrl(GURL url, Referrer referrer) {
        LoadUrlParams loadUrlParams = new LoadUrlParams(url.getSpec());
        loadUrlParams.setTransitionType(PageTransition.LINK);
        loadUrlParams.setReferrer(referrer);
        mTab.loadUrl(loadUrlParams);
    }

    /**
     * Called when the {@code url} is of an image and should be opened in a new page.
     *
     * @param url The image URL to open.
     */
    public void onOpenImageInNewTab(GURL url, Referrer referrer) {
        LoadUrlParams loadUrlParams = new LoadUrlParams(url.getSpec());
        loadUrlParams.setReferrer(referrer);
        mTabModelSelector.openNewTab(
                loadUrlParams, TabLaunchType.FROM_LONGPRESS_BACKGROUND, mTab, isIncognito());
    }

    /**
     * Called when the {@code url} should be opened in an ephemeral page.
     *
     * @param url The URL to open.
     * @param title The title text to show on top control.
     */
    public void onOpenInEphemeralTab(GURL url, String title) {
        if (mEphemeralTabCoordinatorSupplier == null
                || mEphemeralTabCoordinatorSupplier.get() == null) {
            return;
        }
        mEphemeralTabCoordinatorSupplier.get().requestOpenSheet(url, title, mTab.getProfile());
    }

    /**
     * Called when Read Later was selected from the context menu.
     *
     * @param url The URL to be saved to the reading list.
     * @param title The title text to be shown for this item in the reading list.
     */
    public void onReadLater(GURL url, String title) {
        if (url == null || url.isEmpty()) return;
        assert url.isValid();

        Profile profile = mTab.getProfile().getOriginalProfile();
        BookmarkModel bookmarkModel = BookmarkModel.getForProfile(profile);
        bookmarkModel.finishLoadingBookmarkModel(
                () -> {
                    // Add to reading list.
                    BookmarkUtils.addToReadingList(
                            mActivity,
                            bookmarkModel,
                            title,
                            url,
                            mSnackbarManagerSupplier.get(),
                            mTab.getProfile(),
                            mBottomSheetControllerSupplier.get());
                    TrackerFactory.getTrackerForProfile(profile)
                            .notifyEvent(EventConstants.READ_LATER_CONTEXT_MENU_TAPPED);

                    // Add to offline pages.
                    RequestCoordinatorBridge.getForProfile(profile)
                            .savePageLater(
                                    url.getSpec(),
                                    OfflinePageBridge.BOOKMARK_NAMESPACE,
                                    /* userRequested= */ true);
                });
    }

    /**
     * Called when a link should be opened in the main Chrome browser.
     *
     * @param linkUrl URL that should be opened.
     * @param pageUrl URL of the current page.
     */
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

    /**
     * Called when the {@code url} should be opened in a new Chrome page from CCT.
     *
     * @param linkUrl The URL to open.
     * @param isIncognito true if the {@code url} should be opened in a new incognito page.
     */
    public void onOpenInNewChromeTabFromCCT(GURL linkUrl, boolean isIncognito) {
        Intent intent = new Intent(Intent.ACTION_VIEW, Uri.parse(linkUrl.getSpec()));
        intent.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        intent.setClass(ContextUtils.getApplicationContext(), ChromeLauncherActivity.class);
        if (isIncognito) {
            intent.putExtra(IntentHandler.EXTRA_OPEN_NEW_INCOGNITO_TAB, true);
            intent.putExtra(
                    Browser.EXTRA_APPLICATION_ID,
                    ContextUtils.getApplicationContext().getPackageName());
            IntentUtils.addTrustedIntentExtras(intent);
            IntentHandler.setTabLaunchType(intent, TabLaunchType.FROM_EXTERNAL_APP);
        }
        IntentUtils.safeStartActivity(mTab.getContext(), intent);
    }

    /**
     * @return title of the context menu to open a page in external apps.
     */
    public String getTitleForOpenTabInExternalApp() {
        return DefaultBrowserInfo.getTitleOpenInDefaultBrowser(false);
    }

    @Override
    public void onOpenInDefaultBrowser(GURL url) {
        // Most browsers (including Chrome) do not advertise support for data scheme URIs
        // and so cannot handle data scheme view Intents. Use the browser backing the currently
        // running CCT.
        if (TextUtils.equals("data", url.getScheme())) {
            onOpenInNewChromeTabFromCCT(url, false);
            return;
        }

        Intent intent = new Intent(Intent.ACTION_VIEW, Uri.parse(url.getSpec()));
        CustomTabsIntent.setAlwaysUseBrowserUI(intent);
        IntentUtils.safeStartActivity(mTab.getContext(), intent);
    }
}
