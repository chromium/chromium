// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.contextmenu;

import androidx.annotation.IntDef;

import org.chromium.chrome.browser.tab.Tab;
import org.chromium.content_public.common.Referrer;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * A delegate responsible for taking actions based on context menu selections.
 */
public interface ContextMenuItemDelegate {
    // The type of the data to save to the clipboard.
    @IntDef({ClipboardType.LINK_URL, ClipboardType.LINK_TEXT, ClipboardType.IMAGE_URL})
    @Retention(RetentionPolicy.SOURCE)
    public @interface ClipboardType {
        int LINK_URL = 0;
        int LINK_TEXT = 1;
        int IMAGE_URL = 2;
    }

    /**
     * Called when this ContextMenuItemDelegate is about to be destroyed.
     */
    void onDestroy();

    /**
     * @return Whether or not this context menu is being shown for an incognito content.
     */
    boolean isIncognito();

    /**
     * @return Whether or not the current application can show incognito tabs.
     */
    boolean isIncognitoSupported();

    /**
     * @return Whether the "Open in other window" context menu item should be shown.
     */
    boolean isOpenInOtherWindowSupported();

    /**
     * Returns whether or not the Data Reduction Proxy is enabled for input url.
     * @param url Input url to check for the Data Reduction Proxy setting.
     * @return true if the Data Reduction Proxy is enabled for the url.
    */
    boolean isDataReductionProxyEnabledForURL(String url);

    /**
     * Called when the context menu is trying to start a download.
     * @param url Url of the download item.
     * @param isLink Whether or not the download is a link (as opposed to an image/video).
     * @return       Whether or not a download should actually be started.
     */
    boolean startDownload(String url, boolean isLink);

    /**
     * Called when the {@code url} should be opened in the other window with the same incognito
     * state as the current {@link Tab}.
     * @param url The URL to open.
     */
    void onOpenInOtherWindow(String url, Referrer referrer);

    /**
     * Called when the {@code url} should be opened in a new tab with the same incognito state as
     * the current {@link Tab}.
     * @param url The URL to open.
     */
    void onOpenInNewTab(String url, Referrer referrer);

    /**
     * Called when the {@code url} should be opened in a new incognito tab.
     * @param url The URL to open.
     */
    void onOpenInNewIncognitoTab(String url);

    /**
     * Called when the {@code url} is of an image and should be opened in the same tab.
     * @param url The image URL to open.
     */
    void onOpenImageUrl(String url, Referrer referrer);

    /**
     * Called when the {@code url} is of an image and should be opened in a new tab.
     * @param url The image URL to open.
     */
    void onOpenImageInNewTab(String url, Referrer referrer);

    /**
     * Called when the original image should be loaded.
     */
    void onLoadOriginalImage();

    /**
     * Returns whether the load image has been requested on a Lo-Fi image for the current page load.
     * @return true if load image has been requested for the current page load.
     */
    boolean wasLoadOriginalImageRequestedForPageLoad();

    /**
     * Called when the {@code text} should be saved to the clipboard.
     * @param text The text to save to the clipboard.
     * @param clipboardType The type of data in {@code text}.
     */
    void onSaveToClipboard(String text, @ClipboardType int clipboardType);

    /**
     * @return whether an activity is available to handle an intent to call a phone number.
     */
    public boolean supportsCall();

    /**
     * Called when the {@code url} should be parsed to call a phone number.
     * @param url The URL to be parsed to call a phone number.
     */
    void onCall(String url);

    /**
     * @return whether an activity is available to handle an intent to send an email.
     */
    public boolean supportsSendEmailMessage();

    /**
     * Called when the {@code url} should be parsed to send an email.
     * @param url The URL to be parsed to send an email.
     */
    void onSendEmailMessage(String url);

    /**
     * @return whether an activity is available to handle an intent to send a text message.
     */
    public boolean supportsSendTextMessage();

    /**
     * Called when the {@code url} should be parsed to send a text message.
     * @param url The URL to be parsed to send a text message.
     */
    void onSendTextMessage(String url);

    /**
     * Returns whether or not an activity is available to handle intent to add contacts.
     * @return true if an activity is available to handle intent to add contacts.
     */
    public boolean supportsAddToContacts();

    /**
     * Called when the {@code url} should be parsed to add to contacts.
     * @param url The URL to be parsed to add to contacts.
     */
    void onAddToContacts(String url);

   /**
    * @return page url.
    */
    String getPageUrl();

    /**
     * Called when a link should be opened in the main Chrome browser.
     * @param linkUrl URL that should be opened.
     * @param pageUrl URL of the current page.
     */
    void onOpenInChrome(String linkUrl, String pageUrl);

    /**
     * Called when the {@code url} should be opened in a new Chrome tab from CCT.
     * @param linkUrl The URL to open.
     * @param isIncognito true if the {@code url} should be opened in a new incognito tab.
     */
    void onOpenInNewChromeTabFromCCT(String linkUrl, boolean isIncognito);

    /**
     * @return title of the context menu to open a page in external apps.
     */
    String getTitleForOpenTabInExternalApp();

    /**
     * Called when the current Chrome app is not the default to handle a View Intent.
     * @param url The URL to open.
     */
    void onOpenInDefaultBrowser(String url);

    /**
     * Called when the {@code url} should be opened in an ephemeral tab.
     * @param url The URL to open.
     * @param title The title text to show on top control.
     */
    void onOpenInEphemeralTab(String url, String title);
}
