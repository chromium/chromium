// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp.search;

import android.view.View;
import android.widget.ImageView;
import android.widget.TextView;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.lens.LensFeature;
import org.chromium.ui.base.ViewUtils;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;
import org.chromium.ui.widget.ChipView;

/**
 * Responsible for building and setting properties on the search box on new tab page.
 */
class SearchBoxViewBinder
        implements PropertyModelChangeProcessor.ViewBinder<PropertyModel, View, PropertyKey> {
    @Override
    public final void bind(PropertyModel model, View view, PropertyKey propertyKey) {
        ImageView voiceSearchButton =
                view.findViewById(org.chromium.chrome.R.id.voice_search_button);
        ImageView lensButton =
                LensFeature.SEARCH_BOX_START_VARIANT_LENS_CAMERA_ASSISTED_SEARCH.getValue()
                ? view.findViewById(org.chromium.chrome.R.id.lens_camera_button_start)
                : view.findViewById(org.chromium.chrome.R.id.lens_camera_button_end);
        View searchBoxContainer = view;
        final TextView searchBoxTextView = searchBoxContainer.findViewById(R.id.search_box_text);
        final ChipView chipView = searchBoxContainer.findViewById(R.id.query_tiles_chip);

        if (SearchBoxProperties.VISIBILITY == propertyKey) {
            searchBoxContainer.setVisibility(
                    model.get(SearchBoxProperties.VISIBILITY) ? View.VISIBLE : View.GONE);
        } else if (SearchBoxProperties.ALPHA == propertyKey) {
            searchBoxContainer.setAlpha(model.get(SearchBoxProperties.ALPHA));
            // Disable the search box contents if it is the process of being animated away.
            ViewUtils.setEnabledRecursive(
                    searchBoxContainer, searchBoxContainer.getAlpha() == 1.0f);
        } else if (SearchBoxProperties.BACKGROUND == propertyKey) {
            searchBoxContainer.setBackground(model.get(SearchBoxProperties.BACKGROUND));
        } else if (SearchBoxProperties.VOICE_SEARCH_COLOR_STATE_LIST == propertyKey) {
            ApiCompatibilityUtils.setImageTintList(voiceSearchButton,
                    model.get(SearchBoxProperties.VOICE_SEARCH_COLOR_STATE_LIST));
        } else if (SearchBoxProperties.VOICE_SEARCH_DRAWABLE == propertyKey) {
            voiceSearchButton.setImageDrawable(
                    model.get(SearchBoxProperties.VOICE_SEARCH_DRAWABLE));
        } else if (SearchBoxProperties.VOICE_SEARCH_VISIBILITY == propertyKey) {
            voiceSearchButton.setVisibility(model.get(SearchBoxProperties.VOICE_SEARCH_VISIBILITY)
                            ? View.VISIBLE
                            : View.GONE);
        } else if (SearchBoxProperties.LENS_VISIBILITY == propertyKey) {
            lensButton.setVisibility(
                    model.get(SearchBoxProperties.LENS_VISIBILITY) ? View.VISIBLE : View.GONE);
        } else if (SearchBoxProperties.LENS_CLICK_CALLBACK == propertyKey) {
            lensButton.setOnClickListener(model.get(SearchBoxProperties.LENS_CLICK_CALLBACK));
        } else if (SearchBoxProperties.SEARCH_BOX_CLICK_CALLBACK == propertyKey) {
            searchBoxTextView.setOnClickListener(
                    model.get(SearchBoxProperties.SEARCH_BOX_CLICK_CALLBACK));
        } else if (SearchBoxProperties.SEARCH_BOX_TEXT_WATCHER == propertyKey) {
            searchBoxTextView.addTextChangedListener(
                    model.get(SearchBoxProperties.SEARCH_BOX_TEXT_WATCHER));
        } else if (SearchBoxProperties.SEARCH_TEXT == propertyKey) {
            searchBoxTextView.setText(model.get(SearchBoxProperties.SEARCH_TEXT).first);
        } else if (SearchBoxProperties.SEARCH_HINT_VISIBILITY == propertyKey) {
            boolean isHintVisible = model.get(SearchBoxProperties.SEARCH_HINT_VISIBILITY);
            searchBoxTextView.setHint(isHintVisible
                            ? view.getContext().getString(
                                    org.chromium.chrome.R.string.search_or_type_web_address)
                            : null);
        } else if (SearchBoxProperties.VOICE_SEARCH_CLICK_CALLBACK == propertyKey) {
            voiceSearchButton.setOnClickListener(
                    model.get(SearchBoxProperties.VOICE_SEARCH_CLICK_CALLBACK));
        } else if (SearchBoxProperties.SEARCH_BOX_HINT_COLOR == propertyKey) {
            searchBoxTextView.setHintTextColor(
                    model.get(SearchBoxProperties.SEARCH_BOX_HINT_COLOR));
        } else if (SearchBoxProperties.CHIP_TEXT == propertyKey) {
            chipView.getPrimaryTextView().setText(model.get(SearchBoxProperties.CHIP_TEXT));
        } else if (SearchBoxProperties.CHIP_VISIBILITY == propertyKey) {
            chipView.setVisibility(
                    model.get(SearchBoxProperties.CHIP_VISIBILITY) ? View.VISIBLE : View.GONE);
        } else if (SearchBoxProperties.CHIP_DRAWABLE == propertyKey) {
            chipView.setIcon(model.get(SearchBoxProperties.CHIP_DRAWABLE), true);
        } else if (SearchBoxProperties.CHIP_CLICK_CALLBACK == propertyKey) {
            chipView.setOnClickListener(model.get(SearchBoxProperties.CHIP_CLICK_CALLBACK));
        } else if (SearchBoxProperties.CHIP_CANCEL_CALLBACK == propertyKey) {
            chipView.addRemoveIcon();
            chipView.setRemoveIconClickListener(
                    model.get(SearchBoxProperties.CHIP_CANCEL_CALLBACK));
        } else {
            assert false : "Unhandled property detected in SearchBoxViewBinder!";
        }
    }
}
