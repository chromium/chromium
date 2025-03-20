// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab_group_suggestion;

import android.view.View;

import androidx.annotation.NonNull;

import org.chromium.base.Callback;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetContent;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.visited_url_ranking.url_grouping.GroupSuggestion;
import org.chromium.components.visited_url_ranking.url_grouping.GroupSuggestions;
import org.chromium.components.visited_url_ranking.url_grouping.GroupSuggestionsService;
import org.chromium.components.visited_url_ranking.url_grouping.UserResponseMetadata;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.Locale;

/** Mediator for {@link GroupSuggestionsPromotionCoordinator}. */
@NullMarked
public class GroupSuggestionsPromotionMediator implements GroupSuggestionsService.Delegate {

    private final @NonNull PropertyModel mModel;
    private final @NonNull BottomSheetController mBottomSheetController;
    private final @NonNull View mContainerView;
    private final @NonNull GroupSuggestionsService mService;
    private final @NonNull TabModel mTabModel;

    public GroupSuggestionsPromotionMediator(
            @NonNull PropertyModel model,
            GroupSuggestionsService service,
            @NonNull BottomSheetController bottomSheetController,
            @NonNull TabModel tabModel,
            @NonNull View containerView) {
        mModel = model;
        mService = service;
        mBottomSheetController = bottomSheetController;
        mTabModel = tabModel;
        mContainerView = containerView;
        // TODO(crbug.com/397221723): Pass in proper windowId to distinguish windows in
        // multi-window.
        service.registerDelegate(this, 0);
    }

    @Override
    public void showSuggestion(
            @Nullable GroupSuggestions groupSuggestions, Callback<UserResponseMetadata> callback) {
        if (groupSuggestions == null
                || groupSuggestions.groupSuggestions == null
                || groupSuggestions.groupSuggestions.isEmpty()) {
            return;
        }
        GroupSuggestion suggestion = groupSuggestions.groupSuggestions.get(0);
        mModel.set(GroupSuggestionsPromotionProperties.PROMO_CONTENTS, suggestion.promoContents);
        mModel.set(GroupSuggestionsPromotionProperties.PROMO_HEADER, suggestion.promoHeader);
        mModel.set(GroupSuggestionsPromotionProperties.SUGGESTED_NAME, suggestion.suggestedName);
        // TODO(397221723): Replace with actual strings instead of dynamically setting.
        mModel.set(GroupSuggestionsPromotionProperties.ACCEPT_BUTTON_TEXT, "Accept");
        mModel.set(GroupSuggestionsPromotionProperties.REJECT_BUTTON_TEXT, "Reject");
        StringBuilder sb = new StringBuilder();
        int suggestedTabCount = 0;
        for (int i = 0; i < suggestion.tabIds.length; i++) {
            Tab tab = mTabModel.getTabById(suggestion.tabIds[i]);
            if (tab == null) {
                continue;
            }
            suggestedTabCount += 1;
            sb.append(String.format(Locale.getDefault(), "Tab %d: %s\n", i + 1, tab.getTitle()));
        }
        if (suggestedTabCount <= 1) {
            return;
        }
        mModel.set(GroupSuggestionsPromotionProperties.GROUP_CONTENT_STRING, sb.toString());

        BottomSheetContent content = new GroupSuggestionsBottomSheetContent(mContainerView);
        mBottomSheetController.requestShowContent(content, true);
    }

    /** Destroy any members that needs clean up. */
    public void destroy() {
        mService.unregisterDelegate(this);
    }
}
