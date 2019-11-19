// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.contextmenu;

import android.content.Context;
import android.net.MailTo;
import android.text.TextUtils;
import android.util.Pair;
import android.view.ContextMenu;
import android.webkit.MimeTypeMap;
import android.webkit.URLUtil;

import androidx.annotation.IntDef;

import org.chromium.base.library_loader.LibraryProcessType;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeFeatureList;
import org.chromium.chrome.browser.compositor.bottombar.ephemeraltab.EphemeralTabPanel;
import org.chromium.chrome.browser.contextmenu.ChromeContextMenuItem.Item;
import org.chromium.chrome.browser.firstrun.FirstRunStatus;
import org.chromium.chrome.browser.locale.LocaleManager;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.SharedPreferencesManager;
import org.chromium.chrome.browser.search_engines.TemplateUrlServiceFactory;
import org.chromium.chrome.browser.share.LensUtils;
import org.chromium.chrome.browser.share.ShareDelegate;
import org.chromium.chrome.browser.share.ShareParams;
import org.chromium.chrome.browser.util.UrlUtilities;
import org.chromium.components.search_engines.TemplateUrlService;
import org.chromium.components.url_formatter.UrlFormatter;
import org.chromium.content_public.browser.BrowserStartupController;
import org.chromium.content_public.common.ContentUrlConstants;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.net.URI;
import java.net.URISyntaxException;
import java.util.ArrayList;
import java.util.List;

/**
 * A {@link ContextMenuPopulator} used for showing the default Chrome context menu.
 */
public class ChromeContextMenuPopulator implements ContextMenuPopulator {
    private static final String TAG = "CCMenuPopulator";
    private final ContextMenuItemDelegate mDelegate;
    private final @ContextMenuMode int mMode;
    private final ShareDelegate mShareDelegate;

    /**
     * Defines the Groups of each Context Menu Item
     */
    @IntDef({ContextMenuGroup.LINK, ContextMenuGroup.IMAGE, ContextMenuGroup.VIDEO})
    @Retention(RetentionPolicy.SOURCE)
    public @interface ContextMenuGroup {
        int LINK = 0;
        int IMAGE = 1;
        int VIDEO = 2;
    }

    /**
     * Defines the context menu modes
     */
    @IntDef({ContextMenuMode.NORMAL, ContextMenuMode.CUSTOM_TAB, ContextMenuMode.WEB_APP})
    @Retention(RetentionPolicy.SOURCE)
    public @interface ContextMenuMode {
        int NORMAL = 0; /* Default mode*/
        int CUSTOM_TAB = 1; /* Custom tab mode */
        int WEB_APP = 2; /* Full screen mode */
    }

