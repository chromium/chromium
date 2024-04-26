// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import android.app.PendingIntent;
import android.content.BroadcastReceiver;
import android.content.ClipData;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.graphics.drawable.Drawable;
import android.graphics.drawable.InsetDrawable;

import androidx.annotation.IntDef;
import androidx.appcompat.content.res.AppCompatResources;

import org.chromium.base.Callback;
import org.chromium.base.ContextUtils;
import org.chromium.base.ResettersForTesting;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabList;
import org.chromium.chrome.browser.tasks.tab_management.TabUiMetricsHelper.TabListEditorActionMetricGroups;
import org.chromium.chrome.tab_ui.R;
import org.chromium.components.browser_ui.share.ShareImageFileUtils;
import org.chromium.components.browser_ui.share.ShareParams;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.content_public.common.ContentUrlConstants;
import org.chromium.url.GURL;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.HashSet;
import java.util.List;

/** Share action for the {@link TabListEditorMenu}. */
public class TabListEditorShareAction extends TabListEditorAction {
    private static final List<String> UNSUPPORTED_SCHEMES =
            new ArrayList<>(
                    Arrays.asList(
                            UrlConstants.CHROME_SCHEME,
                            UrlConstants.CHROME_NATIVE_SCHEME,
                            ContentUrlConstants.ABOUT_SCHEME));
    private static Callback<Intent> sIntentCallbackForTesting;
    private Context mContext;
    private boolean mSkipUrlCheckForTesting;
    private BroadcastReceiver mBroadcastReceiver;

    // These values are persisted to logs. Entries should not be renumbered and
    // numeric values should never be reused.
    @IntDef({
        TabListEditorShareActionState.UNKNOWN,
        TabListEditorShareActionState.SUCCESS,
        TabListEditorShareActionState.ALL_TABS_FILTERED,
        TabListEditorShareActionState.NUM_ENTRIES
    })
    public @interface TabListEditorShareActionState {
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
     */
    public static TabListEditorAction createAction(
            Context context,
            @ShowMode int showMode,
            @ButtonType int buttonType,
            @IconPosition int iconPosition) {
        Drawable drawable =
                AppCompatResources.getDrawable(context, R.drawable.tab_list_editor_share_icon);
        return new TabListEditorShareAction(
                context, showMode, buttonType, iconPosition, drawable);
    }

    private TabListEditorShareAction(
            Context context,
            @ShowMode int showMode,
            @ButtonType int buttonType,
            @IconPosition int iconPosition,
            Drawable drawable) {
        super(
                R.id.tab_list_editor_share_menu_item,
                showMode,
                buttonType,
                iconPosition,
                R.plurals.tab_selection_editor_share_tabs_action_button,
                R.plurals.accessibility_tab_selection_editor_share_tabs_action_button,
                drawable);
        mContext = context;
        mBroadcastReceiver =
                new BroadcastReceiver() {
                    @Override
                    public void onReceive(Context context, Intent intent) {
                        context.unregisterReceiver(mBroadcastReceiver);
                        // Hide the selection editor if the custom share intent is sent and received
                        // by another app, indicating that the user has completed the share tabs
                        // workflow.
                        getActionDelegate().hideByAction();
                    }
                };
    }

    @Override
    public void onSelectionStateChange(List<Integer> tabIds) {
        boolean enableShare = false;
        List<Tab> selectedTabs = getTabsOrTabsAndRelatedTabsFromSelection();

        for (Tab tab : selectedTabs) {
            if (!shouldFilterUrl(tab.getUrl())) {
                enableShare = true;
                break;
            }
        }

        int size = editorSupportsActionOnRelatedTabs() ? selectedTabs.size() : tabIds.size();
        setEnabledAndItemCount(enableShare, size);
    }

    @Override
    public boolean performAction(List<Tab> tabs) {
        assert !tabs.isEmpty() : "Share action should not be enabled for no tabs.";

        TabList tabList = getTabGroupModelFilter().getTabModel();
        List<Integer> sortedTabIndexList = filterTabs(tabs, tabList);

        if (sortedTabIndexList.size() == 0) {
            TabUiMetricsHelper.recordShareStateHistogram(
                    TabListEditorShareActionState.ALL_TABS_FILTERED);
            return false;
        }

        boolean isOnlyOneTab = (sortedTabIndexList.size() == 1);
        String tabText =
                isOnlyOneTab ? "" : getTabListStringForSharing(sortedTabIndexList, tabList);
        String tabTitle =
                isOnlyOneTab ? tabList.getTabAt(sortedTabIndexList.get(0)).getTitle() : "";
        String tabUrl =
                isOnlyOneTab ? tabList.getTabAt(sortedTabIndexList.get(0)).getUrl().getSpec() : "";
        @TabListEditorActionMetricGroups
        int actionId =
                isOnlyOneTab
                        ? TabListEditorActionMetricGroups.SHARE_TAB
                        : TabListEditorActionMetricGroups.SHARE_TABS;

        ShareParams shareParams =
                new ShareParams.Builder(
                                tabList.getTabAt(sortedTabIndexList.get(0)).getWindowAndroid(),
                                tabTitle,
                                tabUrl)
                        .setText(tabText)
                        .build();

        final Intent shareIntent = new Intent(Intent.ACTION_SEND);
        shareIntent.putExtra(Intent.EXTRA_TEXT, shareParams.getTextAndUrl());
        shareIntent.setType("text/plain");
        var context = mContext;
        var resources = context.getResources();
        shareIntent.putExtra(
                Intent.EXTRA_TITLE,
                resources.getQuantityString(
                        R.plurals.tab_selection_editor_share_sheet_preview_message,
                        sortedTabIndexList.size(),
                        sortedTabIndexList.size()));
        shareIntent.setFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION);

