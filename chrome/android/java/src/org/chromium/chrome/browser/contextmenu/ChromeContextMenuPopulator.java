// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.contextmenu;

import android.app.Activity;
import android.content.Context;
import android.net.MailTo;
import android.text.TextUtils;
import android.util.Pair;
import android.webkit.URLUtil;
import android.widget.Toast;

import androidx.annotation.IntDef;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.Callback;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.contextmenu.ChromeContextMenuItem.Item;
import org.chromium.chrome.browser.contextmenu.ContextMenuCoordinator.ListItemType;
import org.chromium.chrome.browser.feature_engagement.TrackerFactory;
import org.chromium.chrome.browser.locale.LocaleManager;
import org.chromium.chrome.browser.performance_hints.PerformanceHintsObserver;
import org.chromium.chrome.browser.performance_hints.PerformanceHintsObserver.PerformanceClass;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.SharedPreferencesManager;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.search_engines.TemplateUrlServiceFactory;
import org.chromium.components.embedder_support.contextmenu.ContextMenuParams;
import org.chromium.components.embedder_support.util.UrlUtilities;
import org.chromium.components.feature_engagement.FeatureConstants;
import org.chromium.components.feature_engagement.Tracker;
import org.chromium.components.search_engines.TemplateUrlService;
import org.chromium.components.ukm.UkmRecorder;
import org.chromium.components.url_formatter.UrlFormatter;
import org.chromium.content_public.browser.BrowserStartupController;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.common.ContentUrlConstants;
import org.chromium.ui.base.DeviceFormFactor;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.url.GURL;
import org.chromium.url.Origin;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.ArrayList;
import java.util.List;

import static org.chromium.chrome.browser.contextmenu.ContextMenuItemProperties.MENU_ID;
import static org.chromium.chrome.browser.contextmenu.ContextMenuItemProperties.TEXT;

/**
 * A {@link ContextMenuPopulator} used for showing the default Chrome context menu.
 */
