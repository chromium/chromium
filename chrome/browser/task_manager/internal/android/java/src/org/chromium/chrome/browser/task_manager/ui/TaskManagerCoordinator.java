// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.task_manager.ui;

import android.graphics.Bitmap;
import android.graphics.Color;
import android.graphics.Rect;
import android.graphics.Typeface;
import android.graphics.drawable.BitmapDrawable;
import android.graphics.drawable.ColorDrawable;
import android.text.Editable;
import android.text.TextUtils;
import android.text.TextWatcher;
import android.view.LayoutInflater;
import android.view.MotionEvent;
import android.view.View;
import android.view.ViewStub;
import android.view.inputmethod.EditorInfo;
import android.widget.EditText;
import android.widget.LinearLayout;
import android.widget.TextView;

import androidx.annotation.IdRes;
import androidx.annotation.Nullable;
import androidx.annotation.StringRes;
import androidx.annotation.VisibleForTesting;
import androidx.appcompat.content.res.AppCompatResources;
import androidx.recyclerview.widget.LinearLayoutManager;
import androidx.recyclerview.widget.RecyclerView;

import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.task_manager.ui.TaskManagerProperties.Category;
import org.chromium.chrome.browser.task_manager.ui.TaskManagerProperties.SortDescriptor;
import org.chromium.components.browser_ui.widget.BrowserUiListMenuUtils;
import org.chromium.components.browser_ui.widget.chips.ChipView;
import org.chromium.ui.KeyboardVisibilityDelegate;
import org.chromium.ui.listmenu.BasicListMenu;
import org.chromium.ui.listmenu.ListItemType;
import org.chromium.ui.listmenu.ListMenuCheckItemProperties;
import org.chromium.ui.listmenu.ListMenuItemProperties;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;
import org.chromium.ui.modelutil.SimpleRecyclerViewAdapter;
import org.chromium.ui.widget.AnchoredPopupWindow;
import org.chromium.ui.widget.ButtonCompat;
import org.chromium.ui.widget.RectProvider;

import java.util.ArrayList;
import java.util.List;
import java.util.Set;

/** Binds the model and the view of task manager. */
class TaskManagerCoordinator {
    private static final @Category int[] CATEGORIES = {
        Category.TABS_AND_EXTENSIONS, Category.BROWSER, Category.ALL_TASKS,
    };

    private final PropertyModel mHeaderModel;

    private final TaskManagerMediator mMediator;

    private final LinearLayout mHeaderView;
    private final SimpleRecyclerViewAdapter mAdapter;
    private final List<PropertyModelChangeProcessor<PropertyModel, View, PropertyKey>>
            mModelChangeProcessors = new ArrayList<>();

    private @Nullable AnchoredPopupWindow mContextMenuPopup;

