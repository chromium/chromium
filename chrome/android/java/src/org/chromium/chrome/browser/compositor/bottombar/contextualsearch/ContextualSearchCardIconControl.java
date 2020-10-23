// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor.bottombar.contextualsearch;

import android.content.Context;
import android.graphics.drawable.Drawable;
import android.widget.ImageView;

import androidx.appcompat.content.res.AppCompatResources;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.contextualsearch.ResolvedSearchTerm.CardTag;
import org.chromium.ui.base.LocalizationUtils;
import org.chromium.ui.resources.dynamics.DynamicResourceLoader;
import org.chromium.ui.resources.dynamics.ViewResourceInflater;

/**
 * Manages an icon to display in the {@link ContextualSearchBarControl} in a non-action card
 * returned by the server. A Card is a structured result from the CoCa backend that can
 * be rendered directly in the Bar. Action cards have associated intents, like dialing
 * a phone number. This class handles some special cases for the general
 * {@link ContextualSearchImageControl} that is responsible for any image that is rendered
 * in the Bar.
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
     * whether that was successful. We use the Bar's Context, which normally shows the word tapped
     * and its surrounding text to show the dictionary word with its pronunciation in grey.
     * @param contextControl The {@link ContextualSearchContextControl} that displays a two-part
     *        main bar text, to set to the dictionary-word/pronunciation.
     * @param imageControl The control for the image to show in the bar.  If successful we'll show a
     *        dictionary icon here.
     * @param searchTerm The string that represents the search term to display.
     * @param cardTagEnum Which kind of card is being shown in this update.
     * @return Whether the bar could be updated with the given search term.
     */
    boolean didUpdateControlsForDefinition(ContextualSearchContextControl contextControl,
            ContextualSearchImageControl imageControl, String searchTerm,
            @CardTag int cardTagEnum) {
        assert cardTagEnum == CardTag.CT_DEFINITION
                || cardTagEnum == CardTag.CT_CONTEXTUAL_DEFINITION;
        boolean didUpdate = false;
        // The middle-dot character is returned by the server and marks the beginning of the
        // pronunciation.
        int dotSeparatorLocation = searchTerm.indexOf(DEFINITION_MID_DOT);
        if (dotSeparatorLocation > 0 && dotSeparatorLocation < searchTerm.length()) {
            // Style with the pronunciation in gray in the second half.
            String word = searchTerm.substring(0, dotSeparatorLocation);
            String pronunciation =
                    searchTerm.substring(dotSeparatorLocation + 1, searchTerm.length());
            pronunciation = LocalizationUtils.isLayoutRtl() ? pronunciation + DEFINITION_MID_DOT
                                                            : DEFINITION_MID_DOT + pronunciation;
            contextControl.setContextDetails(word, pronunciation);
            didUpdate = true;
        }
        setVectorDrawableResourceId(R.drawable.ic_book_round);
        imageControl.setCardIconResourceId(getIconResId());
        return didUpdate;
    }

    /**
     * Sets this view to display the icon from the given vector drawable resource.
     * @param resId The resource ID of a drawable.
     */
    private void setVectorDrawableResourceId(int resId) {
        Drawable drawable = AppCompatResources.getDrawable(mContext, resId);
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
