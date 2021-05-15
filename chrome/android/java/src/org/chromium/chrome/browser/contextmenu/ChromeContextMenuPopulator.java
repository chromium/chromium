// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.contextmenu;

import static org.chromium.chrome.browser.contextmenu.RevampedContextMenuItemProperties.MENU_ID;
import static org.chromium.chrome.browser.contextmenu.RevampedContextMenuItemProperties.TEXT;
import static org.chromium.chrome.browser.contextmenu.RevampedContextMenuItemWithIconButtonProperties.BUTTON_CONTENT_DESC;
import static org.chromium.chrome.browser.contextmenu.RevampedContextMenuItemWithIconButtonProperties.BUTTON_IMAGE;
import static org.chromium.chrome.browser.contextmenu.RevampedContextMenuItemWithIconButtonProperties.BUTTON_MENU_ID;

import android.app.Activity;
import android.content.ContentResolver;
import android.content.Context;
import android.content.Intent;
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
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.compositor.bottombar.ephemeraltab.EphemeralTabCoordinator;
import org.chromium.chrome.browser.contextmenu.ChromeContextMenuItem.Item;
import org.chromium.chrome.browser.contextmenu.ChromeContextMenuPopulator.ContextMenuUma.Action;
import org.chromium.chrome.browser.contextmenu.RevampedContextMenuCoordinator.ListItemType;
import org.chromium.chrome.browser.feature_engagement.TrackerFactory;
import org.chromium.chrome.browser.firstrun.FirstRunStatus;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.gsa.GSAState;
import org.chromium.chrome.browser.lens.LensEntryPoint;
import org.chromium.chrome.browser.lens.LensUma;
import org.chromium.chrome.browser.locale.LocaleManager;
import org.chromium.chrome.browser.performance_hints.PerformanceHintsObserver;
import org.chromium.chrome.browser.performance_hints.PerformanceHintsObserver.PerformanceClass;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.SharedPreferencesManager;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.read_later.ReadingListUtils;
import org.chromium.chrome.browser.search_engines.TemplateUrlServiceFactory;
import org.chromium.chrome.browser.share.ChromeShareExtras;
import org.chromium.chrome.browser.share.LensUtils;
import org.chromium.chrome.browser.share.ShareDelegate;
import org.chromium.chrome.browser.share.ShareDelegateImpl.ShareOrigin;
import org.chromium.chrome.browser.share.ShareHelper;
import org.chromium.chrome.browser.tasks.tab_management.TabUiFeatureUtilities;
import org.chromium.components.browser_ui.share.ShareParams;
import org.chromium.components.embedder_support.contextmenu.ContextMenuParams;
import org.chromium.components.embedder_support.util.UrlUtilities;
import org.chromium.components.externalauth.ExternalAuthUtils;
import org.chromium.components.feature_engagement.FeatureConstants;
import org.chromium.components.feature_engagement.Tracker;
import org.chromium.components.search_engines.TemplateUrlService;
import org.chromium.components.ukm.UkmRecorder;
import org.chromium.components.url_formatter.UrlFormatter;
import org.chromium.content_public.browser.BrowserStartupController;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.common.ContentUrlConstants;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.url.GURL;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.ArrayList;
import java.util.Collections;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

/**
 * A {@link ContextMenuPopulator} used for showing the default Chrome context menu.
 */
