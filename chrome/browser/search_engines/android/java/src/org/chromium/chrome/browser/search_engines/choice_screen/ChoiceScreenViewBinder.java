// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.search_engines.choice_screen;

import android.view.View;
import android.widget.RadioButton;
import android.widget.TextView;

import androidx.annotation.Nullable;

import org.chromium.chrome.browser.search_engines.R;
import org.chromium.ui.modelutil.LayoutViewBuilder;
import org.chromium.ui.modelutil.MVCListAdapter;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.SimpleRecyclerViewAdapter;
import org.chromium.ui.widget.ButtonCompat;

/**
 * Collection of helpers to modify the {@link View} from {@link ChoiceScreenView} according to
 * provided {@link ChoiceScreenProperties}.
 */
class ChoiceScreenViewBinder {
    /**
     * Updates {@code itemView} to reflect the state from {@code model} for a specific
     * {@code propertyKey}.
     */
    static void bindItem(PropertyModel model, View itemView, PropertyKey propertyKey) {
        if (ChoiceScreenProperties.Item.SHORT_NAME == propertyKey) {
            String shortName = model.get(ChoiceScreenProperties.Item.SHORT_NAME);
            assert shortName != null;
            TextView itemLabel = itemView.findViewById(R.id.search_engine_choice_name);
            itemLabel.setText(shortName);
        } else if (ChoiceScreenProperties.Item.ON_CLICKED == propertyKey) {
            Runnable callback = model.get(ChoiceScreenProperties.Item.ON_CLICKED);
            assert callback != null;
            View.OnClickListener listener = view -> callback.run();
            itemView.setOnClickListener(listener);
            // We need to also set it explicitly on the radio button, it absorbs interactions to
            // toggle its checked state otherwise.
            itemView.findViewById(R.id.search_engine_choice_radio).setOnClickListener(listener);
        } else if (ChoiceScreenProperties.Item.IS_SELECTED == propertyKey) {
            boolean isSelected = model.get(ChoiceScreenProperties.Item.IS_SELECTED);
            RadioButton itemRadio = itemView.findViewById(R.id.search_engine_choice_radio);
            itemRadio.setChecked(isSelected);
        } else {
            assert false : "Failed binding unexpected property.";
        }
    }

    /**
     * Updates {@code mainView} to reflect the state from {@code model} for a specific
     * {@code propertyKey}.
     */
    static void bindContentView(
            PropertyModel model, ChoiceScreenView mainView, PropertyKey propertyKey) {
        if (propertyKey == ChoiceScreenProperties.ITEM_MODELS) {
            MVCListAdapter.ModelList modelList = model.get(ChoiceScreenProperties.ITEM_MODELS);
            assert modelList != null;
            SimpleRecyclerViewAdapter adapter = new SimpleRecyclerViewAdapter(modelList);
            adapter.registerType(ChoiceScreenProperties.DEFAULT_TYPE,
                    new LayoutViewBuilder<>(R.layout.search_engine_choice_item),
                    ChoiceScreenViewBinder::bindItem);
            mainView.setItemsAdapter(adapter);
        } else if (ChoiceScreenProperties.ON_PRIMARY_CLICKED == propertyKey) {
            @Nullable
            Runnable callback = model.get(ChoiceScreenProperties.ON_PRIMARY_CLICKED);
            ButtonCompat button = mainView.findViewById(R.id.choice_screen_primary_button);
            button.setOnClickListener(callback != null ? view -> callback.run() : null);
            button.setEnabled(callback != null);
        } else {
            assert false : "Failed binding unexpected property.";
        }
    }
}