        float padding =
                resources.getDimension(
                        R.dimen.tab_list_editor_share_sheet_preview_thumbnail_padding);
        Drawable drawable =
                new InsetDrawable(
                        AppCompatResources.getDrawable(context, R.drawable.chrome_sync_logo),
                        (int) padding);

        // Create a custom share intent and receiver to assess if another app receives the share
        // intent sent from the tab selection editor.
        Intent receiver = new Intent("SHARE_ACTION");
        PendingIntent pendingIntent =
                PendingIntent.getBroadcast(context, 0, receiver, PendingIntent.FLAG_IMMUTABLE);
        ContextUtils.registerNonExportedBroadcastReceiver(
                context, mBroadcastReceiver, new IntentFilter("SHARE_ACTION"));
        createShareableImageAndSendIntent(shareIntent, drawable, actionId, pendingIntent);
        return true;
    }

    @Override
    public boolean shouldHideEditorAfterAction() {
        // Ensure the selection editor stays open when the user is interacting with the share
        // sheet in case they decide to leave and go back to the selection editor.
        return false;
    }

    private void createShareableImageAndSendIntent(
            Intent shareIntent,
            Drawable drawable,
            @TabListEditorActionMetricGroups int actionId,
            PendingIntent pendingIntent) {
        PostTask.postTask(
                TaskTraits.USER_BLOCKING_MAY_BLOCK,
                () -> {
                    // Allotted thumbnail size is approx. 72 dp, with the icon left at default size.
                    // The padding is adjusted accordingly, taking into account the scaling factor.
                    Bitmap bitmap =
                            Bitmap.createBitmap(
                                    drawable.getIntrinsicWidth(),
                                    drawable.getIntrinsicHeight(),
                                    Bitmap.Config.ARGB_8888);
                    Canvas canvas = new Canvas(bitmap);
                    drawable.setBounds(0, 0, canvas.getWidth(), canvas.getHeight());
                    drawable.draw(canvas);

                    ShareImageFileUtils.generateTemporaryUriFromBitmap(
                            mContext.getResources()
                                    .getString(
                                            R.string
                                                    .tab_selection_editor_share_sheet_preview_thumbnail),
                            bitmap,
                            uri -> {
                                bitmap.recycle();
                                PostTask.postTask(
                                        TaskTraits.UI_DEFAULT,
                                        () -> {
                                            shareIntent.setClipData(ClipData.newRawUri("", uri));
                                            mContext.startActivity(
                                                    Intent.createChooser(
                                                            shareIntent,
                                                            null,
                                                            pendingIntent.getIntentSender()));
                                            TabUiMetricsHelper.recordSelectionEditorActionMetrics(
                                                    actionId);
                                            TabUiMetricsHelper.recordShareStateHistogram(
                                                    TabListEditorShareActionState.SUCCESS);
                                        });

                                if (sIntentCallbackForTesting != null) {
                                    sIntentCallbackForTesting.onResult(shareIntent);
                                }
                            });
                });
    }

    // TODO(crbug.com/40871819): Current filtering does not remove duplicates or show a "Toast" if
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

        // TODO(crbug.com/40871819): Check if this string builder assembles the shareable URLs in
        // accordance with internationalization and translation standards
        for (int i = 0; i < sortedTabIndexList.size(); i++) {
            sb.append(i + 1)
                    .append(". ")
                    .append(list.getTabAt(sortedTabIndexList.get(i)).getUrl().getSpec())
                    .append("\n");
        }
        return sb.toString();
    }

    private boolean shouldFilterUrl(GURL url) {
        if (mSkipUrlCheckForTesting) return false;

        return url == null
                || !url.isValid()
                || url.isEmpty()
                || UNSUPPORTED_SCHEMES.contains(url.getScheme());
    }

    void setSkipUrlCheckForTesting(boolean skip) {
        mSkipUrlCheckForTesting = skip;
        ResettersForTesting.register(() -> mSkipUrlCheckForTesting = false);
    }

    static void setIntentCallbackForTesting(Callback<Intent> callback) {
        sIntentCallbackForTesting = callback;
        ResettersForTesting.register(() -> sIntentCallbackForTesting = null);
    }
}
