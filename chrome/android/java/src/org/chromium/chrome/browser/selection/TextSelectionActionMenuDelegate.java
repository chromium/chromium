// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.selection;

import android.content.Context;
import android.content.pm.ResolveInfo;
import android.text.TextUtils;

import org.chromium.base.SelectionActionMenuClientWrapper.MenuType;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.search_engines.TemplateUrlServiceFactory;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.components.search_engines.TemplateUrl;
import org.chromium.components.search_engines.TemplateUrlService;
import org.chromium.content_public.browser.SelectionMenuItem;
import org.chromium.content_public.browser.selection.SelectionActionMenuDelegate;
import org.chromium.content_public.browser.selection.SelectionUtils;

import java.util.List;

/**
 * Implementation of {@link SelectionActionMenuDelegate} that provides custom menu item behavior for
 * text selection menu, such as dynamic web search titles
 */
@NullMarked
public class TextSelectionActionMenuDelegate implements SelectionActionMenuDelegate {
    private final Tab mTab;

    public TextSelectionActionMenuDelegate(Tab tab) {
        mTab = tab;
    }

    @Override
    public List<SelectionMenuItem> getAdditionalMenuItems(
            @MenuType int menuType,
            boolean isSelectionPassword,
            boolean isSelectionReadOnly,
            String selectedText) {
        return List.of();
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
}
