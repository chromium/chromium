// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab_group_suggestion;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.view.View;
import android.view.View.OnClickListener;

import androidx.annotation.NonNull;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.Callback;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabGroupModelFilter;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.EmptyBottomSheetObserver;
import org.chromium.components.visited_url_ranking.url_grouping.GroupSuggestion;
import org.chromium.components.visited_url_ranking.url_grouping.GroupSuggestions;
import org.chromium.components.visited_url_ranking.url_grouping.GroupSuggestionsService;
import org.chromium.components.visited_url_ranking.url_grouping.UserResponse;
import org.chromium.components.visited_url_ranking.url_grouping.UserResponseMetadata;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.ArrayList;
import java.util.List;
import java.util.Locale;

/** Mediator for {@link GroupSuggestionsPromotionCoordinator}. */
@NullMarked
public class GroupSuggestionsPromotionMediator implements GroupSuggestionsService.Delegate {

    private final @NonNull PropertyModel mModel;
    private final @NonNull BottomSheetController mBottomSheetController;
    private final @NonNull View mContainerView;
    private final @NonNull GroupSuggestionsService mService;
    private final @NonNull TabModel mTabModel;
    private final @NonNull TabGroupModelFilter mTabGroupModelFilter;
    private final @NonNull OnClickListener mOnAcceptClickListener;
    private final @NonNull OnClickListener mOnRejectClickListener;
    private final @NonNull EmptyBottomSheetObserver mBottomSheetObserver;

    private @Nullable GroupSuggestionsBottomSheetContent mCurrentSheetContent;

    public GroupSuggestionsPromotionMediator(
            @NonNull PropertyModel model,
            GroupSuggestionsService service,
            @NonNull BottomSheetController bottomSheetController,
            @NonNull TabGroupModelFilter tabGroupModelFilter,
            @NonNull View containerView) {
        mModel = model;
        mService = service;
        mBottomSheetController = bottomSheetController;
        mTabGroupModelFilter = tabGroupModelFilter;
        mTabModel = mTabGroupModelFilter.getTabModel();
        mContainerView = containerView;
        mOnAcceptClickListener =
                v -> {
                    assert mCurrentSheetContent != null;
                    List<Tab> tabs = new ArrayList<>();
                    for (int tabId : mCurrentSheetContent.getGroupSuggestion().tabIds) {
                        Tab tab = mTabModel.getTabById(tabId);
                        if (tab == null) {
                            continue;
                        }
                        tabs.add(tab);
                    }
                    assert tabs.size() > 1;
                    // TODO(crbug.com/397221723): Replace with group creation using
                    // TabGroupCreationDialogManager.
                    Tab currentTab = mTabModel.getCurrentTabSupplier().get();
                    Tab rootTab =
                            tabs.contains(currentTab) ? assumeNonNull(currentTab) : tabs.get(0);
                    mTabGroupModelFilter.mergeListOfTabsToGroup(tabs, rootTab, true);
                    mBottomSheetController.hideContent(mCurrentSheetContent, true);
                    mCurrentSheetContent
                            .getUserResponseCallback()
                            .onResult(
                                    new UserResponseMetadata(
                                            mCurrentSheetContent.getGroupSuggestion().suggestionId,
                                            UserResponse.ACCEPTED));
                    mCurrentSheetContent = null;
                };
        mOnRejectClickListener =
                v -> {
                    assert mCurrentSheetContent != null;
                    mBottomSheetController.hideContent(mCurrentSheetContent, true);
                    mCurrentSheetContent
                            .getUserResponseCallback()
                            .onResult(
                                    new UserResponseMetadata(
                                            mCurrentSheetContent.getGroupSuggestion().suggestionId,
                                            UserResponse.REJECTED));
                    mCurrentSheetContent = null;
                };
        mBottomSheetObserver =
                new EmptyBottomSheetObserver() {
                    @Override
                    public void onSheetClosed(int reason) {
                        if (mCurrentSheetContent != null) {
                            mCurrentSheetContent
                                    .getUserResponseCallback()
                                    .onResult(
                                            new UserResponseMetadata(
                                                    mCurrentSheetContent.getGroupSuggestion()
                                                            .suggestionId,
                                                    UserResponse.IGNORED));
                            mCurrentSheetContent = null;
                        }
                    }
                };
        mBottomSheetController.addObserver(mBottomSheetObserver);
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
            callback.onResult(new UserResponseMetadata(0, UserResponse.NOT_SHOWN));
            return;
        }
        GroupSuggestion suggestion = groupSuggestions.groupSuggestions.get(0);
        // Never overwrite the current suggestion that is showing.
        if (mCurrentSheetContent != null) {
            callback.onResult(
                    new UserResponseMetadata(suggestion.suggestionId, UserResponse.NOT_SHOWN));
            return;
        }
        mModel.set(GroupSuggestionsPromotionProperties.PROMO_CONTENTS, suggestion.promoContents);
        mModel.set(GroupSuggestionsPromotionProperties.PROMO_HEADER, suggestion.promoHeader);
        mModel.set(GroupSuggestionsPromotionProperties.SUGGESTED_NAME, suggestion.suggestedName);
        // TODO(397221723): Replace with actual strings instead of dynamically setting.
        mModel.set(GroupSuggestionsPromotionProperties.ACCEPT_BUTTON_TEXT, "Accept");
        mModel.set(
                GroupSuggestionsPromotionProperties.ACCEPT_BUTTON_LISTENER, mOnAcceptClickListener);
        mModel.set(GroupSuggestionsPromotionProperties.REJECT_BUTTON_TEXT, "Reject");
        mModel.set(
                GroupSuggestionsPromotionProperties.REJECT_BUTTON_LISTENER, mOnRejectClickListener);
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
            callback.onResult(
                    new UserResponseMetadata(suggestion.suggestionId, UserResponse.NOT_SHOWN));
            return;
        }
        mModel.set(GroupSuggestionsPromotionProperties.GROUP_CONTENT_STRING, sb.toString());

        mCurrentSheetContent =
                new GroupSuggestionsBottomSheetContent(mContainerView, suggestion, callback);
        boolean result = mBottomSheetController.requestShowContent(mCurrentSheetContent, true);

        if (!result) {
            mCurrentSheetContent = null;
            callback.onResult(
                    new UserResponseMetadata(suggestion.suggestionId, UserResponse.NOT_SHOWN));
        }
    }

    /** Destroy any members that needs clean up. */
    public void destroy() {
        if (mCurrentSheetContent != null) {
            mBottomSheetController.hideContent(mCurrentSheetContent, true);
            mCurrentSheetContent = null;
        }
        mService.unregisterDelegate(this);
        mBottomSheetController.removeObserver(mBottomSheetObserver);
    }

    @VisibleForTesting
    @Nullable
    public GroupSuggestionsBottomSheetContent getCurrentSheetContent() {
        return mCurrentSheetContent;
    }
}