    static class ContextMenuUma {
        // Note: these values must match the ContextMenuOptionAndroid enum in enums.xml.
        // Only add values to the end, right before NUM_ENTRIES!
        @IntDef({Action.OPEN_IN_NEW_TAB, Action.OPEN_IN_INCOGNITO_TAB, Action.COPY_LINK_ADDRESS,
                Action.COPY_EMAIL_ADDRESS, Action.COPY_LINK_TEXT, Action.SAVE_LINK,
                Action.SAVE_IMAGE, Action.OPEN_IMAGE, Action.OPEN_IMAGE_IN_NEW_TAB,
                Action.SEARCH_BY_IMAGE, Action.LOAD_ORIGINAL_IMAGE, Action.SAVE_VIDEO,
                Action.SHARE_IMAGE, Action.OPEN_IN_OTHER_WINDOW, Action.SEND_EMAIL,
                Action.ADD_TO_CONTACTS, Action.CALL, Action.SEND_TEXT_MESSAGE,
                Action.COPY_PHONE_NUMBER, Action.OPEN_IN_NEW_CHROME_TAB,
                Action.OPEN_IN_CHROME_INCOGNITO_TAB, Action.OPEN_IN_BROWSER, Action.OPEN_IN_CHROME,
                Action.SHARE_LINK, Action.OPEN_IN_EPHEMERAL_TAB, Action.OPEN_IMAGE_IN_EPHEMERAL_TAB,
                Action.DIRECT_SHARE_LINK, Action.DIRECT_SHARE_IMAGE,
                Action.SEARCH_WITH_GOOGLE_LENS})
        @Retention(RetentionPolicy.SOURCE)
        public @interface Action {
            int OPEN_IN_NEW_TAB = 0;
            int OPEN_IN_INCOGNITO_TAB = 1;
            int COPY_LINK_ADDRESS = 2;
            int COPY_EMAIL_ADDRESS = 3;
            int COPY_LINK_TEXT = 4;
            int SAVE_LINK = 5;
            int SAVE_IMAGE = 6;
            int OPEN_IMAGE = 7;
            int OPEN_IMAGE_IN_NEW_TAB = 8;
            int SEARCH_BY_IMAGE = 9;
            int LOAD_ORIGINAL_IMAGE = 10;
            int SAVE_VIDEO = 11;
            int SHARE_IMAGE = 12;
            int OPEN_IN_OTHER_WINDOW = 13;
            int SEND_EMAIL = 14;
            int ADD_TO_CONTACTS = 15;
            int CALL = 16;
            int SEND_TEXT_MESSAGE = 17;
            int COPY_PHONE_NUMBER = 18;
            int OPEN_IN_NEW_CHROME_TAB = 19;
            int OPEN_IN_CHROME_INCOGNITO_TAB = 20;
            int OPEN_IN_BROWSER = 21;
            int OPEN_IN_CHROME = 22;
            int SHARE_LINK = 23;
            int OPEN_IN_EPHEMERAL_TAB = 24;
            int OPEN_IMAGE_IN_EPHEMERAL_TAB = 25;

            // These are used to record DirectShare histograms in RevampedContextMenuCoordinator and
            // aren't used in onItemSelected.
            int DIRECT_SHARE_LINK = 26;
            int DIRECT_SHARE_IMAGE = 27;

            int SEARCH_WITH_GOOGLE_LENS = 28;

            int NUM_ENTRIES = 29;
        }

        // Note: these values must match the ContextMenuSaveLinkType enum in enums.xml.
        // Only add new values at the end, right before NUM_TYPES. We depend on these specific
        // values in UMA histograms.
        @IntDef({Type.UNKNOWN, Type.TEXT, Type.IMAGE, Type.AUDIO, Type.VIDEO, Type.PDF})
        @Retention(RetentionPolicy.SOURCE)
        public @interface Type {
            int UNKNOWN = 0;
            int TEXT = 1;
            int IMAGE = 2;
            int AUDIO = 3;
            int VIDEO = 4;
            int PDF = 5;
            int NUM_ENTRIES = 6;
        }

        // Note: these values must match the ContextMenuSaveImage enum in enums.xml.
        // Only add new values at the end, right before NUM_ENTRIES.
        @IntDef({TypeSaveImage.LOADED, TypeSaveImage.NOT_DOWNLOADABLE,
                TypeSaveImage.DISABLED_AND_IS_NOT_IMAGE_PARAM,
                TypeSaveImage.DISABLED_AND_IS_IMAGE_PARAM, TypeSaveImage.SHOWN})
        @Retention(RetentionPolicy.SOURCE)
        public @interface TypeSaveImage {
            int LOADED = 0;
            // int FETCHED_LOFI = 1; deprecated
            int NOT_DOWNLOADABLE = 2;
            int DISABLED_AND_IS_NOT_IMAGE_PARAM = 3;
            int DISABLED_AND_IS_IMAGE_PARAM = 4;
            int SHOWN = 5;
            int NUM_ENTRIES = 6;
        }

        /**
         * Records a histogram entry when the user selects an item from a context menu.
         * @param params The ContextMenuParams describing the current context menu.
         * @param action The action that the user selected (e.g. ACTION_SAVE_IMAGE).
         */
        static void record(ContextMenuParams params, @Action int action) {
            String histogramName;
            if (params.isVideo()) {
                histogramName = "ContextMenu.SelectedOptionAndroid.Video";
            } else if (params.isImage()) {
                histogramName = params.isAnchor() ? "ContextMenu.SelectedOptionAndroid.ImageLink"
                                                  : "ContextMenu.SelectedOptionAndroid.Image";
            } else {
                assert params.isAnchor();
                histogramName = "ContextMenu.SelectedOptionAndroid.Link";
            }
            RecordHistogram.recordEnumeratedHistogram(histogramName, action, Action.NUM_ENTRIES);
        }

