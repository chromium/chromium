// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor.bottombar.contextualsearch;

import android.content.Context;
import android.graphics.drawable.Drawable;
import android.widget.ImageView;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.download.home.list.view.UiUtils;
import org.chromium.ui.base.LocalizationUtils;
import org.chromium.ui.resources.dynamics.DynamicResourceLoader;
import org.chromium.ui.resources.dynamics.ViewResourceInflater;

/**
 * Manages an icon to display for a non-action card returned by the server.
 */
public class ContextualSearchCardIconControl extends ViewResourceInflater {
    // A separator that we expect in the title of a dictionary response.
    private static final String DEFINITION_MID_DOT = "\u00b7";

    private final Context mContext;
    private boolean mHasIcon;

    /**
     * @param context The Android Context used to inflate the View.
     * @param resourceLoader The resource loader that will handle the snapshot capturing.
     */
    public ContextualSearchCardIconControl(Context context, DynamicResourceLoader resourceLoader) {
        super(R.layout.contextual_search_card_icon_view, R.id.contextual_search_card_icon_view,
                context, null, resourceLoader);
        mContext = context;
    }

    /**
     * Tries to update the given controls to display a dictionary definition card, and returns
     * whether that was successful. We use the Context, which normally shows the word tapped and its
     * surrounding text to show the dictionary word and its pronunciation.
     * @param contextControl The {@link ContextualSearchContextControl} that displays a two-part
     *        main bar text, to set to the dictionary-word/pronunciation.
     * @param imageControl The control for the image to show in the bar.  If successful we'll show a
     *        dictionary icon here.
     * @param searchTerm The string that represents the search term to display.
     */
    boolean didUpdateControlsForDefinition(ContextualSearchContextControl contextControl,
            ContextualSearchImageControl imageControl, String searchTerm) {
        // This middle-dot character is returned by the server and marks the beginning of the
        // pronunciation.
        int dotSeparatorLocation = searchTerm.indexOf(DEFINITION_MID_DOT);
        if (dotSeparatorLocation <= 0 || dotSeparatorLocation >= searchTerm.length() - 1) {
            return false;
        }

        // Style with the pronunciation in gray in the second half.
        String word = searchTerm.substring(0, dotSeparatorLocation);
        String pronunciation = searchTerm.substring(dotSeparatorLocation + 1, searchTerm.length());
        pronunciation = LocalizationUtils.isLayoutRtl() ? pronunciation + DEFINITION_MID_DOT
                                                        : DEFINITION_MID_DOT + pronunciation;
        contextControl.setContextDetails(word, pronunciation);
        setVectorDrawableResourceId(R.drawable.ic_book_round);
        imageControl.setCardIconResourceId(getIconResId());
        return true;
    }

    /**
     * Sets this view to display the icon from the given vector drawable resource.
     * @param resId The resource ID of a drawable.
     */
    private void setVectorDrawableResourceId(int resId) {
        Drawable drawable = UiUtils.getDrawable(mContext, resId);
        if (drawable != null) {
            inflate();
            ((ImageView) getView()).setImageDrawable(drawable);
            invalidate();
            mHasIcon = true;
        }
    }

    /**
     * @return The resource id for the icon associated with the card, if present.
     */
    private int getIconResId() {
        return mHasIcon ? getViewId() : 0;
    }

    @Override
    protected boolean shouldAttachView() {
        return false;
    }
}
