// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.contextmenu;

import static org.chromium.chrome.browser.contextmenu.ContextMenuItemProperties.ENABLED;
import static org.chromium.chrome.browser.contextmenu.ContextMenuItemProperties.MENU_ID;
import static org.chromium.chrome.browser.contextmenu.ContextMenuItemProperties.TEXT;
import static org.chromium.chrome.browser.contextmenu.ContextMenuItemWithIconButtonProperties.BUTTON_CONTENT_DESC;
import static org.chromium.chrome.browser.contextmenu.ContextMenuItemWithIconButtonProperties.BUTTON_IMAGE;
import static org.chromium.chrome.browser.contextmenu.ContextMenuItemWithIconButtonProperties.BUTTON_MENU_ID;

import android.app.Activity;
import android.content.ContentResolver;
import android.content.Context;
import android.graphics.drawable.Drawable;
import android.net.MailTo;
import android.net.Uri;
import android.text.TextUtils;
import android.util.Pair;
import android.webkit.URLUtil;

import androidx.annotation.IntDef;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.Callback;
import org.chromium.base.ContextUtils;
import org.chromium.base.shared_preferences.SharedPreferencesManager;
import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.contextmenu.ChromeContextMenuItem.Item;
import org.chromium.chrome.browser.contextmenu.ContextMenuCoordinator.ListItemType;
import org.chromium.chrome.browser.download.DownloadUtils;
import org.chromium.chrome.browser.ephemeraltab.EphemeralTabCoordinator;
import org.chromium.chrome.browser.feature_engagement.TrackerFactory;
import org.chromium.chrome.browser.firstrun.FirstRunStatus;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.gsa.GSAUtils;
import org.chromium.chrome.browser.lens.LensController;
import org.chromium.chrome.browser.lens.LensEntryPoint;
import org.chromium.chrome.browser.lens.LensIntentParams;
import org.chromium.chrome.browser.lens.LensMetrics;
import org.chromium.chrome.browser.locale.LocaleManager;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.read_later.ReadingListUtils;
import org.chromium.chrome.browser.search_engines.TemplateUrlServiceFactory;
import org.chromium.chrome.browser.share.ChromeShareExtras;
import org.chromium.chrome.browser.share.LensUtils;
import org.chromium.chrome.browser.share.ShareDelegate;
import org.chromium.chrome.browser.share.ShareDelegate.ShareOrigin;
import org.chromium.chrome.browser.share.ShareHelper;
import org.chromium.chrome.browser.share.ShareUtils;
import org.chromium.chrome.browser.share.link_to_text.LinkToTextHelper;
import org.chromium.chrome.browser.tab.TabContextMenuItemDelegate;
import org.chromium.components.browser_ui.share.ShareParams;
import org.chromium.components.embedder_support.contextmenu.ChipDelegate;
import org.chromium.components.embedder_support.contextmenu.ChipRenderParams;
import org.chromium.components.embedder_support.contextmenu.ContextMenuImageFormat;
import org.chromium.components.embedder_support.contextmenu.ContextMenuNativeDelegate;
import org.chromium.components.embedder_support.contextmenu.ContextMenuParams;
import org.chromium.components.embedder_support.contextmenu.ContextMenuPopulator;
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
import org.chromium.ui.base.DeviceFormFactor;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.widget.Toast;
import org.chromium.url.GURL;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.ArrayList;
import java.util.List;

/** A {@link ContextMenuPopulator} used for showing the default Chrome context menu. */
public class ChromeContextMenuPopulator implements ContextMenuPopulator {
    private final Context mContext;
    private final TabContextMenuItemDelegate mItemDelegate;
    private final @ContextMenuMode int mMode;
    private final Supplier<ShareDelegate> mShareDelegateSupplier;
    private final ExternalAuthUtils mExternalAuthUtils;
    private final ContextMenuParams mParams;
    private @Nullable UkmRecorder.Bridge mUkmRecorderBridge;
    private ContextMenuNativeDelegate mNativeDelegate;
    private static final String LENS_SUPPORT_STATUS_HISTOGRAM_NAME =
            "ContextMenu.LensSupportStatus";
    private final boolean mIsDownloadRestrictedByPolicy;

