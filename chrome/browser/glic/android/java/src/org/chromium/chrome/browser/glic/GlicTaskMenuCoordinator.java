// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.glic;

import android.content.Context;
import android.content.res.Resources;
import android.graphics.Color;
import android.graphics.drawable.ColorDrawable;
import android.text.TextUtils;
import android.view.View;
import android.view.View.OnClickListener;
import android.widget.PopupWindow.OnDismissListener;

import androidx.annotation.DrawableRes;
import androidx.annotation.IntDef;
import androidx.annotation.VisibleForTesting;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.actor.ActorTask;
import org.chromium.chrome.browser.actor.ActorTaskState;
import org.chromium.chrome.browser.glic.GlicKeyedService.GlicInvocationSource;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tab.TabSelectionType;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModelUtils;
import org.chromium.chrome.browser.url_constants.UrlConstantResolver;
import org.chromium.components.browser_ui.widget.BrowserUiListMenuUtils;
import org.chromium.components.browser_ui.widget.ListItemBuilder;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.ui.listmenu.BasicListMenu;
import org.chromium.ui.listmenu.ListMenu;
import org.chromium.ui.listmenu.ListMenuItemProperties;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.util.AttrUtils;
import org.chromium.ui.widget.AnchoredPopupWindow;
import org.chromium.ui.widget.RectProvider;
import org.chromium.ui.widget.ViewRectProvider;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.List;
import java.util.function.Supplier;

/**
 * Coordinates the shared task list drop-down list menu for Glic buttons. Takes a collection of
 * supplied active background tasks and presents them inside an anchored window.
 */
@NullMarked
public class GlicTaskMenuCoordinator {
    @IntDef({ButtonSource.TOOLBAR, ButtonSource.TAB_STRIP, ButtonSource.BOTTOM_BAR})
    @Retention(RetentionPolicy.SOURCE)
    public @interface ButtonSource {
        int TOOLBAR = 0;
        int TAB_STRIP = 1;
        int BOTTOM_BAR = 2;
    }

    private final Context mContext;

    private final Supplier<@Nullable TabModelSelector> mTabModelSelectorSupplier;
    private final GlicButtonDelegate mToggleGlicCallback;
    private final @Nullable GlicSplitButtonDelegateBridge mDelegateBridge;
    private final @GlicInvocationSource int mInvocationSource;
    private final @ButtonSource int mButtonSource;
    private @Nullable AnchoredPopupWindow mMenuWindow;
    private @Nullable OnDismissListener mOnDismiss;

    /**
     * Sets a listener to be called when the task menu is dismissed.
     *
     * @param onDismiss The listener to set.
     */
    public void setOnDismiss(@Nullable OnDismissListener onDismiss) {
        mOnDismiss = onDismiss;
    }

    /**
     * Constructs the task menu coordinator with a delegate bridge.
     *
     * @param context The Android context.
     * @param tabModelSelectorSupplier Supplier for the active TabModelSelector.
     * @param toggleGlicCallback Callback to activate or open the Glic UI sheet panel.
     * @param delegateBridge Delegate bridge for native task interaction events.
     * @param invocationSource The Glic invocation source.
     * @param buttonSource The source button triggering the menu.
     */
    public GlicTaskMenuCoordinator(
            Context context,
            Supplier<@Nullable TabModelSelector> tabModelSelectorSupplier,
            GlicButtonDelegate toggleGlicCallback,
            @Nullable GlicSplitButtonDelegateBridge delegateBridge,
            @GlicInvocationSource int invocationSource,
            @ButtonSource int buttonSource) {
        mContext = context;
        mTabModelSelectorSupplier = tabModelSelectorSupplier;
        mToggleGlicCallback = toggleGlicCallback;
        mDelegateBridge = delegateBridge;
        mInvocationSource = invocationSource;
        mButtonSource = buttonSource;
    }

    /**
     * Constructs the task menu coordinator without a delegate bridge.
     *
     * @param context The Android context.
     * @param tabModelSelectorSupplier Supplier for the active TabModelSelector.
     * @param toggleGlicCallback Callback to activate or open the Glic UI sheet panel.
     * @param invocationSource The Glic invocation source.
     * @param buttonSource The source button triggering the menu.
     */
    public GlicTaskMenuCoordinator(
            Context context,
            Supplier<@Nullable TabModelSelector> tabModelSelectorSupplier,
            GlicButtonDelegate toggleGlicCallback,
            @GlicInvocationSource int invocationSource,
            @ButtonSource int buttonSource) {
        this(
                context,
                tabModelSelectorSupplier,
                toggleGlicCallback,
                /* delegateBridge= */ null,
                invocationSource,
                buttonSource);
    }

