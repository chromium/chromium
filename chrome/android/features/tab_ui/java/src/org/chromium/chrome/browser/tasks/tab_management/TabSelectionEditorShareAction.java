// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import android.content.ComponentName;
import android.content.Context;
import android.graphics.drawable.Drawable;

import androidx.annotation.IntDef;
import androidx.annotation.VisibleForTesting;
import androidx.appcompat.content.res.AppCompatResources;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.browser.share.ChromeShareExtras;
import org.chromium.chrome.browser.share.ShareDelegate;
import org.chromium.chrome.browser.share.ShareDelegate.ShareOrigin;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabList;
import org.chromium.chrome.tab_ui.R;
import org.chromium.components.browser_ui.share.ShareParams;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.content_public.common.ContentUrlConstants;
import org.chromium.ui.widget.Toast;
import org.chromium.url.GURL;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.HashSet;
import java.util.List;

/**
 * Share action for the {@link TabSelectionEditorMenu}.
 */
public class TabSelectionEditorShareAction extends TabSelectionEditorAction {
    private static final List<String> UNSUPPORTED_SCHEMES =
            new ArrayList<>(Arrays.asList(UrlConstants.CHROME_SCHEME,
                    UrlConstants.CHROME_NATIVE_SCHEME, ContentUrlConstants.ABOUT_SCHEME));
    private Context mContext;
    private Supplier<ShareDelegate> mShareDelegateSupplier;
    private boolean mSkipUrlCheckForTesting;

    // These values are persisted to logs. Entries should not be renumbered and
    // numeric values should never be reused.
    @IntDef({TabSelectionEditorShareActionState.UNKNOWN, TabSelectionEditorShareActionState.SUCCESS,
            TabSelectionEditorShareActionState.ALL_TABS_FILTERED,
            TabSelectionEditorShareActionState.NUM_ENTRIES})
    public @interface TabSelectionEditorShareActionState {
        int UNKNOWN = 0;
        int SUCCESS = 1;
        int ALL_TABS_FILTERED = 2;

        // Be sure to also update enums.xml when updating these values.
        int NUM_ENTRIES = 3;
    }

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
                context, showMode, buttonType, iconPosition, shareDelegateSupplier, drawable);
    }

    private TabSelectionEditorShareAction(Context context, @ShowMode int showMode,
            @ButtonType int buttonType, @IconPosition int iconPosition,
            Supplier<ShareDelegate> shareDelegateSupplier, Drawable drawable) {
        super(R.id.tab_selection_editor_share_menu_item, showMode, buttonType, iconPosition,
                R.plurals.tab_selection_editor_share_tabs_action_button,
                R.plurals.accessibility_tab_selection_editor_share_tabs_action_button, drawable);
        mShareDelegateSupplier = shareDelegateSupplier;
        mContext = context;
    }

    @Override
    public void onSelectionStateChange(List<Integer> tabIds) {
        boolean enableShare = false;
        List<Tab> selectedTabs = getTabsAndRelatedTabsFromSelection();

        for (Tab tab : selectedTabs) {
            if (!shouldFilterUrl(tab.getUrl())) {
                enableShare = true;
                break;
            }
        }

        int size = editorSupportsActionOnRelatedTabs() ? selectedTabs.size() : tabIds.size();
        setEnabledAndItemCount(enableShare && !tabIds.isEmpty(), size);
    }

    @Override
    public boolean performAction(List<Tab> tabs) {
        assert !tabs.isEmpty() : "Share action should not be enabled for no tabs.";

        TabList tabList = getTabModelSelector().getCurrentModel();
        List<Integer> sortedTabIndexList = filterTabs(tabs, tabList);

        if (sortedTabIndexList.size() == 0) {
            showToastOnShareFail();
            logShareActionState(TabSelectionEditorShareActionState.ALL_TABS_FILTERED);
            return false;
        }

        boolean isOnlyOneTab = (sortedTabIndexList.size() == 1);
        String tabText =
                isOnlyOneTab ? "" : getTabListStringForSharing(sortedTabIndexList, tabList);
        String tabTitle =
                isOnlyOneTab ? tabList.getTabAt(sortedTabIndexList.get(0)).getTitle() : "";
        String tabUrl =
                isOnlyOneTab ? tabList.getTabAt(sortedTabIndexList.get(0)).getUrl().getSpec() : "";
        String userAction = isOnlyOneTab ? "TabMultiSelectV2.SharedTabAsTextList"
                                         : "TabMultiSelectV2.SharedTabsListAsTextList";

        ShareParams shareParams =
                new ShareParams
                        .Builder(tabList.getTabAt(sortedTabIndexList.get(0)).getWindowAndroid(),
                                tabTitle, tabUrl)
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
        logShareActionState(TabSelectionEditorShareActionState.SUCCESS);
        return true;
    }

    @Override
    public boolean shouldHideEditorAfterAction() {
        return true;
    }

    // TODO(crbug.com/1373579): Current filtering does not remove duplicates or show a "Toast" if
    // no shareable URLs are present after filtering.
    private List<Integer> filterTabs(List<Tab> tabs, TabList tabList) {
        assert tabs.size() > 0;
        List<Integer> sortedTabIndexList = new ArrayList<Integer>();

        HashSet<Tab> selectedTabs = new HashSet<Tab>(tabs);
        for (int i = 0; i < tabList.getCount(); i++) {
            Tab tab = tabList.getTabAt(i);
            if (!selectedTabs.contains(tab)) continue;

            if (!shouldFilterUrl(tab.getUrl())) {
                sortedTabIndexList.add(i);
            }
        }
        return sortedTabIndexList;
    }

    private String getTabListStringForSharing(List<Integer> sortedTabIndexList, TabList list) {
        StringBuilder sb = new StringBuilder();

        // TODO(crbug.com/1373579): Check if this string builder assembles the shareable URLs in
        // accordance with internationalization and translation standards
        for (int i = 0; i < sortedTabIndexList.size(); i++) {
            sb.append(i + 1)
                    .append(". ")
                    .append(list.getTabAt(sortedTabIndexList.get(i)).getUrl().getSpec())
                    .append("\n");
        }
        return sb.toString();
    }

    private static void logShareActionState(@TabSelectionEditorShareActionState int action) {
        RecordHistogram.recordEnumeratedHistogram("Android.TabMultiSelectV2.SharingState", action,
                TabSelectionEditorShareActionState.NUM_ENTRIES);
    }

    private void showToastOnShareFail() {
        // TODO(crbug.com/1373579): Consider changing from the more generic current string to a
        // descriptive situational string indicating what went wrong.
        String toastText = mContext.getResources().getString(
                R.string.browser_sharing_error_dialog_text_internal_error);
        Toast toast =
                Toast.makeText(mContext.getApplicationContext(), toastText, Toast.LENGTH_SHORT);
        toast.show();
    }

    private boolean shouldFilterUrl(GURL url) {
        if (mSkipUrlCheckForTesting) return false;

        return url == null || !url.isValid() || url.isEmpty()
                || UNSUPPORTED_SCHEMES.contains(url.getScheme());
    }

    @VisibleForTesting
    void setSkipUrlCheckForTesting(boolean skip) {
        mSkipUrlCheckForTesting = skip;
    }
}