    // True when the tracker indicates IPH in the form of "new" label needs to be shown.
    private Boolean mShowEphemeralTabNewLabel;

    /** Defines the Groups of each Context Menu Item */
    @IntDef({ContextMenuGroup.LINK, ContextMenuGroup.IMAGE, ContextMenuGroup.VIDEO})
    @Retention(RetentionPolicy.SOURCE)
    public @interface ContextMenuGroup {
        int LINK = 0;
        int IMAGE = 1;
        int VIDEO = 2;
    }

    /** Defines the context menu modes */
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
        @IntDef({
            Action.OPEN_IN_NEW_TAB,
            Action.OPEN_IN_INCOGNITO_TAB,
            Action.COPY_LINK_ADDRESS,
            Action.COPY_EMAIL_ADDRESS,
            Action.COPY_LINK_TEXT,
            Action.SAVE_LINK,
            Action.SAVE_IMAGE,
            Action.OPEN_IMAGE,
            Action.OPEN_IMAGE_IN_NEW_TAB,
            Action.SEARCH_BY_IMAGE,
            Action.LOAD_ORIGINAL_IMAGE,
            Action.SAVE_VIDEO,
            Action.SHARE_IMAGE,
            Action.OPEN_IN_OTHER_WINDOW,
            Action.OPEN_IN_NEW_WINDOW,
            Action.SEND_EMAIL,
            Action.ADD_TO_CONTACTS,
            Action.CALL,
            Action.SEND_TEXT_MESSAGE,
            Action.COPY_PHONE_NUMBER,
            Action.OPEN_IN_NEW_CHROME_TAB,
            Action.OPEN_IN_CHROME_INCOGNITO_TAB,
            Action.OPEN_IN_BROWSER,
            Action.OPEN_IN_CHROME,
            Action.SHARE_LINK,
            Action.OPEN_IN_EPHEMERAL_TAB,
            Action.OPEN_IMAGE_IN_EPHEMERAL_TAB,
            Action.DIRECT_SHARE_LINK,
            Action.DIRECT_SHARE_IMAGE,
            Action.SEARCH_WITH_GOOGLE_LENS,
            Action.COPY_IMAGE,
            Action.SHOP_IMAGE_WITH_GOOGLE_LENS,
            Action.READ_LATER,
            Action.SHOP_WITH_GOOGLE_LENS_CHIP,
            Action.TRANSLATE_WITH_GOOGLE_LENS_CHIP,
            Action.SHARE_HIGHLIGHT,
            Action.REMOVE_HIGHLIGHT,
            Action.LEARN_MORE,
            Action.OPEN_IN_NEW_TAB_IN_GROUP
        })
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
    }

    /**
     * Builds a {@link ChromeContextMenuPopulator}.
     *
     * @param itemDelegate The {@link TabContextMenuItemDelegate} that will be notified with actions
     *     to perform when menu items are selected.
     * @param shareDelegate The Supplier of {@link ShareDelegate} that will be notified when a share
     *     action is performed.
     * @param mode Defines the context menu mode
     * @param externalAuthUtils {@link ExternalAuthUtils} instance.
     * @param context The {@link Context} used to retrieve the strings.
     * @param params The {@link ContextMenuParams} to populate the menu items.
     * @param nativeDelegate The {@link ContextMenuNativeDelegate} used to interact with native.
     */
    public ChromeContextMenuPopulator(
            TabContextMenuItemDelegate itemDelegate,
            Supplier<ShareDelegate> shareDelegate,
            @ContextMenuMode int mode,
            ExternalAuthUtils externalAuthUtils,
            Context context,
            ContextMenuParams params,
            ContextMenuNativeDelegate nativeDelegate) {
        mItemDelegate = itemDelegate;
        mShareDelegateSupplier = shareDelegate;
        mMode = mode;
        mExternalAuthUtils = externalAuthUtils;
        mContext = context;
        mParams = params;
        mNativeDelegate = nativeDelegate;
        mIsDownloadRestrictedByPolicy = DownloadUtils.isDownloadRestrictedByPolicy(getProfile());
    }

    /**
     * Gets the link of the item or empty text if the Url is empty.
     *
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
        mShowEphemeralTabNewLabel = null;

        List<Pair<Integer, ModelList>> groupedItems = new ArrayList<>();

        if (mParams.isAnchor()) {
            ModelList linkGroup = new ModelList();
            if (FirstRunStatus.getFirstRunFlowComplete()
                    && !isEmptyUrl(mParams.getUrl())
                    && UrlUtilities.isAcceptedScheme(mParams.getUrl())) {
                if (mMode == ContextMenuMode.NORMAL) {
                    linkGroup.add(createListItem(Item.OPEN_IN_NEW_TAB_IN_GROUP));
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
                    linkGroup.add(
                            createListItem(
                                    Item.SAVE_LINK_AS,
                                    /* showInProductHelp= */ false,
                                    !mIsDownloadRestrictedByPolicy));
                }
                if (!mParams.isImage()
                        && ReadingListUtils.isReadingListSupported(mParams.getLinkUrl())) {
                    linkGroup.add(createListItem(Item.READ_LATER, shouldTriggerReadLaterHelpUi()));
                }
                if (enableShareFromContextMenu()) {
                    linkGroup.add(createShareListItem(Item.SHARE_LINK, Item.DIRECT_SHARE_LINK));
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
                imageGroup.add(
                        createListItem(
                                Item.OPEN_IMAGE_IN_EPHEMERAL_TAB, mShowEphemeralTabNewLabel));
            }
            imageGroup.add(createListItem(Item.COPY_IMAGE));
            if (isSrcDownloadableScheme) {
                imageGroup.add(
                        createListItem(
                                Item.SAVE_IMAGE,
                                /* showInProductHelp= */ false,
                                !mIsDownloadRestrictedByPolicy));
            }

            if (mMode == ContextMenuMode.CUSTOM_TAB || mMode == ContextMenuMode.NORMAL) {
                if (checkSupportsGoogleSearchByImage(isSrcDownloadableScheme)) {
                    // Determine which image search menu item would be shown.
                    boolean shouldShowSearchImageWithLens =
                            shouldShowSearchWithLensAndRecordMetrics(
                                    mParams.getPageUrl(), mItemDelegate.isIncognito());
                    if (shouldShowSearchImageWithLens) {
                        imageGroup.add(createListItem(Item.SEARCH_WITH_GOOGLE_LENS, true));
                        maybeRecordUkmLensShown();
                    } else {
                        imageGroup.add(createListItem(Item.SEARCH_BY_IMAGE));
                        maybeRecordUkmSearchByImageShown();
                    }
                } else {
                    LensMetrics.recordLensSupportStatus(
                            LENS_SUPPORT_STATUS_HISTOGRAM_NAME,
                            LensMetrics.LensSupportStatus.SEARCH_BY_IMAGE_UNAVAILABLE);
                }
            }

            if (enableShareFromContextMenu()) {
                imageGroup.add(createShareListItem(Item.SHARE_IMAGE, Item.DIRECT_SHARE_IMAGE));
            }

            groupedItems.add(new Pair<>(R.string.contextmenu_image_title, imageGroup));
        }

        if (mParams.isVideo()
                && FirstRunStatus.getFirstRunFlowComplete()
                && mParams.canSaveMedia()
                && UrlUtilities.isDownloadableScheme(mParams.getSrcUrl())) {
            ModelList videoGroup = new ModelList();
            videoGroup.add(
                    createListItem(
                            Item.SAVE_VIDEO,
                            /* showInProductHelp= */ false,
                            !mIsDownloadRestrictedByPolicy));
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

        if (mMode != ContextMenuMode.NORMAL && FirstRunStatus.getFirstRunFlowComplete()) {
            ModelList items =
                    groupedItems.isEmpty()
                            ? new ModelList()
                            : groupedItems.get(
                                            mMode == ContextMenuMode.CUSTOM_TAB
                                                    ? 0
                                                    : groupedItems.size() - 1)
                                    .second;
            if (UrlUtilities.isAcceptedScheme(mParams.getUrl())) {
                if (mMode == ContextMenuMode.WEB_APP) {
                    items.add(createListItem(Item.OPEN_IN_CHROME));
                } else if (mMode == ContextMenuMode.CUSTOM_TAB && !mItemDelegate.isIncognito()) {
                    boolean addNewEntries =
                            !UrlUtilities.isInternalScheme(mParams.getUrl())
                                    && !isEmptyUrl(mParams.getUrl());
                    if (ChromeSharedPreferences.getInstance()
                                    .readBoolean(ChromePreferenceKeys.CHROME_DEFAULT_BROWSER, false)
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
        if (itemId == R.id.contextmenu_open_in_new_tab) {
            recordContextMenuSelection(ContextMenuUma.Action.OPEN_IN_NEW_TAB);
            mItemDelegate.onOpenInNewTab(
                    mParams.getUrl(),
                    mParams.getReferrer(),
                    /* navigateToTab= */ false,
                    mParams.getAdditionalNavigationParams());
        } else if (itemId == R.id.contextmenu_open_in_new_tab_in_group) {
            recordContextMenuSelection(ContextMenuUma.Action.OPEN_IN_NEW_TAB_IN_GROUP);
            mItemDelegate.onOpenInNewTabInGroup(mParams.getUrl(), mParams.getReferrer());
        } else if (itemId == R.id.contextmenu_open_in_incognito_tab) {
            recordContextMenuSelection(ContextMenuUma.Action.OPEN_IN_INCOGNITO_TAB);
            mItemDelegate.onOpenInNewIncognitoTab(mParams.getUrl());
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
            mItemDelegate.onSaveToClipboard(
                    mParams.getUnfilteredLinkUrl().getSpec(),
                    TabContextMenuItemDelegate.ClipboardType.LINK_URL);
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
                        TabContextMenuItemDelegate.ClipboardType.LINK_URL);
            } else if (UrlUtilities.isTelScheme(mParams.getLinkUrl())) {
                recordContextMenuSelection(ContextMenuUma.Action.COPY_PHONE_NUMBER);
                mItemDelegate.onSaveToClipboard(
                        UrlUtilities.getTelNumber(mParams.getLinkUrl()),
                        TabContextMenuItemDelegate.ClipboardType.LINK_URL);
            }
        } else if (itemId == R.id.contextmenu_copy_link_text) {
            recordContextMenuSelection(ContextMenuUma.Action.COPY_LINK_TEXT);
            mItemDelegate.onSaveToClipboard(
                    mParams.getLinkText(), TabContextMenuItemDelegate.ClipboardType.LINK_TEXT);
        } else if (itemId == R.id.contextmenu_save_image) {
            recordContextMenuSelection(ContextMenuUma.Action.SAVE_IMAGE);
            if (mIsDownloadRestrictedByPolicy) {
                showDownloadRestrictedToast();
            } else if (mItemDelegate.startDownload(mParams.getSrcUrl(), false)) {
                mNativeDelegate.startDownload(false);
            }
        } else if (itemId == R.id.contextmenu_save_video) {
            recordContextMenuSelection(ContextMenuUma.Action.SAVE_VIDEO);
            if (mIsDownloadRestrictedByPolicy) {
                showDownloadRestrictedToast();
            } else if (mItemDelegate.startDownload(mParams.getSrcUrl(), false)) {
                mNativeDelegate.startDownload(false);
            }
        } else if (itemId == R.id.contextmenu_save_link_as) {
            recordContextMenuSelection(ContextMenuUma.Action.SAVE_LINK);
            GURL url = mParams.getUnfilteredLinkUrl();
            if (mIsDownloadRestrictedByPolicy) {
                showDownloadRestrictedToast();
            } else if (mItemDelegate.startDownload(url, true)) {
                mNativeDelegate.startDownload(true);
            }
        } else if (itemId == R.id.contextmenu_share_link) {
            recordContextMenuSelection(ContextMenuUma.Action.SHARE_LINK);
            // TODO(crbug.com/40549331): Migrate ShareParams to GURL.
            ShareParams linkShareParams =
                    new ShareParams.Builder(
                                    getWindow(),
                                    ContextMenuUtils.getTitle(mParams),
                                    mParams.getUrl().getSpec())
                            .build();
            mShareDelegateSupplier
                    .get()
                    .share(
                            linkShareParams,
                            new ChromeShareExtras.Builder().setSaveLastUsed(true).build(),
                            ShareOrigin.CONTEXT_MENU);
        } else if (itemId == R.id.contextmenu_read_later) {
            recordContextMenuSelection(ContextMenuUma.Action.READ_LATER);
            // TODO(crbug.com/40156623): Download the page to offline page backend.
            String title = mParams.getTitleText();
            if (TextUtils.isEmpty(title)) {
                title = mParams.getLinkText();
            }
            mItemDelegate.onReadLater(mParams.getUrl(), title);
        } else if (itemId == R.id.contextmenu_direct_share_link) {
            recordContextMenuSelection(ContextMenuUma.Action.DIRECT_SHARE_LINK);
            final ShareParams shareParams =
                    new ShareParams.Builder(
                                    getWindow(),
                                    ContextMenuUtils.getTitle(mParams),
                                    mParams.getUrl().getSpec())
                            .build();
            mShareDelegateSupplier
                    .get()
                    .share(
                            shareParams,
                            new ChromeShareExtras.Builder().setShareDirectly(true).build(),
                            ShareOrigin.CONTEXT_MENU);
        } else if (itemId == R.id.contextmenu_search_with_google_lens) {
            recordContextMenuSelection(ContextMenuUma.Action.SEARCH_WITH_GOOGLE_LENS);
            searchWithGoogleLens(LensEntryPoint.CONTEXT_MENU_SEARCH_MENU_ITEM);
            SharedPreferencesManager prefManager = ChromeSharedPreferences.getInstance();
            prefManager.writeBoolean(
                    ChromePreferenceKeys.CONTEXT_MENU_SEARCH_WITH_GOOGLE_LENS_CLICKED, true);
        } else if (itemId == R.id.contextmenu_search_by_image) {
            LensMetrics.recordAmbientSearchQuery(
                    LensMetrics.AmbientSearchEntryPoint.CONTEXT_MENU_SEARCH_IMAGE_WITH_WEB);
            recordContextMenuSelection(ContextMenuUma.Action.SEARCH_BY_IMAGE);
            mNativeDelegate.searchForImage();
        } else if (itemId == R.id.contextmenu_share_image) {
            recordContextMenuSelection(ContextMenuUma.Action.SHARE_IMAGE);
            shareImage();
        } else if (itemId == R.id.contextmenu_direct_share_image) {
            recordContextMenuSelection(ContextMenuUma.Action.DIRECT_SHARE_IMAGE);
            mNativeDelegate.retrieveImageForShare(
                    ContextMenuImageFormat.ORIGINAL,
                    (Uri uri) -> {
                        assert ShareHelper.getLastShareComponentName() != null;
                        ShareParams params =
                                new ShareParams.Builder(getWindow(), mParams.getTitleText(), "")
                                        .setSingleImageUri(uri)
                                        .build();
                        ShareHelper.shareDirectly(
                                params,
                                ShareHelper.getLastShareComponentName(),
                                getProfile(),
                                false);
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
        } else if (itemId == R.id.contextmenu_share_highlight) {
            recordContextMenuSelection(ContextMenuUma.Action.SHARE_HIGHLIGHT);
            shareHighlighting();
        } else if (itemId == R.id.contextmenu_remove_highlight) {
            recordContextMenuSelection(ContextMenuUma.Action.REMOVE_HIGHLIGHT);
            LinkToTextHelper.removeHighlightsAllFrames(mItemDelegate.getWebContents());
        } else if (itemId == R.id.contextmenu_learn_more) {
            recordContextMenuSelection(ContextMenuUma.Action.LEARN_MORE);
            mItemDelegate.onOpenInNewTab(
                    new GURL(LinkToTextHelper.SHARED_HIGHLIGHTING_SUPPORT_URL),
                    mParams.getReferrer(),
                    /* navigateToTab= */ true,
                    /* additionalNavigationParams= */ null);
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

    private void shareHighlighting() {
        ShareParams linkShareParams =
                new ShareParams.Builder(
                                getWindow(), /* title= */ "", /* url= */ mParams.getUrl().getSpec())
                        .build();

        mShareDelegateSupplier
                .get()
                .share(
                        linkShareParams,
                        new ChromeShareExtras.Builder()
                                .setSaveLastUsed(true)
                                .setIsReshareHighlightedText(true)
                                .setRenderFrameHost(mNativeDelegate.getRenderFrameHost())
                                .setDetailedContentType(
                                        ChromeShareExtras.DetailedContentType.HIGHLIGHTED_TEXT)
                                .build(),
                        ShareOrigin.MOBILE_ACTION_MODE);
    }

    /** Copy the image, that triggered the current context menu, to system clipboard. */
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
        mNativeDelegate.retrieveImageForShare(
                ContextMenuImageFormat.ORIGINAL,
                (Uri imageUri) -> {
                    ContentResolver contentResolver =
                            ContextUtils.getApplicationContext().getContentResolver();
                    String mimeType = contentResolver.getType(imageUri);
                    ShareParams imageShareParams =
                            new ShareParams.Builder(
                                            getWindow(),
                                            ContextMenuUtils.getTitle(mParams),
                                            /* url= */ "")
                                    .setSingleImageUri(imageUri)
                                    .setFileContentType(mimeType)
                                    .build();
                    int detailedContentType;
                    if (mimeType.equals("image/gif")) {
                        detailedContentType = ChromeShareExtras.DetailedContentType.GIF;
                    } else {
                        detailedContentType = ChromeShareExtras.DetailedContentType.IMAGE;
                    }
                    mShareDelegateSupplier
                            .get()
                            .share(
                                    imageShareParams,
                                    new ChromeShareExtras.Builder()
                                            .setSaveLastUsed(true)
                                            .setImageSrcUrl(mParams.getSrcUrl())
                                            .setContentUrl(mParams.getPageUrl())
                                            .setDetailedContentType(detailedContentType)
                                            .build(),
                                    ShareOrigin.CONTEXT_MENU);
                });
    }

    /**
     * @return The service that handles TemplateUrls.
     */
    protected TemplateUrlService getTemplateUrlService() {
        return TemplateUrlServiceFactory.getForProfile(getProfile());
    }

    /**
     * Search for the image by intenting to the lens app with the image data attached.
     * @param lensEntryPoint The entry point that launches the Lens app.
     */
    protected void searchWithGoogleLens(@LensEntryPoint int lensEntryPoint) {
        mNativeDelegate.retrieveImageForShare(
                ContextMenuImageFormat.PNG,
                (Uri imageUri) -> {
                    LensIntentParams intentParams = getLensIntentParams(lensEntryPoint, imageUri);
                    LensController.getInstance().startLens(getWindow(), intentParams);
                });
    }

    /**
     * Build the intent params for Lens Context Menu features.
     * @param lensEntryPoint The entry point that launches the Lens app.
     * @param imageUri The image url that the context menu was triggered on.
     * @return A LensIntentParams. Will be used to launch the Lens app.
     */
    @VisibleForTesting
    protected LensIntentParams getLensIntentParams(
            @LensEntryPoint int lensEntryPoint, Uri imageUri) {
        return new LensIntentParams.Builder(lensEntryPoint, isIncognito())
                .withImageUri(imageUri)
                .withImageTitleOrAltText(mParams.getTitleText())
                .withSrcUrl(mParams.getSrcUrl().getValidSpecOrEmpty())
                .withPageUrl(mParams.getPageUrl().getValidSpecOrEmpty())
                .build();
    }

    @Override
    public @Nullable ChipDelegate getChipDelegate() {
        if (LensChipDelegate.isEnabled(isIncognito(), isTabletScreen())) {
            // TODO(crbug.com/40549331): Migrate LensChipDelegate to GURL.
            return new LensChipDelegate(
                    mParams.getPageUrl().getSpec(),
                    mParams.getTitleText(),
                    mParams.getSrcUrl().getSpec(),
                    getPageTitle(),
                    isIncognito(),
                    isTabletScreen(),
                    mItemDelegate.getWebContents(),
                    mNativeDelegate,
                    getOnChipClickedCallback(),
                    getOnChipShownCallback());
        }
        return null;
    }

    private Callback<Integer> getOnChipShownCallback() {
        return (Integer result) -> {
            int chipType = result.intValue();
            maybeRecordUkmLensChipShown(chipType);
        };
    }

    private Callback<Integer> getOnChipClickedCallback() {
        return (Integer result) -> {
            int chipType = result.intValue();
            switch (chipType) {
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
        return url == null
                || url.isEmpty()
                || url.getSpec().equals(ContentUrlConstants.ABOUT_BLANK_DISPLAY_URL);
    }

    /** Record a UMA ping and a UKM ping if enabled. */
    private void recordContextMenuSelection(int actionId) {
        if (LensUtils.shouldLogUkmForLensContextMenuFeatures()) {
            maybeRecordActionUkm("ContextMenuAndroid.Selected", actionId);
        }
    }

    /**
     * Whether the lens menu items should be shown based on a set of application
     * compatibility checks.
     *
     * @param pageUrl The Url associated with the main frame of the page that triggered the context
     *         menu.
     * @param isIncognito Whether the user is incognito.
     * @return A boolean. True if "Search image with Google Lens" should be enabled, otherwise
     *         False.
     */
    private boolean shouldShowSearchWithLensAndRecordMetrics(GURL pageUrl, boolean isIncognito) {
        // If Google Lens feature is not supported, show search by image menu item.
        if (!LensUtils.isGoogleLensFeatureEnabled(isIncognito)) {
            return false;
        }

        final TemplateUrlService templateUrlServiceInstance = getTemplateUrlService();
        String versionName = LensUtils.getLensActivityVersionNameIfAvailable(mContext);
        if (!templateUrlServiceInstance.isDefaultSearchEngineGoogle()) {
            LensMetrics.recordLensSupportStatus(
                    LENS_SUPPORT_STATUS_HISTOGRAM_NAME,
                    LensMetrics.LensSupportStatus.NON_GOOGLE_SEARCH_ENGINE);
            return false;
        }
        if (TextUtils.isEmpty(versionName)) {
            LensMetrics.recordLensSupportStatus(
                    LENS_SUPPORT_STATUS_HISTOGRAM_NAME,
                    LensMetrics.LensSupportStatus.ACTIVITY_NOT_ACCESSIBLE);
            return false;
        }
        if (GSAUtils.isAgsaVersionBelowMinimum(
                versionName, LensUtils.getMinimumAgsaVersionForLensSupport())) {
            LensMetrics.recordLensSupportStatus(
                    LENS_SUPPORT_STATUS_HISTOGRAM_NAME, LensMetrics.LensSupportStatus.OUT_OF_DATE);
            return false;
        }

        if (LensUtils.isDeviceOsBelowMinimum()) {
            LensMetrics.recordLensSupportStatus(
                    LENS_SUPPORT_STATUS_HISTOGRAM_NAME, LensMetrics.LensSupportStatus.LEGACY_OS);
            return false;
        }

        if (!LensUtils.isValidAgsaPackage(mExternalAuthUtils)) {
            LensMetrics.recordLensSupportStatus(
                    LENS_SUPPORT_STATUS_HISTOGRAM_NAME,
                    LensMetrics.LensSupportStatus.INVALID_PACKAGE);
            return false;
        }

        LensMetrics.recordLensSupportStatus(
                LENS_SUPPORT_STATUS_HISTOGRAM_NAME,
                LensMetrics.LensSupportStatus.LENS_SEARCH_SUPPORTED);
        return true;
    }

    private ListItem createListItem(@Item int item) {
        return createListItem(item, /* showInProductHelp= */ false, /* enabled= */ true);
    }

    private ListItem createListItem(@Item int item, boolean showInProductHelp) {
        return createListItem(item, showInProductHelp, /* enabled= */ true);
    }

    private ListItem createListItem(@Item int item, boolean showInProductHelp, boolean enabled) {
        final PropertyModel model =
                new PropertyModel.Builder(ContextMenuItemProperties.ALL_KEYS)
                        .with(MENU_ID, ChromeContextMenuItem.getMenuId(item))
                        .with(
                                TEXT,
                                ChromeContextMenuItem.getTitle(
                                        mContext, getProfile(), item, showInProductHelp))
                        .with(ENABLED, enabled)
                        .build();
        return new ListItem(ListItemType.CONTEXT_MENU_ITEM, model);
    }

    private ListItem createShareListItem(@Item int item, @Item int iconButtonItem) {
        final boolean isLink = item == Item.SHARE_LINK;
        final Pair<Drawable, CharSequence> shareInfo = createRecentShareAppInfo(isLink);
        final PropertyModel model =
                new PropertyModel.Builder(ContextMenuItemWithIconButtonProperties.ALL_KEYS)
                        .with(MENU_ID, ChromeContextMenuItem.getMenuId(item))
                        .with(ENABLED, true)
                        .with(
                                TEXT,
                                ChromeContextMenuItem.getTitle(mContext, getProfile(), item, false))
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
        return isLink
                ? ShareHelper.getShareableIconAndNameForText()
                // Use jpeg as a generic image type to be shared. Most apps accepting jpeg
                // should also accept other image types.
                : ShareHelper.getShareableIconAndNameForFileContentType("image/jpeg");
    }

    /**
     * If not disabled record a UKM for opening the context menu with the search by image option.
     */
    private void maybeRecordUkmSearchByImageShown() {
        if (LensUtils.shouldLogUkmForLensContextMenuFeatures()) {
            maybeRecordBooleanUkm("ContextMenuAndroid.Shown", "SearchByImage");
        }
    }

    /** If not disabled record a UKM for opening the context menu with the lens item. */
    private void maybeRecordUkmLensShown() {
        maybeRecordBooleanUkm("ContextMenuAndroid.Shown", "SearchWithGoogleLens");
    }

    private void maybeRecordUkmLensChipShown(int chipType) {
        String actionName = null;
        switch (chipType) {
            case ChipRenderParams.ChipType.LENS_TRANSLATE_CHIP:
                if (!LensUtils.shouldLogUkmByFeature(
                        ChromeFeatureList.CONTEXT_MENU_TRANSLATE_WITH_GOOGLE_LENS)) {
                    return;
                }
                actionName = "TranslateWithGoogleLensChip";
                break;
            default:
                // Unreachable value.
                assert false : "Invalid chip type provided to callback.";
        }
        maybeRecordBooleanUkm("ContextMenuAndroid.Shown", actionName);
    }

    /** Initialize the bridge if not yet created. */
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
        return isSrcDownloadableScheme
                && templateUrlServiceInstance.isLoaded()
                && templateUrlServiceInstance.isSearchByImageAvailable()
                && templateUrlServiceInstance.getDefaultSearchEngineTemplateUrl() != null
                && !LocaleManager.getInstance().needToCheckForSearchEnginePromo();
    }

    /** Returns the profile of the current tab via the item delegate. */
    private Profile getProfile() {
        return Profile.fromWebContents(mItemDelegate.getWebContents());
    }

    private boolean enableShareFromContextMenu() {
        return ShareUtils.enableShareForAutomotive(
                mMode == ContextMenuMode.CUSTOM_TAB || mMode == ContextMenuMode.WEB_APP);
    }

    private void showDownloadRestrictedToast() {
        Toast.makeText(
                        mContext,
                        R.string.download_message_single_download_blocked,
                        Toast.LENGTH_SHORT)
                .show();
    }
}
