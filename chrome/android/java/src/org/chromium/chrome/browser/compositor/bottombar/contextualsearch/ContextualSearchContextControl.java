// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor.bottombar.contextualsearch;

import android.content.Context;
import android.view.View;
import android.view.ViewGroup;
import android.widget.TextView;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeFeatureList;
import org.chromium.chrome.browser.compositor.bottombar.OverlayPanel;
import org.chromium.chrome.browser.compositor.bottombar.OverlayPanelRepaddingTextView;
import org.chromium.ui.resources.dynamics.DynamicResourceLoader;

/**
 * Controls the Search Context View that is used as a dynamic resource.
 */
public class ContextualSearchContextControl extends OverlayPanelRepaddingTextView {
    /**
     * The selected text View.
     */
    private TextView mSelectedText;

    /**
     * The end of the surrounding text View.
     */
    private TextView mEndText;

    /**
     * @param panel             The panel.
     * @param context           The Android Context used to inflate the View.
     * @param container         The container View used to inflate the View.
     * @param resourceLoader    The resource loader that will handle the snapshot capturing.
     */
    public ContextualSearchContextControl(OverlayPanel panel,
                                          Context context,
                                          ViewGroup container,
                                          DynamicResourceLoader resourceLoader) {
        super(panel, R.layout.contextual_search_context_view, R.id.contextual_search_context_view,
                context, container, resourceLoader,
                (ChromeFeatureList.isEnabled(ChromeFeatureList.OVERLAY_NEW_LAYOUT)
                                ? R.dimen.contextual_search_end_padding
                                : R.dimen.contextual_search_padded_button_width),
                R.dimen.contextual_search_padded_button_width);
    }

    /**
     * Sets the details of the search context to display in the control.
     * @param selection The portion of the context that represents the user's selection.
     * @param end The portion of the context after the selection.
     */
    public void setContextDetails(String selection, String end) {
        inflate();

        mSelectedText.setText(sanitizeText(selection));
        mEndText.setText(sanitizeText(end));

        invalidate();
    }

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();

        View view = getView();
        mSelectedText = (TextView) view.findViewById(R.id.selected_text);
        mEndText = (TextView) view.findViewById(R.id.surrounding_text_end);
    }
}
