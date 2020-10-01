// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.contextmenu;

import static org.chromium.chrome.browser.contextmenu.RevampedContextMenuItemProperties.MENU_ID;
import static org.chromium.chrome.browser.contextmenu.RevampedContextMenuItemProperties.TEXT;
import static org.chromium.chrome.browser.contextmenu.RevampedContextMenuShareItemProperties.CONTENT_DESC;
import static org.chromium.chrome.browser.contextmenu.RevampedContextMenuShareItemProperties.IMAGE;

import android.app.Activity;
import android.content.ContentResolver;
import android.content.Context;
import android.content.Intent;
import android.content.res.Resources;
import android.graphics.Bitmap;
import android.graphics.drawable.Drawable;
import android.net.MailTo;
import android.net.Uri;
import android.text.TextUtils;
import android.util.Pair;
import android.webkit.MimeTypeMap;
import android.webkit.URLUtil;

import androidx.annotation.IntDef;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.Callback;
import org.chromium.base.ContextUtils;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.compositor.bottombar.ephemeraltab.EphemeralTabCoordinator;
import org.chromium.chrome.browser.contextmenu.ChromeContextMenuItem.Item;
import org.chromium.chrome.browser.contextmenu.RevampedContextMenuCoordinator.ListItemType;
import org.chromium.chrome.browser.externalauth.ExternalAuthUtils;
import org.chromium.chrome.browser.feature_engagement.TrackerFactory;
import org.chromium.chrome.browser.firstrun.FirstRunStatus;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.gsa.GSAState;
import org.chromium.chrome.browser.locale.LocaleManager;
import org.chromium.chrome.browser.metrics.UkmRecorder;
import org.chromium.chrome.browser.performance_hints.PerformanceHintsObserver;
import org.chromium.chrome.browser.performance_hints.PerformanceHintsObserver.PerformanceClass;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.SharedPreferencesManager;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.search_engines.TemplateUrlServiceFactory;
import org.chromium.chrome.browser.share.ChromeShareExtras;
import org.chromium.chrome.browser.share.LensUtils;
import org.chromium.chrome.browser.share.ShareDelegate;
import org.chromium.chrome.browser.share.ShareHelper;
import org.chromium.components.browser_ui.share.ShareImageFileUtils;
import org.chromium.components.browser_ui.share.ShareParams;
import org.chromium.components.embedder_support.contextmenu.ContextMenuParams;
import org.chromium.components.embedder_support.util.UrlUtilities;
import org.chromium.components.feature_engagement.FeatureConstants;
import org.chromium.components.feature_engagement.Tracker;
import org.chromium.components.search_engines.TemplateUrlService;
import org.chromium.components.url_formatter.UrlFormatter;
import org.chromium.content_public.browser.BrowserStartupController;
import org.chromium.content_public.browser.RenderFrameHost;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.common.ContentUrlConstants;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.url.URI;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.net.URISyntaxException;
import java.util.ArrayList;
import java.util.Collections;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

/**
 * A {@link ContextMenuPopulator} used for showing the default Chrome context menu.
 */
public class ChromeContextMenuPopulator implements ContextMenuPopulator {
    private static final int MAX_SHARE_DIMEN_PX = 2048;

    private final Context mContext;
    private final ContextMenuItemDelegate mDelegate;
    private final @ContextMenuMode int mMode;
    private final Supplier<ShareDelegate> mShareDelegateSupplier;
    private final ExternalAuthUtils mExternalAuthUtils;
    private final ContextMenuParams mParams;
    private boolean mEnableLensWithSearchByImageText;
    private @Nullable UkmRecorder.Bridge mUkmRecorderBridge;
    private long mNativeChromeContextMenuPopulator;
    private static final String LENS_SEARCH_MENU_ITEM_KEY = "searchWithGoogleLensMenuItem";
    private static final String LENS_SHOP_MENU_ITEM_KEY = "shopWithGoogleLensMenuItem";
    private static final String SEARCH_BY_IMAGE_MENU_ITEM_KEY = "searchByImageMenuItem";

    // True when the tracker indicates IPH in the form of "new" label needs to be shown.
    private Boolean mShowEphemeralTabNewLabel;

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

    /**
     * See function for details.
     */
    private static byte[] sHardcodedImageBytesForTesting;
    private static String sHardcodedImageExtensionForTesting;

