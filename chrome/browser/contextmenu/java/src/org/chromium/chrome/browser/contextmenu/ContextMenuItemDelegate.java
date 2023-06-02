// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.contextmenu;

import android.net.Uri;

import androidx.annotation.IntDef;
import androidx.annotation.Nullable;

import org.chromium.content_public.browser.Impression;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.common.Referrer;
import org.chromium.url.GURL;

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
     * @return The title of the current tab associated with this delegate..
     */
    String getPageTitle();

    /**
     * @return The web contents of the current tab owned by this delegate.
     */
    WebContents getWebContents();

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
     * @return Whether Chrome can get itself into multi-window mode.
     */
    boolean canEnterMultiWindowMode();

    /**
     * Called when the context menu is trying to start a download.
     * @param url Url of the download item.
     * @param isLink Whether or not the download is a link (as opposed to an image/video).
     * @return       Whether or not a download should actually be started.
     */
    boolean startDownload(GURL url, boolean isLink);

    /**
     * Called when the {@code url} should be opened in the other window with the same incognito
     * state as the current {@link Tab}.
     * @param url The URL to open.
     */
    void onOpenInOtherWindow(GURL url, Referrer referrer);

    /**
     * Called when the {@code url} should be opened in a new tab with the same incognito state as
     * the current {@link Tab}.
     * @param url The URL to open.
     * @param navigateToTab Whether or not to navigate to the new tab.
     * @param impression The attribution impression to associate with the navigation.
     */
    void onOpenInNewTab(
            GURL url, Referrer referrer, boolean navigateToTab, @Nullable Impression impression);

    /**
     * Called when {@code url} should be opened in a new tab in the same group as the current
     * {@link Tab}.
     * @param url The URL to open.
     */
    void onOpenInNewTabInGroup(GURL url, Referrer referrer);

    /**
     * Called when the {@code url} should be opened in a new incognito tab.
     * @param url The URL to open.
     */
    void onOpenInNewIncognitoTab(GURL url);

    /**
     * Called when the {@code url} is of an image and should be opened in the same tab.
     * @param url The image URL to open.
     */
    void onOpenImageUrl(GURL url, Referrer referrer);

    /**
     * Called when the {@code url} is of an image and should be opened in a new tab.
     * @param url The image URL to open.
     */
    void onOpenImageInNewTab(GURL url, Referrer referrer);

    /**
     * Called when the {@code text} should be saved to the clipboard.
     * @param text The text to save to the clipboard.
     * @param clipboardType The type of data in {@code text}.
     */
    void onSaveToClipboard(String text, @ClipboardType int clipboardType);

    /**
     * Called when the image should be saved to the clipboard.
     * @param Uri The (@link Uri) of the image to save to the clipboard.
     */
    void onSaveImageToClipboard(Uri uri);

    /**
     * @return whether an activity is available to handle an intent to call a phone number.
     */
    public boolean supportsCall();

    /**
     * Called when the {@code url} should be parsed to call a phone number.
     * @param url The URL to be parsed to call a phone number.
     */
    void onCall(GURL url);

    /**
     * @return whether an activity is available to handle an intent to send an email.
     */
    public boolean supportsSendEmailMessage();

    /**
     * Called when the {@code url} should be parsed to send an email.
     * @param url The URL to be parsed to send an email.
     */
    void onSendEmailMessage(GURL url);

    /**
     * @return whether an activity is available to handle an intent to send a text message.
     */
    public boolean supportsSendTextMessage();

    /**
     * Called when the {@code url} should be parsed to send a text message.
     * @param url The URL to be parsed to send a text message.
     */
    void onSendTextMessage(GURL url);

    /**
     * Returns whether or not an activity is available to handle intent to add contacts.
     * @return true if an activity is available to handle intent to add contacts.
     */
    public boolean supportsAddToContacts();

    /**
     * Called when the {@code url} should be parsed to add to contacts.
     * @param url The URL to be parsed to add to contacts.
     */
    void onAddToContacts(GURL url);

    /**
     * @return page url.
     */
    GURL getPageUrl();

    /**
     * Called when a link should be opened in the main Chrome browser.
     * @param linkUrl URL that should be opened.
     * @param pageUrl URL of the current page.
     */
    void onOpenInChrome(GURL linkUrl, GURL pageUrl);

    /**
     * Called when the {@code url} should be opened in a new Chrome tab from CCT.
     * @param linkUrl The URL to open.
     * @param isIncognito true if the {@code url} should be opened in a new incognito tab.
     */
    void onOpenInNewChromeTabFromCCT(GURL linkUrl, boolean isIncognito);

    /**
     * @return title of the context menu to open a page in external apps.
     */
    String getTitleForOpenTabInExternalApp();

    /**
     * Called when the current Chrome app is not the default to handle a View Intent.
     * @param url The URL to open.
     */
    void onOpenInDefaultBrowser(GURL url);

    /**
     * Called when the {@code url} should be opened in an ephemeral tab.
     * @param url The URL to open.
     * @param title The title text to show on top control.
     */
    void onOpenInEphemeralTab(GURL url, String title);

    /**
     * Called when Read Later was selected from the context menu.
     * @param url The URL to be saved to the reading list.
     * @param title The title text to be shown for this item in the reading list.
     */
    void onReadLater(GURL url, String title);
}
