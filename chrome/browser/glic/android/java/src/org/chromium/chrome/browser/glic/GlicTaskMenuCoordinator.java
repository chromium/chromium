// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.glic;

import android.content.Context;
import android.graphics.Color;
import android.graphics.drawable.ColorDrawable;
import android.view.View;

import androidx.annotation.VisibleForTesting;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.actor.ActorTask;
import org.chromium.chrome.browser.tab.TabSelectionType;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModelUtils;
import org.chromium.components.browser_ui.widget.BrowserUiListMenuUtils;
import org.chromium.components.browser_ui.widget.ListItemBuilder;
import org.chromium.ui.listmenu.BasicListMenu;
import org.chromium.ui.listmenu.ListMenu;
import org.chromium.ui.listmenu.ListMenuItemProperties;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.widget.AnchoredPopupWindow;
import org.chromium.ui.widget.RectProvider;
import org.chromium.ui.widget.ViewRectProvider;

import java.util.List;
import java.util.Set;
import java.util.function.Supplier;

/**
 * Coordinates the shared task list drop-down list menu for Glic buttons. Takes a collection of
 * supplied active background tasks and presents them inside an anchored window.
 */
@NullMarked
public class GlicTaskMenuCoordinator {
    private final Context mContext;

    private final Supplier<@Nullable TabModelSelector> mTabModelSelectorSupplier;
    private final GlicButtonDelegate mToggleGlicCallback;
    private @Nullable AnchoredPopupWindow mMenuWindow;

    /**
     * Constructs the task menu coordinator.
     *
     * @param context The Android context.
     * @param tabModelSelectorSupplier Supplier for the active TabModelSelector.
     * @param toggleGlicCallback Callback to activate or open the Glic UI sheet panel.
     */
    public GlicTaskMenuCoordinator(
            Context context,
            Supplier<@Nullable TabModelSelector> tabModelSelectorSupplier,
            GlicButtonDelegate toggleGlicCallback) {
        mContext = context;

        mTabModelSelectorSupplier = tabModelSelectorSupplier;
        mToggleGlicCallback = toggleGlicCallback;
    }

    /**
     * Displays the task menu anchored directly against an Android View layout element.
     *
     * @param anchorView The View to anchor the pop-up list overlay.
     * @param tasks The collection of active actor tasks to list.
     */
    public void show(View anchorView, List<ActorTask> tasks) {
        showInternal(new ViewRectProvider(anchorView), anchorView.getRootView(), tasks);
    }

    /**
     * Displays the task menu anchored to coordinate boundaries supplied by a RectProvider.
     *
     * @param rectProvider Coordinates defining the geometric anchor frame.
     * @param rootView The root view hierarchy stack to inject the popup layer.
     * @param tasks The collection of active actor tasks to list.
     */
    public void show(RectProvider rectProvider, View rootView, List<ActorTask> tasks) {
        showInternal(rectProvider, rootView, tasks);
    }

    /** Safely dismisses and tears down the floating task popup window overlay if visible. */
    public void dismiss() {
        if (mMenuWindow != null) {
            mMenuWindow.dismiss();
            mMenuWindow = null;
        }
    }

    /**
     * Checks whether the pop-up menu bubble is active and visible on screen.
     *
     * @return true if the window drop-down overlay is visible.
     */
    public boolean isShowing() {
        return mMenuWindow != null && mMenuWindow.isShowing();
    }

    private void showInternal(RectProvider rectProvider, View rootView, List<ActorTask> tasks) {
        ModelList modelList = buildModelList(tasks);

        ListMenu.Delegate delegate =
                new ListMenu.Delegate() {
                    @Override
                    public void onItemSelected(PropertyModel model, View view) {
                        View.OnClickListener listener =
                                model.get(ListMenuItemProperties.CLICK_LISTENER);

                        if (listener != null) {
                            listener.onClick(view);
                        }
                    }
                };

        BasicListMenu listMenu =
                BrowserUiListMenuUtils.getBasicListMenu(mContext, modelList, delegate);
        View contentView = listMenu.getContentView();

        // Add gap to the right of the menu so it is not at the right edge of the screen.
        int endOffsetPx =
                mContext.getResources().getDimensionPixelSize(R.dimen.glic_task_menu_end_offset);
        int lateralPadding = contentView.getPaddingLeft() + contentView.getPaddingRight();
        int widthPx = listMenu.getMaxItemWidth() + lateralPadding;

        int maxWidthPx =
                mContext.getResources().getDimensionPixelSize(R.dimen.glic_task_menu_max_width);
        widthPx = Math.min(widthPx, maxWidthPx);

        mMenuWindow =
                new AnchoredPopupWindow.Builder(
                                mContext,
                                rootView,
                                new ColorDrawable(Color.TRANSPARENT),
                                () -> contentView,
                                rectProvider)
                        .setFocusable(true)
                        .setTouchModal(true)
                        .setDismissOnTouchInteraction(true)
                        .setHorizontalOverlapAnchor(true)
                        .setVerticalOverlapAnchor(false)
                        .setPreferredHorizontalOrientation(
                                AnchoredPopupWindow.HorizontalOrientation.LAYOUT_DIRECTION)
                        .setDesiredContentWidth(widthPx)
                        .setMaxWidth(maxWidthPx)
                        .setMargin(endOffsetPx)
                        .setAnimateFromAnchor(true)
                        .setAllowNonTouchableSize(true)
                        .build();
        mMenuWindow.show();
    }

    @VisibleForTesting
    ModelList buildModelList(List<ActorTask> tasks) {
        ModelList modelList = new ModelList();
        int endIconWidthPx =
                mContext.getResources().getDimensionPixelSize(R.dimen.glic_menu_dot_width);

        // TODO(crbug.com/498721993): Listen to the task and update menu item when needed.
        for (ActorTask task : tasks) {
            ListItemBuilder builder =
                    new ListItemBuilder()
                            .withTitle(task.getTitle())
                            .withIsIncognito(false)
                            .withIsTextEllipsizedAtEnd(true)
                            .withClickListener(
                                    v -> {
                                        switchToActuatingTab(task.getLastActedTabs());
                                        mToggleGlicCallback.onClick(/* preventClose= */ true);
                                        dismiss();
                                    });

            if (GlicButtonStateController.mapTaskStateToButtonState(task.getState())
                    == GlicButtonStateController.ButtonState.NEEDS_REVIEW) {
                builder.withStartIconRes(R.drawable.ic_hourglass_empty_24dp)
                        .withEndIconRes(R.drawable.glic_menu_dot)
                        .withEndIconWidth(endIconWidthPx);
            } else {
                builder.withStartIconRes(R.drawable.ic_arrow_selector_spark_24dp);
            }

            modelList.add(builder.build());
        }

        // Divider
        modelList.add(BasicListMenu.buildMenuDivider(false));

        // Ask Gemini
        modelList.add(
                new ListItemBuilder()
                        .withTitleRes(R.string.glic_button_entrypoint_ask_gemini_label)
                        .withStartIconRes(R.drawable.ic_spark_24dp)
                        .withIsIncognito(false)
                        .withClickListener(
                                v -> {
                                    mToggleGlicCallback.onClick(/* preventClose= */ false);
                                    dismiss();
                                })
                        .build());
        return modelList;
    }

    private void switchToActuatingTab(Set<Integer> tabs) {
        if (!tabs.isEmpty()) {
            int tabId = tabs.iterator().next();
            TabModelSelector selector = mTabModelSelectorSupplier.get();
            if (selector != null) {
                TabModelUtils.selectTabById(selector, tabId, TabSelectionType.FROM_USER);
            }
        }
    }
}
