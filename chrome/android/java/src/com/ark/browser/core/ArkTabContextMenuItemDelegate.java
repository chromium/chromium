// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.ark.browser.core;

import android.app.Activity;
import android.content.Context;
import android.content.Intent;
import android.net.MailTo;
import android.net.Uri;
import android.provider.Browser;
import android.provider.ContactsContract;
import android.text.TextUtils;
import android.widget.Toast;

import androidx.browser.customtabs.CustomTabsIntent;

import com.ark.browser.adblock.AdblockPlusHelper;
import com.ark.browser.event.LoadUrlEvent;
import com.ark.browser.tab.ArkTabImpl;
import com.ark.browser.tab.TabListManager;
import com.ark.browser.utils.ArkLogger;
import com.zpj.toast.ZToast;

import org.chromium.base.ContextUtils;
import org.chromium.base.IntentUtils;
import org.chromium.base.Log;
import org.chromium.base.PackageManagerUtils;
import org.chromium.chrome.browser.DefaultBrowserInfo;
import org.chromium.chrome.browser.IntentHandler;
import org.chromium.chrome.browser.contextmenu.ContextMenuItemDelegate;
import org.chromium.chrome.browser.document.ChromeLauncherActivity;
import org.chromium.chrome.browser.download.ChromeDownloadDelegate;
import org.chromium.chrome.browser.multiwindow.MultiWindowUtils;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tab.TabUtils;
import org.chromium.chrome.browser.tab.state.CriticalPersistedTabData;
import org.chromium.chrome.browser.tabmodel.document.TabDelegate;
import org.chromium.components.embedder_support.contextmenu.ContextMenuParams;
import org.chromium.components.embedder_support.util.UrlUtilities;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.common.Referrer;
import org.chromium.ui.base.Clipboard;
import org.chromium.ui.base.PageTransition;
import org.chromium.url.GURL;
import org.chromium.url.Origin;

/**
 * A default {@link ContextMenuItemDelegate} that supports the context menu functionality in Tab.
 */
public class ArkTabContextMenuItemDelegate implements ContextMenuItemDelegate {
    private final ArkTabImpl mTab;

    /**
     * Builds a {@link ArkTabContextMenuItemDelegate} instance.
     */
    public ArkTabContextMenuItemDelegate(Tab tab) {
        mTab = (ArkTabImpl) tab;
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
        return true;
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
    }

    @Override
    public void onSaveImageToClipboard(Uri uri) {
        Clipboard.getInstance().setImageUri(uri);
    }

    @Override
    public boolean supportsCall() {
        Intent intent = new Intent(Intent.ACTION_VIEW);
        intent.setData(Uri.parse("tel:"));
        return mTab.canResolveActivity(intent);
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
        return mTab.canResolveActivity(intent);
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
        return mTab.canResolveActivity(intent);
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
        return mTab.canResolveActivity(intent);
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
        Toast.makeText(ContextUtils.getApplicationContext(), "onOpenInNewTab", Toast.LENGTH_SHORT).show();


        LoadUrlParams params = new LoadUrlParams(url);
        params.setReferrer(referrer);
        LoadUrlEvent.post(params, true);

    }

    @Override
    public void onOpenInNewTabInGroup(GURL url, Referrer referrer) {
//        RecordUserAction.record("MobileNewTabOpened");
//        RecordUserAction.record("LinkOpenedInNewTab");
//        LoadUrlParams loadUrlParams = new LoadUrlParams(url.getSpec());
//        loadUrlParams.setReferrer(referrer);
//        mTabModelSelector.openNewTab(loadUrlParams,
//                TabLaunchType.FROM_LONGPRESS_BACKGROUND_IN_GROUP, mTab, isIncognito());
        Toast.makeText(ContextUtils.getApplicationContext(), "onOpenInNewTabInGroup", Toast.LENGTH_SHORT).show();
        LoadUrlParams params = new LoadUrlParams(url);
        params.setReferrer(referrer);
        LoadUrlEvent.post(params, true);
    }

    @Override
    public void onOpenInNewIncognitoTab(GURL url, Origin initiatorOrigin) {
        Toast.makeText(ContextUtils.getApplicationContext(), "TODO onOpenInNewIncognitoTab", Toast.LENGTH_SHORT).show();
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
        mTab.loadInNewPage(loadUrlParams);
    }

    @Override
    public void onOpenImageInNewTab(GURL url, Referrer referrer) {
        Toast.makeText(ContextUtils.getApplicationContext(), "onOpenImageInNewTab", Toast.LENGTH_SHORT).show();
        LoadUrlParams params = new LoadUrlParams(url);
        params.setReferrer(referrer);
        LoadUrlEvent.post(params, true);
    }

    @Override
    public void onOpenInEphemeralTab(GURL url, String title) {
        Toast.makeText(ContextUtils.getApplicationContext(),
                "onOpenInEphemeralTab title=" + title, Toast.LENGTH_SHORT).show();
    }

    @Override
    public void onReadLater(GURL url, String title) {
    }

    @Override
    public void onOpenInChrome(GURL linkUrl, GURL pageUrl) {
        Context applicationContext = ContextUtils.getApplicationContext();
        Intent chromeIntent = new Intent(Intent.ACTION_VIEW, Uri.parse(linkUrl.getSpec()));
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

    @Override
    public void onMarkAds(ContextMenuParams params) {
        if (mTab != null && mTab.getWebContents() != null) {
            String pageUrl = params.getPageUrl().getSpec();
            String srcUrl = params.getSrcUrl().getSpec();
            
            if (!markAd(params.getCssSelector())) {
                markAd(params.getParentCssSelector());
            }

            markAd(pageUrl, params.getTagName(), params.getIdAttribute(), params.getClassAttribute(), srcUrl);
            markAd(pageUrl, params.getParentTagName(), params.getParentIdAttribute(),
                    params.getParentClassAttribute(), srcUrl);
        }
    }

    private boolean markAd(String cssSelector) {
        if (!TextUtils.isEmpty(cssSelector)) {
            String js = AdblockPlusHelper.getAdblockJs(cssSelector);
            ArkLogger.e(this, "onMarkAds js=" + js);
            mTab.getWebContents().evaluateJavaScript(js, null);
            return true;
        }
        return false;
    }

    private void markAd(String pageUrl, String tag, String id, String classAttribute, String srcUrl) {
        if (!TextUtils.isEmpty(tag) && tag.toLowerCase().equals("body")) {
            return;
        }
        String js = AdblockPlusHelper.getAdblockJs(tag, id, srcUrl);
        Log.d("MarkAd", "js=" + js);
        if (!js.isEmpty()) {
            mTab.getWebContents().evaluateJavaScript(js, null);
            AdblockPlusHelper.appendMarkAsAd(ContextUtils.getApplicationContext(),
                    pageUrl, tag, classAttribute, id, srcUrl);
        }
    }

    @Override
    public boolean canMoveTab() {
        return mTab.getTabInfo().getPages().size() > 1;
    }

    @Override
    public void moveTab() {
        boolean r = TabListManager.moveToNewTab(mTab.getPageInfo());
        if (r) {
            ZToast.success("移动页面成功！");
        } else {
            ZToast.error("移动页面失败！");
        }
    }
}
