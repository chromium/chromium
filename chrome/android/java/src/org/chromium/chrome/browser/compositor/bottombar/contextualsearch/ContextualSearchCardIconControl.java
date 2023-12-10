// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor.bottombar.contextualsearch;

import android.content.Context;
import android.graphics.drawable.Drawable;
import android.widget.ImageView;

import androidx.appcompat.content.res.AppCompatResources;

import org.chromium.chrome.R;
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
    private final Context mContext;
    private boolean mHasIcon;

    /**
     * @param context The Android Context used to inflate the View.
     * @param resourceLoader The resource loader that will handle the snapshot capturing.
     */
    public ContextualSearchCardIconControl(Context context, DynamicResourceLoader resourceLoader) {
        super(
                R.layout.contextual_search_card_icon_view,
                R.id.contextual_search_card_icon_view,
                context,
                null,
                resourceLoader);
        mContext = context;
    }

    /** Sets the icon to a vector drawable dictionary definition image. */
    void setVectorDrawableDefinitionIcon() {
        setVectorDrawableResourceId(R.drawable.ic_book_round);
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
