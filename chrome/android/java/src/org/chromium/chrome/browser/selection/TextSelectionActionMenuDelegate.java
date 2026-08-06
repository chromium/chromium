// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.selection;

import android.content.Context;
import android.content.pm.ResolveInfo;
import android.text.TextUtils;
import android.view.Menu;
import android.view.MenuItem;
import android.view.View;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.DeviceInfo;
import org.chromium.base.SelectionActionMenuClientWrapper.MenuType;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.dom_distiller.ReaderModeManager;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.glic.GlicEnabling;
import org.chromium.chrome.browser.glic.GlicKeyedService.GlicInvocationSource;
import org.chromium.chrome.browser.glic.GlicKeyedServiceHandler;
import org.chromium.chrome.browser.glic.GlicMetrics;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.search_engines.TemplateUrlServiceFactory;
import org.chromium.chrome.browser.share.link_to_text.LinkToTextCoordinator;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab_bottom_sheet.TabBottomSheetUtils;
import org.chromium.chrome.browser.ui.side_panel.AndroidSidePanelEnabledFn;
import org.chromium.components.dom_distiller.core.DomDistillerUrlUtils;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.components.embedder_support.util.UrlUtilities;
import org.chromium.components.search_engines.TemplateUrl;
import org.chromium.components.search_engines.TemplateUrlService;
import org.chromium.content_public.browser.RenderFrameHost;
import org.chromium.content_public.browser.SelectionMenuItem;
import org.chromium.content_public.browser.SelectionMenuItem.ItemGroupOffset;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.selection.SelectionActionMenuDelegate;
import org.chromium.content_public.browser.selection.SelectionUtils;
import org.chromium.url.GURL;

import java.util.ArrayList;
import java.util.List;

/**
 * Implementation of {@link SelectionActionMenuDelegate} that provides custom menu item behavior for
 * text selection menu, such as dynamic web search titles and Reading Mode.
 */
@NullMarked
public class TextSelectionActionMenuDelegate implements SelectionActionMenuDelegate {
    @VisibleForTesting static final String PARAM_SHOW_ASK_GEMINI_ON_SELECTION = "show_on_selection";

    @VisibleForTesting
    static final String PARAM_ASK_GEMINI_SELECTION_MENU_POSITION = "selection_menu_position";

    @VisibleForTesting
    static final String PARAM_ASK_GEMINI_SEND_SELECTED_TEXT = "send_selected_text";

    @VisibleForTesting static final String ASK_GEMINI_POSITION_SECONDARY = "secondary";

    private final Tab mTab;
    private @Nullable String mSelectedText;

    public TextSelectionActionMenuDelegate(Tab tab) {
        mTab = tab;
    }

    @Override
    public List<SelectionMenuItem> getAdditionalMenuItems(
            @MenuType int menuType,
            boolean isSelectionPassword,
            boolean isSelectionReadOnly,
            String selectedText) {
        mSelectedText = selectedText;
        List<SelectionMenuItem> items = new ArrayList<>();
        if (shouldShowReadingMode(menuType)) {
            items.add(
                    new SelectionMenuItem.Builder(R.string.contextmenu_open_in_reading_mode)
                            .setId(R.id.contextmenu_open_in_reading_mode)
                            .setGroupId(R.id.select_action_menu_delegate_items)
                            .setOrderAndCategory(
                                    Menu.CATEGORY_SECONDARY, // Show at end of section.
                                    SelectionMenuItem.ItemGroupOffset.DEFAULT_ITEMS)
                            .setShowAsActionFlags(
                                    MenuItem.SHOW_AS_ACTION_NEVER
                                            | MenuItem.SHOW_AS_ACTION_WITH_TEXT)
                            .build());
        }
        if (shouldShowAskGeminiForSelection(menuType, isSelectionPassword, selectedText)) {
            SelectionMenuItem.Builder builder =
                    new SelectionMenuItem.Builder(R.string.glic_button_entrypoint_ask_gemini_label)
                            .setId(R.id.contextmenu_ask_gemini)
                            .setGroupId(R.id.select_action_menu_delegate_items)
                            .setShowAsActionFlags(
                                    MenuItem.SHOW_AS_ACTION_ALWAYS
                                            | MenuItem.SHOW_AS_ACTION_WITH_TEXT);
            setAskGeminiOrderAndCategory(builder);
            items.add(builder.build());
        }
        if (menuType == MenuType.DROPDOWN
                && ChromeFeatureList.isEnabled(ChromeFeatureList.COPY_LINK_TO_HIGHLIGHT)
                && !selectedText.isEmpty()
                && !isSelectionPassword
                && isSelectionReadOnly) {
            SelectionMenuItem copyLinkItem =
                    new SelectionMenuItem.Builder(R.string.contextmenu_copy_link_to_highlight)
                            .setId(R.id.contextmenu_copy_link_to_highlight)
                            .setGroupId(org.chromium.content.R.id.select_action_menu_delegate_items)
                            .setOrderAndCategory(1, ItemGroupOffset.DEFAULT_ITEMS)
                            .build();

            items.add(copyLinkItem);
        }
        return items;
    }

    @Override
    public List<ResolveInfo> filterTextProcessingActivities(
            @MenuType int menuType, List<ResolveInfo> activities) {
        return activities;
    }

    @Override
    public boolean canReuseCachedSelectionMenu(@MenuType int menuType) {
        return true;
    }