    /** Sets up the UI in the activity, binding the activity and the model. */
    TaskManagerCoordinator(
            View taskManagerView,
            PropertyModel headerModel,
            ModelList tasksModel,
            TaskManagerMediator mediator) {
        mHeaderModel = headerModel;
        mMediator = mediator;

        mHeaderView = taskManagerView.findViewById(R.id.header_linear_layout);
        mHeaderView.setBackground(null);
        mHeaderView.setDividerDrawable(
                AppCompatResources.getDrawable(
                        mHeaderView.getContext(), R.drawable.task_header_divider));
        mHeaderView.setShowDividers(LinearLayout.SHOW_DIVIDER_MIDDLE);

        mModelChangeProcessors.add(
                PropertyModelChangeProcessor.create(
                        headerModel,
                        mHeaderView,
                        (model, view, key) -> {
                            for (PropertyKey columnKey : TaskManagerProperties.ALL_COLUMN_KEYS) {
                                view.findViewById(getTaskItemViewId(columnKey))
                                        .setOnClickListener(
                                                _ -> mMediator.cycleSortOrder(columnKey));
                            }
                            bindHeader(model, view, key);
                        }));

        RecyclerView recyclerView = taskManagerView.findViewById(R.id.tasks_view);
        recyclerView.setLayoutManager(
                new LinearLayoutManager(
                        recyclerView.getContext(), LinearLayoutManager.VERTICAL, false));
        recyclerView.addOnItemTouchListener(
                new RecyclerView.SimpleOnItemTouchListener() {
                    @Override
                    public boolean onInterceptTouchEvent(RecyclerView rv, MotionEvent e) {
                        if ((e.getButtonState() & MotionEvent.BUTTON_SECONDARY) == 0) return false;
                        if (e.getActionMasked() != MotionEvent.ACTION_DOWN) return true;
                        View child = rv.findChildViewUnder(e.getX(), e.getY());
                        int[] location = getEventLocationInWindow(rv, e);
                        showContextMenu(
                                child,
                                new RectProvider(
                                        new Rect(
                                                location[0],
                                                location[1],
                                                location[0],
                                                location[1])));
                        return true;
                    }
                });

        mAdapter = new SimpleRecyclerViewAdapter(tasksModel);
        recyclerView.setAdapter(mAdapter);

        mAdapter.registerType(
                TaskManagerProperties.RowType.TASK,
                (parent) ->
                        LayoutInflater.from(parent.getContext())
                                .inflate(R.layout.task_item, parent, false),
                (model, view, key) -> {
                    mModelChangeProcessors.add(
                            PropertyModelChangeProcessor.create(
                                    headerModel,
                                    view,
                                    TaskManagerCoordinator::bindHeaderModelAndTaskView));

                    view.setOnClickListener(_ -> mMediator.toggleSelection(model));
                    bindTask(model, view, key);
                });

        ButtonCompat killButton = taskManagerView.findViewById(R.id.kill_button);
        killButton.setText(R.string.task_manager_kill);
        killButton.setEnabled(false);
        killButton.setOnClickListener((view) -> mMediator.killSelectedTasks());
        mMediator.onHasKillableSelectedTaskChanged(
                (hasSelectedTask) -> killButton.setEnabled(hasSelectedTask));

        if (ChromeFeatureList.isEnabled(ChromeFeatureList.TASK_MANAGER_TOOLBAR)) {
            initToolbar(taskManagerView, headerModel);
        }

        mMediator.startObserving();
    }

    int[] getEventLocationInWindow(View view, MotionEvent e) {
        int[] location = new int[2];
        view.getLocationInWindow(location);
        location[0] += Math.round(e.getX());
        location[1] += Math.round(e.getY());
        return location;
    }

    /** Revert the bindings made in the constructor. */
    void destroy() {
        if (mContextMenuPopup != null) {
            mContextMenuPopup.dismiss();
            mContextMenuPopup = null;
        }

        mMediator.stopObserving();

        mModelChangeProcessors.forEach(PropertyModelChangeProcessor::destroy);
        mAdapter.destroy();
    }

    @VisibleForTesting
    void showContextMenu(@Nullable View anchorView, RectProvider rectProvider) {
        if (mContextMenuPopup != null && mContextMenuPopup.isShowing()) {
            return;
        }

        View anchor =
                (anchorView != null
                                && anchorView != mHeaderView.getRootView()
                                && anchorView != mHeaderView.getParent())
                        ? anchorView
                        : mHeaderView;

        ModelList menuItems = buildContextMenuModelList();
        BasicListMenu listMenu =
                BrowserUiListMenuUtils.getBasicListMenu(
                        anchor.getContext(),
                        menuItems,
                        (itemModel, view) -> {
                            int resId = itemModel.get(ListMenuItemProperties.MENU_ITEM_ID);
                            PropertyKey columnKey = getColumnKeyForResourceId(resId);
                            if (columnKey != null && mMediator.toggleColumnFiltering(columnKey)) {
                                boolean currentChecked =
                                        itemModel.get(ListMenuCheckItemProperties.CHECKED);
                                itemModel.set(ListMenuCheckItemProperties.CHECKED, !currentChecked);
                            }
                        });

        View contentView = listMenu.getContentView();
        final int lateralPadding = contentView.getPaddingLeft() + contentView.getPaddingRight();
        int minWidth =
                anchor.getResources()
                        .getDimensionPixelSize(org.chromium.ui.R.dimen.list_menu_width);
        int desiredWidth = Math.max(minWidth, listMenu.getMaxItemWidth() + lateralPadding);

        mContextMenuPopup =
                new AnchoredPopupWindow.Builder(
                                anchor.getContext(),
                                anchor,
                                new ColorDrawable(Color.TRANSPARENT),
                                () -> contentView,
                                rectProvider)
                        .setFocusable(true)
                        .setOutsideTouchable(true)
                        .setDismissOnScreenSizeChange(true)
                        .setDesiredContentWidth(desiredWidth)
                        .setAnimateFromAnchor(true)
                        .addOnDismissListener(() -> mContextMenuPopup = null)
                        .build();

        mContextMenuPopup.show();
    }

