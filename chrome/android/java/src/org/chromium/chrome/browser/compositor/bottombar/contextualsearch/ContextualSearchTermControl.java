// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor.bottombar.contextualsearch;

import android.content.Context;
import android.view.View;
import android.view.ViewGroup;
import android.widget.TextView;

import androidx.annotation.Px;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.compositor.bottombar.OverlayPanel;
import org.chromium.chrome.browser.compositor.bottombar.OverlayPanelTextViewInflater;
import org.chromium.ui.resources.dynamics.DynamicResourceLoader;

/**
 * Controls showing the Search Term - what we'll search for - in unstyled text at the top of the
 * Bar.
 * <p>This is similar to the {@link ContextualSearchContextControl} which places two-part styled
 * text in the same location of the Bar. Typically the UX flow starts by showing the selection
 * (which is the Search Term0 using this View and if page context is available to resolve the query
 * then this View is immediately replaced by the Context Control (Context being the selection and
 * page content), until the server returns the Resolved Search Term which is displayed with this
 * control. If there's no access to page context then the selection is the Search Term.
 * <p>This is used as a dynamic resource within the {@link ContextualSearchBarControl}.
 */
public class ContextualSearchTermControl extends OverlayPanelTextViewInflater {
    /** The search term View. */
    private TextView mSearchTerm;

    /**
     * @param panel             The panel.
     * @param context           The Android Context used to inflate the View.
     * @param container         The container View used to inflate the View.
     * @param resourceLoader    The resource loader that will handle the snapshot capturing.
     */
    public ContextualSearchTermControl(
            OverlayPanel panel,
            Context context,
            ViewGroup container,
            DynamicResourceLoader resourceLoader) {
        super(
                panel,
                R.layout.contextual_search_term_view,
                R.id.contextual_search_term_view,
                context,
                container,
                resourceLoader,
                R.dimen.contextual_search_end_padding,
                R.dimen.contextual_search_padded_button_width);
    }

    /**
     * Sets the search term to display in the control.
     * @param searchTerm The string that represents the search term.
     */
    public void setSearchTerm(String searchTerm) {
        inflate();

        mSearchTerm.setText(sanitizeText(searchTerm));

        invalidate();
    }

    /** Returns the search term's TextView height. */
    @Px
    int getTextViewHeight() {
        return mSearchTerm == null ? 0 : mSearchTerm.getHeight();
    }

    // ========================================================================================
    // OverlayPanelInflater overrides
    // ========================================================================================

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();

        View view = getView();
        mSearchTerm = view.findViewById(R.id.contextual_search_term);
    }

    // ========================================================================================
    // OverlayPanelTextViewInflater overrides
    // ========================================================================================

    @Override
    protected TextView getTextView() {
        return mSearchTerm;
    }
}
