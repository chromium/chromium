// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor.bottombar.contextualsearch;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.content.Context;
import android.graphics.drawable.Drawable;
import android.view.View;
import android.widget.ImageView;

import androidx.appcompat.content.res.AppCompatResources;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.R;
import org.chromium.ui.resources.dynamics.DynamicResourceLoader;
import org.chromium.ui.resources.dynamics.ViewResourceInflater;

/**
 * Manages an icon to display in the {@link ContextualSearchBarControl} in a non-action card
 * returned by the server. A Card is a structured result from the CoCa backend that can be rendered
 * directly in the Bar. Action cards have associated intents, like dialing a phone number. This
 * class handles some special cases for the general {@link ContextualSearchImageControl} that is
 * responsible for any image that is rendered in the Bar.
 */
@NullMarked
public class ContextualSearchCardIconControl extends ViewResourceInflater {
    private final Context mContext;

    /**
     * @param context The Android Context used to inflate the View.
     * @param resourceLoader The resource loader that will handle the snapshot capturing.
     */
    public ContextualSearchCardIconControl(
            Context context, @Nullable DynamicResourceLoader resourceLoader) {
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
            View view = assumeNonNull(getView());
            ((ImageView) view).setImageDrawable(drawable);
            invalidate();
        }
    }

    @Override
    protected boolean shouldAttachView() {
        return false;
    }
}