    @VisibleForTesting
    ModelList buildContextMenuModelList() {
        Set<PropertyKey> selectedColumns = Set.of(mHeaderModel.get(TaskManagerProperties.COLUMNS));
        ModelList listItems = new ModelList();

        for (PropertyKey columnKey : TaskManagerProperties.ALL_COLUMN_KEYS) {
            boolean isChecked = selectedColumns.contains(columnKey);
            PropertyModel itemModel =
                    new PropertyModel.Builder(ListMenuCheckItemProperties.ALL_KEYS)
                            .with(
                                    ListMenuItemProperties.TITLE,
                                    mHeaderView
                                            .getContext()
                                            .getString(getColumnTextResourceId(columnKey)))
                            .with(ListMenuCheckItemProperties.CHECKED, isChecked)
                            .with(ListMenuItemProperties.ENABLED, true)
                            .with(
                                    ListMenuItemProperties.MENU_ITEM_ID,
                                    getColumnTextResourceId(columnKey))
                            .build();

            listItems.add(new ListItem(ListItemType.MENU_ITEM_WITH_CHECKBOX, itemModel));
        }

        return listItems;
    }

    @VisibleForTesting
    @Nullable
    AnchoredPopupWindow getContextMenuPopupForTesting() {
        return mContextMenuPopup;
    }

    private static void bindHeader(PropertyModel model, View view, PropertyKey unused) {
        @Nullable SortDescriptor descriptor = model.get(TaskManagerProperties.SORT_DESCRIPTOR);
        Set<PropertyKey> selectedKeys = Set.of(model.get(TaskManagerProperties.COLUMNS));

        for (PropertyKey columnKey : TaskManagerProperties.ALL_COLUMN_KEYS) {
            TextView textView = view.findViewById(getTaskItemViewId(columnKey));
            if (!selectedKeys.contains(columnKey)) {
                textView.setVisibility(View.GONE);
                continue;
            }
            textView.setVisibility(View.VISIBLE);

            textView.setText(getColumnTextResourceId(columnKey));

            // TOOD(crbug.com/380158700): Descriptive message for a11y.
            if (descriptor != null && descriptor.key == columnKey) {
                if (descriptor.ascending) {
                    textView.append(" ▲");
                } else {
                    textView.append(" ▼");
                }
            }

            textView.setTypeface(null, Typeface.BOLD);
        }
    }

