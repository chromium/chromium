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
import android.support.customtabs.CustomTabsIntent;

import org.chromium.base.ContextUtils;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.chrome.browser.DefaultBrowserInfo;
import org.chromium.chrome.browser.IntentHandler;
import org.chromium.chrome.browser.LaunchIntentDispatcher;
import org.chromium.chrome.browser.UrlConstants;
import org.chromium.chrome.browser.contextmenu.ContextMenuItemDelegate;
import org.chromium.chrome.browser.document.ChromeLauncherActivity;
import org.chromium.chrome.browser.download.ChromeDownloadDelegate;
import org.chromium.chrome.browser.experiments.EphemeralTab;
import org.chromium.chrome.browser.multiwindow.MultiWindowUtils;
import org.chromium.chrome.browser.net.spdyproxy.DataReductionProxySettings;
import org.chromium.chrome.browser.preferences.PrefServiceBridge;
import org.chromium.chrome.browser.tabmodel.TabModel.TabLaunchType;
import org.chromium.chrome.browser.tabmodel.document.TabDelegate;
import org.chromium.chrome.browser.util.IntentUtils;
import org.chromium.chrome.browser.util.UrlUtilities;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.common.Referrer;
import org.chromium.ui.base.Clipboard;
import org.chromium.ui.base.PageTransition;

import java.net.URI;
import java.util.Locale;

/**
 * A default {@link ContextMenuItemDelegate} that supports the context menu functionality in Tab.
 */
public class TabContextMenuItemDelegate implements ContextMenuItemDelegate {
    private final Tab mTab;
    private boolean mLoadOriginalImageRequestedForPageLoad;
    private EmptyTabObserver mDataReductionProxyContextMenuTabObserver;

    /**
     * Builds a {@link TabContextMenuItemDelegate} instance.
     */
    public TabContextMenuItemDelegate(Tab tab) {
        mTab = tab;
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
    public boolean isIncognito() {
        return mTab.isIncognito();
    }

    @Override
    public boolean isIncognitoSupported() {
        return PrefServiceBridge.getInstance().isIncognitoModeEnabled();
    }

    @Override
    public boolean isOpenInOtherWindowSupported() {
        return MultiWindowUtils.getInstance().isOpenInOtherWindowSupported(mTab.getActivity());
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
        IntentUtils.safeStartActivity(mTab.getActivity(), intent);
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
        IntentUtils.safeStartActivity(mTab.getActivity(), intent);
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
        IntentUtils.safeStartActivity(mTab.getActivity(), intent);
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
        IntentUtils.safeStartActivity(mTab.getActivity(), intent);
    }

    @Override
    public void onOpenInOtherWindow(String url, Referrer referrer) {
        TabDelegate tabDelegate = new TabDelegate(mTab.isIncognito());
        LoadUrlParams loadUrlParams = new LoadUrlParams(url);
        loadUrlParams.setReferrer(referrer);
        tabDelegate.createTabInOtherWindow(loadUrlParams, mTab.getActivity(), mTab.getParentId());
    }

    @Override
    public void onOpenInNewTab(String url, Referrer referrer) {
        RecordUserAction.record("MobileNewTabOpened");
        LoadUrlParams loadUrlParams = new LoadUrlParams(url);
        loadUrlParams.setReferrer(referrer);
        mTab.getTabModelSelector().openNewTab(
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
        mTab.getTabModelSelector().openNewTab(new LoadUrlParams(url),
                TabLaunchType.FROM_LONGPRESS_FOREGROUND, mTab, true);
    }

    @Override
    public String getPageUrl() {
        return mTab.getUrl();
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
        boolean useOriginal = isSpdyProxyEnabledForUrl(url);
        LoadUrlParams loadUrlParams = new LoadUrlParams(url);
        loadUrlParams.setVerbatimHeaders(useOriginal
                        ? DataReductionProxySettings.getInstance()
                                  .getDataReductionProxyPassThroughHeader()
                        : null);
        loadUrlParams.setReferrer(referrer);
        mTab.getActivity().getTabModelSelector().openNewTab(loadUrlParams,
                TabLaunchType.FROM_LONGPRESS_BACKGROUND, mTab, isIncognito());
    }

    @Override
    public void onOpenInChrome(String linkUrl, String pageUrl) {
        Context applicationContext = ContextUtils.getApplicationContext();
        Intent chromeIntent = new Intent(Intent.ACTION_VIEW, Uri.parse(linkUrl));
        chromeIntent.setPackage(applicationContext.getPackageName());
        chromeIntent.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        if (applicationContext.getPackageManager()
                        .queryIntentActivities(chromeIntent, 0)
                        .isEmpty()) {
            // If Chrome can't handle intent fallback to using any other VIEW handlers.
            chromeIntent.setPackage(null);
        }

        // For "Open in Chrome" from the context menu in FullscreenActivity we want to bypass
        // CustomTab, and this flag ensures we open in TabbedChrome.
        chromeIntent.putExtra(LaunchIntentDispatcher.EXTRA_IS_ALLOWED_TO_RETURN_TO_PARENT, false);

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
            Context context = mTab.getActivity();
            if (context == null) context = applicationContext;
            context.startActivity(chromeIntent);
            activityStarted = true;
        }
    }

    @Override
    public void onOpenInNewChromeTabFromCCT(String linkUrl, boolean isIncognito) {
        Intent intent = new Intent(Intent.ACTION_VIEW, Uri.parse(linkUrl));
        intent.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        intent.setClass(mTab.getApplicationContext(), ChromeLauncherActivity.class);
        intent.putExtra(LaunchIntentDispatcher.EXTRA_IS_ALLOWED_TO_RETURN_TO_PARENT, false);
        if (isIncognito) {
            intent.putExtra(IntentHandler.EXTRA_OPEN_NEW_INCOGNITO_TAB, true);
            intent.putExtra(
                    Browser.EXTRA_APPLICATION_ID, mTab.getApplicationContext().getPackageName());
            IntentHandler.addTrustedIntentExtras(intent);
            IntentHandler.setTabLaunchType(intent, TabLaunchType.FROM_EXTERNAL_APP);
        }
        IntentUtils.safeStartActivity(mTab.getActivity(), intent);
    }

    @Override
    public String getTitleForOpenTabInExternalApp() {
        return DefaultBrowserInfo.getTitleOpenInDefaultBrowser(false);
    }

    @Override
    public void onOpenInDefaultBrowser(String url) {
        Intent intent = new Intent(Intent.ACTION_VIEW, Uri.parse(url));
        CustomTabsIntent.setAlwaysUseBrowserUI(intent);
        IntentUtils.safeStartActivity(mTab.getActivity(), intent);
    }

    @Override
    public void onOpenInEphemeralTab(String url, Referrer referrer) {
        EphemeralTab.onOpen(url, referrer, isIncognito());
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