        /**
         * Records the content types when user downloads the file by long pressing the
         * save link context menu option.
         */
        static void recordSaveLinkTypes(String url) {
            String extension = MimeTypeMap.getFileExtensionFromUrl(url);
            @Type
            int mimeType = Type.UNKNOWN;
            if (extension != null) {
                String type = MimeTypeMap.getSingleton().getMimeTypeFromExtension(extension);
                if (type != null) {
                    if (type.startsWith("text")) {
                        mimeType = Type.TEXT;
                    } else if (type.startsWith("image")) {
                        mimeType = Type.IMAGE;
                    } else if (type.startsWith("audio")) {
                        mimeType = Type.AUDIO;
                    } else if (type.startsWith("video")) {
                        mimeType = Type.VIDEO;
                    } else if (type.equals("application/pdf")) {
                        mimeType = Type.PDF;
                    }
                }
            }
            RecordHistogram.recordEnumeratedHistogram(
                    "ContextMenu.SaveLinkType", mimeType, Type.NUM_ENTRIES);
        }

        /**
         * Helper method to record MobileDownload.ContextMenu.SaveImage UMA
         * @param type Type to record
         */
        static void recordSaveImageUma(int type) {
            RecordHistogram.recordEnumeratedHistogram(
                    "MobileDownload.ContextMenu.SaveImage", type, TypeSaveImage.NUM_ENTRIES);
        }

        // Note: these values must match the ContextMenuLensSupportStatus enum in enums.xml.
        // Only add new values at the end, right before NUM_ENTRIES.
        @IntDef({LensSupportStatus.LENS_SUPPORTED, LensSupportStatus.NON_GOOGLE_SEARCH_ENGINE,
                LensSupportStatus.ACTIVITY_NOT_ACCESSIBLE, LensSupportStatus.OUT_OF_DATE,
                LensSupportStatus.SEARCH_BY_IMAGE_UNAVAILABLE, LensSupportStatus.LEGACY_OS})
        @Retention(RetentionPolicy.SOURCE)
        public @interface LensSupportStatus {
            int LENS_SUPPORTED = 0;
            int NON_GOOGLE_SEARCH_ENGINE = 1;
            int ACTIVITY_NOT_ACCESSIBLE = 2;
            int OUT_OF_DATE = 3;
            int SEARCH_BY_IMAGE_UNAVAILABLE = 4;
            int LEGACY_OS = 5;
            int NUM_ENTRIES = 6;
        }

        /**
         * Helper method to keep track of cases where the Lens app was not supported.
         */
        static void recordLensSupportStatus(@LensSupportStatus int reason) {
            RecordHistogram.recordEnumeratedHistogram(
                    "ContextMenu.LensSupportStatus", reason, LensSupportStatus.NUM_ENTRIES);
        }
    }

    /**
     * Builds a {@link ChromeContextMenuPopulator}.
     * @param delegate The {@link ContextMenuItemDelegate} that will be notified with actions
     *                 to perform when menu items are selected.
     * @param shareDelegate The {@link ShareDelegate} that will be notified when a share action is
     *                      performed.
     * @param mode Defines the context menu mode
     */
    public ChromeContextMenuPopulator(ContextMenuItemDelegate delegate, ShareDelegate shareDelegate,
            @ContextMenuMode int mode) {
        mDelegate = delegate;
        mShareDelegate = shareDelegate;
        mMode = mode;
    }

    @Override
    public void onDestroy() {
        mDelegate.onDestroy();
    }

    /**
     * Gets the link of the item or the alternate text of an image.
     * @return A string with either the link or with the alternate text.
     */
    public static String createHeaderText(ContextMenuParams params) {
        if (!isEmptyUrl(params.getLinkUrl())) {
            return getUrlText(params);
        } else if (!TextUtils.isEmpty(params.getTitleText())) {
            return params.getTitleText();
        }
        return "";
    }