    /**
     * Displays the task menu anchored directly against an Android View layout element.
     *
     * @param anchorView The View to anchor the pop-up list overlay.
     * @param tasks The collection of active actor tasks to list.
     */
    public void show(View anchorView, List<ActorTask> tasks) {
        showInternal(
                new ViewRectProvider(anchorView), anchorView.getRootView(), buildModelList(tasks));
    }

    /**
     * Displays the task menu anchored to coordinate boundaries supplied by a RectProvider.
     *
     * @param rectProvider Coordinates defining the geometric anchor frame.
     * @param rootView The root view hierarchy stack to inject the popup layer.
     * @param tasks The collection of active actor tasks to list.
     */
    public void show(RectProvider rectProvider, View rootView, List<ActorTask> tasks) {
        showInternal(rectProvider, rootView, buildModelList(tasks));
    }

    /**
     * Displays the task menu anchored to coordinate boundaries supplied by a RectProvider using
     * pre-computed row data.
     *
     * @param rectProvider Coordinates defining the geometric anchor frame.
     * @param rootView The root view hierarchy stack to inject the popup layer.
     * @param rows The prioritized list of task rows to list.
     */
    public void showFromRowData(
            RectProvider rectProvider, View rootView, List<ActorTaskRowData> rows) {
        showInternal(rectProvider, rootView, buildModelListFromRowData(rows));
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

    private void showInternal(RectProvider rectProvider, View rootView, ModelList modelList) {
        dismiss();
        if (modelList.isEmpty()) {
            return;
        }

        ListMenu.Delegate delegate =
                new ListMenu.Delegate() {
                    @Override
                    public void onItemSelected(PropertyModel model, View view) {
                        OnClickListener listener = model.get(ListMenuItemProperties.CLICK_LISTENER);

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
        int maxWidthPx = AttrUtils.getDimensionPixelSize(mContext, R.attr.glicTaskMenuMaxWidth);
        int widthPx;

        if (mButtonSource == ButtonSource.TAB_STRIP) {
            widthPx = maxWidthPx;
        } else {
            int lateralPadding = contentView.getPaddingLeft() + contentView.getPaddingRight();
            widthPx = Math.min(listMenu.getMaxItemWidth() + lateralPadding, maxWidthPx);
        }

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
        mMenuWindow.addOnDismissListener(
                () -> {
                    if (mOnDismiss != null) {
                        mOnDismiss.onDismiss();
                    }
                    if (mDelegateBridge != null) {
                        mDelegateBridge.onActorTaskListBubbleDismissed();
                    }
                });
        mMenuWindow.show();
    }

    /** Constructs the model list from a collection of actor task row data. */
    @VisibleForTesting
    ModelList buildModelListFromRowData(List<ActorTaskRowData> rows) {
        ModelList modelList = new ModelList();
        for (ActorTaskRowData row : rows) {
            OnClickListener clickListener =
                    row.isEnabled
                            ? v -> {
                                TabModelSelector selector = mTabModelSelectorSupplier.get();
                                if (selector != null && row.tabId != Tab.INVALID_TAB_ID) {
                                    TabModelUtils.selectTabById(
                                            selector, row.tabId, TabSelectionType.FROM_USER);
                                }
                                mToggleGlicCallback.onClick(
                                        /* preventClose= */ true, mInvocationSource);
                                if (mDelegateBridge != null) {
                                    mDelegateBridge.onTaskRowClicked(row.taskId);
                                }
                                dismiss();
                            }
                            : null;

            modelList.add(
                    buildTaskListItem(
                            row.title,
                            row.subtitle,
                            row.isEnabled,
                            row.needsReview,
                            clickListener));
        }

        maybeAddOpenChatSection(modelList);
        return modelList;
    }

    /** Constructs the model list from a collection of actor tasks. */
    @VisibleForTesting
    ModelList buildModelList(List<ActorTask> tasks) {
        ModelList modelList = new ModelList();
        // TODO(crbug.com/498721993): Listen to the task and update menu item when needed.
        for (ActorTask task : tasks) {
            boolean needsReview =
                    GlicButtonStateController.mapTaskStateToButtonState(task.getState())
                            == GlicButtonStateController.ButtonState.NEEDS_REVIEW;
            String subtitle =
                    mButtonSource == ButtonSource.TAB_STRIP
                            ? getTaskSubtitle(mContext, task)
                            : null;
            OnClickListener clickListener =
                    v -> {
                        switchToActuatingTab(task);
                        mToggleGlicCallback.onClick(/* preventClose= */ true, mInvocationSource);
                        if (mDelegateBridge != null) {
                            mDelegateBridge.onTaskRowClicked(task.getId());
                        }
                        dismiss();
                    };

            modelList.add(
                    buildTaskListItem(
                            task.getTitle(),
                            subtitle,
                            /* isEnabled= */ true,
                            needsReview,
                            clickListener));
        }

        maybeAddOpenChatSection(modelList);
        return modelList;
    }

    private ListItem buildTaskListItem(
            String title,
            @Nullable String subtitle,
            boolean isEnabled,
            boolean needsReview,
            @Nullable OnClickListener clickListener) {
        int endIconWidthPx =
                AttrUtils.getDimensionPixelSize(mContext, R.attr.glicTaskMenuEndIconWidth);
        ListItemBuilder builder =
                new ListItemBuilder()
                        .withTitle(title)
                        .withIsIncognito(false)
                        .withIsTextEllipsizedAtEnd(true)
                        .withEnabled(isEnabled)
                        .withStartIconRes(
                                needsReview
                                        ? R.drawable.ic_hourglass_empty_24dp
                                        : R.drawable.ic_arrow_selector_spark_24dp);

        if (clickListener != null) {
            builder.withClickListener(clickListener);
        }

        if (mButtonSource == ButtonSource.TAB_STRIP) {
            int verticalPaddingPx =
                    mContext.getResources()
                            .getDimensionPixelSize(R.dimen.glic_task_menu_item_vertical_padding);
            builder.withVerticalPaddingPx(verticalPaddingPx)
                    .withTextAppearanceStyle(R.style.TextAppearance_TextLarge)
                    .withSubtitleTextAppearanceStyle(R.style.TextAppearance_TextMedium);
            if (!TextUtils.isEmpty(subtitle)) {
                builder.withSubtitle(subtitle);
            }
        }

        int endIconRes = getEndIconRes(needsReview, mButtonSource == ButtonSource.TAB_STRIP);
        builder.withEndIconWidth(endIconWidthPx)
                .withEndIconRes(endIconRes)
                .withShouldTintEndIcon(endIconRes != R.drawable.glic_menu_dot);

        return builder.build();
    }

    private void maybeAddOpenChatSection(ModelList modelList) {
        if (!shouldShowOpenChat()) {
            return;
        }
        // Divider
        modelList.add(BasicListMenu.buildMenuDivider(false));

        // Open Chat
        modelList.add(
                new ListItemBuilder()
                        .withTitleRes(R.string.glic_open_gemini_label)
                        .withStartIconRes(R.drawable.ic_spark_24dp)
                        .withIsIncognito(false)
                        .withClickListener(
                                v -> {
                                    mToggleGlicCallback.onClick(
                                            /* preventClose= */ false, mInvocationSource);
                                    dismiss();
                                })
                        .build());
    }

    private String getTaskSubtitle(Context context, ActorTask task) {
        boolean hasTab = false;
        TabModelSelector selector = mTabModelSelectorSupplier.get();
        if (selector != null) {
            for (int tabId : task.getLastActedTabs()) {
                if (selector.getTabById(tabId) != null) {
                    hasTab = true;
                    break;
                }
            }
        }

        if (!hasTab && task.isCompleted()) {
            return context.getString(R.string.actor_task_list_bubble_row_tab_closed_subtitle);
        }

        switch (task.getState()) {
            case ActorTaskState.WAITING_ON_USER:
            case ActorTaskState.PAUSED_BY_ACTOR:
                return context.getString(R.string.actor_task_list_bubble_row_check_task_subtitle);
            case ActorTaskState.FINISHED:
                return context.getString(
                        R.string.actor_task_list_bubble_row_completed_task_subtitle);
            case ActorTaskState.FAILED:
                return context.getString(R.string.actor_task_list_bubble_row_failed_task_subtitle);
            case ActorTaskState.PAUSED_BY_USER:
                return context.getString(R.string.actor_task_list_bubble_row_paused_task_subtitle);
            default:
                return context.getString(R.string.actor_task_list_bubble_row_acting_task_subtitle);
        }
    }

    private void switchToActuatingTab(ActorTask task) {
        TabModelSelector selector = mTabModelSelectorSupplier.get();
        if (selector == null) return;

        for (int tabId : task.getLastActedTabs()) {
            if (selector.getTabById(tabId) != null) {
                TabModelUtils.selectTabById(selector, tabId, TabSelectionType.FROM_USER);
                return;
            }
        }

        if (task.isCompleted()) {
            // If the task is completed/stopped and its tab was closed by the user, open a new NTP
            // tab.
            selector.openNewTab(
                    new LoadUrlParams(UrlConstantResolver.getOriginalNativeNtpUrl()),
                    TabLaunchType.FROM_CHROME_UI,
                    /* parent= */ null,
                    /* incognito= */ false);
        }
    }

    @DrawableRes
    private static int getEndIconRes(boolean needsReview, boolean showActionIcons) {
        if (showActionIcons) {
            return needsReview
                    ? R.drawable.glic_menu_end_icon_needs_review
                    : R.drawable.glic_menu_end_icon_standard;
        }
        return needsReview ? R.drawable.glic_menu_dot : Resources.ID_NULL;
    }

    private boolean shouldShowOpenChat() {
        return mButtonSource != ButtonSource.TAB_STRIP;
    }
}
