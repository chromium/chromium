// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks.bar;

import android.content.Context;
import android.content.res.ColorStateList;
import android.content.res.Resources;
import android.graphics.drawable.Drawable;
import android.util.AttributeSet;
import android.widget.ImageView;
import android.widget.LinearLayout;
import android.widget.TextView;

import androidx.annotation.ColorRes;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.appcompat.content.res.AppCompatResources;
import androidx.core.widget.ImageViewCompat;

import org.chromium.base.CallbackController;
import org.chromium.base.supplier.LazyOneshotSupplier;
import org.chromium.chrome.R;

/**
 * View for a button in the bookmark bar which provides users with bookmark access from top chrome.
 */
class BookmarkBarButton extends LinearLayout {

    private ImageView mIcon;
    private TextView mTitle;

    private @Nullable CallbackController mIconCallbackController;

    /**
     * Constructor that is called when inflating a bookmark bar button from XML.
     *
     * @param context the context the bookmark bar button is running in.
     * @param attrs the attributes of the XML tag that is inflating the bookmark bar button.
     */
    public BookmarkBarButton(@NonNull Context context, @Nullable AttributeSet attrs) {
        super(context, attrs);
    }

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();
        mIcon = findViewById(R.id.bookmark_bar_button_icon);
        mTitle = findViewById(R.id.bookmark_bar_button_title);
    }

    /**
     * Sets the supplier for the icon to render in the bookmark bar button.
     *
     * @param iconSupplier the supplier for the icon to render.
     */
    public void setIconSupplier(@Nullable LazyOneshotSupplier<Drawable> iconSupplier) {
        if (mIconCallbackController != null) {
            mIconCallbackController.destroy();
            mIconCallbackController = null;
        }

        if (iconSupplier == null) {
            mIcon.setImageDrawable(null);
            return;
        }

        mIconCallbackController = new CallbackController();
        iconSupplier.onAvailable(mIconCallbackController.makeCancelable(mIcon::setImageDrawable));
        mIcon.setImageDrawable(iconSupplier.get());
    }

    /**
     * Sets the tint list of the icon to render in the bookmark bar button.
     *
     * @param id the resource identifier for the tint list.
     */
    public void setIconTintList(@ColorRes int id) {
        final ColorStateList tintList =
                id != Resources.ID_NULL
                        ? AppCompatResources.getColorStateList(getContext(), id)
                        : null;

        if (ImageViewCompat.getImageTintList(mIcon) != tintList) {
            ImageViewCompat.setImageTintList(mIcon, tintList);
        }
    }

    /**
     * Sets the title to render in the bookmark bar button.
     *
     * @param title the title to render.
     */
    public void setTitle(@Nullable String title) {
        mTitle.setText(title);
    }

    /**
     * @return the icon which is rendered in the bookmark bar button.
     */
    @Nullable
    Drawable getIconForTesting() {
        return mIcon.getDrawable();
    }

    /**
     * @return the tint list of the icon which is rendered in the bookmark bar button.
     */
    @Nullable
    ColorStateList getIconTintListForTesting() {
        return ImageViewCompat.getImageTintList(mIcon);
    }

    /**
     * @return the title which is rendered in the bookmark bar button.
     */
    @Nullable
    CharSequence getTitleForTesting() {
        return mTitle.getText();
    }
}