    /**
     * Gets the link of the item or empty text if the Url is empty.
     * @return A string with the link or an empty string.
     */
    public static String createUrlText(ContextMenuParams params) {
        if (!isEmptyUrl(params.getLinkUrl())) {
            return getUrlText(params);
        }
        return "";
    }

    private static String getUrlText(ContextMenuParams params) {
        // The context menu can be created without native library
        // being loaded. Only use native URL formatting methods
        // if the native libraries have been loaded.
        if (BrowserStartupController.get(LibraryProcessType.PROCESS_BROWSER)
                        .isFullBrowserStarted()) {
            return UrlFormatter.formatUrlForDisplayOmitHTTPScheme(params.getLinkUrl());
        }
        return params.getLinkUrl();
    }

    @Override
    public List<Pair<Integer, List<ContextMenuItem>>> buildContextMenu(
            ContextMenu menu, Context context, ContextMenuParams params) {
        boolean hasSaveImage = false;

        // clang-format off
        List<Pair<Integer, List<ContextMenuItem>>> groupedItems = new ArrayList<>();
        // clang-format on

        if (params.isAnchor()) {
            List<ContextMenuItem> linkTab = new ArrayList<>();
            if (FirstRunStatus.getFirstRunFlowComplete() && mMode == ContextMenuMode.NORMAL
                    && !isEmptyUrl(params.getUrl())
                    && UrlUtilities.isAcceptedScheme(params.getUrl())) {
                linkTab.add(new ChromeContextMenuItem(Item.OPEN_IN_NEW_TAB));
                if (!mDelegate.isIncognito() && mDelegate.isIncognitoSupported()) {
                    linkTab.add(new ChromeContextMenuItem(Item.OPEN_IN_INCOGNITO_TAB));
                }
                if (mDelegate.isOpenInOtherWindowSupported()) {
                    linkTab.add(new ChromeContextMenuItem(Item.OPEN_IN_OTHER_WINDOW));
                }
                if (EphemeralTabPanel.isSupported()) {
                    linkTab.add(new ChromeContextMenuItem(Item.OPEN_IN_EPHEMERAL_TAB));
                }
            }
            if (!MailTo.isMailTo(params.getLinkUrl())
                    && !UrlUtilities.isTelScheme(params.getLinkUrl())) {
                linkTab.add(new ChromeContextMenuItem(Item.COPY_LINK_ADDRESS));
                if (!params.getLinkText().trim().isEmpty() && !params.isImage()) {
                    linkTab.add(new ChromeContextMenuItem(Item.COPY_LINK_TEXT));
                }
            }
            if (FirstRunStatus.getFirstRunFlowComplete()) {
                if (!mDelegate.isIncognito()
                        && UrlUtilities.isDownloadableScheme(params.getLinkUrl())) {
                    linkTab.add(new ChromeContextMenuItem(Item.SAVE_LINK_AS));
                }
                linkTab.add(new ShareContextMenuItem(
                        R.string.contextmenu_share_link, R.id.contextmenu_share_link, true));
                if (UrlUtilities.isTelScheme(params.getLinkUrl())) {
                    if (mDelegate.supportsCall()) {
                        linkTab.add(new ChromeContextMenuItem(Item.CALL));
                    }
                    if (mDelegate.supportsSendTextMessage()) {
                        linkTab.add(new ChromeContextMenuItem(Item.SEND_MESSAGE));
                    }
                    if (mDelegate.supportsAddToContacts()) {
                        linkTab.add(new ChromeContextMenuItem(Item.ADD_TO_CONTACTS));
                    }
                }
                if (MailTo.isMailTo(params.getLinkUrl())) {
                    if (mDelegate.supportsSendEmailMessage()) {
                        linkTab.add(new ChromeContextMenuItem(Item.SEND_MESSAGE));
                    }
                    if (!TextUtils.isEmpty(MailTo.parse(params.getLinkUrl()).getTo())
                            && mDelegate.supportsAddToContacts()) {
                        linkTab.add(new ChromeContextMenuItem(Item.ADD_TO_CONTACTS));
                    }
                }
            }
            if (UrlUtilities.isTelScheme(params.getLinkUrl())
                    || MailTo.isMailTo(params.getLinkUrl())) {
                linkTab.add(new ChromeContextMenuItem(Item.COPY));
            }
            if (!linkTab.isEmpty()) {
                groupedItems.add(new Pair<>(R.string.contextmenu_link_title, linkTab));
            }
        }

        if (params.isImage() && FirstRunStatus.getFirstRunFlowComplete()) {
            List<ContextMenuItem> imageTab = new ArrayList<>();
            boolean isSrcDownloadableScheme = UrlUtilities.isDownloadableScheme(params.getSrcUrl());

                // Avoid showing open image option for same image which is already opened.
                if (mMode == ContextMenuMode.CUSTOM_TAB
                        && !mDelegate.getPageUrl().equals(params.getSrcUrl())) {
                    imageTab.add(new ChromeContextMenuItem(Item.OPEN_IMAGE));
                }
                if (mMode == ContextMenuMode.NORMAL) {
                    imageTab.add(new ChromeContextMenuItem(Item.OPEN_IMAGE_IN_NEW_TAB));
                }
                if (EphemeralTabPanel.isSupported()) {
                    imageTab.add(new ChromeContextMenuItem(Item.OPEN_IMAGE_IN_EPHEMERAL_TAB));
                }
                if (isSrcDownloadableScheme) {
                    imageTab.add(new ChromeContextMenuItem(Item.SAVE_IMAGE));
                    hasSaveImage = true;
                }
                if (mMode == ContextMenuMode.CUSTOM_TAB || mMode == ContextMenuMode.NORMAL) {
                    final TemplateUrlService templateUrlServiceInstance = getTemplateUrlService();
                    final boolean enableGoogleLensFeature = ChromeFeatureList.isEnabled(
                            ChromeFeatureList.CONTEXT_MENU_SEARCH_WITH_GOOGLE_LENS);
                    if (isSrcDownloadableScheme && templateUrlServiceInstance.isLoaded()
                            && templateUrlServiceInstance.isSearchByImageAvailable()
                            && templateUrlServiceInstance.getDefaultSearchEngineTemplateUrl()
                                    != null
                            && !LocaleManager.getInstance().needToCheckForSearchEnginePromo()) {
                        // All behavior relating to Lens integration is gated by Feature Flag.
                        if (enableGoogleLensFeature
                                && shouldShowLensMenuItemAndRecordMetrics(
                                        context, templateUrlServiceInstance)) {
                            imageTab.add(new ChromeContextMenuItem(Item.SEARCH_WITH_GOOGLE_LENS));
                        } else {
                            imageTab.add(new ChromeContextMenuItem(Item.SEARCH_BY_IMAGE));
                        }
                    } else if (enableGoogleLensFeature) {
                        ContextMenuUma.recordLensSupportStatus(
                                ContextMenuUma.LensSupportStatus.SEARCH_BY_IMAGE_UNAVAILABLE);
                    }
                }
                imageTab.add(new ShareContextMenuItem(
                        R.string.contextmenu_share_image, R.id.contextmenu_share_image, false));

                recordSaveImageContextMenuResult(isSrcDownloadableScheme);
                groupedItems.add(new Pair<>(R.string.contextmenu_image_title, imageTab));
        }

        if (params.isVideo() && FirstRunStatus.getFirstRunFlowComplete() && params.canSaveMedia()
                && UrlUtilities.isDownloadableScheme(params.getSrcUrl())) {
            List<ContextMenuItem> videoTab = new ArrayList<>();
            videoTab.add(new ChromeContextMenuItem(Item.SAVE_VIDEO));
            groupedItems.add(new Pair<>(R.string.contextmenu_video_title, videoTab));
        }

        if (mMode != ContextMenuMode.NORMAL && FirstRunStatus.getFirstRunFlowComplete()) {
            List<ContextMenuItem> tab = groupedItems.isEmpty()
                    ? new ArrayList<>()
                    : groupedItems
                              .get(mMode == ContextMenuMode.CUSTOM_TAB ? 0
                                                                       : groupedItems.size() - 1)
                              .second;
            if (mMode == ContextMenuMode.WEB_APP) {
                tab.add(new ChromeContextMenuItem(Item.OPEN_IN_CHROME));
            } else if (mMode == ContextMenuMode.CUSTOM_TAB) {
                boolean addNewEntries = false;
                try {
                    URI uri = new URI(params.getUrl());
                    if (!UrlUtilities.isInternalScheme(uri) && !isEmptyUrl(params.getUrl())) {
                        addNewEntries = true;
                    }
                } catch (URISyntaxException ignore) {
                }
                if (SharedPreferencesManager.getInstance().readBoolean(
                            ChromePreferenceKeys.CHROME_DEFAULT_BROWSER, false)
                        && addNewEntries) {
                    if (mDelegate.isIncognitoSupported()) {
                        tab.add(0, new ChromeContextMenuItem(Item.OPEN_IN_CHROME_INCOGNITO_TAB));
                    }
                    tab.add(0, new ChromeContextMenuItem(Item.OPEN_IN_NEW_CHROME_TAB));
                } else if (addNewEntries) {
                    tab.add(0, new ChromeContextMenuItem(Item.OPEN_IN_BROWSER_ID));
                }
            }
            if (groupedItems.isEmpty() && !tab.isEmpty()) {
                groupedItems.add(new Pair<>(R.string.contextmenu_link_title, tab));
            }
        }

        if (!groupedItems.isEmpty()
                && BrowserStartupController.get(LibraryProcessType.PROCESS_BROWSER)
                           .isFullBrowserStarted()) {
            if (!hasSaveImage) {
                ContextMenuUma.recordSaveImageUma(params.isImage()
                                ? ContextMenuUma.TypeSaveImage.DISABLED_AND_IS_IMAGE_PARAM
                                : ContextMenuUma.TypeSaveImage.DISABLED_AND_IS_NOT_IMAGE_PARAM);
            } else {
                ContextMenuUma.recordSaveImageUma(ContextMenuUma.TypeSaveImage.SHOWN);
            }
        }

        return groupedItems;
    }