public class ChromeContextMenuPopulator implements ContextMenuPopulator {
    private final Context mContext;
    private final ContextMenuItemDelegate mItemDelegate;
    private final @ContextMenuMode int mMode;
    private final Supplier<ShareDelegate> mShareDelegateSupplier;
    private final ExternalAuthUtils mExternalAuthUtils;
    private final ContextMenuParams mParams;
    private @Nullable UkmRecorder.Bridge mUkmRecorderBridge;
    private ContextMenuNativeDelegate mNativeDelegate;
    private static final String LENS_SEARCH_MENU_ITEM_KEY = "searchWithGoogleLensMenuItem";
    private static final String LENS_SHOP_MENU_ITEM_KEY = "shopWithGoogleLensMenuItem";
    private static final String SEARCH_BY_IMAGE_MENU_ITEM_KEY = "searchByImageMenuItem";
    private static final String LENS_SUPPORT_STATUS_HISTOGRAM_NAME =
            "ContextMenu.LensSupportStatus";

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
                Action.SEARCH_SIMILAR_PRODUCTS, Action.READ_LATER,
                Action.SHOP_WITH_GOOGLE_LENS_CHIP, Action.TRANSLATE_WITH_GOOGLE_LENS_CHIP})
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
            int READ_LATER = 33;
            int SHOP_WITH_GOOGLE_LENS_CHIP = 34;
            int TRANSLATE_WITH_GOOGLE_LENS_CHIP = 35;
            int NUM_ENTRIES = 36;
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
        static void recordSaveLinkTypes(GURL url) {
            String extension = MimeTypeMap.getFileExtensionFromUrl(url.getSpec());
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
    }

    /**
     * Builds a {@link ChromeContextMenuPopulator}.
     * @param itemDelegate The {@link ContextMenuItemDelegate} that will be notified with actions
     *                 to perform when menu items are selected.
     * @param shareDelegate The Supplier of {@link ShareDelegate} that will be notified when a share
     *                      action is performed.
     * @param mode Defines the context menu mode
     * @param externalAuthUtils {@link ExternalAuthUtils} instance.
     * @param context The {@link Context} used to retrieve the strings.
     * @param params The {@link ContextMenuParams} to populate the menu items.
     * @param nativeDelegate The {@link ContextMenuNativeDelegate} used to interact with native.
     */
    public ChromeContextMenuPopulator(ContextMenuItemDelegate itemDelegate,
            Supplier<ShareDelegate> shareDelegate, @ContextMenuMode int mode,
            ExternalAuthUtils externalAuthUtils, Context context, ContextMenuParams params,
            ContextMenuNativeDelegate nativeDelegate) {
        mItemDelegate = itemDelegate;
        mShareDelegateSupplier = shareDelegate;
        mMode = mode;
        mExternalAuthUtils = externalAuthUtils;
        mContext = context;
        mParams = params;
        mNativeDelegate = nativeDelegate;
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
        // ContextMenuParams can only be created after the browser has started.
        assert BrowserStartupController.getInstance().isFullBrowserStarted();
        return UrlFormatter.formatUrlForDisplayOmitSchemeOmitTrivialSubdomains(
                params.getLinkUrl().getSpec());
    }

    @Override
    public List<Pair<Integer, ModelList>> buildContextMenu() {
        boolean hasSaveImage = false;
        mShowEphemeralTabNewLabel = null;

        List<Pair<Integer, ModelList>> groupedItems = new ArrayList<>();

        if (mParams.isAnchor()) {
            ModelList linkGroup = new ModelList();
            if (FirstRunStatus.getFirstRunFlowComplete() && !isEmptyUrl(mParams.getUrl())
                    && UrlUtilities.isAcceptedScheme(mParams.getUrl().getSpec())) {
                if (mMode == ContextMenuMode.NORMAL) {
                    if (TabUiFeatureUtilities.ENABLE_TAB_GROUP_AUTO_CREATION.getValue()) {
                        linkGroup.add(createListItem(Item.OPEN_IN_NEW_TAB));
                    } else {
                        if (TabUiFeatureUtilities.showContextMenuOpenNewTabInGroupItemFirst()) {
                            linkGroup.add(createListItem(Item.OPEN_IN_NEW_TAB_IN_GROUP));
                            linkGroup.add(createListItem(Item.OPEN_IN_NEW_TAB));
                        } else {
                            linkGroup.add(createListItem(Item.OPEN_IN_NEW_TAB));
                            linkGroup.add(createListItem(Item.OPEN_IN_NEW_TAB_IN_GROUP));
                        }
                    }
                    if (!mItemDelegate.isIncognito() && mItemDelegate.isIncognitoSupported()) {
                        linkGroup.add(createListItem(Item.OPEN_IN_INCOGNITO_TAB));
                    }
                    if (mItemDelegate.isOpenInOtherWindowSupported()) {
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
            if (!MailTo.isMailTo(mParams.getLinkUrl().getSpec())
                    && !UrlUtilities.isTelScheme(mParams.getLinkUrl())) {
                linkGroup.add(createListItem(Item.COPY_LINK_ADDRESS));
                if (!mParams.getLinkText().trim().isEmpty() && !mParams.isImage()) {
                    linkGroup.add(createListItem(Item.COPY_LINK_TEXT));
                }
            }
            if (FirstRunStatus.getFirstRunFlowComplete()) {
                if (!mItemDelegate.isIncognito()
                        && UrlUtilities.isDownloadableScheme(mParams.getLinkUrl())) {
                    linkGroup.add(createListItem(Item.SAVE_LINK_AS));
                }
                if (!mParams.isImage() && ChromeFeatureList.isEnabled(ChromeFeatureList.READ_LATER)
                        && ReadingListUtils.isReadingListSupported(
                                mParams.getLinkUrl().getValidSpecOrEmpty())) {
                    linkGroup.add(createListItem(Item.READ_LATER, shouldTriggerReadLaterHelpUi()));
                }
                linkGroup.add(createShareListItem(Item.SHARE_LINK, Item.DIRECT_SHARE_LINK));
                if (UrlUtilities.isTelScheme(mParams.getLinkUrl())) {
                    if (mItemDelegate.supportsCall()) {
                        linkGroup.add(createListItem(Item.CALL));
                    }
                    if (mItemDelegate.supportsSendTextMessage()) {
                        linkGroup.add(createListItem(Item.SEND_MESSAGE));
                    }
                    if (mItemDelegate.supportsAddToContacts()) {
                        linkGroup.add(createListItem(Item.ADD_TO_CONTACTS));
                    }
                }
                if (MailTo.isMailTo(mParams.getLinkUrl().getSpec())) {
                    if (mItemDelegate.supportsSendEmailMessage()) {
                        linkGroup.add(createListItem(Item.SEND_MESSAGE));
                    }
                    if (!TextUtils.isEmpty(MailTo.parse(mParams.getLinkUrl().getSpec()).getTo())
                            && mItemDelegate.supportsAddToContacts()) {
                        linkGroup.add(createListItem(Item.ADD_TO_CONTACTS));
                    }
                }
            }
            if (UrlUtilities.isTelScheme(mParams.getLinkUrl())
                    || MailTo.isMailTo(mParams.getLinkUrl().getSpec())) {
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
                    && !mItemDelegate.getPageUrl().equals(mParams.getSrcUrl())) {
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
                imageGroup.add(createShareListItem(Item.SHARE_IMAGE, Item.DIRECT_SHARE_IMAGE));
            }

            if (mMode == ContextMenuMode.CUSTOM_TAB || mMode == ContextMenuMode.NORMAL) {
                if (checkSupportsGoogleSearchByImage(isSrcDownloadableScheme)) {
                    // All behavior relating to Lens integration is gated by Feature Flag.
                    // A map to indicate which image search menu item would be shown.
                    Map<String, Boolean> imageSearchMenuItemsToShow =
                            getSearchByImageMenuItemsToShowAndRecordMetrics(
                                    mParams.getPageUrl(), mItemDelegate.isIncognito());
                    if (imageSearchMenuItemsToShow.get(LENS_SEARCH_MENU_ITEM_KEY)) {
                        imageGroup.add(createListItem(Item.SEARCH_WITH_GOOGLE_LENS, true));
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
                    LensUma.recordLensSupportStatus(LENS_SUPPORT_STATUS_HISTOGRAM_NAME,
                            LensUma.LensSupportStatus.SEARCH_BY_IMAGE_UNAVAILABLE);
                }
            }

            // By default show 'Share Image' after 'Search with Google Lens'.
            // IMPORTANT: Must stay consistent with logic before the above Lens block.
            if (!addedShareImageAboveLens) {
                imageGroup.add(createShareListItem(Item.SHARE_IMAGE, Item.DIRECT_SHARE_IMAGE));
            }

            // Show Lens Shopping Menu Item when the Lens Shopping feature is supported.
            if (showLensShoppingMenuItem) {
                if (LensUtils.useLensWithShopSimilarProducts()) {
                    imageGroup.add(createListItem(Item.SHOP_SIMILAR_PRODUCTS, true));
                    // If the image is classified as shoppy always use the Shop Image with Google
                    // Lens item text.
                } else if (LensUtils.useLensWithShopImageWithGoogleLens()) {
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
            } else if (mMode == ContextMenuMode.CUSTOM_TAB
                    && mItemDelegate.supportsOpenInChromeFromCct()) {
                boolean addNewEntries = !UrlUtilities.isInternalScheme(mParams.getUrl())
                        && !isEmptyUrl(mParams.getUrl());
                if (SharedPreferencesManager.getInstance().readBoolean(
                            ChromePreferenceKeys.CHROME_DEFAULT_BROWSER, false)
                        && addNewEntries) {
                    if (mItemDelegate.isIncognitoSupported()) {
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

    @VisibleForTesting
    boolean shouldTriggerReadLaterHelpUi() {
        Tracker tracker = TrackerFactory.getTrackerForProfile(Profile.getLastUsedRegularProfile());
        return tracker.isInitialized()
                && tracker.shouldTriggerHelpUI(FeatureConstants.READ_LATER_CONTEXT_MENU_FEATURE);
    }

    @Override
    public boolean isIncognito() {
        return mItemDelegate.isIncognito();
    }

    @Override
    public String getPageTitle() {
        return mItemDelegate.getPageTitle();
    }

    @Override
    public boolean onItemSelected(int itemId) {
        if (itemId == R.id.contextmenu_open_in_new_tab) {
            recordContextMenuSelection(ContextMenuUma.Action.OPEN_IN_NEW_TAB);
            mItemDelegate.onOpenInNewTab(mParams.getUrl(), mParams.getReferrer());
        } else if (itemId == R.id.contextmenu_open_in_new_tab_in_group) {
            mItemDelegate.onOpenInNewTabInGroup(mParams.getUrl(), mParams.getReferrer());
        } else if (itemId == R.id.contextmenu_open_in_incognito_tab) {
            recordContextMenuSelection(ContextMenuUma.Action.OPEN_IN_INCOGNITO_TAB);
            mItemDelegate.onOpenInNewIncognitoTab(mParams.getUrl());
        } else if (itemId == R.id.contextmenu_open_in_other_window) {
            recordContextMenuSelection(ContextMenuUma.Action.OPEN_IN_OTHER_WINDOW);
            mItemDelegate.onOpenInOtherWindow(mParams.getUrl(), mParams.getReferrer());
        } else if (itemId == R.id.contextmenu_open_in_ephemeral_tab) {
            recordContextMenuSelection(ContextMenuUma.Action.OPEN_IN_EPHEMERAL_TAB);
            mItemDelegate.onOpenInEphemeralTab(mParams.getUrl(), mParams.getLinkText());
        } else if (itemId == R.id.contextmenu_open_image) {
            recordContextMenuSelection(ContextMenuUma.Action.OPEN_IMAGE);
            mItemDelegate.onOpenImageUrl(mParams.getSrcUrl(), mParams.getReferrer());
        } else if (itemId == R.id.contextmenu_open_image_in_new_tab) {
            recordContextMenuSelection(ContextMenuUma.Action.OPEN_IMAGE_IN_NEW_TAB);
            mItemDelegate.onOpenImageInNewTab(mParams.getSrcUrl(), mParams.getReferrer());
        } else if (itemId == R.id.contextmenu_open_image_in_ephemeral_tab) {
            recordContextMenuSelection(ContextMenuUma.Action.OPEN_IMAGE_IN_EPHEMERAL_TAB);
            String title = mParams.getTitleText();
            if (TextUtils.isEmpty(title)) {
                title = URLUtil.guessFileName(mParams.getSrcUrl().getSpec(), null, null);
            }
            mItemDelegate.onOpenInEphemeralTab(mParams.getSrcUrl(), title);
        } else if (itemId == R.id.contextmenu_copy_image) {
            recordContextMenuSelection(ContextMenuUma.Action.COPY_IMAGE);
            copyImageToClipboard();
        } else if (itemId == R.id.contextmenu_copy_link_address) {
            recordContextMenuSelection(ContextMenuUma.Action.COPY_LINK_ADDRESS);
            mItemDelegate.onSaveToClipboard(mParams.getUnfilteredLinkUrl().getSpec(),
                    ContextMenuItemDelegate.ClipboardType.LINK_URL);
        } else if (itemId == R.id.contextmenu_call) {
            recordContextMenuSelection(ContextMenuUma.Action.CALL);
            mItemDelegate.onCall(mParams.getLinkUrl());
        } else if (itemId == R.id.contextmenu_send_message) {
            if (MailTo.isMailTo(mParams.getLinkUrl().getSpec())) {
                recordContextMenuSelection(ContextMenuUma.Action.SEND_EMAIL);
                mItemDelegate.onSendEmailMessage(mParams.getLinkUrl());
            } else if (UrlUtilities.isTelScheme(mParams.getLinkUrl())) {
                recordContextMenuSelection(ContextMenuUma.Action.SEND_TEXT_MESSAGE);
                mItemDelegate.onSendTextMessage(mParams.getLinkUrl());
            }
        } else if (itemId == R.id.contextmenu_add_to_contacts) {
            recordContextMenuSelection(ContextMenuUma.Action.ADD_TO_CONTACTS);
            mItemDelegate.onAddToContacts(mParams.getLinkUrl());
        } else if (itemId == R.id.contextmenu_copy) {
            if (MailTo.isMailTo(mParams.getLinkUrl().getSpec())) {
                recordContextMenuSelection(ContextMenuUma.Action.COPY_EMAIL_ADDRESS);
                mItemDelegate.onSaveToClipboard(
                        MailTo.parse(mParams.getLinkUrl().getSpec()).getTo(),
                        ContextMenuItemDelegate.ClipboardType.LINK_URL);
            } else if (UrlUtilities.isTelScheme(mParams.getLinkUrl())) {
                recordContextMenuSelection(ContextMenuUma.Action.COPY_PHONE_NUMBER);
                mItemDelegate.onSaveToClipboard(UrlUtilities.getTelNumber(mParams.getLinkUrl()),
                        ContextMenuItemDelegate.ClipboardType.LINK_URL);
            }
        } else if (itemId == R.id.contextmenu_copy_link_text) {
            recordContextMenuSelection(ContextMenuUma.Action.COPY_LINK_TEXT);
            mItemDelegate.onSaveToClipboard(
                    mParams.getLinkText(), ContextMenuItemDelegate.ClipboardType.LINK_TEXT);
        } else if (itemId == R.id.contextmenu_save_image) {
            recordContextMenuSelection(ContextMenuUma.Action.SAVE_IMAGE);
            if (mItemDelegate.startDownload(mParams.getSrcUrl(), false)) {
                mNativeDelegate.startDownload(false);
            }
        } else if (itemId == R.id.contextmenu_save_video) {
            recordContextMenuSelection(ContextMenuUma.Action.SAVE_VIDEO);
            if (mItemDelegate.startDownload(mParams.getSrcUrl(), false)) {
                mNativeDelegate.startDownload(false);
            }
        } else if (itemId == R.id.contextmenu_save_link_as) {
            recordContextMenuSelection(ContextMenuUma.Action.SAVE_LINK);
            GURL url = mParams.getUnfilteredLinkUrl();
            if (mItemDelegate.startDownload(url, true)) {
                ContextMenuUma.recordSaveLinkTypes(url);
                mNativeDelegate.startDownload(true);
            }
        } else if (itemId == R.id.contextmenu_share_link) {
            recordContextMenuSelection(ContextMenuUma.Action.SHARE_LINK);
            // TODO(https://crbug.com/783819): Migrate ShareParams to GURL.
            ShareParams linkShareParams =
                    new ShareParams
                            .Builder(getWindow(), ContextMenuUtils.getTitle(mParams),
                                    mParams.getUrl().getSpec())
                            .build();
            mShareDelegateSupplier.get().share(linkShareParams,
                    new ChromeShareExtras.Builder().setSaveLastUsed(true).build(),
                    ShareOrigin.CONTEXT_MENU);
        } else if (itemId == R.id.contextmenu_read_later) {
            recordContextMenuSelection(ContextMenuUma.Action.READ_LATER);
            // TODO(crbug.com/1147475): Download the page to offline page backend.
            String title = mParams.getTitleText();
            if (TextUtils.isEmpty(title)) {
                title = mParams.getLinkText();
            }
            mItemDelegate.onReadLater(mParams.getUrl(), title);
        } else if (itemId == R.id.contextmenu_direct_share_link) {
            recordContextMenuSelection(ContextMenuUma.Action.DIRECT_SHARE_LINK);
            final ShareParams shareParams =
                    new ShareParams
                            .Builder(getWindow(), mParams.getUrl().getSpec(),
                                    mParams.getUrl().getSpec())
                            .build();
            ShareHelper.shareWithLastUsedComponent(shareParams);
        } else if (itemId == R.id.contextmenu_search_with_google_lens) {
            recordContextMenuSelection(ContextMenuUma.Action.SEARCH_WITH_GOOGLE_LENS);
            searchWithGoogleLens(
                    LensEntryPoint.CONTEXT_MENU_SEARCH_MENU_ITEM, /*requiresConfirmation=*/false);
            SharedPreferencesManager prefManager = SharedPreferencesManager.getInstance();
            prefManager.writeBoolean(
                    ChromePreferenceKeys.CONTEXT_MENU_SEARCH_WITH_GOOGLE_LENS_CLICKED, true);
        } else if (itemId == R.id.contextmenu_search_by_image) {
            recordContextMenuSelection(ContextMenuUma.Action.SEARCH_BY_IMAGE);
            mNativeDelegate.searchForImage();
        } else if (itemId == R.id.contextmenu_shop_similar_products) {
            recordContextMenuSelection(ContextMenuUma.Action.SHOP_SIMILAR_PRODUCTS);
            searchWithGoogleLens(
                    LensEntryPoint.CONTEXT_MENU_SHOP_MENU_ITEM, /*requiresConfirmation=*/true);
            SharedPreferencesManager prefManager = SharedPreferencesManager.getInstance();
            prefManager.writeBoolean(
                    ChromePreferenceKeys.CONTEXT_MENU_SHOP_SIMILAR_PRODUCTS_CLICKED, true);
        } else if (itemId == R.id.contextmenu_shop_image_with_google_lens) {
            recordContextMenuSelection(ContextMenuUma.Action.SHOP_IMAGE_WITH_GOOGLE_LENS);
            searchWithGoogleLens(
                    LensEntryPoint.CONTEXT_MENU_SHOP_MENU_ITEM, /*requiresConfirmation=*/false);
            SharedPreferencesManager prefManager = SharedPreferencesManager.getInstance();
            prefManager.writeBoolean(
                    ChromePreferenceKeys.CONTEXT_MENU_SHOP_IMAGE_WITH_GOOGLE_LENS_CLICKED, true);
        } else if (itemId == R.id.contextmenu_search_similar_products) {
            recordContextMenuSelection(ContextMenuUma.Action.SEARCH_SIMILAR_PRODUCTS);
            searchWithGoogleLens(
                    LensEntryPoint.CONTEXT_MENU_SHOP_MENU_ITEM, /*requiresConfirmation=*/true);
            SharedPreferencesManager prefManager = SharedPreferencesManager.getInstance();
            prefManager.writeBoolean(
                    ChromePreferenceKeys.CONTEXT_MENU_SEARCH_SIMILAR_PRODUCTS_CLICKED, true);
        } else if (itemId == R.id.contextmenu_share_image) {
            recordContextMenuSelection(ContextMenuUma.Action.SHARE_IMAGE);
            shareImage();
        } else if (itemId == R.id.contextmenu_direct_share_image) {
            recordContextMenuSelection(ContextMenuUma.Action.DIRECT_SHARE_IMAGE);
            mNativeDelegate.retrieveImageForShare(ContextMenuImageFormat.ORIGINAL, (Uri uri) -> {
                ShareHelper.shareImage(getWindow(), ShareHelper.getLastShareComponentName(), uri);
            });
        } else if (itemId == R.id.contextmenu_open_in_chrome) {
            recordContextMenuSelection(ContextMenuUma.Action.OPEN_IN_CHROME);
            mItemDelegate.onOpenInChrome(mParams.getUrl(), mParams.getPageUrl());
        } else if (itemId == R.id.contextmenu_open_in_new_chrome_tab) {
            recordContextMenuSelection(ContextMenuUma.Action.OPEN_IN_NEW_CHROME_TAB);
            mItemDelegate.onOpenInNewChromeTabFromCCT(mParams.getUrl(), false);
        } else if (itemId == R.id.contextmenu_open_in_chrome_incognito_tab) {
            recordContextMenuSelection(ContextMenuUma.Action.OPEN_IN_CHROME_INCOGNITO_TAB);
            mItemDelegate.onOpenInNewChromeTabFromCCT(mParams.getUrl(), true);
        } else if (itemId == R.id.contextmenu_open_in_browser_id) {
            recordContextMenuSelection(ContextMenuUma.Action.OPEN_IN_BROWSER);
            mItemDelegate.onOpenInDefaultBrowser(mParams.getUrl());
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
        return mItemDelegate.getWebContents().getTopLevelNativeWindow();
    }

    private Activity getActivity() {
        return getWindow().getActivity().get();
    }

    /**
     * Copy the image, that triggered the current context menu, to system clipboard.
     */
    private void copyImageToClipboard() {
        mNativeDelegate.retrieveImageForShare(
                ContextMenuImageFormat.ORIGINAL, mItemDelegate::onSaveImageToClipboard);
    }

    /**
     * Share the image that triggered the current context menu.
     * Package-private, allowing access only from the context menu item to ensure that
     * it will use the right activity set when the menu was displayed.
     */
    private void shareImage() {
        mNativeDelegate.retrieveImageForShare(ContextMenuImageFormat.ORIGINAL, (Uri imageUri) -> {
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
                            .build(),
                    ShareOrigin.CONTEXT_MENU);
        });
    }

    /**
     * @return The service that handles TemplateUrls.
     */
    protected TemplateUrlService getTemplateUrlService() {
        return TemplateUrlServiceFactory.get();
    }

    /**
     * Search for the image by intenting to the lens app with the image data attached.
     * @param lensEntryPoint The entry point that launches the Lens app.
     * @param requiresConfirmation Whether the request requires an account dialog.
     */
    protected void searchWithGoogleLens(
            @LensEntryPoint int lensEntryPoint, boolean requiresConfirmation) {
        mNativeDelegate.retrieveImageForShare(ContextMenuImageFormat.PNG, (Uri imageUri) -> {
            ShareHelper.shareImageWithGoogleLens(getWindow(), imageUri, mItemDelegate.isIncognito(),
                    mParams.getSrcUrl(), mParams.getTitleText(), mParams.getPageUrl(),
                    lensEntryPoint, requiresConfirmation);
        });
    }

    @Override
    public @Nullable ChipDelegate getChipDelegate() {
        // TODO(crbug/1181101): Use #isLensAvailable to check Lens availablility before creating
        // chip delegate.
        if (LensUtils.enableImageChip() || LensUtils.enableTranslateChip()) {
            // TODO(crbug.com/783819): Migrate LensChipDelegate to GURL.
            return new LensChipDelegate(mParams.getPageUrl().getSpec(), mParams.getTitleText(),
                    mParams.getSrcUrl().getSpec(), getPageTitle(), isIncognito(),
                    mItemDelegate.getWebContents(), mNativeDelegate, getOnChipClickedCallback(),
                    getOnChipShownCallback());
        }
        return null;
    }

    private Callback<Integer> getOnChipShownCallback() {
        return (Integer result) -> {
            int chipType = result.intValue();
            switch (chipType) {
                case ChipRenderParams.ChipType.LENS_SHOPPING_CHIP:
                    maybeRecordBooleanUkm("ContextMenuAndroid.Shown", "ShopWithGoogleLensChip");
                    return;
                case ChipRenderParams.ChipType.LENS_TRANSLATE_CHIP:
                    maybeRecordBooleanUkm(
                            "ContextMenuAndroid.Shown", "TranslateWithGoogleLensChip");
                    return;
                default:
                    // Unreachable value.
                    throw new IllegalArgumentException("Invalid chip type provided to callback.");
            }
        };
    }

    private Callback<Integer> getOnChipClickedCallback() {
        return (Integer result) -> {
            int chipType = result.intValue();
            switch (chipType) {
                case ChipRenderParams.ChipType.LENS_SHOPPING_CHIP:
                    recordContextMenuSelection(ContextMenuUma.Action.SHOP_WITH_GOOGLE_LENS_CHIP);
                    return;
                case ChipRenderParams.ChipType.LENS_TRANSLATE_CHIP:
                    recordContextMenuSelection(
                            ContextMenuUma.Action.TRANSLATE_WITH_GOOGLE_LENS_CHIP);
                    return;
                default:
                    // Unreachable value.
                    throw new IllegalArgumentException("Invalid chip type provided to callback.");
            }
        };
    }

    /**
     * Checks whether a url is empty or blank.
     * @param url The url need to be checked.
     * @return True if the url is empty or "about:blank".
     */
    private static boolean isEmptyUrl(GURL url) {
        return url == null || url.isEmpty()
                || url.getSpec().equals(ContentUrlConstants.ABOUT_BLANK_DISPLAY_URL);
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
        ContextMenuUma.record(mItemDelegate.getWebContents(), mParams, actionId);
        maybeRecordActionUkm("ContextMenuAndroid.Selected", actionId);
    }

    /**
     * Whether the lens menu items should be shown based on a set of application
     * compatibility checks.
     *
     * @param pageUrl The Url associated with the main frame of the page that triggered the context
     *         menu.
     * @param isIncognito Whether the user is incognito.
     * @return An immutable map. Can be used to check whether a specific Lens menu item is enabled.
     */
    private Map<String, Boolean> getSearchByImageMenuItemsToShowAndRecordMetrics(
            GURL pageUrl, boolean isIncognito) {
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
            LensUma.recordLensSupportStatus(LENS_SUPPORT_STATUS_HISTOGRAM_NAME,
                    LensUma.LensSupportStatus.NON_GOOGLE_SEARCH_ENGINE);

            return Collections.unmodifiableMap(new HashMap<String, Boolean>() {
                {
                    put(LENS_SEARCH_MENU_ITEM_KEY, false);
                    put(LENS_SHOP_MENU_ITEM_KEY, false);
                    put(SEARCH_BY_IMAGE_MENU_ITEM_KEY, true);
                }
            });
        }
        if (TextUtils.isEmpty(versionName)) {
            LensUma.recordLensSupportStatus(LENS_SUPPORT_STATUS_HISTOGRAM_NAME,
                    LensUma.LensSupportStatus.ACTIVITY_NOT_ACCESSIBLE);
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
            LensUma.recordLensSupportStatus(
                    LENS_SUPPORT_STATUS_HISTOGRAM_NAME, LensUma.LensSupportStatus.OUT_OF_DATE);
            return Collections.unmodifiableMap(new HashMap<String, Boolean>() {
                {
                    put(LENS_SEARCH_MENU_ITEM_KEY, false);
                    put(LENS_SHOP_MENU_ITEM_KEY, false);
                    put(SEARCH_BY_IMAGE_MENU_ITEM_KEY, true);
                }
            });
        }

        if (LensUtils.isDeviceOsBelowMinimum()) {
            LensUma.recordLensSupportStatus(
                    LENS_SUPPORT_STATUS_HISTOGRAM_NAME, LensUma.LensSupportStatus.LEGACY_OS);
            return Collections.unmodifiableMap(new HashMap<String, Boolean>() {
                {
                    put(LENS_SEARCH_MENU_ITEM_KEY, false);
                    put(LENS_SHOP_MENU_ITEM_KEY, false);
                    put(SEARCH_BY_IMAGE_MENU_ITEM_KEY, true);
                }
            });
        }

        if (!LensUtils.isValidAgsaPackage(mExternalAuthUtils)) {
            LensUma.recordLensSupportStatus(
                    LENS_SUPPORT_STATUS_HISTOGRAM_NAME, LensUma.LensSupportStatus.INVALID_PACKAGE);
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
            if (LensUtils.isInShoppingAllowlist(pageUrl)) {
                // Hide Search With Google Lens menu item when experiment only with Lens Shopping
                // menu items.
                if (!LensUtils.showBothSearchAndShopImageWithLens()) {
                    LensUma.recordLensSupportStatus(LENS_SUPPORT_STATUS_HISTOGRAM_NAME,
                            LensUma.LensSupportStatus.LENS_SHOP_SUPPORTED);
                    return Collections.unmodifiableMap(new HashMap<String, Boolean>() {
                        {
                            put(LENS_SEARCH_MENU_ITEM_KEY, false);
                            put(LENS_SHOP_MENU_ITEM_KEY, true);
                            put(SEARCH_BY_IMAGE_MENU_ITEM_KEY, false);
                        }
                    });
                }
                LensUma.recordLensSupportStatus(LENS_SUPPORT_STATUS_HISTOGRAM_NAME,
                        LensUma.LensSupportStatus.LENS_SHOP_AND_SEARCH_SUPPORTED);
                return Collections.unmodifiableMap(new HashMap<String, Boolean>() {
                    {
                        put(LENS_SEARCH_MENU_ITEM_KEY, true);
                        put(LENS_SHOP_MENU_ITEM_KEY, true);
                        put(SEARCH_BY_IMAGE_MENU_ITEM_KEY, false);
                    }
                });
            }
        }

        LensUma.recordLensSupportStatus(LENS_SUPPORT_STATUS_HISTOGRAM_NAME,
                LensUma.LensSupportStatus.LENS_SEARCH_SUPPORTED);
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

    private ListItem createShareListItem(@Item int item, @Item int iconButtonItem) {
        final boolean isLink = item == Item.SHARE_LINK;
        final Pair<Drawable, CharSequence> shareInfo = createRecentShareAppInfo(isLink);
        final PropertyModel model =
                new PropertyModel.Builder(RevampedContextMenuItemWithIconButtonProperties.ALL_KEYS)
                        .with(MENU_ID, ChromeContextMenuItem.getMenuId(item))
                        .with(TEXT, ChromeContextMenuItem.getTitle(mContext, item, false))
                        .with(BUTTON_IMAGE, shareInfo.first)
                        .with(BUTTON_CONTENT_DESC, shareInfo.second)
                        .with(BUTTON_MENU_ID, ChromeContextMenuItem.getMenuId(iconButtonItem))
                        .build();
        return new ListItem(ListItemType.CONTEXT_MENU_ITEM_WITH_ICON_BUTTON, model);
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
        if (!LensUtils.shouldLogUkm(mItemDelegate.isIncognito())) return;
        initializeUkmRecorderBridge();
        WebContents webContents = mItemDelegate.getWebContents();
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
        if (!LensUtils.shouldLogUkm(mItemDelegate.isIncognito())) return;
        initializeUkmRecorderBridge();
        WebContents webContents = mItemDelegate.getWebContents();
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
}