public class ChromeContextMenuPopulator implements ContextMenuPopulator {
    private final Context mContext;
    private final ContextMenuItemDelegate mItemDelegate;
    private final @ContextMenuMode int mMode;
    private final ContextMenuParams mParams;
    private final @Nullable Origin mInitiatingOrigin;
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
                Action.SHARE_IMAGE, Action.OPEN_IN_OTHER_WINDOW, Action.OPEN_IN_NEW_WINDOW,
                Action.SEND_EMAIL, Action.ADD_TO_CONTACTS, Action.CALL, Action.SEND_TEXT_MESSAGE,
                Action.COPY_PHONE_NUMBER, Action.OPEN_IN_NEW_CHROME_TAB,
                Action.OPEN_IN_CHROME_INCOGNITO_TAB, Action.OPEN_IN_BROWSER, Action.OPEN_IN_CHROME,
                Action.SHARE_LINK, Action.OPEN_IN_EPHEMERAL_TAB, Action.OPEN_IMAGE_IN_EPHEMERAL_TAB,
                Action.DIRECT_SHARE_LINK, Action.DIRECT_SHARE_IMAGE, Action.SEARCH_WITH_GOOGLE_LENS,
                Action.COPY_IMAGE, Action.SHOP_IMAGE_WITH_GOOGLE_LENS, Action.READ_LATER,
                Action.SHOP_WITH_GOOGLE_LENS_CHIP, Action.TRANSLATE_WITH_GOOGLE_LENS_CHIP,
                Action.SHARE_HIGHLIGHT, Action.REMOVE_HIGHLIGHT, Action.LEARN_MORE,
                Action.OPEN_IN_NEW_TAB_IN_GROUP})
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
            int DIRECT_SHARE_LINK = 26;
            int DIRECT_SHARE_IMAGE = 27;
            int SEARCH_WITH_GOOGLE_LENS = 28;
            int COPY_IMAGE = 29;
            // int SHOP_SIMILAR_PRODUCTS = 30;  Deprecated since 06/2021.
            int SHOP_IMAGE_WITH_GOOGLE_LENS = 31;
            // int SEARCH_SIMILAR_PRODUCTS = 32;  // Deprecated since 06/2021.
            int READ_LATER = 33;
            int SHOP_WITH_GOOGLE_LENS_CHIP = 34;
            int TRANSLATE_WITH_GOOGLE_LENS_CHIP = 35;
            int SHARE_HIGHLIGHT = 36;
            int REMOVE_HIGHLIGHT = 37;
            int LEARN_MORE = 38;
            int OPEN_IN_NEW_TAB_IN_GROUP = 39;
            int OPEN_IN_NEW_WINDOW = 40;
            int NUM_ENTRIES = 41;
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

        // This is used for recording the enum histogram:
        //   * ContextMenu.SelectedOptionAndroid.ImageLink.NewTabOption
        //   * ContextMenu.SelectedOptionAndroid.Link.NewTabOption
        // OPEN_IN_NEW_TAB_FIRST_SELECTED_OPEN_IN_NEW_TAB means the context menu shows the
        //   'open in new tab' item before the 'open in new tab in group' item and the
        //   'open in new tab' item is selected.
        // OPEN_IN_NEW_TAB_FIRST_SELECTED_OPEN_IN_NEW_TAB_IN_GROUP means the context menu shows the
        //    'open in new tab' item before the 'open in new tab in group' item and the
        //    'open in new tab in group' item is selected.
        // OPEN_IN_NEW_TAB_IN_GROUP_FIRST_SELECTED_OPEN_IN_NEW_TAB means the context menu shows the
        //    'open in new tab in group' item before the 'open in new tab' item and the
        //    'open in new tab' item is selected.
        // OPEN_IN_NEW_TAB_IN_GROUP_FIRST_SELECTED_OPEN_IN_NEW_TAB_IN_GROUP means the context menu
        // shows the
        //    'open in new tab in group' item before the 'open in new tab' item and the
        //    'open in new tab in group' item is selected.
        @IntDef({SelectedNewTabCreationEnum.OPEN_IN_NEW_TAB_FIRST_SELECTED_OPEN_IN_NEW_TAB,
                SelectedNewTabCreationEnum.OPEN_IN_NEW_TAB_FIRST_SELECTED_OPEN_IN_NEW_TAB_IN_GROUP,
                SelectedNewTabCreationEnum.OPEN_IN_NEW_TAB_IN_GROUP_FIRST_SELECTED_OPEN_IN_NEW_TAB,
                SelectedNewTabCreationEnum
                        .OPEN_IN_NEW_TAB_IN_GROUP_FIRST_SELECTED_OPEN_IN_NEW_TAB_IN_GROUP})
        @Retention(RetentionPolicy.SOURCE)
        private @interface SelectedNewTabCreationEnum {
            int OPEN_IN_NEW_TAB_FIRST_SELECTED_OPEN_IN_NEW_TAB = 0;
            int OPEN_IN_NEW_TAB_FIRST_SELECTED_OPEN_IN_NEW_TAB_IN_GROUP = 1;
            int OPEN_IN_NEW_TAB_IN_GROUP_FIRST_SELECTED_OPEN_IN_NEW_TAB = 2;
            int OPEN_IN_NEW_TAB_IN_GROUP_FIRST_SELECTED_OPEN_IN_NEW_TAB_IN_GROUP = 3;

            int NUM_ENTRIES = 4;
        }

        /**
         * Records a histogram entry when the user selects an item from a context menu.
         * @param params The ContextMenuParams describing the current context menu.
         * @param action The action that the user selected (e.g. ACTION_SAVE_IMAGE).
         */
        static void record(WebContents webContents, ContextMenuParams params, @Action int action) {
            String histogramName = String.format("ContextMenu.SelectedOptionAndroid.%s",
                    ContextMenuUtils.getContextMenuTypeForHistogram(params));

            // Record SharedHighlightingInteraction only for Shared Highlighting V2 menu options
            // (share highlight, remove highlight and learn more).
            if (params.getOpenedFromHighlight() && !params.isVideo() && !params.isImage()) {
                assert histogramName.equals(
                        "ContextMenu.SelectedOptionAndroid.SharedHighlightingInteraction");
                if (action != Action.SHARE_HIGHLIGHT || action != Action.REMOVE_HIGHLIGHT
                        || action != Action.LEARN_MORE) {
                    histogramName = "ContextMenu.SelectedOptionAndroid.Link";
                }
            }

            RecordHistogram.recordEnumeratedHistogram(histogramName, action, Action.NUM_ENTRIES);

            if (params.isAnchor() && !params.isVideo() && !params.getOpenedFromHighlight()) {
                if (params.isImage()) {
                    assert histogramName.equals("ContextMenu.SelectedOptionAndroid.ImageLink");
                } else {
                    assert histogramName.equals("ContextMenu.SelectedOptionAndroid.Link");
                }
            }

            if (params.isAnchor()
                    && PerformanceHintsObserver.getPerformanceClassForURL(
                               webContents, params.getLinkUrl())
                            == PerformanceClass.PERFORMANCE_FAST) {
                RecordHistogram.recordEnumeratedHistogram(
                        histogramName + ".PerformanceClassFast", action, Action.NUM_ENTRIES);
            }
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
     * @param mode Defines the context menu mode
     * @param context The {@link Context} used to retrieve the strings.
     * @param params The {@link ContextMenuParams} to populate the menu items.
     * @param nativeDelegate The {@link ContextMenuNativeDelegate} used to interact with native.
     */
    public ChromeContextMenuPopulator(ContextMenuItemDelegate itemDelegate,
            @ContextMenuMode int mode, Context context, ContextMenuParams params,
            ContextMenuNativeDelegate nativeDelegate) {
        mItemDelegate = itemDelegate;
        mMode = mode;
        mContext = context;
        mParams = params;
        if (itemDelegate.getWebContents() != null
                && itemDelegate.getWebContents().getFocusedFrame() != null) {
            mInitiatingOrigin =
                    itemDelegate.getWebContents().getFocusedFrame().getLastCommittedOrigin();
        } else {
            mInitiatingOrigin = null;
        }
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

    @VisibleForTesting
    boolean isTabletScreen() {
        return DeviceFormFactor.isNonMultiDisplayContextOnTablet(mContext);
    }

    @Override
    public List<Pair<Integer, ModelList>> buildContextMenu() {
        boolean hasSaveImage = false;
        mShowEphemeralTabNewLabel = null;

        List<Pair<Integer, ModelList>> groupedItems = new ArrayList<>();

        if (mParams.isAnchor()) {
            ModelList linkGroup = new ModelList();
            if (!isEmptyUrl(mParams.getUrl())
                    && UrlUtilities.isAcceptedScheme(mParams.getUrl())) {
                if (mMode == ContextMenuMode.NORMAL) {
                    linkGroup.add(createListItem(Item.OPEN_IN_NEW_TAB));
                    if (!mItemDelegate.isIncognito() && mItemDelegate.isIncognitoSupported()) {
                        linkGroup.add(createListItem(Item.OPEN_IN_INCOGNITO_TAB));
                    }
                    if (mItemDelegate.isOpenInOtherWindowSupported()) {
                        linkGroup.add(createListItem(Item.OPEN_IN_OTHER_WINDOW));
                    } else if (isTabletScreen() && mItemDelegate.canEnterMultiWindowMode()) {
                        linkGroup.add(createListItem(Item.OPEN_IN_NEW_WINDOW));
                    }
                }
//                if ((mMode == ContextMenuMode.NORMAL || mMode == ContextMenuMode.CUSTOM_TAB)) {
//                    mShowEphemeralTabNewLabel = shouldTriggerEphemeralTabHelpUi();
//                    linkGroup.add(
//                            createListItem(Item.OPEN_IN_EPHEMERAL_TAB, mShowEphemeralTabNewLabel));
//                }
            }
            if (!MailTo.isMailTo(mParams.getLinkUrl().getSpec())
                    && !UrlUtilities.isTelScheme(mParams.getLinkUrl())) {
                linkGroup.add(createListItem(Item.COPY_LINK_ADDRESS));
                if (!mParams.getLinkText().trim().isEmpty() && !mParams.isImage()) {
                    linkGroup.add(createListItem(Item.COPY_LINK_TEXT));
                }
            }
            if (!mItemDelegate.isIncognito()
                    && UrlUtilities.isDownloadableScheme(mParams.getLinkUrl())) {
                linkGroup.add(createListItem(Item.SAVE_LINK_AS));
            }
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
            if (UrlUtilities.isTelScheme(mParams.getLinkUrl())
                    || MailTo.isMailTo(mParams.getLinkUrl().getSpec())) {
                linkGroup.add(createListItem(Item.COPY));
            }
            if (linkGroup.size() > 0) {
                groupedItems.add(new Pair<>(R.string.contextmenu_link_title, linkGroup));
            }
        }

        if (mParams.isImage()) {
            ModelList imageGroup = new ModelList();
            boolean isSrcDownloadableScheme =
                    UrlUtilities.isDownloadableScheme(mParams.getSrcUrl());
            // Avoid showing open image option for same image which is already opened.
//            if (mMode == ContextMenuMode.CUSTOM_TAB
//                    && !mItemDelegate.getPageUrl().equals(mParams.getSrcUrl())) {
//                imageGroup.add(createListItem(Item.OPEN_IMAGE));
//            }
            if (!mItemDelegate.getPageUrl().equals(mParams.getSrcUrl())) {
                imageGroup.add(createListItem(Item.OPEN_IMAGE));
            }
//            if ((mMode == ContextMenuMode.NORMAL || mMode == ContextMenuMode.CUSTOM_TAB)) {
//                if (mShowEphemeralTabNewLabel == null) {
//                    mShowEphemeralTabNewLabel = shouldTriggerEphemeralTabHelpUi();
//                }
//                imageGroup.add(createListItem(
//                        Item.OPEN_IMAGE_IN_EPHEMERAL_TAB, mShowEphemeralTabNewLabel));
//            }
            imageGroup.add(createListItem(Item.COPY_IMAGE));
            if (isSrcDownloadableScheme) {
                imageGroup.add(createListItem(Item.SAVE_IMAGE));
                hasSaveImage = true;
            }

            if (checkSupportsGoogleSearchByImage(isSrcDownloadableScheme)) {
                imageGroup.add(createListItem(Item.SEARCH_BY_IMAGE));
            }

            recordSaveImageContextMenuResult(isSrcDownloadableScheme);
            groupedItems.add(new Pair<>(R.string.contextmenu_image_title, imageGroup));
        }

        if (mParams.isVideo() && mParams.canSaveMedia()
                && UrlUtilities.isDownloadableScheme(mParams.getSrcUrl())) {
            ModelList videoGroup = new ModelList();
            videoGroup.add(createListItem(Item.SAVE_VIDEO));
            groupedItems.add(new Pair<>(R.string.contextmenu_video_title, videoGroup));
        }

        if (mParams.getOpenedFromHighlight()) {
            ModelList sharedHighlightingGroup = new ModelList();
            if (mMode == ContextMenuMode.NORMAL) {
                sharedHighlightingGroup.add(createListItem(Item.SHARE_HIGHLIGHT));
            }
            sharedHighlightingGroup.add(createListItem(Item.REMOVE_HIGHLIGHT));
            if (mMode == ContextMenuMode.NORMAL) {
                sharedHighlightingGroup.add(createListItem(Item.LEARN_MORE));
            }
            groupedItems.add(new Pair<>(null, sharedHighlightingGroup));
        }

        if (mMode != ContextMenuMode.NORMAL) {
            ModelList items = groupedItems.isEmpty()
                    ? new ModelList()
                    : groupedItems
                              .get(mMode == ContextMenuMode.CUSTOM_TAB ? 0
                                                                       : groupedItems.size() - 1)
                              .second;
            if (UrlUtilities.isAcceptedScheme(mParams.getUrl())) {
                if (mMode == ContextMenuMode.WEB_APP) {
                    items.add(createListItem(Item.OPEN_IN_CHROME));
                } else if (mMode == ContextMenuMode.CUSTOM_TAB && !mItemDelegate.isIncognito()) {
                    boolean addNewEntries = !UrlUtilities.isInternalScheme(mParams.getUrl())
                            && !isEmptyUrl(mParams.getUrl());
                    if (SharedPreferencesManager.getInstance().readBoolean(
                                ChromePreferenceKeys.CHROME_DEFAULT_BROWSER, false)
                            && addNewEntries) {
                        if (mItemDelegate.isIncognitoSupported()) {
                            items.add(0, createListItem(Item.OPEN_IN_CHROME_INCOGNITO_TAB));
                        }
                        items.add(0, createListItem(Item.OPEN_IN_NEW_CHROME_TAB));
                    } else if (addNewEntries && UrlUtilities.isAcceptedScheme(mParams.getUrl())) {
                        items.add(0, createListItem(Item.OPEN_IN_BROWSER_ID));
                    }
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

        ModelList myGroup = new ModelList();
        myGroup.add(createListItem(Item.FREE_COPY));
        if (mItemDelegate.canMoveTab()) {
            myGroup.add(createListItem(Item.MOVE_TO_NEW_TAB));
        }

        if (!TextUtils.isEmpty(mParams.getCssSelector())
                || !TextUtils.isEmpty(mParams.getParentCssSelector())
                || !TextUtils.isEmpty(mParams.getTagName())
                || !TextUtils.isEmpty(mParams.getIdAttribute())
                || !TextUtils.isEmpty(mParams.getClassAttribute())
                || !TextUtils.isEmpty(mParams.getParentTagName())
                || !TextUtils.isEmpty(mParams.getParentIdAttribute())
                || !TextUtils.isEmpty(mParams.getParentClassAttribute())) {
            myGroup.add(createListItem(Item.MARK_ADS));
        }

        groupedItems.add(new Pair<>(null, myGroup));

        return groupedItems;
    }

    @VisibleForTesting
    boolean shouldTriggerEphemeralTabHelpUi() {
        Tracker tracker = TrackerFactory.getTrackerForProfile(getProfile());
        return tracker.isInitialized()
                && tracker.shouldTriggerHelpUI(FeatureConstants.EPHEMERAL_TAB_FEATURE);
    }

    @VisibleForTesting
    boolean shouldTriggerReadLaterHelpUi() {
        Tracker tracker = TrackerFactory.getTrackerForProfile(getProfile());
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
        if (itemId == R.id.contextmenu_move_to_new_tab) {
            mItemDelegate.moveTab();
        } else if (itemId == R.id.contextmenu_mark_ads) {
            Toast.makeText(mContext, "TODO mark ads", Toast.LENGTH_SHORT).show();
            mItemDelegate.onMarkAds(mParams);
        }  else if (itemId == R.id.contextmenu_free_copy) {
            Toast.makeText(mContext, "TODO free copy", Toast.LENGTH_SHORT).show();
            mItemDelegate.freeCopy(mParams);
        } else if (itemId == R.id.contextmenu_open_in_new_tab) {
            recordContextMenuSelection(ContextMenuUma.Action.OPEN_IN_NEW_TAB);
            mItemDelegate.onOpenInNewTab(
                    mParams.getUrl(), mParams.getReferrer(), /*navigateToTab=*/false);
        } else if (itemId == R.id.contextmenu_open_in_new_tab_in_group) {
            recordContextMenuSelection(ContextMenuUma.Action.OPEN_IN_NEW_TAB_IN_GROUP);
            mItemDelegate.onOpenInNewTabInGroup(mParams.getUrl(), mParams.getReferrer());
        } else if (itemId == R.id.contextmenu_open_in_incognito_tab) {
            recordContextMenuSelection(ContextMenuUma.Action.OPEN_IN_INCOGNITO_TAB);
            mItemDelegate.onOpenInNewIncognitoTab(mParams.getUrl(), mInitiatingOrigin);
        } else if (itemId == R.id.contextmenu_open_in_other_window) {
            recordContextMenuSelection(ContextMenuUma.Action.OPEN_IN_OTHER_WINDOW);
            mItemDelegate.onOpenInOtherWindow(mParams.getUrl(), mParams.getReferrer());
        } else if (itemId == R.id.contextmenu_open_in_new_window) {
            recordContextMenuSelection(ContextMenuUma.Action.OPEN_IN_NEW_WINDOW);
            // |openInOtherWindow| can handle opening in a new window as well.
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
                mNativeDelegate.startDownload(true);
            }
        } else if (itemId == R.id.contextmenu_share_link) {
            Toast.makeText(mContext, "分享链接", Toast.LENGTH_SHORT).show();
        } else if (itemId == R.id.contextmenu_read_later) {
            recordContextMenuSelection(ContextMenuUma.Action.READ_LATER);
            // TODO(crbug.com/1147475): Download the page to offline page backend.
            String title = mParams.getTitleText();
            if (TextUtils.isEmpty(title)) {
                title = mParams.getLinkText();
            }
            mItemDelegate.onReadLater(mParams.getUrl(), title);
        } else if (itemId == R.id.contextmenu_direct_share_link) {
            Toast.makeText(mContext, "直接分享链接", Toast.LENGTH_SHORT).show();
        } else if (itemId == R.id.contextmenu_search_by_image) {
            recordContextMenuSelection(ContextMenuUma.Action.SEARCH_BY_IMAGE);
            mNativeDelegate.searchForImage();
        } else if (itemId == R.id.contextmenu_share_image) {
            Toast.makeText(mContext, "图片分享", Toast.LENGTH_SHORT).show();
        } else if (itemId == R.id.contextmenu_direct_share_image) {
            Toast.makeText(mContext, "图片直接分享", Toast.LENGTH_SHORT).show();
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
        } else if (itemId == R.id.contextmenu_share_highlight) {
            Toast.makeText(mContext, "分享highlight", Toast.LENGTH_SHORT).show();
        } else if (itemId == R.id.contextmenu_remove_highlight) {
            Toast.makeText(mContext, "remove highlight", Toast.LENGTH_SHORT).show();
        } else if (itemId == R.id.contextmenu_learn_more) {
            Toast.makeText(mContext, "learn more", Toast.LENGTH_SHORT).show();
        } else {
            assert false;
        }

        return true;
    }

    @Override
    public void onMenuClosed() {
        if (mShowEphemeralTabNewLabel != null && mShowEphemeralTabNewLabel) {
            Tracker tracker = TrackerFactory.getTrackerForProfile(getProfile());
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
     * @return The service that handles TemplateUrls.
     */
    protected TemplateUrlService getTemplateUrlService() {
        return TemplateUrlServiceFactory.get();
    }

    @Override
    public @Nullable ChipDelegate getChipDelegate() {
        return null;
    }

    private Callback<Integer> getOnChipShownCallback() {
        return (Integer result) -> {
            int chipType = result.intValue();
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
    }

    private ListItem createListItem(@Item int item) {
        return createListItem(item, false);
    }

    private ListItem createListItem(@Item int item, boolean showInProductHelp) {
        final PropertyModel model =
                new PropertyModel.Builder(ContextMenuItemProperties.ALL_KEYS)
                        .with(MENU_ID, ChromeContextMenuItem.getMenuId(item))
                        .with(TEXT,
                                ChromeContextMenuItem.getTitle(mContext, item, showInProductHelp))
                        .build();
        return new ListItem(ListItemType.CONTEXT_MENU_ITEM, model);
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
        // Disable UKM reporting when incognito.
        if (mItemDelegate.isIncognito()) return;
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
        // Disable UKM reporting when incognito.
        if (mItemDelegate.isIncognito()) return;
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
        if (!templateUrlServiceInstance.isLoaded()) {
            templateUrlServiceInstance.load();
        }
        return isSrcDownloadableScheme && templateUrlServiceInstance.isLoaded()
                && templateUrlServiceInstance.isSearchByImageAvailable()
                && templateUrlServiceInstance.getDefaultSearchEngineTemplateUrl() != null
                && !LocaleManager.getInstance().needToCheckForSearchEnginePromo();
    }

    /** Returns the profile of the current tab via the item delegate. */
    private Profile getProfile() {
        return Profile.fromWebContents(mItemDelegate.getWebContents());
    }
}