    private static void bindTask(PropertyModel model, View view, PropertyKey key) {
        if (key == TaskManagerProperties.IS_SELECTED) {
            view.setSelected(model.get(TaskManagerProperties.IS_SELECTED));
            return;
        } else if (key == TaskManagerProperties.TASK_ICON) {
            Bitmap bitmap = model.get(TaskManagerProperties.TASK_ICON);
            TextView textView =
                    view.findViewById(getTaskItemViewId(TaskManagerProperties.TASK_NAME));
            if (bitmap != null) {
                int size = view.getResources().getDimensionPixelSize(R.dimen.default_favicon_size);
                Bitmap scaledBitmap = Bitmap.createScaledBitmap(bitmap, size, size, true);
                textView.setCompoundDrawablesRelativeWithIntrinsicBounds(
                        new BitmapDrawable(view.getResources(), scaledBitmap), null, null, null);
                textView.setCompoundDrawablePadding(20);
            } else {
                textView.setCompoundDrawablesRelativeWithIntrinsicBounds(null, null, null, null);
                textView.setCompoundDrawablePadding(0);
            }
            return;
        }

        if (!List.of(TaskManagerProperties.ALL_COLUMN_KEYS).contains(key)) {
            return;
        }

        TextView textView = view.findViewById(getTaskItemViewId(key));

        if (key == TaskManagerProperties.TASK_NAME) {
            textView.setText(model.get(TaskManagerProperties.TASK_NAME));
        } else if (key == TaskManagerProperties.MEMORY_FOOTPRINT) {
            textView.setText(
                    PropertyStringifier.getMemoryUsageText(
                            view.getContext(), model.get(TaskManagerProperties.MEMORY_FOOTPRINT)));
        } else if (key == TaskManagerProperties.CPU) {
            textView.setText(
                    PropertyStringifier.getCpuUsageText(
                            view.getContext(), model.get(TaskManagerProperties.CPU)));
        } else if (key == TaskManagerProperties.NETWORK_USAGE) {
            textView.setText(
                    PropertyStringifier.getNetworkUsageText(
                            view.getContext(), model.get(TaskManagerProperties.NETWORK_USAGE)));
        } else if (key == TaskManagerProperties.PROCESS_ID) {
            textView.setText(String.valueOf(model.get(TaskManagerProperties.PROCESS_ID)));
        } else if (key == TaskManagerProperties.GPU_MEMORY) {
            textView.setText(
                    PropertyStringifier.getMemoryUsageText(
                            view.getContext(), model.get(TaskManagerProperties.GPU_MEMORY)));
        } else {
            throw new IllegalArgumentException("column key " + key + " not supported");
        }
    }

    private static void bindHeaderModelAndTaskView(
            PropertyModel model, View view, PropertyKey key) {
        if (key != TaskManagerProperties.COLUMNS) return;

        Set<PropertyKey> selectedKeys = Set.of(model.get(TaskManagerProperties.COLUMNS));
        for (PropertyKey columnKey : TaskManagerProperties.ALL_COLUMN_KEYS) {
            View textView = view.findViewById(getTaskItemViewId(columnKey));
            if (selectedKeys.contains(columnKey)) {
                textView.setVisibility(View.VISIBLE);
            } else {
                textView.setVisibility(View.GONE);
            }
        }
    }

    /** Converts the given header column key to the resource id of the text for the column. */
    static @StringRes int getColumnTextResourceId(PropertyKey columnKey) {
        if (columnKey == TaskManagerProperties.TASK_NAME) {
            return R.string.task_manager_task_column;
        } else if (columnKey == TaskManagerProperties.MEMORY_FOOTPRINT) {
            return R.string.task_manager_mem_footprint_column;
        } else if (columnKey == TaskManagerProperties.CPU) {
            return R.string.task_manager_cpu_column;
        } else if (columnKey == TaskManagerProperties.NETWORK_USAGE) {
            return R.string.task_manager_net_column;
        } else if (columnKey == TaskManagerProperties.PROCESS_ID) {
            return R.string.task_manager_process_id_column;
        } else if (columnKey == TaskManagerProperties.GPU_MEMORY) {
            return R.string.task_manager_video_memory_column;
        } else {
            throw new IllegalArgumentException("column key " + columnKey + " not supported");
        }
    }

    /** Converts the given resource id of the text for the column to the column property key. */
    private static @Nullable PropertyKey getColumnKeyForResourceId(@StringRes int resId) {
        for (PropertyKey key : TaskManagerProperties.ALL_COLUMN_KEYS) {
            if (getColumnTextResourceId(key) == resId) {
                return key;
            }
        }
        return null;
    }

