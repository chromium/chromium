// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import android.content.ComponentName;
import android.content.Context;
import android.graphics.drawable.Drawable;

import androidx.appcompat.content.res.AppCompatResources;

import org.chromium.base.metrics.RecordUserAction;
import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.browser.share.ChromeShareExtras;
import org.chromium.chrome.browser.share.ShareDelegate;
import org.chromium.chrome.browser.share.ShareDelegate.ShareOrigin;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.tab_ui.R;
import org.chromium.components.browser_ui.share.ShareParams;

import java.util.List;

/**
 * Share action for the {@link TabSelectionEditorMenu}.
 */
public class TabSelectionEditorShareAction extends TabSelectionEditorAction {
    private Supplier<ShareDelegate> mShareDelegateSupplier;

    /**
     * Create an action for sharing tabs.
     * @param context for loading resources.
     * @param showMode whether to show an action view.
     * @param buttonType the type of the action view.
     * @param iconPosition the position of the icon in the action view.
     * @param shareDelegateSupplier the share delegate supplier for initiating a share action.
     */
    public static TabSelectionEditorAction createAction(Context context, @ShowMode int showMode,
            @ButtonType int buttonType, @IconPosition int iconPosition,
            Supplier<ShareDelegate> shareDelegateSupplier) {
        Drawable drawable =
                AppCompatResources.getDrawable(context, R.drawable.tab_selection_editor_share_icon);
        return new TabSelectionEditorShareAction(
                showMode, buttonType, iconPosition, shareDelegateSupplier, drawable);
    }

    private TabSelectionEditorShareAction(@ShowMode int showMode, @ButtonType int buttonType,
            @IconPosition int iconPosition, Supplier<ShareDelegate> shareDelegateSupplier,
            Drawable drawable) {
        super(R.id.tab_selection_editor_share_menu_item, showMode, buttonType, iconPosition,
                R.string.tab_selection_editor_share_tabs_action_button,
                R.plurals.accessibility_tab_selection_editor_share_tabs_action_button, drawable);
        mShareDelegateSupplier = shareDelegateSupplier;
    }

    @Override
    public void onSelectionStateChange(List<Integer> tabIds) {
        int size = editorSupportsActionOnRelatedTabs()
                ? getTabCountIncludingRelatedTabs(getTabModelSelector(), tabIds)
                : tabIds.size();

        setEnabledAndItemCount(!tabIds.isEmpty(), size);
    }

    @Override
    public void performAction(List<Tab> tabs) {
        assert !tabs.isEmpty() : "Share action should not be enabled for no tabs.";

        boolean isOnlyOneTab = (tabs.size() == 1);
        String tabText = isOnlyOneTab ? "" : getTabListStringForSharing(tabs);
        String tabTitle = isOnlyOneTab ? tabs.get(0).getTitle() : "";
        String tabUrl = isOnlyOneTab ? tabs.get(0).getUrl().getSpec() : "";
        String userAction = isOnlyOneTab ? "TabMultiSelectV2.SharedTabAsTextList"
                                         : "TabMultiSelectV2.SharedTabsListAsTextList";

        ShareParams shareParams =
                new ShareParams.Builder(tabs.get(0).getWindowAndroid(), tabTitle, tabUrl)
                        .setText(tabText)
                        .setCallback(new ShareParams.TargetChosenCallback() {
                            @Override
                            public void onTargetChosen(ComponentName chosenComponent) {
                                RecordUserAction.record(userAction);
                            }

                            @Override
                            public void onCancel() {}
                        })
                        .build();
        ChromeShareExtras chromeShareExtras = new ChromeShareExtras.Builder()
                                                      .setSharingTabGroup(true)
                                                      .setSaveLastUsed(true)
                                                      .build();
        mShareDelegateSupplier.get().share(shareParams, chromeShareExtras, ShareOrigin.TAB_GROUP);
    }

    @Override
    public boolean shouldHideEditorAfterAction() {
        return true;
    }

    private String getTabListStringForSharing(List<Tab> tabs) {
        StringBuilder sb = new StringBuilder();
        assert tabs.size() > 0;
        for (int i = 0; i < tabs.size(); i++) {
            sb.append(i + 1).append(". ").append(tabs.get(i).getUrl().getSpec()).append("\n");
        }
        return sb.toString();
    }
}
