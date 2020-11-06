// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.app.appmenu;

import android.content.Context;
import android.content.res.Resources;
import android.content.res.TypedArray;
import android.view.LayoutInflater;
import android.view.MenuItem;
import android.view.View;
import android.view.ViewGroup;
import android.widget.AdapterView;
import android.widget.AdapterView.OnItemClickListener;
import android.widget.ArrayAdapter;
import android.widget.ListView;

import androidx.annotation.ColorRes;
import androidx.annotation.Nullable;
import androidx.appcompat.content.res.AppCompatResources;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.ui.appmenu.AppMenuClickHandler;
import org.chromium.chrome.browser.ui.appmenu.CustomViewBinder;
import org.chromium.components.browser_ui.widget.highlight.ViewHighlighter;
import org.chromium.components.browser_ui.widget.text.TextViewWithCompoundDrawables;
import org.chromium.ui.modaldialog.DialogDismissalCause;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogProperties;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.ArrayList;
import java.util.List;

/**
 * A custom binder used to bind the add to menu item, also implementation of all dependencies for
 * the add to dialog.
 */
class AddToMenuItemViewBinder extends ArrayAdapter<MenuItem>
        implements CustomViewBinder, OnItemClickListener, ModalDialogProperties.Controller {
    private static final int ADD_TO_ITEM_VIEW_TYPE = 0;
    private final List<MenuItem> mAddToMenuItems;
    private final ModalDialogManager mModalDialogManager;
    private AppMenuClickHandler mAppMenuClickHandler;
    private PropertyModel mPropertyModel;
    private Context mContext;
    private Integer mHighlightedItemId;

    /**
     * @param context The {@link Context} for the dialog.
     * @param modalDialogManager {@link ModalDialogManager} to control the dialog.
     */
    AddToMenuItemViewBinder(Context context, ModalDialogManager modalDialogManager) {
        super(context, R.layout.add_to_menu_dialog_item);
        mAddToMenuItems = new ArrayList<MenuItem>();
        mContext = context;
        mModalDialogManager = modalDialogManager;
    }

    /**
     * Implementation of {@link CustomViewBinder#getViewTypeCount}.
     */
    @Override
    public int getViewTypeCount() {
        return 1;
    }

    /**
     * Implementation of {@link CustomViewBinder#getItemViewType}.
     */
    @Override
    public int getItemViewType(int id) {
        return id == R.id.add_to_menu_id ? ADD_TO_ITEM_VIEW_TYPE : CustomViewBinder.NOT_HANDLED;
    }

    /**
     * Implementation of {@link CustomViewBinder#getView}.
     */
    @Override
    public View getView(MenuItem item, @Nullable View convertView, ViewGroup parent,
            LayoutInflater inflater, AppMenuClickHandler appMenuClickHandler,
            @Nullable Integer highlightedItemId) {
        assert item.getItemId() == R.id.add_to_menu_id;
        mAppMenuClickHandler = appMenuClickHandler;

        AddToMenuItemViewHolder holder;
        if (convertView == null || !(convertView.getTag() instanceof AddToMenuItemViewHolder)) {
            holder = new AddToMenuItemViewHolder();
            convertView = inflater.inflate(R.layout.custom_view_menu_item, parent, false);
            holder.title = convertView.findViewById(R.id.title);
            convertView.setTag(holder);
        } else {
            holder = (AddToMenuItemViewHolder) convertView.getTag();
        }

        holder.title.setCompoundDrawablesRelative(item.getIcon(), null, null, null);
        holder.title.setText(item.getTitle());
        // Setting |holder.title| to non-focusable will allow TalkBack highlighting the whole view
        // of the menu item, not just title text.
        holder.title.setFocusable(false);
        convertView.setOnClickListener(v -> showAddToDialog());

        // The submenu is prepared by AppMenuPropertiesDelegateImpl.
        // TODO(1136985): Move the logic for "Add to" option out of AppMenuPropertiesDelegateImpl
        // once the experiment is done.
        assert item.hasSubMenu();
        mHighlightedItemId = null;
        for (int i = 0; i < item.getSubMenu().size(); ++i) {
            if (item.getSubMenu().getItem(i).isVisible()) {
                mAddToMenuItems.add(item.getSubMenu().getItem(i));
                if (highlightedItemId != null
                        && item.getSubMenu().getItem(i).getItemId() == highlightedItemId) {
                    mHighlightedItemId = highlightedItemId;
                    ViewHighlighter.turnOnRectangularHighlight(convertView);
                }
            }
        }

        if (mHighlightedItemId == null) {
            ViewHighlighter.turnOffHighlight(convertView);
        }

        return convertView;
    }

    /**
     * Implementation of {@link CustomViewBinder#supportsEnterAnimation}.
     */
    @Override
    public boolean supportsEnterAnimation(int id) {
        return true;
    }

    /**
     * Implementation of {@link CustomViewBinder#getPixelHeight}.
     */
    @Override
    public int getPixelHeight(Context context) {
        TypedArray a = context.obtainStyledAttributes(
                new int[] {android.R.attr.listPreferredItemHeightSmall});
        return a.getDimensionPixelSize(0, 0);
    }

    /**
     * Implementation of {@link ArrayAdapter#getView}.
     */
    @Override
    public View getView(int position, View convertView, ViewGroup parent) {
        if (convertView == null) {
            LayoutInflater inflater = LayoutInflater.from(mContext);
            convertView = inflater.inflate(R.layout.add_to_menu_dialog_item, parent, false);
        }
        TextViewWithCompoundDrawables option = (TextViewWithCompoundDrawables) convertView;

        MenuItem item = mAddToMenuItems.get(position);
        option.setCompoundDrawablesRelative(item.getIcon(), null, null, null);
        option.setText(item.getTitle());
        option.setEnabled(item.isEnabled());
        @ColorRes
        int theme = item.isChecked() ? R.color.blue_mode_tint
                                     : R.color.default_icon_color_secondary_tint_list;
        option.setDrawableTintColor(
                AppCompatResources.getColorStateList(convertView.getContext(), theme));

        if (mHighlightedItemId != null && item.getItemId() == mHighlightedItemId) {
            ViewHighlighter.turnOnRectangularHighlight(convertView);
        } else {
            ViewHighlighter.turnOffHighlight(convertView);
        }

        return convertView;
    }

    /**
     * Implementation of {@link ArrayAdapter#getCount}.
     */
    @Override
    public int getCount() {
        return mAddToMenuItems.size();
    }

    /**
     * Implementation of {@link AdapterView.OnItemClickListener#onItemClick}.
     */
    @Override
    public void onItemClick(AdapterView<?> parent, View view, int position, long id) {
        if (!view.isEnabled()) {
            return;
        }
        mAppMenuClickHandler.onItemClick(mAddToMenuItems.get(position));
        mModalDialogManager.dismissDialog(mPropertyModel, DialogDismissalCause.ACTION_ON_CONTENT);
    }

    /**
     * Implementation of {@link ModalDialogProperties.Controller#onClick}.
     */
    @Override
    public void onClick(PropertyModel model, int buttonType) {
        switch (buttonType) {
            case ModalDialogProperties.ButtonType.POSITIVE:
                mModalDialogManager.dismissDialog(
                        model, DialogDismissalCause.POSITIVE_BUTTON_CLICKED);
                break;
            default:
        }
    }

    /**
     * Implementation of {@link ModalDialogProperties.Controller#onDismiss}.
     */
    @Override
    public void onDismiss(PropertyModel model, int dismissalCause) {
        mPropertyModel = null;
    }

    private PropertyModel getModalDialogModel() {
        Resources resources = mContext.getResources();
        return new PropertyModel.Builder(ModalDialogProperties.ALL_KEYS)
                .with(ModalDialogProperties.CONTROLLER, this)
                .with(ModalDialogProperties.POSITIVE_BUTTON_TEXT, resources, R.string.close)
                .with(ModalDialogProperties.CUSTOM_VIEW, createAddToMenu())
                .with(ModalDialogProperties.CANCEL_ON_TOUCH_OUTSIDE, true)
                .build();
    }

    private void showAddToDialog() {
        mPropertyModel = getModalDialogModel();
        mModalDialogManager.showDialog(mPropertyModel, ModalDialogManager.ModalDialogType.APP);
    }

    private View createAddToMenu() {
        LayoutInflater inflater = LayoutInflater.from(mContext);
        View addToMenu = inflater.inflate(R.layout.add_to_menu_dialog, null);
        ListView list = addToMenu.findViewById(R.id.list);
        list.setAdapter(this);
        list.setOnItemClickListener(this);
        return addToMenu;
    }

    private static class AddToMenuItemViewHolder { public TextViewWithCompoundDrawables title; }
}