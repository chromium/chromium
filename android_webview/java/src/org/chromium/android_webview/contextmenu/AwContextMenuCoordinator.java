// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.contextmenu;

import static org.chromium.android_webview.contextmenu.AwContextMenuItemProperties.MENU_ID;

import android.app.Activity;
import android.content.Context;
import android.graphics.Color;
import android.graphics.Rect;
import android.graphics.drawable.ColorDrawable;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewStub;
import android.view.Window;
import android.widget.ListView;

import androidx.activity.ComponentDialog;
import androidx.annotation.IntDef;

import org.chromium.android_webview.AwContents;
import org.chromium.android_webview.AwSettings.HyperlinkContextMenuItems;
import org.chromium.android_webview.R;
import org.chromium.build.annotations.Nullable;
import org.chromium.components.embedder_support.contextmenu.ContextMenuParams;
import org.chromium.components.embedder_support.contextmenu.ContextMenuUtils;
import org.chromium.content_public.browser.LoadCommittedDetails;
import org.chromium.content_public.browser.Visibility;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.WebContentsObserver;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modelutil.LayoutViewBuilder;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.ModelListAdapter;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.widget.AnchoredPopupWindow;
import org.chromium.ui.widget.RectProvider;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.List;

/** The main coordinator for the context menu, responsible for creating the context menu */
public class AwContextMenuCoordinator {
    @Retention(RetentionPolicy.SOURCE)
    @IntDef({ListItemType.DIVIDER, ListItemType.HEADER, ListItemType.CONTEXT_MENU_ITEM})
    public @interface ListItemType {
        int DIVIDER = 0;
        int HEADER = 1;
        int CONTEXT_MENU_ITEM = 2;
    }

    private static final int INVALID_ITEM_ID = -1;

    private ListView mListView;
    private @Nullable AwContextMenuPopulator mCurrentPopulator;
    private final AwContextMenuHeaderCoordinator mHeaderCoordinator;
    private final WindowAndroid mWindowAndroid;
    private final Context mContext;
    private final WebContents mWebContents;
    private final ContextMenuParams mParams;
    private final List<ModelList> mItems;
    private ComponentDialog mDialog;
    private AnchoredPopupWindow mPopupWindow;
    private WebContentsObserver mWebContentsObserver;
    private final boolean mIsDragDropEnabled;
    private final boolean mUsePopupWindow;

    AwContextMenuCoordinator(
            WindowAndroid windowAndroid,
            WebContents webContents,
            ContextMenuParams params,
            boolean isDragDropEnabled,
            boolean usePopupWindow,
            @HyperlinkContextMenuItems int menuItems) {
        mWindowAndroid = windowAndroid;
        mContext = windowAndroid.getContext().get();
        mWebContents = webContents;
        mParams = params;
        mUsePopupWindow = usePopupWindow;
        mIsDragDropEnabled = isDragDropEnabled;

        mCurrentPopulator =
                new AwContextMenuPopulator(
                        mContext,
                        windowAndroid.getActivity().get(),
                        mWebContents,
                        mParams,
                        mUsePopupWindow,
                        menuItems);

        mItems = mCurrentPopulator.buildContextMenu();
        mHeaderCoordinator = new AwContextMenuHeaderCoordinator(mParams, mContext);
    }

    public void dismiss() {
        if (mWebContentsObserver != null) {
            mWebContentsObserver.observe(null);
        }

        if (mPopupWindow != null) {
            mPopupWindow.dismiss();
            mPopupWindow = null;
        }

        if (mDialog != null) {
            mDialog.dismiss();
            mDialog = null;
        }

        if (mCurrentPopulator != null) {
            mCurrentPopulator.onMenuClosed();
            mCurrentPopulator = null;
        }
    }

    void displayMenu() {
        if (mItems.isEmpty()) {
            return;
        }

        View layout =
                LayoutInflater.from(mContext)
                        .inflate(R.layout.aw_context_menu_fullscreen_container, null);
        View menu =
                mUsePopupWindow
                        ? LayoutInflater.from(mContext)
                                .inflate(R.layout.aw_context_menu_dropdown, null)
                        : ((ViewStub) layout.findViewById(R.id.aw_context_menu_stub)).inflate();

        // We only want to set the header icon if the context menu is displayed as a dropdown.
        if (mUsePopupWindow) {
            AwContents awContents = AwContents.fromWebContents(mWebContents);
            mHeaderCoordinator.setHeaderIcon(mParams.getPageUrl(), awContents.getFavicon());
        }

        ListItem headerItem = new ListItem(ListItemType.HEADER, mHeaderCoordinator.getModel());

        ModelList listItems = getItemList(headerItem, mItems, mUsePopupWindow);
        ModelListAdapter adapter =
                new ModelListAdapter(listItems) {
                    @Override
                    public boolean areAllItemsEnabled() {
                        return false;
                    }

                    @Override
                    public boolean isEnabled(int position) {
                        return getItemViewType(position) == ListItemType.CONTEXT_MENU_ITEM;
                    }

                    @Override
                    public long getItemId(int position) {
                        if (getItemViewType(position) == ListItemType.CONTEXT_MENU_ITEM) {
                            return ((ListItem) getItem(position)).model.get(MENU_ID);
                        }
                        return INVALID_ITEM_ID;
                    }
                };

        mListView = menu.findViewById(R.id.context_menu_list_view);
        registerViewTypes(adapter);
        mListView.setAdapter(adapter);
        mListView.setOnItemClickListener(
                (parent, view, position, id) -> {
                    assert id != INVALID_ITEM_ID;
                    clickItem((int) id, mWindowAndroid.getActivity().get());
                });

        if (mUsePopupWindow) {
            showAsPopupWindow(menu);
        } else {
            showAsDialog(layout);
        }
    }

