// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp.search;

import android.os.Build;
import android.view.View;
import android.view.ViewGroup;
import android.view.ViewGroup.MarginLayoutParams;
import android.widget.ImageView;
import android.widget.TextView;

import androidx.core.widget.ImageViewCompat;

import com.airbnb.lottie.LottieAnimationView;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.R;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

/** Responsible for building and setting properties on the search box on new tab page. */
@NullMarked
class SearchBoxViewBinder
        implements PropertyModelChangeProcessor.ViewBinder<PropertyModel, View, PropertyKey> {
    @Override
    public final void bind(PropertyModel model, View view, PropertyKey propertyKey) {
        ImageView voiceSearchButton = view.findViewById(R.id.voice_search_button);
        ImageView lensButton = view.findViewById(R.id.lens_camera_button);
        LottieAnimationView composeplateButton = view.findViewById(R.id.composeplate_button);
        View searchBoxlayout = view;
        View searchBoxContainer = searchBoxlayout.findViewById(R.id.search_box_container);
        final TextView searchBoxTextView = searchBoxlayout.findViewById(R.id.search_box_text);

        if (SearchBoxProperties.VISIBILITY == propertyKey) {
            searchBoxlayout.setVisibility(
                    model.get(SearchBoxProperties.VISIBILITY) ? View.VISIBLE : View.GONE);
        } else if (SearchBoxProperties.ALPHA == propertyKey) {
            searchBoxlayout.setAlpha(model.get(SearchBoxProperties.ALPHA));
        } else if (SearchBoxProperties.VOICE_SEARCH_COLOR_STATE_LIST == propertyKey) {
            ImageViewCompat.setImageTintList(
                    voiceSearchButton,
                    model.get(SearchBoxProperties.VOICE_SEARCH_COLOR_STATE_LIST));
            ImageViewCompat.setImageTintList(
                    lensButton, model.get(SearchBoxProperties.VOICE_SEARCH_COLOR_STATE_LIST));
        } else if (SearchBoxProperties.VOICE_SEARCH_DRAWABLE == propertyKey) {
            voiceSearchButton.setImageDrawable(
                    model.get(SearchBoxProperties.VOICE_SEARCH_DRAWABLE));
        } else if (SearchBoxProperties.VOICE_SEARCH_VISIBILITY == propertyKey) {
            voiceSearchButton.setVisibility(
                    model.get(SearchBoxProperties.VOICE_SEARCH_VISIBILITY)
                            ? View.VISIBLE
                            : View.GONE);
        } else if (SearchBoxProperties.COMPOSEPLATE_BUTTON_VISIBILITY == propertyKey) {
            ((SearchBoxContainerView) view)
                    .setComposeplateButtonVisibility(
                            model.get(SearchBoxProperties.COMPOSEPLATE_BUTTON_VISIBILITY));
        } else if (SearchBoxProperties.LENS_VISIBILITY == propertyKey) {
            lensButton.setVisibility(
                    model.get(SearchBoxProperties.LENS_VISIBILITY) ? View.VISIBLE : View.GONE);
        } else if (SearchBoxProperties.LENS_CLICK_CALLBACK == propertyKey) {
            lensButton.setOnClickListener(model.get(SearchBoxProperties.LENS_CLICK_CALLBACK));
        } else if (SearchBoxProperties.SEARCH_BOX_CLICK_CALLBACK == propertyKey) {
            var searchBoxClickListener = model.get(SearchBoxProperties.SEARCH_BOX_CLICK_CALLBACK);
            searchBoxTextView.setOnClickListener(searchBoxClickListener);
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.UPSIDE_DOWN_CAKE) {
                if (searchBoxClickListener != null) {
                    searchBoxTextView.setHandwritingDelegatorCallback(
                            () ->
                                    model.get(SearchBoxProperties.SEARCH_BOX_CLICK_CALLBACK)
                                            .onClick(searchBoxTextView));
                } else {
                    searchBoxTextView.setHandwritingDelegatorCallback(null);
                }
            }
        } else if (SearchBoxProperties.SEARCH_BOX_DRAG_CALLBACK == propertyKey) {
            searchBoxTextView.setOnDragListener(
                    model.get(SearchBoxProperties.SEARCH_BOX_DRAG_CALLBACK));
        } else if (SearchBoxProperties.SEARCH_BOX_TEXT_WATCHER == propertyKey) {
            searchBoxTextView.addTextChangedListener(
                    model.get(SearchBoxProperties.SEARCH_BOX_TEXT_WATCHER));
        } else if (SearchBoxProperties.SEARCH_TEXT == propertyKey) {
            searchBoxTextView.setText(model.get(SearchBoxProperties.SEARCH_TEXT));
        } else if (SearchBoxProperties.SEARCH_HINT_VISIBILITY == propertyKey) {
            boolean isHintVisible = model.get(SearchBoxProperties.SEARCH_HINT_VISIBILITY);
            searchBoxTextView.setHint(
                    isHintVisible
                            ? view.getContext().getString(R.string.omnibox_empty_hint)
                            : null);
        } else if (SearchBoxProperties.VOICE_SEARCH_CLICK_CALLBACK == propertyKey) {
            voiceSearchButton.setOnClickListener(
                    model.get(SearchBoxProperties.VOICE_SEARCH_CLICK_CALLBACK));
        } else if (SearchBoxProperties.COMPOSEPLATE_BUTTON_CLICK_CALLBACK == propertyKey) {
            composeplateButton.setOnClickListener(
                    model.get(SearchBoxProperties.COMPOSEPLATE_BUTTON_CLICK_CALLBACK));
        } else if (SearchBoxProperties.SEARCH_BOX_HEIGHT == propertyKey) {
            ViewGroup.LayoutParams lp = searchBoxlayout.getLayoutParams();
            lp.height = model.get(SearchBoxProperties.SEARCH_BOX_HEIGHT);
            searchBoxlayout.setLayoutParams(lp);
        } else if (SearchBoxProperties.SEARCH_BOX_TOP_MARGIN == propertyKey) {
            MarginLayoutParams marginLayoutParams =
                    (MarginLayoutParams) searchBoxlayout.getLayoutParams();
            marginLayoutParams.topMargin = model.get(SearchBoxProperties.SEARCH_BOX_TOP_MARGIN);
        } else if (SearchBoxProperties.SEARCH_BOX_END_PADDING == propertyKey) {
            searchBoxContainer.setPadding(
                    searchBoxContainer.getPaddingLeft(),
                    searchBoxContainer.getPaddingTop(),
                    model.get(SearchBoxProperties.SEARCH_BOX_END_PADDING),
                    searchBoxContainer.getPaddingBottom());
        } else if (SearchBoxProperties.SEARCH_BOX_START_PADDING == propertyKey) {
            searchBoxContainer.setPadding(
                    model.get(SearchBoxProperties.SEARCH_BOX_START_PADDING),
                    searchBoxContainer.getPaddingTop(),
                    searchBoxContainer.getPaddingEnd(),
                    searchBoxContainer.getPaddingBottom());
        } else if (SearchBoxProperties.SEARCH_BOX_TEXT_STYLE_RES_ID == propertyKey) {
            searchBoxTextView.setTextAppearance(
                    model.get(SearchBoxProperties.SEARCH_BOX_TEXT_STYLE_RES_ID));
        } else if (SearchBoxProperties.ENABLE_SEARCH_BOX_EDIT_TEXT == propertyKey) {
            searchBoxTextView.setEnabled(
                    model.get(SearchBoxProperties.ENABLE_SEARCH_BOX_EDIT_TEXT));
        } else if (SearchBoxProperties.SEARCH_BOX_HINT_TEXT == propertyKey) {
            searchBoxTextView.setHint(model.get(SearchBoxProperties.SEARCH_BOX_HINT_TEXT));
        } else if (SearchBoxProperties.APPLY_WHITE_BACKGROUND_WITH_SHADOW == propertyKey) {
            ((SearchBoxContainerView) searchBoxlayout)
                    .applyWhiteBackgroundWithShadow(
                            model.get(SearchBoxProperties.APPLY_WHITE_BACKGROUND_WITH_SHADOW));
        } else if (SearchBoxProperties.COMPOSEPLATE_BUTTON_ICON_RAW_RES_ID == propertyKey) {
            composeplateButton.setAnimation(
                    model.get(SearchBoxProperties.COMPOSEPLATE_BUTTON_ICON_RAW_RES_ID));
        } else {
            assert false : "Unhandled property detected in SearchBoxViewBinder!";
        }
    }
}
