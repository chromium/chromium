// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp.search;

import android.content.res.ColorStateList;
import android.os.Build;
import android.text.TextWatcher;
import android.view.View;
import android.view.ViewGroup;
import android.view.ViewGroup.MarginLayoutParams;

import androidx.core.widget.ImageViewCompat;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.R;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

/** Responsible for building and setting properties on the search box on new tab page. */
@NullMarked
class SearchBoxViewBinder
        implements PropertyModelChangeProcessor.ViewBinder<
                PropertyModel, SearchBoxContainerView, PropertyKey> {
    @Override
    public final void bind(
            PropertyModel model, SearchBoxContainerView view, PropertyKey propertyKey) {
        if (SearchBoxProperties.ALPHA == propertyKey) {
            view.setAlpha(model.get(SearchBoxProperties.ALPHA));
        } else if (SearchBoxProperties.APPLY_WHITE_BACKGROUND == propertyKey) {
            view.applyWhiteBackground(model.get(SearchBoxProperties.APPLY_WHITE_BACKGROUND));
        } else if (SearchBoxProperties.DSE_ICON_DRAWABLE == propertyKey) {
            view.setDseIconDrawable(model.get(SearchBoxProperties.DSE_ICON_DRAWABLE));
        } else if (SearchBoxProperties.ENABLE_SEARCH_BOX_EDIT_TEXT == propertyKey) {
            view.mHintTextView.setEnabled(
                    model.get(SearchBoxProperties.ENABLE_SEARCH_BOX_EDIT_TEXT));
        } else if (SearchBoxProperties.LENS_CLICK_CALLBACK == propertyKey) {
            view.mLensButton.setOnClickListener(model.get(SearchBoxProperties.LENS_CLICK_CALLBACK));
        } else if (SearchBoxProperties.LENS_VISIBILITY == propertyKey) {
            view.mLensButton.setVisibility(
                    model.get(SearchBoxProperties.LENS_VISIBILITY) ? View.VISIBLE : View.GONE);
        } else if (SearchBoxProperties.PLUS_BUTTON_CLICK_CALLBACK == propertyKey) {
            view.setPlusButtonClickListener(
                    model.get(SearchBoxProperties.PLUS_BUTTON_CLICK_CALLBACK));
        } else if (SearchBoxProperties.PLUS_BUTTON_VISIBILITY == propertyKey) {
            boolean visible = model.get(SearchBoxProperties.PLUS_BUTTON_VISIBILITY);
            view.mPlusButton.setVisibility(visible ? View.VISIBLE : View.GONE);
            view.mDseIconView.setVisibility(visible ? View.GONE : View.VISIBLE);
        } else if (SearchBoxProperties.SEARCH_BOX_CLICK_CALLBACK == propertyKey) {
            var searchBoxClickListener = model.get(SearchBoxProperties.SEARCH_BOX_CLICK_CALLBACK);
            view.mHintTextView.setOnClickListener(searchBoxClickListener);
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.UPSIDE_DOWN_CAKE) {
                if (searchBoxClickListener != null) {
                    view.mHintTextView.setHandwritingDelegatorCallback(
                            () ->
                                    model.get(SearchBoxProperties.SEARCH_BOX_CLICK_CALLBACK)
                                            .onClick(view.mHintTextView));
                } else {
                    view.mHintTextView.setHandwritingDelegatorCallback(null);
                }
            }
        } else if (SearchBoxProperties.SEARCH_BOX_DRAG_CALLBACK == propertyKey) {
            view.mHintTextView.setOnDragListener(
                    model.get(SearchBoxProperties.SEARCH_BOX_DRAG_CALLBACK));
        } else if (SearchBoxProperties.SEARCH_BOX_END_PADDING == propertyKey) {
            view.setPadding(
                    view.getPaddingLeft(),
                    view.getPaddingTop(),
                    model.get(SearchBoxProperties.SEARCH_BOX_END_PADDING),
                    view.getPaddingBottom());
        } else if (SearchBoxProperties.SEARCH_BOX_HEIGHT == propertyKey) {
            ViewGroup.LayoutParams lp = view.getLayoutParams();
            lp.height = model.get(SearchBoxProperties.SEARCH_BOX_HEIGHT);
            view.setLayoutParams(lp);
        } else if (SearchBoxProperties.SEARCH_BOX_HINT_TEXT == propertyKey) {
            view.mHintTextView.setHint(model.get(SearchBoxProperties.SEARCH_BOX_HINT_TEXT));
        } else if (SearchBoxProperties.SEARCH_BOX_TEXT_STYLE_RES_ID == propertyKey) {
            view.mHintTextView.setTextAppearance(
                    model.get(SearchBoxProperties.SEARCH_BOX_TEXT_STYLE_RES_ID));
        } else if (SearchBoxProperties.SEARCH_BOX_TEXT_WATCHER == propertyKey) {
            // Sets the search box text watcher. Previously added text watcher will be removed.
            TextWatcher oldWatcher =
                    (TextWatcher) view.mHintTextView.getTag(R.id.ntp_search_box_text_watcher_tag);
            if (oldWatcher != null) {
                view.mHintTextView.removeTextChangedListener(oldWatcher);
            }
            TextWatcher newWatcher = model.get(SearchBoxProperties.SEARCH_BOX_TEXT_WATCHER);
            if (newWatcher != null) {
                view.mHintTextView.addTextChangedListener(newWatcher);
            }
            view.mHintTextView.setTag(R.id.ntp_search_box_text_watcher_tag, newWatcher);
        } else if (SearchBoxProperties.SEARCH_BOX_TOP_MARGIN == propertyKey) {
            MarginLayoutParams marginLayoutParams = (MarginLayoutParams) view.getLayoutParams();
            marginLayoutParams.topMargin = model.get(SearchBoxProperties.SEARCH_BOX_TOP_MARGIN);
        } else if (SearchBoxProperties.SEARCH_HINT_VISIBILITY == propertyKey) {
            boolean isHintVisible = model.get(SearchBoxProperties.SEARCH_HINT_VISIBILITY);
            view.mHintTextView.setHint(
                    isHintVisible
                            ? view.getContext().getString(R.string.omnibox_empty_hint)
                            : null);
        } else if (SearchBoxProperties.SEARCH_TEXT == propertyKey) {
            view.mHintTextView.setText(model.get(SearchBoxProperties.SEARCH_TEXT));
        } else if (SearchBoxProperties.VOICE_SEARCH_CLICK_CALLBACK == propertyKey) {
            view.mVoiceSearchButton.setOnClickListener(
                    model.get(SearchBoxProperties.VOICE_SEARCH_CLICK_CALLBACK));
        } else if (SearchBoxProperties.VOICE_SEARCH_COLOR_STATE_LIST == propertyKey) {
            ColorStateList tint = model.get(SearchBoxProperties.VOICE_SEARCH_COLOR_STATE_LIST);
            ImageViewCompat.setImageTintList(view.mVoiceSearchButton, tint);
            ImageViewCompat.setImageTintList(view.mLensButton, tint);
            ImageViewCompat.setImageTintList(view.mPlusButton, tint);
        } else if (SearchBoxProperties.VOICE_SEARCH_VISIBILITY == propertyKey) {
            view.mVoiceSearchButton.setVisibility(
                    model.get(SearchBoxProperties.VOICE_SEARCH_VISIBILITY)
                            ? View.VISIBLE
                            : View.GONE);
        } else {
            assert false : "Unhandled property detected in SearchBoxViewBinder!";
        }
    }
}