    @Override
    public boolean onItemSelected(ContextMenuHelper helper, ContextMenuParams params, int itemId) {
        if (itemId == R.id.contextmenu_open_in_new_tab) {
            ContextMenuUma.record(params, ContextMenuUma.Action.OPEN_IN_NEW_TAB);
            mDelegate.onOpenInNewTab(params.getUrl(), params.getReferrer());
        } else if (itemId == R.id.contextmenu_open_in_incognito_tab) {
            ContextMenuUma.record(params, ContextMenuUma.Action.OPEN_IN_INCOGNITO_TAB);
            mDelegate.onOpenInNewIncognitoTab(params.getUrl());
        } else if (itemId == R.id.contextmenu_open_in_other_window) {
            ContextMenuUma.record(params, ContextMenuUma.Action.OPEN_IN_OTHER_WINDOW);
            mDelegate.onOpenInOtherWindow(params.getUrl(), params.getReferrer());
        } else if (itemId == R.id.contextmenu_open_in_ephemeral_tab) {
            ContextMenuUma.record(params, ContextMenuUma.Action.OPEN_IN_EPHEMERAL_TAB);
            mDelegate.onOpenInEphemeralTab(params.getUrl(), params.getLinkText());
        } else if (itemId == R.id.contextmenu_open_image) {
            ContextMenuUma.record(params, ContextMenuUma.Action.OPEN_IMAGE);
            mDelegate.onOpenImageUrl(params.getSrcUrl(), params.getReferrer());
        } else if (itemId == R.id.contextmenu_open_image_in_new_tab) {
            ContextMenuUma.record(params, ContextMenuUma.Action.OPEN_IMAGE_IN_NEW_TAB);
            mDelegate.onOpenImageInNewTab(params.getSrcUrl(), params.getReferrer());
        } else if (itemId == R.id.contextmenu_open_image_in_ephemeral_tab) {
            ContextMenuUma.record(params, ContextMenuUma.Action.OPEN_IMAGE_IN_EPHEMERAL_TAB);
            String title = params.getTitleText();
            if (TextUtils.isEmpty(title)) {
                title = URLUtil.guessFileName(params.getSrcUrl(), null, null);
            }
            mDelegate.onOpenInEphemeralTab(params.getSrcUrl(), title);
        } else if (itemId == R.id.contextmenu_copy_link_address) {
            ContextMenuUma.record(params, ContextMenuUma.Action.COPY_LINK_ADDRESS);
            mDelegate.onSaveToClipboard(
                    params.getUnfilteredLinkUrl(), ContextMenuItemDelegate.ClipboardType.LINK_URL);
        } else if (itemId == R.id.contextmenu_call) {
            ContextMenuUma.record(params, ContextMenuUma.Action.CALL);
            mDelegate.onCall(params.getLinkUrl());
        } else if (itemId == R.id.contextmenu_send_message) {
            if (MailTo.isMailTo(params.getLinkUrl())) {
                ContextMenuUma.record(params, ContextMenuUma.Action.SEND_EMAIL);
                mDelegate.onSendEmailMessage(params.getLinkUrl());
            } else if (UrlUtilities.isTelScheme(params.getLinkUrl())) {
                ContextMenuUma.record(params, ContextMenuUma.Action.SEND_TEXT_MESSAGE);
                mDelegate.onSendTextMessage(params.getLinkUrl());
            }
        } else if (itemId == R.id.contextmenu_add_to_contacts) {
            ContextMenuUma.record(params, ContextMenuUma.Action.ADD_TO_CONTACTS);
            mDelegate.onAddToContacts(params.getLinkUrl());
        } else if (itemId == R.id.contextmenu_copy) {
            if (MailTo.isMailTo(params.getLinkUrl())) {
                ContextMenuUma.record(params, ContextMenuUma.Action.COPY_EMAIL_ADDRESS);
                mDelegate.onSaveToClipboard(MailTo.parse(params.getLinkUrl()).getTo(),
                        ContextMenuItemDelegate.ClipboardType.LINK_URL);
            } else if (UrlUtilities.isTelScheme(params.getLinkUrl())) {
                ContextMenuUma.record(params, ContextMenuUma.Action.COPY_PHONE_NUMBER);
                mDelegate.onSaveToClipboard(UrlUtilities.getTelNumber(params.getLinkUrl()),
                        ContextMenuItemDelegate.ClipboardType.LINK_URL);
            }
        } else if (itemId == R.id.contextmenu_copy_link_text) {
            ContextMenuUma.record(params, ContextMenuUma.Action.COPY_LINK_TEXT);
            mDelegate.onSaveToClipboard(
                    params.getLinkText(), ContextMenuItemDelegate.ClipboardType.LINK_TEXT);
        } else if (itemId == R.id.contextmenu_save_image) {
            ContextMenuUma.record(params, ContextMenuUma.Action.SAVE_IMAGE);
            if (mDelegate.startDownload(params.getSrcUrl(), false)) {
                helper.startContextMenuDownload(
                        false, mDelegate.isDataReductionProxyEnabledForURL(params.getSrcUrl()));
            }
        } else if (itemId == R.id.contextmenu_save_video) {
            ContextMenuUma.record(params, ContextMenuUma.Action.SAVE_VIDEO);
            if (mDelegate.startDownload(params.getSrcUrl(), false)) {
                helper.startContextMenuDownload(false, false);
            }
        } else if (itemId == R.id.contextmenu_save_link_as) {
            ContextMenuUma.record(params, ContextMenuUma.Action.SAVE_LINK);
            String url = params.getUnfilteredLinkUrl();
            if (mDelegate.startDownload(url, true)) {
                ContextMenuUma.recordSaveLinkTypes(url);
                helper.startContextMenuDownload(true, false);
            }
        } else if (itemId == R.id.contextmenu_share_link) {
            ContextMenuUma.record(params, ContextMenuUma.Action.SHARE_LINK);
            ShareParams linkShareParams =
                    new ShareParams.Builder(helper.getWindow(), params.getUrl(), params.getUrl())
                            .setShareDirectly(false)
                            .setSaveLastUsed(true)
                            .build();
            mShareDelegate.share(linkShareParams);
        } else if (itemId == R.id.contextmenu_search_with_google_lens) {
            ContextMenuUma.record(params, ContextMenuUma.Action.SEARCH_WITH_GOOGLE_LENS);
            helper.searchWithGoogleLens(mDelegate.isIncognito());
        } else if (itemId == R.id.contextmenu_search_by_image) {
            ContextMenuUma.record(params, ContextMenuUma.Action.SEARCH_BY_IMAGE);
            helper.searchForImage();
        } else if (itemId == R.id.contextmenu_share_image) {
            ContextMenuUma.record(params, ContextMenuUma.Action.SHARE_IMAGE);
            helper.shareImage();
        } else if (itemId == R.id.contextmenu_open_in_chrome) {
            ContextMenuUma.record(params, ContextMenuUma.Action.OPEN_IN_CHROME);
            mDelegate.onOpenInChrome(params.getUrl(), params.getPageUrl());
        } else if (itemId == R.id.contextmenu_open_in_new_chrome_tab) {
            ContextMenuUma.record(params, ContextMenuUma.Action.OPEN_IN_NEW_CHROME_TAB);
            mDelegate.onOpenInNewChromeTabFromCCT(params.getUrl(), false);
        } else if (itemId == R.id.contextmenu_open_in_chrome_incognito_tab) {
            ContextMenuUma.record(params, ContextMenuUma.Action.OPEN_IN_CHROME_INCOGNITO_TAB);
            mDelegate.onOpenInNewChromeTabFromCCT(params.getUrl(), true);
        } else if (itemId == R.id.contextmenu_open_in_browser_id) {
            ContextMenuUma.record(params, ContextMenuUma.Action.OPEN_IN_BROWSER);
            mDelegate.onOpenInDefaultBrowser(params.getUrl());
        } else {
            assert false;
        }

        return true;
    }