    /**
     * The tests trigger the context menu via JS rather than via a true native call which means
     * the native code does not have a reference to the image's render frame host. Instead allow
     * test cases to hardcode the test image bytes that will be shared.
     * @param hardcodedImageBytes The hard coded image bytes to fake or null if image should not be
     *         faked.
     * @param hardcodedImageExtension The hard coded image extension.
     */
    @VisibleForTesting
    public static void setHardcodedImageBytesForTesting(
            byte[] hardcodedImageBytes, String hardcodedImageExtension) {
        sHardcodedImageBytesForTesting = hardcodedImageBytes;
        sHardcodedImageExtensionForTesting = hardcodedImageExtension;
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
                Action.DIRECT_SHARE_LINK, Action.DIRECT_SHARE_IMAGE, Action.SEARCH_WITH_GOOGLE_LENS,
                Action.COPY_IMAGE, Action.SHOP_SIMILAR_PRODUCTS, Action.SHOP_IMAGE_WITH_GOOGLE_LENS,
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
            int COPY_IMAGE = 29;
            int SHOP_SIMILAR_PRODUCTS = 30;
            int SHOP_IMAGE_WITH_GOOGLE_LENS = 31;
            int SEARCH_SIMILAR_PRODUCTS = 32;
            int NUM_ENTRIES = 33;
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
        static void record(WebContents webContents, ContextMenuParams params, @Action int action) {
            String histogramName;
            if (params.isVideo()) {
                histogramName = "ContextMenu.SelectedOptionAndroid.Video";
            } else if (params.isImage()) {
                if (LensUtils.isInShoppingAllowlist(params.getPageUrl())) {
                    String shoppingHistogramName = params.isAnchor()
                            ? "ContextMenu.SelectedOptionAndroid.ImageLink.ShoppingDomain"
                            : "ContextMenu.SelectedOptionAndroid.Image.ShoppingDomain";
                    RecordHistogram.recordEnumeratedHistogram(
                            shoppingHistogramName, action, Action.NUM_ENTRIES);
                }
                histogramName = params.isAnchor() ? "ContextMenu.SelectedOptionAndroid.ImageLink"
                                                  : "ContextMenu.SelectedOptionAndroid.Image";

            } else {
                assert params.isAnchor();
                histogramName = "ContextMenu.SelectedOptionAndroid.Link";
            }
            RecordHistogram.recordEnumeratedHistogram(histogramName, action, Action.NUM_ENTRIES);
            if (params.isAnchor()
                    && PerformanceHintsObserver.getPerformanceClassForURL(
                               webContents, params.getLinkUrl())
                            == PerformanceClass.PERFORMANCE_FAST) {
                RecordHistogram.recordEnumeratedHistogram(
                        histogramName + ".PerformanceClassFast", action, Action.NUM_ENTRIES);
            }
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
        @IntDef({LensSupportStatus.LENS_SEARCH_SUPPORTED,
                LensSupportStatus.NON_GOOGLE_SEARCH_ENGINE,
                LensSupportStatus.ACTIVITY_NOT_ACCESSIBLE, LensSupportStatus.OUT_OF_DATE,
                LensSupportStatus.SEARCH_BY_IMAGE_UNAVAILABLE, LensSupportStatus.LEGACY_OS,
                LensSupportStatus.INVALID_PACKAGE, LensSupportStatus.LENS_SHOP_SUPPORTED,
                LensSupportStatus.LENS_SHOP_AND_SEARCH_SUPPORTED})
        @Retention(RetentionPolicy.SOURCE)
        public @interface LensSupportStatus {
            int LENS_SEARCH_SUPPORTED = 0;
            int NON_GOOGLE_SEARCH_ENGINE = 1;
            int ACTIVITY_NOT_ACCESSIBLE = 2;
            int OUT_OF_DATE = 3;
            int SEARCH_BY_IMAGE_UNAVAILABLE = 4;
            int LEGACY_OS = 5;
            int INVALID_PACKAGE = 6;
            int LENS_SHOP_SUPPORTED = 7;
            int LENS_SHOP_AND_SEARCH_SUPPORTED = 8;
            int NUM_ENTRIES = 9;
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
     * @param shareDelegate The Supplier of {@link ShareDelegate} that will be notified when a share
     *                      action is performed.
     * @param mode Defines the context menu mode
     * @param externalAuthUtils {@link ExternalAuthUtils} instance.
     * @param context The {@link Context} used to retrieve the strings.
     * @param params The {@link ContextMenuParams} to populate the menu items.
     * @param renderFrameHost The {@link RenderFrameHost} to fetch the decoded images from.
     */
    public ChromeContextMenuPopulator(ContextMenuItemDelegate delegate,
            Supplier<ShareDelegate> shareDelegate, @ContextMenuMode int mode,
            ExternalAuthUtils externalAuthUtils, Context context, ContextMenuParams params,
            RenderFrameHost renderFrameHost) {
        mDelegate = delegate;
        mShareDelegateSupplier = shareDelegate;
        mMode = mode;
        mExternalAuthUtils = externalAuthUtils;
        mContext = context;
        mParams = params;
        mNativeChromeContextMenuPopulator = ChromeContextMenuPopulatorJni.get().init(
                delegate.getWebContents(), params, renderFrameHost);
    }

    @Override
    public void onDestroy() {
        mNativeChromeContextMenuPopulator = 0;
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
        if (BrowserStartupController.getInstance().isFullBrowserStarted()) {
            return UrlFormatter.formatUrlForDisplayOmitSchemeOmitTrivialSubdomains(
                    params.getLinkUrl());
        }
        return params.getLinkUrl();
    }

    @Override
    public List<Pair<Integer, ModelList>> buildContextMenu(boolean isShoppyImage) {
        boolean hasSaveImage = false;
        mShowEphemeralTabNewLabel = null;

        List<Pair<Integer, ModelList>> groupedItems = new ArrayList<>();

        if (mParams.isAnchor()) {
            ModelList linkGroup = new ModelList();
            if (FirstRunStatus.getFirstRunFlowComplete() && !isEmptyUrl(mParams.getUrl())
                    && UrlUtilities.isAcceptedScheme(mParams.getUrl())) {
                if (mMode == ContextMenuMode.NORMAL) {
                    linkGroup.add(createListItem(Item.OPEN_IN_NEW_TAB));
                    if (!mDelegate.isIncognito() && mDelegate.isIncognitoSupported()) {
                        linkGroup.add(createListItem(Item.OPEN_IN_INCOGNITO_TAB));
                    }
                    if (mDelegate.isOpenInOtherWindowSupported()) {
                        linkGroup.add(createListItem(Item.OPEN_IN_OTHER_WINDOW));
                    }
                }
                if ((mMode == ContextMenuMode.NORMAL || mMode == ContextMenuMode.CUSTOM_TAB)
                        && EphemeralTabCoordinator.isSupported()) {
                    mShowEphemeralTabNewLabel = shouldTriggerEphemeralTabHelpUi();
                    linkGroup.add(
                            createListItem(Item.OPEN_IN_EPHEMERAL_TAB, mShowEphemeralTabNewLabel));
                }
            }
            if (!MailTo.isMailTo(mParams.getLinkUrl())
                    && !UrlUtilities.isTelScheme(mParams.getLinkUrl())) {
                linkGroup.add(createListItem(Item.COPY_LINK_ADDRESS));
                if (!mParams.getLinkText().trim().isEmpty() && !mParams.isImage()) {
                    linkGroup.add(createListItem(Item.COPY_LINK_TEXT));
                }
            }
            if (FirstRunStatus.getFirstRunFlowComplete()) {
                if (!mDelegate.isIncognito()
                        && UrlUtilities.isDownloadableScheme(mParams.getLinkUrl())) {
                    linkGroup.add(createListItem(Item.SAVE_LINK_AS));
                }
                linkGroup.add(createShareListItem(Item.SHARE_LINK));
                if (UrlUtilities.isTelScheme(mParams.getLinkUrl())) {
                    if (mDelegate.supportsCall()) {
                        linkGroup.add(createListItem(Item.CALL));
                    }
                    if (mDelegate.supportsSendTextMessage()) {
                        linkGroup.add(createListItem(Item.SEND_MESSAGE));
                    }
                    if (mDelegate.supportsAddToContacts()) {
                        linkGroup.add(createListItem(Item.ADD_TO_CONTACTS));
                    }
                }
                if (MailTo.isMailTo(mParams.getLinkUrl())) {
                    if (mDelegate.supportsSendEmailMessage()) {
                        linkGroup.add(createListItem(Item.SEND_MESSAGE));
                    }
                    if (!TextUtils.isEmpty(MailTo.parse(mParams.getLinkUrl()).getTo())
                            && mDelegate.supportsAddToContacts()) {
                        linkGroup.add(createListItem(Item.ADD_TO_CONTACTS));
                    }
                }
            }
            if (UrlUtilities.isTelScheme(mParams.getLinkUrl())
                    || MailTo.isMailTo(mParams.getLinkUrl())) {
                linkGroup.add(createListItem(Item.COPY));
            }
            if (linkGroup.size() > 0) {
                groupedItems.add(new Pair<>(R.string.contextmenu_link_title, linkGroup));
            }
        }

        if (mParams.isImage() && FirstRunStatus.getFirstRunFlowComplete()) {
            ModelList imageGroup = new ModelList();
            boolean isSrcDownloadableScheme =
                    UrlUtilities.isDownloadableScheme(mParams.getSrcUrl());
            boolean showLensShoppingMenuItem = false;
            // Avoid showing open image option for same image which is already opened.
            if (mMode == ContextMenuMode.CUSTOM_TAB
                    && !mDelegate.getPageUrl().equals(mParams.getSrcUrl())) {
                imageGroup.add(createListItem(Item.OPEN_IMAGE));
            }
            if (mMode == ContextMenuMode.NORMAL) {
                imageGroup.add(createListItem(Item.OPEN_IMAGE_IN_NEW_TAB));
            }
            if ((mMode == ContextMenuMode.NORMAL || mMode == ContextMenuMode.CUSTOM_TAB)
                    && EphemeralTabCoordinator.isSupported()) {
                if (mShowEphemeralTabNewLabel == null) {
                    mShowEphemeralTabNewLabel = shouldTriggerEphemeralTabHelpUi();
                }
                imageGroup.add(createListItem(
                        Item.OPEN_IMAGE_IN_EPHEMERAL_TAB, mShowEphemeralTabNewLabel));
            }
            imageGroup.add(createListItem(Item.COPY_IMAGE));
            if (isSrcDownloadableScheme) {
                imageGroup.add(createListItem(Item.SAVE_IMAGE));
                hasSaveImage = true;
            }

            // If set, show 'Share Image' before 'Search with Google Lens'.
            // IMPORTANT: Must stay consistent with logic after the below Lens block.
            boolean addedShareImageAboveLens = false;
            if (LensUtils.orderShareImageBeforeLens()) {
                addedShareImageAboveLens = true;
                imageGroup.add(createShareListItem(Item.SHARE_IMAGE));
            }

            if (mMode == ContextMenuMode.CUSTOM_TAB || mMode == ContextMenuMode.NORMAL) {
                if (checkSupportsGoogleSearchByImage(isSrcDownloadableScheme)) {
                    // All behavior relating to Lens integration is gated by Feature Flag.
                    // A map to indicate which image search menu item would be shown.
                    Map<String, Boolean> imageSearchMenuItemsToShow =
                            getSearchByImageMenuItemsToShowAndRecordMetrics(
                                    mParams.getPageUrl(), isShoppyImage, mDelegate.isIncognito());
                    if (imageSearchMenuItemsToShow.get(LENS_SEARCH_MENU_ITEM_KEY)) {
                        if (LensUtils.useLensWithSearchByImageText()) {
                            mEnableLensWithSearchByImageText = true;
                            imageGroup.add(createListItem(Item.SEARCH_BY_IMAGE));
                        } else {
                            imageGroup.add(createListItem(Item.SEARCH_WITH_GOOGLE_LENS, true));
                        }
                        maybeRecordUkmLensShown();
                    } else if (imageSearchMenuItemsToShow.get(SEARCH_BY_IMAGE_MENU_ITEM_KEY)) {
                        imageGroup.add(createListItem(Item.SEARCH_BY_IMAGE));
                        maybeRecordUkmSearchByImageShown();
                    }
                    // Check whether we should show Lens Shopping menu item.
                    if (imageSearchMenuItemsToShow.get(LENS_SHOP_MENU_ITEM_KEY)) {
                        showLensShoppingMenuItem = true;
                    }
                } else if (ChromeFeatureList.isEnabled(
                                   ChromeFeatureList.CONTEXT_MENU_SEARCH_WITH_GOOGLE_LENS)) {
                    ContextMenuUma.recordLensSupportStatus(
                            ContextMenuUma.LensSupportStatus.SEARCH_BY_IMAGE_UNAVAILABLE);
                }
            }

            // By default show 'Share Image' after 'Search with Google Lens'.
            // IMPORTANT: Must stay consistent with logic before the above Lens block.
            if (!addedShareImageAboveLens) {
                imageGroup.add(createShareListItem(Item.SHARE_IMAGE));
            }

            // Show Lens Shopping Menu Item when the Lens Shopping feature is supported.
            if (showLensShoppingMenuItem) {
                if (LensUtils.useLensWithShopSimilarProducts()) {
                    imageGroup.add(createListItem(Item.SHOP_SIMILAR_PRODUCTS, true));
                    // If the image is classified as shoppy always use the Shop Image with Google
                    // Lens item text.
                } else if (LensUtils.useLensWithShopImageWithGoogleLens()
                        || (LensUtils.enableShoppyImageMenuItem() && isShoppyImage)) {
                    imageGroup.add(createListItem(Item.SHOP_IMAGE_WITH_GOOGLE_LENS, true));
                } else if (LensUtils.useLensWithSearchSimilarProducts()) {
                    imageGroup.add(createListItem(Item.SEARCH_SIMILAR_PRODUCTS, true));
                }
                maybeRecordUkmLensShoppingShown();
            }

            recordSaveImageContextMenuResult(isSrcDownloadableScheme);
            groupedItems.add(new Pair<>(R.string.contextmenu_image_title, imageGroup));
        }

        if (mParams.isVideo() && FirstRunStatus.getFirstRunFlowComplete() && mParams.canSaveMedia()
                && UrlUtilities.isDownloadableScheme(mParams.getSrcUrl())) {
            ModelList videoGroup = new ModelList();
            videoGroup.add(createListItem(Item.SAVE_VIDEO));
            groupedItems.add(new Pair<>(R.string.contextmenu_video_title, videoGroup));
        }

        if (mMode != ContextMenuMode.NORMAL && FirstRunStatus.getFirstRunFlowComplete()) {
            ModelList items = groupedItems.isEmpty()
                    ? new ModelList()
                    : groupedItems
                              .get(mMode == ContextMenuMode.CUSTOM_TAB ? 0
                                                                       : groupedItems.size() - 1)
                              .second;
            if (mMode == ContextMenuMode.WEB_APP) {
                items.add(createListItem(Item.OPEN_IN_CHROME));
            } else if (mMode == ContextMenuMode.CUSTOM_TAB) {
                boolean addNewEntries = false;
                try {
                    URI uri = new URI(mParams.getUrl());
                    if (!UrlUtilities.isInternalScheme(uri) && !isEmptyUrl(mParams.getUrl())) {
                        addNewEntries = true;
                    }
                } catch (URISyntaxException ignore) {
                }
                if (SharedPreferencesManager.getInstance().readBoolean(
                            ChromePreferenceKeys.CHROME_DEFAULT_BROWSER, false)
                        && addNewEntries) {
                    if (mDelegate.isIncognitoSupported()) {
                        items.add(0, createListItem(Item.OPEN_IN_CHROME_INCOGNITO_TAB));
                    }
                    items.add(0, createListItem(Item.OPEN_IN_NEW_CHROME_TAB));
                } else if (addNewEntries) {
                    items.add(0, createListItem(Item.OPEN_IN_BROWSER_ID));
                }
            }
            if (groupedItems.isEmpty() && items.size() > 0) {
                groupedItems.add(new Pair<>(R.string.contextmenu_link_title, items));
            }
        }

        if (!groupedItems.isEmpty()
                && BrowserStartupController.getInstance().isFullBrowserStarted()) {
            if (!hasSaveImage) {
                ContextMenuUma.recordSaveImageUma(mParams.isImage()
                                ? ContextMenuUma.TypeSaveImage.DISABLED_AND_IS_IMAGE_PARAM
                                : ContextMenuUma.TypeSaveImage.DISABLED_AND_IS_NOT_IMAGE_PARAM);
            } else {
                ContextMenuUma.recordSaveImageUma(ContextMenuUma.TypeSaveImage.SHOWN);
            }
        }

        return groupedItems;
    }

    @VisibleForTesting
    boolean shouldTriggerEphemeralTabHelpUi() {
        // TODO (https://crbug.com/1048632): Use the current profile (i.e., regular profile or
        // incognito profile) instead of always using regular profile. It works correctly now, but
        // it is not safe.
        Tracker tracker = TrackerFactory.getTrackerForProfile(Profile.getLastUsedRegularProfile());
        return tracker.isInitialized()
                && tracker.shouldTriggerHelpUI(FeatureConstants.EPHEMERAL_TAB_FEATURE);
    }

    @Override
    public boolean isIncognito() {
        return mDelegate.isIncognito();
    }

    @Override
    public boolean onItemSelected(int itemId) {
        if (itemId == R.id.contextmenu_open_in_new_tab) {
            recordContextMenuSelection(ContextMenuUma.Action.OPEN_IN_NEW_TAB);
            mDelegate.onOpenInNewTab(mParams.getUrl(), mParams.getReferrer());
        } else if (itemId == R.id.contextmenu_open_in_incognito_tab) {
            recordContextMenuSelection(ContextMenuUma.Action.OPEN_IN_INCOGNITO_TAB);
            mDelegate.onOpenInNewIncognitoTab(mParams.getUrl());
        } else if (itemId == R.id.contextmenu_open_in_other_window) {
            recordContextMenuSelection(ContextMenuUma.Action.OPEN_IN_OTHER_WINDOW);
            mDelegate.onOpenInOtherWindow(mParams.getUrl(), mParams.getReferrer());
        } else if (itemId == R.id.contextmenu_open_in_ephemeral_tab) {
            recordContextMenuSelection(ContextMenuUma.Action.OPEN_IN_EPHEMERAL_TAB);
            mDelegate.onOpenInEphemeralTab(mParams.getUrl(), mParams.getLinkText());
        } else if (itemId == R.id.contextmenu_open_image) {
            recordContextMenuSelection(ContextMenuUma.Action.OPEN_IMAGE);
            mDelegate.onOpenImageUrl(mParams.getSrcUrl(), mParams.getReferrer());
        } else if (itemId == R.id.contextmenu_open_image_in_new_tab) {
            recordContextMenuSelection(ContextMenuUma.Action.OPEN_IMAGE_IN_NEW_TAB);
            mDelegate.onOpenImageInNewTab(mParams.getSrcUrl(), mParams.getReferrer());
        } else if (itemId == R.id.contextmenu_open_image_in_ephemeral_tab) {
            recordContextMenuSelection(ContextMenuUma.Action.OPEN_IMAGE_IN_EPHEMERAL_TAB);
            String title = mParams.getTitleText();
            if (TextUtils.isEmpty(title)) {
                title = URLUtil.guessFileName(mParams.getSrcUrl(), null, null);
            }
            mDelegate.onOpenInEphemeralTab(mParams.getSrcUrl(), title);
        } else if (itemId == R.id.contextmenu_copy_image) {
            recordContextMenuSelection(ContextMenuUma.Action.COPY_IMAGE);
            copyImageToClipboard();
        } else if (itemId == R.id.contextmenu_copy_link_address) {
            recordContextMenuSelection(ContextMenuUma.Action.COPY_LINK_ADDRESS);
            mDelegate.onSaveToClipboard(
                    mParams.getUnfilteredLinkUrl(), ContextMenuItemDelegate.ClipboardType.LINK_URL);
        } else if (itemId == R.id.contextmenu_call) {
            recordContextMenuSelection(ContextMenuUma.Action.CALL);
            mDelegate.onCall(mParams.getLinkUrl());
        } else if (itemId == R.id.contextmenu_send_message) {
            if (MailTo.isMailTo(mParams.getLinkUrl())) {
                recordContextMenuSelection(ContextMenuUma.Action.SEND_EMAIL);
                mDelegate.onSendEmailMessage(mParams.getLinkUrl());
            } else if (UrlUtilities.isTelScheme(mParams.getLinkUrl())) {
                recordContextMenuSelection(ContextMenuUma.Action.SEND_TEXT_MESSAGE);
                mDelegate.onSendTextMessage(mParams.getLinkUrl());
            }
        } else if (itemId == R.id.contextmenu_add_to_contacts) {
            recordContextMenuSelection(ContextMenuUma.Action.ADD_TO_CONTACTS);
            mDelegate.onAddToContacts(mParams.getLinkUrl());
        } else if (itemId == R.id.contextmenu_copy) {
            if (MailTo.isMailTo(mParams.getLinkUrl())) {
                recordContextMenuSelection(ContextMenuUma.Action.COPY_EMAIL_ADDRESS);
                mDelegate.onSaveToClipboard(MailTo.parse(mParams.getLinkUrl()).getTo(),
                        ContextMenuItemDelegate.ClipboardType.LINK_URL);
            } else if (UrlUtilities.isTelScheme(mParams.getLinkUrl())) {
                recordContextMenuSelection(ContextMenuUma.Action.COPY_PHONE_NUMBER);
                mDelegate.onSaveToClipboard(UrlUtilities.getTelNumber(mParams.getLinkUrl()),
                        ContextMenuItemDelegate.ClipboardType.LINK_URL);
            }
        } else if (itemId == R.id.contextmenu_copy_link_text) {
            recordContextMenuSelection(ContextMenuUma.Action.COPY_LINK_TEXT);
            mDelegate.onSaveToClipboard(
                    mParams.getLinkText(), ContextMenuItemDelegate.ClipboardType.LINK_TEXT);
        } else if (itemId == R.id.contextmenu_save_image) {
            recordContextMenuSelection(ContextMenuUma.Action.SAVE_IMAGE);
            if (mDelegate.startDownload(mParams.getSrcUrl(), false)) {
                startContextMenuDownload(false);
            }
        } else if (itemId == R.id.contextmenu_save_video) {
            recordContextMenuSelection(ContextMenuUma.Action.SAVE_VIDEO);
            if (mDelegate.startDownload(mParams.getSrcUrl(), false)) {
                startContextMenuDownload(false);
            }
        } else if (itemId == R.id.contextmenu_save_link_as) {
            recordContextMenuSelection(ContextMenuUma.Action.SAVE_LINK);
            String url = mParams.getUnfilteredLinkUrl();
            if (mDelegate.startDownload(url, true)) {
                ContextMenuUma.recordSaveLinkTypes(url);
                startContextMenuDownload(true);
            }
        } else if (itemId == R.id.contextmenu_share_link) {
            recordContextMenuSelection(ContextMenuUma.Action.SHARE_LINK);
            ShareParams linkShareParams =
                    new ShareParams
                            .Builder(getWindow(), ContextMenuUtils.getTitle(mParams),
                                    mParams.getUrl())
                            .build();
            mShareDelegateSupplier.get().share(
                    linkShareParams, new ChromeShareExtras.Builder().setSaveLastUsed(true).build());
        } else if (itemId == R.id.contextmenu_search_with_google_lens) {
            recordContextMenuSelection(ContextMenuUma.Action.SEARCH_WITH_GOOGLE_LENS);
            searchWithGoogleLens(mDelegate.isIncognito());
            SharedPreferencesManager prefManager = SharedPreferencesManager.getInstance();
            prefManager.writeBoolean(
                    ChromePreferenceKeys.CONTEXT_MENU_SEARCH_WITH_GOOGLE_LENS_CLICKED, true);
        } else if (itemId == R.id.contextmenu_search_by_image) {
            if (mEnableLensWithSearchByImageText) {
                recordContextMenuSelection(ContextMenuUma.Action.SEARCH_WITH_GOOGLE_LENS);
                searchWithGoogleLens(mDelegate.isIncognito());
            } else {
                recordContextMenuSelection(ContextMenuUma.Action.SEARCH_BY_IMAGE);
                searchForImage();
            }
        } else if (itemId == R.id.contextmenu_shop_similar_products) {
            recordContextMenuSelection(ContextMenuUma.Action.SHOP_SIMILAR_PRODUCTS);
            shopWithGoogleLens(mDelegate.isIncognito(),
                    /*requiresConfirmation=*/true);
            SharedPreferencesManager prefManager = SharedPreferencesManager.getInstance();
            prefManager.writeBoolean(
                    ChromePreferenceKeys.CONTEXT_MENU_SHOP_SIMILAR_PRODUCTS_CLICKED, true);
        } else if (itemId == R.id.contextmenu_shop_image_with_google_lens) {
            recordContextMenuSelection(ContextMenuUma.Action.SHOP_IMAGE_WITH_GOOGLE_LENS);
            shopWithGoogleLens(mDelegate.isIncognito(), /*requiresConfirmation=*/false);
            SharedPreferencesManager prefManager = SharedPreferencesManager.getInstance();
            prefManager.writeBoolean(
                    ChromePreferenceKeys.CONTEXT_MENU_SHOP_IMAGE_WITH_GOOGLE_LENS_CLICKED, true);
        } else if (itemId == R.id.contextmenu_search_similar_products) {
            recordContextMenuSelection(ContextMenuUma.Action.SEARCH_SIMILAR_PRODUCTS);
            shopWithGoogleLens(mDelegate.isIncognito(),
                    /*requiresConfirmation=*/true);
            SharedPreferencesManager prefManager = SharedPreferencesManager.getInstance();
            prefManager.writeBoolean(
                    ChromePreferenceKeys.CONTEXT_MENU_SEARCH_SIMILAR_PRODUCTS_CLICKED, true);
        } else if (itemId == R.id.contextmenu_share_image) {
            recordContextMenuSelection(ContextMenuUma.Action.SHARE_IMAGE);
            shareImage();
        } else if (itemId == R.id.contextmenu_open_in_chrome) {
            recordContextMenuSelection(ContextMenuUma.Action.OPEN_IN_CHROME);
            mDelegate.onOpenInChrome(mParams.getUrl(), mParams.getPageUrl());
        } else if (itemId == R.id.contextmenu_open_in_new_chrome_tab) {
            recordContextMenuSelection(ContextMenuUma.Action.OPEN_IN_NEW_CHROME_TAB);
            mDelegate.onOpenInNewChromeTabFromCCT(mParams.getUrl(), false);
        } else if (itemId == R.id.contextmenu_open_in_chrome_incognito_tab) {
            recordContextMenuSelection(ContextMenuUma.Action.OPEN_IN_CHROME_INCOGNITO_TAB);
            mDelegate.onOpenInNewChromeTabFromCCT(mParams.getUrl(), true);
        } else if (itemId == R.id.contextmenu_open_in_browser_id) {
            recordContextMenuSelection(ContextMenuUma.Action.OPEN_IN_BROWSER);
            mDelegate.onOpenInDefaultBrowser(mParams.getUrl());
        } else {
            assert false;
        }

        return true;
    }

    @Override
    public void onMenuClosed() {
        if (mShowEphemeralTabNewLabel != null && mShowEphemeralTabNewLabel) {
            // TODO (https://crbug.com/1048632): Use the current profile (i.e., regular profile or
            // incognito profile) instead of always using regular profile. It works correctly now,
            // but it is not safe.
            Tracker tracker =
                    TrackerFactory.getTrackerForProfile(Profile.getLastUsedRegularProfile());
            if (tracker.isInitialized()) tracker.dismissed(FeatureConstants.EPHEMERAL_TAB_FEATURE);
        }
    }

    private WindowAndroid getWindow() {
        return mDelegate.getWebContents().getTopLevelNativeWindow();
    }

    private Activity getActivity() {
        return getWindow().getActivity().get();
    }

    /**
     * Copy the image, that triggered the current context menu, to system clipboard.
     */
    private void copyImageToClipboard() {
        retrieveImage(ContextMenuImageFormat.ORIGINAL,
                (Uri imageUri) -> { mDelegate.onSaveImageToClipboard(imageUri); });
    }

    /**
     * Search for the image by intenting to the lens app with the image data attached.
     * @param isIncognito Whether the image to search came from an incognito context.
     */
    private void searchWithGoogleLens(boolean isIncognito) {
        retrieveImage(ContextMenuImageFormat.PNG, (Uri imageUri) -> {
            ShareHelper.shareImageWithGoogleLens(getWindow(), imageUri, isIncognito,
                    mParams.getSrcUrl(), mParams.getTitleText(),
                    /* isShoppyImage*/ false, /* requiresConfirmation*/ false);
        });
    }

    /**
     * Search for the image by intenting to the lens app with the image data attached.
     * @param isIncognito Whether the image to search came from an incognito context.
     * @param requiresConfirmation Whether the request requires an account dialog.
     */
    private void shopWithGoogleLens(boolean isIncognito, boolean requiresConfirmation) {
        retrieveImage(ContextMenuImageFormat.PNG, (Uri imageUri) -> {
            ShareHelper.shareImageWithGoogleLens(getWindow(), imageUri, isIncognito,
                    mParams.getSrcUrl(), mParams.getTitleText(), /* isShoppyImage*/ true,
                    requiresConfirmation);
        });
    }

    /**
     * Share the image that triggered the current context menu.
     * Package-private, allowing access only from the context menu item to ensure that
     * it will use the right activity set when the menu was displayed.
     */
    private void shareImage() {
        retrieveImage(ContextMenuImageFormat.ORIGINAL, (Uri imageUri) -> {
            if (!mShareDelegateSupplier.get().isSharingHubV15Enabled()) {
                ShareHelper.shareImage(getWindow(), null, imageUri);
                return;
            }
            ContentResolver contentResolver =
                    ContextUtils.getApplicationContext().getContentResolver();
            ShareParams imageShareParams =
                    new ShareParams
                            .Builder(getWindow(), ContextMenuUtils.getTitle(mParams), /*url=*/"")
                            .setFileUris(new ArrayList<>(Collections.singletonList(imageUri)))
                            .setFileContentType(contentResolver.getType(imageUri))
                            .build();
            mShareDelegateSupplier.get().share(imageShareParams,
                    new ChromeShareExtras.Builder()
                            .setSaveLastUsed(true)
                            .setImageSrcUrl(mParams.getSrcUrl())
                            .build());
        });
    }

    @Override
    public void retrieveImage(@ContextMenuImageFormat int imageFormat, Callback<Uri> callback) {
        if (mNativeChromeContextMenuPopulator == 0) return;
        final Activity activity = getActivity();

        Callback<ImageCallbackResult> imageRetrievalCallback = (result) -> {
            if (activity == null) return;
            ShareImageFileUtils.generateTemporaryUriFromData(
                    activity, result.imageData, result.extension, callback);
        };

        if (sHardcodedImageBytesForTesting != null) {
            imageRetrievalCallback.onResult(createImageCallbackResultForTesting());
        } else {
            ChromeContextMenuPopulatorJni.get().retrieveImageForShare(
                    mNativeChromeContextMenuPopulator, ChromeContextMenuPopulator.this,
                    imageRetrievalCallback, MAX_SHARE_DIMEN_PX, MAX_SHARE_DIMEN_PX, imageFormat);
        }
    }

    /**
     * Starts a download based on the current {@link ContextMenuParams}.
     * @param isLink Whether or not the download target is a link.
     */
    private void startContextMenuDownload(boolean isLink) {
        if (mNativeChromeContextMenuPopulator == 0) return;
        ChromeContextMenuPopulatorJni.get().onStartDownload(
                mNativeChromeContextMenuPopulator, ChromeContextMenuPopulator.this, isLink);
    }

    /**
     * Trigger an image search for the current image that triggered the context menu.
     */
    private void searchForImage() {
        if (mNativeChromeContextMenuPopulator == 0) return;
        ChromeContextMenuPopulatorJni.get().searchForImage(
                mNativeChromeContextMenuPopulator, ChromeContextMenuPopulator.this);
    }

    /**
     * Gets the thumbnail of the current image that triggered the context menu.
     * @param callback Called once the the thumbnail is received.
     */
    @Override
    public void getThumbnail(final Callback<Bitmap> callback) {
        if (mNativeChromeContextMenuPopulator == 0) return;

        final Resources res = getActivity().getResources();
        final int maxHeightPx =
                res.getDimensionPixelSize(R.dimen.revamped_context_menu_header_image_max_size);
        final int maxWidthPx =
                res.getDimensionPixelSize(R.dimen.revamped_context_menu_header_image_max_size);

        ChromeContextMenuPopulatorJni.get().retrieveImageForContextMenu(
                mNativeChromeContextMenuPopulator, ChromeContextMenuPopulator.this, callback,
                maxWidthPx, maxHeightPx);
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
        if (!BrowserStartupController.getInstance().isFullBrowserStarted()) {
            return;
        }

        ContextMenuUma.recordSaveImageUma(ContextMenuUma.TypeSaveImage.LOADED);

        if (!isDownloadableScheme) {
            ContextMenuUma.recordSaveImageUma(ContextMenuUma.TypeSaveImage.NOT_DOWNLOADABLE);
        }
    }

    /**
     * Record a UMA ping and a UKM ping if enabled.
     */
    private void recordContextMenuSelection(int actionId) {
        ContextMenuUma.record(mDelegate.getWebContents(), mParams, actionId);
        maybeRecordActionUkm("ContextMenuAndroid.Selected", actionId);
    }

    /**
     * Whether the lens menu items should be shown based on a set of application
     * compatibility checks.
     *
     * @param pageUrl The Url associated with the main frame of the page that triggered the context
     *         menu.
     * @param isShoppyImage Whether the image has been identified to have clear shopping intent.
     * @param isShoppyImage Whether the user is incognito.
     * @return An immutable map. Can be used to check whether a specific Lens menu item is enabled.
     */
    private Map<String, Boolean> getSearchByImageMenuItemsToShowAndRecordMetrics(
            String pageUrl, boolean isShoppyImage, boolean isIncognito) {
        // If Google Lens feature is not supported, show search by image menu item.
        if (!LensUtils.isGoogleLensFeatureEnabled(isIncognito)) {
            // TODO(yusuyoutube): Cleanup. Remove repetition.
            return Collections.unmodifiableMap(new HashMap<String, Boolean>() {
                {
                    put(LENS_SEARCH_MENU_ITEM_KEY, false);
                    put(LENS_SHOP_MENU_ITEM_KEY, false);
                    put(SEARCH_BY_IMAGE_MENU_ITEM_KEY, true);
                }
            });
        }
        final TemplateUrlService templateUrlServiceInstance = getTemplateUrlService();
        String versionName = LensUtils.getLensActivityVersionNameIfAvailable(mContext);
        if (!templateUrlServiceInstance.isDefaultSearchEngineGoogle()) {
            ContextMenuUma.recordLensSupportStatus(
                    ContextMenuUma.LensSupportStatus.NON_GOOGLE_SEARCH_ENGINE);

            return Collections.unmodifiableMap(new HashMap<String, Boolean>() {
                {
                    put(LENS_SEARCH_MENU_ITEM_KEY, false);
                    put(LENS_SHOP_MENU_ITEM_KEY, false);
                    put(SEARCH_BY_IMAGE_MENU_ITEM_KEY, true);
                }
            });
        }
        if (TextUtils.isEmpty(versionName)) {
            ContextMenuUma.recordLensSupportStatus(
                    ContextMenuUma.LensSupportStatus.ACTIVITY_NOT_ACCESSIBLE);
            return Collections.unmodifiableMap(new HashMap<String, Boolean>() {
                {
                    put(LENS_SEARCH_MENU_ITEM_KEY, false);
                    put(LENS_SHOP_MENU_ITEM_KEY, false);
                    put(SEARCH_BY_IMAGE_MENU_ITEM_KEY, true);
                }
            });
        }
        if (GSAState.getInstance(mContext).isAgsaVersionBelowMinimum(
                    versionName, LensUtils.getMinimumAgsaVersionForLensSupport())) {
            ContextMenuUma.recordLensSupportStatus(ContextMenuUma.LensSupportStatus.OUT_OF_DATE);
            return Collections.unmodifiableMap(new HashMap<String, Boolean>() {
                {
                    put(LENS_SEARCH_MENU_ITEM_KEY, false);
                    put(LENS_SHOP_MENU_ITEM_KEY, false);
                    put(SEARCH_BY_IMAGE_MENU_ITEM_KEY, true);
                }
            });
        }

        if (LensUtils.isDeviceOsBelowMinimum()) {
            ContextMenuUma.recordLensSupportStatus(ContextMenuUma.LensSupportStatus.LEGACY_OS);
            return Collections.unmodifiableMap(new HashMap<String, Boolean>() {
                {
                    put(LENS_SEARCH_MENU_ITEM_KEY, false);
                    put(LENS_SHOP_MENU_ITEM_KEY, false);
                    put(SEARCH_BY_IMAGE_MENU_ITEM_KEY, true);
                }
            });
        }

        if (!LensUtils.isValidAgsaPackage(mExternalAuthUtils)) {
            ContextMenuUma.recordLensSupportStatus(
                    ContextMenuUma.LensSupportStatus.INVALID_PACKAGE);
            return Collections.unmodifiableMap(new HashMap<String, Boolean>() {
                {
                    put(LENS_SEARCH_MENU_ITEM_KEY, false);
                    put(LENS_SHOP_MENU_ITEM_KEY, false);
                    put(SEARCH_BY_IMAGE_MENU_ITEM_KEY, true);
                }
            });
        }

        // In Lens Shopping Menu Item experiment, fallback to Search image with Google Lens
        // When the url is not in domain allowlist and AGSA version is equal to or greater than the
        // minimum shopping supported version.
        if (LensUtils.isGoogleLensShoppingFeatureEnabled(isIncognito)
                && !GSAState.getInstance(mContext).isAgsaVersionBelowMinimum(
                        versionName, LensUtils.getMinimumAgsaVersionForLensShoppingSupport())) {
            if (LensUtils.isInShoppingAllowlist(pageUrl) || isShoppyImage) {
                // Hide Search With Google Lens menu item when experiment only with Lens Shopping
                // menu items.
                if (!LensUtils.showBothSearchAndShopImageWithLens()) {
                    ContextMenuUma.recordLensSupportStatus(
                            ContextMenuUma.LensSupportStatus.LENS_SHOP_SUPPORTED);
                    return Collections.unmodifiableMap(new HashMap<String, Boolean>() {
                        {
                            put(LENS_SEARCH_MENU_ITEM_KEY, false);
                            put(LENS_SHOP_MENU_ITEM_KEY, true);
                            put(SEARCH_BY_IMAGE_MENU_ITEM_KEY, false);
                        }
                    });
                }
                ContextMenuUma.recordLensSupportStatus(
                        ContextMenuUma.LensSupportStatus.LENS_SHOP_AND_SEARCH_SUPPORTED);
                return Collections.unmodifiableMap(new HashMap<String, Boolean>() {
                    {
                        put(LENS_SEARCH_MENU_ITEM_KEY, true);
                        put(LENS_SHOP_MENU_ITEM_KEY, true);
                        put(SEARCH_BY_IMAGE_MENU_ITEM_KEY, false);
                    }
                });
            }
        }

        ContextMenuUma.recordLensSupportStatus(
                ContextMenuUma.LensSupportStatus.LENS_SEARCH_SUPPORTED);
        return Collections.unmodifiableMap(new HashMap<String, Boolean>() {
            {
                put(LENS_SEARCH_MENU_ITEM_KEY, true);
                put(LENS_SHOP_MENU_ITEM_KEY, false);
                put(SEARCH_BY_IMAGE_MENU_ITEM_KEY, false);
            }
        });
    }

    private ListItem createListItem(@Item int item) {
        return createListItem(item, false);
    }

    private ListItem createListItem(@Item int item, boolean showInProductHelp) {
        final PropertyModel model =
                new PropertyModel.Builder(RevampedContextMenuItemProperties.ALL_KEYS)
                        .with(MENU_ID, ChromeContextMenuItem.getMenuId(item))
                        .with(TEXT,
                                ChromeContextMenuItem.getTitle(mContext, item, showInProductHelp))
                        .build();
        return new ListItem(ListItemType.CONTEXT_MENU_ITEM, model);
    }

    private ListItem createShareListItem(@Item int item) {
        final boolean isLink = item == Item.SHARE_LINK;
        final Pair<Drawable, CharSequence> shareInfo = createRecentShareAppInfo(isLink);
        final PropertyModel model =
                new PropertyModel.Builder(RevampedContextMenuShareItemProperties.ALL_KEYS)
                        .with(MENU_ID, ChromeContextMenuItem.getMenuId(item))
                        .with(TEXT, ChromeContextMenuItem.getTitle(mContext, item, false))
                        .with(IMAGE, shareInfo.first)
                        .with(CONTENT_DESC, shareInfo.second)
                        .build();
        return new ListItem(ListItemType.CONTEXT_MENU_SHARE_ITEM, model);
    }

    /**
     * Return the icon and name of the most recently shared app by certain app.
     * @param isLink Whether the item is SHARE_LINK.
     */
    private static Pair<Drawable, CharSequence> createRecentShareAppInfo(boolean isLink) {
        Intent shareIntent = isLink ? ShareHelper.getShareLinkAppCompatibilityIntent()
                                    : ShareHelper.getShareImageIntent(null);
        return ShareHelper.getShareableIconAndName(shareIntent);
    }

    /**
     * If not disabled record a UKM for opening the context menu with the search by image option.
     */
    private void maybeRecordUkmSearchByImageShown() {
        maybeRecordBooleanUkm("ContextMenuAndroid.Shown", "SearchByImage");
    }

    /**
     * If not disabled record a UKM for opening the context menu with the lens item.
     */
    private void maybeRecordUkmLensShown() {
        maybeRecordBooleanUkm("ContextMenuAndroid.Shown", "SearchWithGoogleLens");
    }

    /**
     * If not disabled record a UKM for opening the context menu with the lens shopping
     * item.
     */
    private void maybeRecordUkmLensShoppingShown() {
        maybeRecordBooleanUkm("ContextMenuAndroid.Shown", "ShopWithGoogleLens");
    }

    /**
     * Initialize the bridge if not yet created.
     */
    private void initializeUkmRecorderBridge() {
        if (mUkmRecorderBridge == null) {
            mUkmRecorderBridge = new UkmRecorder.Bridge();
        }
    }

    /**
     * Record a boolean UKM if the lens feature is enabled.
     * @param eventName The name of the UKM event to record.
     * @param metricName The name of the UKM metric to record.
     */
    private void maybeRecordBooleanUkm(String eventName, String metricName) {
        if (!LensUtils.shouldLogUkm(mDelegate.isIncognito())) return;
        initializeUkmRecorderBridge();
        WebContents webContents = mDelegate.getWebContents();
        if (webContents != null) {
            mUkmRecorderBridge.recordEventWithBooleanMetric(webContents, eventName, metricName);
        }
    }

    /**
     * Record a UKM for a menu action if the lens feature is enabled.
     * @param eventName The name of the boolean UKM event to record.
     * @param actionId The id of the action corresponding the ContextMenuUma.Action enum.
     */
    private void maybeRecordActionUkm(String eventName, int actionId) {
        if (!LensUtils.shouldLogUkm(mDelegate.isIncognito())) return;
        initializeUkmRecorderBridge();
        WebContents webContents = mDelegate.getWebContents();
        if (webContents != null) {
            mUkmRecorderBridge.recordEventWithIntegerMetric(
                    webContents, eventName, "Action", actionId);
        }
    }

    /**
     * Check if the search by image is supported.
     * @param isSrcDownloadableScheme Whether the source url has a downloadable scheme.
     * @return True if search by image is supported.
     */
    private boolean checkSupportsGoogleSearchByImage(boolean isSrcDownloadableScheme) {
        final TemplateUrlService templateUrlServiceInstance = getTemplateUrlService();
        return isSrcDownloadableScheme && templateUrlServiceInstance.isLoaded()
                && templateUrlServiceInstance.isSearchByImageAvailable()
                && templateUrlServiceInstance.getDefaultSearchEngineTemplateUrl() != null
                && !LocaleManager.getInstance().needToCheckForSearchEnginePromo();
    }

    /**
     * The class hold the |retrieveImageForShare| callback result.
     */
    @VisibleForTesting
    static class ImageCallbackResult {
        public byte[] imageData;
        public String extension;

        public ImageCallbackResult(byte[] imageData, String extension) {
            this.imageData = imageData;
            this.extension = extension;
        }
    }

    private static ImageCallbackResult createImageCallbackResultForTesting() {
        return new ImageCallbackResult(
                sHardcodedImageBytesForTesting, sHardcodedImageExtensionForTesting);
    }

    @CalledByNative
    private static ImageCallbackResult createImageCallbackResult(
            byte[] imageData, String extension) {
        return new ImageCallbackResult(imageData, extension);
    }

    @NativeMethods
    interface Natives {
        long init(WebContents webContents, ContextMenuParams contextMenuParams,
                RenderFrameHost renderFrameHost);
        void onStartDownload(long nativeChromeContextMenuPopulator,
                ChromeContextMenuPopulator caller, boolean isLink);
        void retrieveImageForShare(long nativeChromeContextMenuPopulator,
                ChromeContextMenuPopulator caller, Callback<ImageCallbackResult> callback,
                int maxWidthPx, int maxHeightPx, @ContextMenuImageFormat int imageFormat);
        void retrieveImageForContextMenu(long nativeChromeContextMenuPopulator,
                ChromeContextMenuPopulator caller, Callback<Bitmap> callback, int maxWidthPx,
                int maxHeightPx);
        void searchForImage(
                long nativeChromeContextMenuPopulator, ChromeContextMenuPopulator caller);
    }
}
