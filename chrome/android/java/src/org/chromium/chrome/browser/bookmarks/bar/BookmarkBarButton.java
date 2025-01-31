// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks.bar;

import android.content.Context;
import android.graphics.drawable.Drawable;
import android.util.AttributeSet;
import android.widget.ImageView;
import android.widget.LinearLayout;
import android.widget.TextView;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import org.chromium.chrome.R;

/**
 * View for a button in the bookmark bar which provides users with bookmark access from top chrome.
 */
class BookmarkBarButton extends LinearLayout {

    private ImageView mIcon;
    private TextView mTitle;

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
     * Sets the icon to render in the bookmark bar button.
     *
     * @param icon the icon to render.
     */
    public void setIcon(@Nullable Drawable icon) {
        mIcon.setImageDrawable(icon);
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
     * @return the title which is rendered in the bookmark bar button.
     */
    @Nullable
    CharSequence getTitleForTesting() {
        return mTitle.getText();
    }
}
