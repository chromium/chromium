// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks.shopping;

import android.content.Context;
import android.util.AttributeSet;
import android.view.View;
import android.view.ViewGroup;
import android.widget.FrameLayout;

import org.chromium.base.Callback;
import org.chromium.chrome.R;
import org.chromium.ui.modelutil.MVCListAdapter;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.MVCListAdapter.ViewBuilder;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModel.WritableBooleanPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableObjectPropertyKey;
import org.chromium.ui.modelutil.SimpleRecyclerViewAdapter;
import org.chromium.ui.widget.ChipView;

import androidx.recyclerview.widget.LinearLayoutManager;
import androidx.recyclerview.widget.RecyclerView;

/** The coordinator for a list of chips that can be selected to filter items. */
public class ChipList extends RecyclerView {

    public static class ChipProperties {
        public static final int BASIC_CHIP = 0;

        public static final WritableObjectPropertyKey<String> TITLE =
                new WritableObjectPropertyKey<>();
        public static final WritableBooleanPropertyKey VISIBLE = new WritableBooleanPropertyKey();

        static final WritableBooleanPropertyKey SELECTED = new WritableBooleanPropertyKey();

        static final WritableObjectPropertyKey<Callback<PropertyModel>> CLICK_HANDLER =
                new WritableObjectPropertyKey<>();

        static final PropertyKey[] ALL_KEYS =
                new PropertyKey[]{TITLE, VISIBLE, SELECTED, CLICK_HANDLER};
    }

    private ModelList mChipModels;

    private Callback<String> mSelectionUpdatedCallback;

    public ChipList(Context context) {
        super(context);
    }

    public ChipList(Context context, AttributeSet atts) {
        super(context, atts);
    }

    public void init(int chipStyle, ModelList chipModels,
            Callback<String> selectionUpdatedCallback) {
        mChipModels = chipModels;
        mSelectionUpdatedCallback = selectionUpdatedCallback;

        setLayoutManager(
                new LinearLayoutManager(getContext(), LinearLayoutManager.HORIZONTAL, false));
        SimpleRecyclerViewAdapter adapter = new SimpleRecyclerViewAdapter(mChipModels);
        adapter.registerType(ChipProperties.BASIC_CHIP,
                parent -> {
                    ChipView chip = new ChipView(getContext(), chipStyle);
                    FrameLayout marginContainer = new FrameLayout(getContext());
                    marginContainer.addView(chip);
                    MarginLayoutParams params = new MarginLayoutParams(
                            ViewGroup.LayoutParams.WRAP_CONTENT,
                            ViewGroup.LayoutParams.WRAP_CONTENT);
                    int margin =
                            getResources().getDimensionPixelSize(R.dimen.shopping_chip_side_margin);
                    params.leftMargin = margin;
                    params.rightMargin = margin;
                    marginContainer.setLayoutParams(params);
                    return marginContainer;
                }, ChipList::bindChip);
        setAdapter(adapter);
    }

    public MVCListAdapter.ListItem createChipListItem(String title) {
        PropertyModel model =  new PropertyModel.Builder(ChipProperties.ALL_KEYS)
                .with(ChipProperties.TITLE, title)
                .with(ChipProperties.VISIBLE, true)
                .with(ChipProperties.SELECTED, false)
                .with(ChipProperties.CLICK_HANDLER, (m) -> {
                    boolean previouslySelected = m.get(ChipProperties.SELECTED);
                    for (int i = 0; i < mChipModels.size(); i++) {
                        mChipModels.get(i).model.set(ChipProperties.SELECTED, false);
                    }
                    if (!previouslySelected) {
                        m.set(ChipProperties.SELECTED, !m.get(ChipProperties.SELECTED));
                    }
                    mSelectionUpdatedCallback.onResult(
                            previouslySelected ? null : m.get(ChipProperties.TITLE));
                })
                .build();
        return new MVCListAdapter.ListItem(ChipProperties.BASIC_CHIP, model);
    }

    private static void bindChip(PropertyModel model, ViewGroup group, PropertyKey key) {
        ChipView chip = (ChipView) group.getChildAt(0);
        if (ChipProperties.TITLE == key) {
            chip.getPrimaryTextView().setText(model.get(ChipProperties.TITLE));
        } else if (ChipProperties.VISIBLE == key) {
            chip.setVisibility(model.get(ChipProperties.VISIBLE) ? View.VISIBLE : View.GONE);
        } else if (ChipProperties.SELECTED == key) {
            chip.setSelected(model.get(ChipProperties.SELECTED));
        } else if (ChipProperties.CLICK_HANDLER == key) {
            Callback<PropertyModel> handler = model.get(ChipProperties.CLICK_HANDLER);
            chip.setClickable(handler != null);
            if (handler != null) {
                chip.setOnClickListener(view -> handler.onResult(model));
            } else {
                chip.setOnClickListener(null);
            }
        }
    }

}