    /**
     * @return The service that handles TemplateUrls.
     */
    protected TemplateUrlService getTemplateUrlService() {
        return TemplateUrlServiceFactory.get();
    }

    /**
     * Checks whether a url is empty or blank.
     * @param url The url need to be checked.
     * @return True if the url is empty or "about:blank".
     */
    private static boolean isEmptyUrl(String url) {
        return TextUtils.isEmpty(url) || url.equals(ContentUrlConstants.ABOUT_BLANK_DISPLAY_URL);
    }

    /**
     * Record the UMA related to save image context menu option.
     * @param isDownloadableScheme The image is downloadable.
     */
    private void recordSaveImageContextMenuResult(boolean isDownloadableScheme) {
        if (!BrowserStartupController.get(LibraryProcessType.PROCESS_BROWSER)
                        .isFullBrowserStarted()) {
            return;
        }

        ContextMenuUma.recordSaveImageUma(ContextMenuUma.TypeSaveImage.LOADED);

        if (!isDownloadableScheme) {
            ContextMenuUma.recordSaveImageUma(ContextMenuUma.TypeSaveImage.NOT_DOWNLOADABLE);
        }
    }

    /**
     * Whether the lens menu item should be shown based on a set of application
     * compatibility checks.
     *
     * @param context The current application context
     * @param TemplateUrlService The service which identifies the current default search engine.
     * @return Whether to show the menu item
     */
    private boolean shouldShowLensMenuItemAndRecordMetrics(
            Context context, TemplateUrlService templateUrlServiceInstance) {
        String versionName = LensUtils.getLensActivityVersionNameIfAvailable(context);
        if (!templateUrlServiceInstance.isDefaultSearchEngineGoogle()) {
            ContextMenuUma.recordLensSupportStatus(
                    ContextMenuUma.LensSupportStatus.NON_GOOGLE_SEARCH_ENGINE);
            return false;
        }
        if (TextUtils.isEmpty(versionName)) {
            ContextMenuUma.recordLensSupportStatus(
                    ContextMenuUma.LensSupportStatus.ACTIVITY_NOT_ACCESSIBLE);
            return false;
        }
        if (LensUtils.isAgsaVersionBelowMinimum(versionName)) {
            ContextMenuUma.recordLensSupportStatus(ContextMenuUma.LensSupportStatus.OUT_OF_DATE);
            return false;
        }

        if (LensUtils.isDeviceOsBelowMinimum()) {
            ContextMenuUma.recordLensSupportStatus(ContextMenuUma.LensSupportStatus.LEGACY_OS);
            return false;
        }

        ContextMenuUma.recordLensSupportStatus(ContextMenuUma.LensSupportStatus.LENS_SUPPORTED);
        return true;
    }
}
