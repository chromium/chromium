// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.glic;

import android.content.Context;
import android.content.res.Resources;
import android.graphics.Color;
import android.graphics.drawable.ColorDrawable;
import android.view.View;

import androidx.annotation.DrawableRes;
import androidx.annotation.IntDef;
import androidx.annotation.VisibleForTesting;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.actor.ActorTask;
import org.chromium.chrome.browser.actor.ActorTaskState;
import org.chromium.chrome.browser.glic.GlicKeyedService.GlicInvocationSource;
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
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.util.AttrUtils;
import org.chromium.ui.widget.AnchoredPopupWindow;
import org.chromium.ui.widget.RectProvider;
import org.chromium.ui.widget.ViewRectProvider;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.List;
import java.util.Set;
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
    private final @GlicInvocationSource int mInvocationSource;
    private final @ButtonSource int mButtonSource;
    private @Nullable AnchoredPopupWindow mMenuWindow;

    /**
     * Constructs the task menu coordinator.
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
        mContext = context;

        mTabModelSelectorSupplier = tabModelSelectorSupplier;
        mToggleGlicCallback = toggleGlicCallback;
        mInvocationSource = invocationSource;
        mButtonSource = buttonSource;
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
        mMenuWindow.show();
    }

    @VisibleForTesting
    ModelList buildModelList(List<ActorTask> tasks) {
        ModelList modelList = new ModelList();
        int endIconWidthPx =
                AttrUtils.getDimensionPixelSize(mContext, R.attr.glicTaskMenuEndIconWidth);

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
                                        mToggleGlicCallback.onClick(
                                                /* preventClose= */ true, mInvocationSource);
                                        dismiss();
                                    });

            boolean needsReview =
                    GlicButtonStateController.mapTaskStateToButtonState(task.getState())
                            == GlicButtonStateController.ButtonState.NEEDS_REVIEW;

            if (needsReview) {
                builder.withStartIconRes(R.drawable.ic_hourglass_empty_24dp);
            } else {
                builder.withStartIconRes(R.drawable.ic_arrow_selector_spark_24dp);
            }

            if (mButtonSource == ButtonSource.TAB_STRIP) {
                builder.withSubtitle(getTaskSubtitle(mContext, task));
            }

            int endIconRes = getEndIconRes(needsReview, mButtonSource == ButtonSource.TAB_STRIP);
            builder.withEndIconWidth(endIconWidthPx)
                    .withEndIconRes(endIconRes)
                    .withShouldTintEndIcon(endIconRes != R.drawable.glic_menu_dot);

            modelList.add(builder.build());
        }

        if (shouldShowAskGemini()) {
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
                                        mToggleGlicCallback.onClick(
                                                /* preventClose= */ false, mInvocationSource);
                                        dismiss();
                                    })
                            .build());
        }
        return modelList;
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

        if (!hasTab) {
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

    private void switchToActuatingTab(Set<Integer> tabs) {
        TabModelSelector selector = mTabModelSelectorSupplier.get();
        if (selector == null) return;

        if (!tabs.isEmpty()) {
            for (int tabId : tabs) {
                if (selector.getTabById(tabId) != null) {
                    TabModelUtils.selectTabById(selector, tabId, TabSelectionType.FROM_USER);
                    break;
                }
            }
        } else {
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

    private boolean shouldShowAskGemini() {
        return mButtonSource != ButtonSource.TAB_STRIP;
    }
}
