// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks;

import android.content.Context;
import android.graphics.drawable.Drawable;
import android.util.AttributeSet;
import android.widget.ImageView;
import android.widget.LinearLayout;
import android.widget.TextView;

import org.chromium.chrome.tab_ui.R;

/** View of the tab on the single tab tab switcher. */
class SingleTabView extends LinearLayout {
    private final Context mContext;
    private ImageView mFavicon;
    private TextView mTitle;

    /** Default constructor needed to inflate via XML. */
    public SingleTabView(Context context, AttributeSet attrs) {
        super(context, attrs);
        mContext = context;
    }

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();

        mFavicon = findViewById(R.id.tab_favicon_view);
        mTitle = findViewById(R.id.tab_title_view);
    }

    /**
     * Set the favicon.
     * @param favicon The given favicon {@link Drawable}.
     */
    public void setFavicon(Drawable favicon) {
        mFavicon.setImageDrawable(favicon);
    }

    /**
     * Set the title.
     * @param title The given title.
     */
    public void setTitle(String title) {
        mTitle.setText(title);
    }
}