    /**
     * Converts the given header column key to the resource id of the corresponding view in the task
     * item view.
     */
    static @IdRes int getTaskItemViewId(PropertyKey columnKey) {
        if (columnKey == TaskManagerProperties.TASK_NAME) {
            return R.id.task_name;
        } else if (columnKey == TaskManagerProperties.MEMORY_FOOTPRINT) {
            return R.id.memory_footprint;
        } else if (columnKey == TaskManagerProperties.CPU) {
            return R.id.cpu;
        } else if (columnKey == TaskManagerProperties.NETWORK_USAGE) {
            return R.id.network_usage;
        } else if (columnKey == TaskManagerProperties.PROCESS_ID) {
            return R.id.process_id;
        } else if (columnKey == TaskManagerProperties.GPU_MEMORY) {
            return R.id.gpu_memory_id;
        } else {
            throw new IllegalArgumentException("column key " + columnKey + " not supported");
        }
    }

    /** Converts the given category to the resource id of the corresponding ChipView. */
    static @IdRes int getCategoryChipId(@Category int category) {
        switch (category) {
            case Category.TABS_AND_EXTENSIONS:
                return R.id.category_chip_tabs;
            case Category.BROWSER:
                return R.id.category_chip_browser;
            case Category.ALL_TASKS:
                return R.id.category_chip_all;
            default:
                throw new IllegalArgumentException("category " + category + " not supported");
        }
    }

    private void initToolbar(View taskManagerView, PropertyModel headerModel) {
        ViewStub toolbarStub = taskManagerView.findViewById(R.id.task_manager_toolbar_stub);
        if (toolbarStub == null) return;
        View toolbarView = toolbarStub.inflate();
        initCategoryChips(toolbarView, headerModel);
        initSearchBox(toolbarView);
    }

    private void initCategoryChips(View toolbar, PropertyModel headerModel) {
        View chipsContainer = toolbar.findViewById(R.id.category_chips_container);
        if (chipsContainer == null) return;

        for (@Category int category : CATEGORIES) {
            ChipView chip = toolbar.findViewById(getCategoryChipId(category));
            if (chip != null) {
                // Update the chip data
                chip.setOnClickListener(v -> mMediator.setSelectedCategory(category));
            }
        }

        // Update the chip view
        mModelChangeProcessors.add(
                PropertyModelChangeProcessor.create(
                        headerModel,
                        chipsContainer,
                        (model, view, key) -> {
                            if (key == TaskManagerProperties.SELECTED_CATEGORY) {
                                updateCategoryChipsView(view, model);
                            }
                        }));
    }

    private static void updateCategoryChipsView(View container, PropertyModel model) {
        @Category int selected = model.get(TaskManagerProperties.SELECTED_CATEGORY);
        for (@Category int category : CATEGORIES) {
            ChipView chip = container.findViewById(getCategoryChipId(category));
            if (chip != null) {
                chip.setSelected(selected == category);
            }
        }
    }

    private void initSearchBox(View toolbar) {
        EditText searchInput = toolbar.findViewById(R.id.search_input);
        View clearSearchButton = toolbar.findViewById(R.id.clear_search_button);
        if (searchInput == null || clearSearchButton == null) return;

        searchInput.addTextChangedListener(
                new TextWatcher() {
                    @Override
                    public void beforeTextChanged(
                            CharSequence s, int start, int count, int after) {}

                    @Override
                    public void onTextChanged(CharSequence s, int start, int before, int count) {
                        boolean isEmpty = TextUtils.isEmpty(s);
                        clearSearchButton.setVisibility(isEmpty ? View.GONE : View.VISIBLE);
                        mMediator.setSearchQuery(s != null ? s.toString() : "");
                    }

                    @Override
                    public void afterTextChanged(Editable s) {}
                });

        searchInput.setOnEditorActionListener(
                (v, actionId, event) -> {
                    if (actionId == EditorInfo.IME_ACTION_SEARCH) {
                        KeyboardVisibilityDelegate.getInstance().hideKeyboard(v);
                        return true;
                    }
                    return false;
                });

        clearSearchButton.setOnClickListener(
                v -> {
                    searchInput.setText("");
                });
    }
}