    /**
     * Displays the context menu as a dialog.
     *
     * @param layout The view containing the layout of the menu.
     */
    private void showAsDialog(View layout) {
        mDialog = new ComponentDialog(mContext);
        Window dialogWindow = mDialog.getWindow();
        if (dialogWindow == null) return;
        dialogWindow.getDecorView().setBackground(new ColorDrawable(Color.TRANSPARENT));
        mDialog.setContentView(layout);

        mDialog.setOnShowListener(dialogInterface -> {});
        mDialog.setOnDismissListener(dialogInterface -> dismiss());
        mDialog.show();
    }

    /**
     * Displays the context menu as an anchored popup window.
     *
     * @param menu The view containing the layout of the menu.
     */
    private void showAsPopupWindow(View menu) {
        View dragDispatchingTargetView = mWebContents.getViewAndroidDelegate().getContainerView();

        Rect rect =
                ContextMenuUtils.getContextMenuAnchorRect(
                        mContext,
                        mWindowAndroid.getWindow(),
                        mWebContents,
                        mParams,
                        0,
                        true,
                        dragDispatchingTargetView);

        Integer desiredPopupContentWidth = null;
        if (mIsDragDropEnabled) {
            desiredPopupContentWidth =
                    mContext.getResources()
                            .getDimensionPixelSize(R.dimen.context_menu_popup_max_width);
        } else if (mParams.getOpenedFromHighlight()) {
            desiredPopupContentWidth =
                    mContext.getResources().getDimensionPixelSize(R.dimen.context_menu_small_width);
        }

        mPopupWindow =
                new AnchoredPopupWindow(
                        /* context= */ mContext,
                        /* rootView= */ dragDispatchingTargetView,
                        /* background= */ new ColorDrawable(Color.TRANSPARENT),
                        /* contentView= */ menu,
                        /* anchorRectProvider= */ new RectProvider(rect));

        mPopupWindow.setSmartAnchorWithMaxWidth(true);
        mPopupWindow.setVerticalOverlapAnchor(true);
        mPopupWindow.setOutsideTouchable(true);
        if (desiredPopupContentWidth != null) {
            mPopupWindow.setDesiredContentWidth(desiredPopupContentWidth);
        }

        // Required for dismissing the popup on backpress or if the webcontents visibility changes.
        mWebContentsObserver =
                new WebContentsObserver(mWebContents) {
                    @Override
                    public void navigationEntryCommitted(LoadCommittedDetails details) {
                        dismiss();
                    }

                    @Override
                    public void onVisibilityChanged(@Visibility int visibility) {
                        if (visibility != Visibility.VISIBLE) dismiss();
                    }
                };

        mPopupWindow.addOnDismissListener(this::dismiss);
        mPopupWindow.show();
    }

    /**
     * Execute an action for the selected item and close the menu.
     *
     * @param id The id of the item.
     * @param activity The current activity.
     */
    private void clickItem(int id, Activity activity) {
        if (activity.isFinishing() || activity.isDestroyed()) return;

        mCurrentPopulator.onItemSelected(id);
        dismiss();
    }

    private static ModelList getItemList(
            ListItem headerItem, List<ModelList> items, boolean usePopupWindow) {
        ModelList itemList = new ModelList();
        itemList.add(headerItem);

        if (usePopupWindow) {
            itemList.add(new ListItem(ListItemType.DIVIDER, new PropertyModel.Builder().build()));
        }

        for (ModelList item : items) {
            itemList.addAll(item);
        }

        return itemList;
    }

    private void registerViewTypes(ModelListAdapter adapter) {
        adapter.registerType(
                ListItemType.HEADER,
                new LayoutViewBuilder(R.layout.aw_context_menu_header),
                AwContextMenuHeaderViewBinder::bind);

        adapter.registerType(
                ListItemType.DIVIDER,
                new LayoutViewBuilder(R.layout.aw_context_menu_divider),
                (model, view, propertyKey) -> {});

        adapter.registerType(
                ListItemType.CONTEXT_MENU_ITEM,
                new LayoutViewBuilder(R.layout.aw_context_menu_row),
                AwContextMenuItemViewBinder::bind);
    }

    public void clickListItemForTesting(int id) {
        mListView.performItemClick(null, -1, id);
    }

    public AnchoredPopupWindow getPopupWindowForTesting() {
        return mPopupWindow;
    }

    public ComponentDialog getDialogForTesting() {
        return mDialog;
    }

    public ListView getListViewForTest() {
        return mListView;
    }

    public AwContextMenuHeaderCoordinator getHeaderCoordinatorForTesting() {
        return mHeaderCoordinator;
    }
}
