// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab_group_suggestion;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.content.Context;
import android.view.LayoutInflater;
import android.widget.LinearLayout;

import androidx.annotation.NonNull;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.tabmodel.TabGroupModelFilter;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

/** Coordinator for a GroupSuggestions promotion UI. */
@NullMarked
public class GroupSuggestionsPromotionCoordinator {
    public static final String CREATE_SUGGESTIONS_PROMOTION_UI_PARAM =
            "create_suggestions_promotion_ui";

    private final @NonNull PropertyModelChangeProcessor mModelChangeProcessor;
    private final @NonNull GroupSuggestionsPromotionMediator mMediator;

    public GroupSuggestionsPromotionCoordinator(
            @NonNull Context context,
            @NonNull BottomSheetController bottomSheetController,
            @NonNull TabGroupModelFilter tabGroupModelFilter) {
        LinearLayout groupSuggestionsBottomSheetContainer =
                (LinearLayout)
                        LayoutInflater.from(context)
                                .inflate(
                                        R.layout.group_suggestions_bottom_sheet_container,
                                        /* root= */ null);
        PropertyModel model = new PropertyModel(GroupSuggestionsPromotionProperties.ALL_KEYS);
        mModelChangeProcessor =
                PropertyModelChangeProcessor.create(
                        model,
                        groupSuggestionsBottomSheetContainer,
                        GroupSuggestionsPromotionBinder::bind);
        mMediator =
                new GroupSuggestionsPromotionMediator(
                        model,
                        GroupSuggestionsServiceFactory.getForProfile(
                                assumeNonNull(tabGroupModelFilter.getTabModel().getProfile())),
                        bottomSheetController,
                        tabGroupModelFilter,
                        groupSuggestionsBottomSheetContainer);
    }

    /** Destroy any members that needs clean up. */
    public void destroy() {
        mModelChangeProcessor.destroy();
        mMediator.destroy();
    }
}