    @Override
    public boolean handleMenuItemClick(
            SelectionMenuItem item, WebContents webContents, @Nullable View containerView) {
        if (item.id == R.id.contextmenu_open_in_reading_mode) {
            ReaderModeManager readerModeManager =
                    mTab.getUserDataHost().getUserData(ReaderModeManager.class);
            if (readerModeManager != null) {
                readerModeManager.activateReaderMode(ReaderModeManager.EntryPoint.CONTEXT_MENU);
            }
            return true;
        } else if (item.id == R.id.contextmenu_copy_link_to_highlight) {
            RenderFrameHost rfh = webContents.getFocusedFrame();
            if (rfh != null) {
                LinkToTextCoordinator.copyLinkToText(mTab, rfh);
                return true;
            }
        } else if (item.id == R.id.contextmenu_ask_gemini) {
            if (mTab.isDestroyed()) return false;
            Profile profile = mTab.getProfile();
            if (profile == null) return false;
            GURL url = mTab.getUrl();
            boolean isNtp = url != null && UrlUtilities.isNtpUrl(url);
            GlicMetrics.recordEntryPointClick(
                    GlicInvocationSource.WEB_CONTENTS_CONTEXT_MENU, isNtp);
            if (ChromeFeatureList.getFieldTrialParamByFeatureAsBoolean(
                    ChromeFeatureList.CLANK_GLIC_CONTEXT_MENU,
                    PARAM_ASK_GEMINI_SEND_SELECTED_TEXT,
                    true)) {
                String text = mSelectedText != null ? mSelectedText : "";
                return GlicKeyedServiceHandler.invokeWithPrompt(
                        profile, mTab, text, GlicInvocationSource.WEB_CONTENTS_CONTEXT_MENU);
            }
            return GlicKeyedServiceHandler.invoke(
                    profile, mTab, GlicInvocationSource.WEB_CONTENTS_CONTEXT_MENU);
        }
        return false;
    }

    @Override
    public @Nullable String getWebSearchMenuItemTitle(Context context, String selectedText) {
        Profile profile = mTab.getProfile();
        if (profile == null) return null;
        TemplateUrlService service = TemplateUrlServiceFactory.getForProfile(profile);
        TemplateUrl templateUrl = service.getDefaultSearchEngineTemplateUrl();
        if (templateUrl == null) return null;
        String fullName = service.getFullNameFromTemplateUrl(templateUrl.getKeyword());
        if (TextUtils.isEmpty(fullName)) return null;
        String sanitizedText = SelectionUtils.sanitizeTextForMenu(selectedText);
        if (sanitizedText.isEmpty()) return null;
        return context.getString(R.string.contextmenu_search_web_for_text, fullName, sanitizedText);
    }

    // TODO(b/543135302): Move Ask Gemini menu enabling checks and feature params into GlicEnabling
    // as helper methods.
    /**
     * Whether to show the "Ask Gemini" item in the mobile text selection (floating action mode)
     * menu. Mirrors {@code ChromeContextMenuPopulator#shouldShowAskGeminiForLink}: targets the
     * mobile form factor where Glic is presented in a bottom sheet.
     */
    private boolean shouldShowAskGeminiForSelection(
            @MenuType int menuType, boolean isSelectionPassword, String selectedText) {
        if (menuType != MenuType.FLOATING) return false;
        if (TextUtils.isEmpty(selectedText) || isSelectionPassword) return false;
        if (mTab.isDestroyed() || mTab.isIncognito()) return false;
        Profile profile = mTab.getProfile();
        if (profile == null) return false;
        return ChromeFeatureList.isEnabled(ChromeFeatureList.CLANK_GLIC_CONTEXT_MENU)
                && ChromeFeatureList.getFieldTrialParamByFeatureAsBoolean(
                        ChromeFeatureList.CLANK_GLIC_CONTEXT_MENU,
                        PARAM_SHOW_ASK_GEMINI_ON_SELECTION,
                        true)
                && !AndroidSidePanelEnabledFn.isEnabled()
                && TabBottomSheetUtils.isTabBottomSheetEnabled()
                && !DeviceInfo.isAutomotive()
                && GlicEnabling.isEnabledForProfile(profile);
    }

    private void setAskGeminiOrderAndCategory(SelectionMenuItem.Builder builder) {
        String position =
                ChromeFeatureList.getFieldTrialParamByFeature(
                        ChromeFeatureList.CLANK_GLIC_CONTEXT_MENU,
                        PARAM_ASK_GEMINI_SELECTION_MENU_POSITION);
        if (ASK_GEMINI_POSITION_SECONDARY.equals(position)) {
            builder.setOrderAndCategory(0, ItemGroupOffset.SECONDARY_ASSIST_ITEMS);
        } else {
            builder.setOrderAndCategory(0, ItemGroupOffset.ASSIST_ITEMS);
        }
    }

    private boolean shouldShowReadingMode(@MenuType int menuType) {
        if (mTab.isDestroyed()) return false;

        GURL pageUrl = mTab.getUrl();
        if (pageUrl == null || !pageUrl.isValid()) return false;

        String scheme = pageUrl.getScheme();
        boolean isChromeOrNativePage =
                UrlConstants.CHROME_SCHEME.equals(scheme)
                        || UrlConstants.CHROME_NATIVE_SCHEME.equals(scheme)
                        || mTab.isNativePage();
        return !isChromeOrNativePage
                && !DomDistillerUrlUtils.isDistilledPage(pageUrl)
                && menuType == MenuType.DROPDOWN;
    }
}
