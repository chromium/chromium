// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download.dialogs;

import android.content.Context;
import android.util.AttributeSet;
import android.widget.CheckBox;
import android.widget.ScrollView;
import android.widget.TextView;

import org.chromium.chrome.browser.download.R;

/** Dialog that is displayed to ask user where they want to open a download. */
public class OpenDownloadCustomView extends ScrollView {
    private TextView mTitle;
    private TextView mSubtitleView;
    private CheckBox mAutoOpenCheckbox;

    public OpenDownloadCustomView(Context context, AttributeSet attrs) {
        super(context, attrs);
    }

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();

        mTitle = findViewById(R.id.title);
        mSubtitleView = findViewById(R.id.subtitle);
        mAutoOpenCheckbox = findViewById(R.id.auto_open_checkbox);
    }

    void setTitle(CharSequence title) {
        mTitle.setText(title);
    }

    void setSubtitle(CharSequence subtitle) {
        mSubtitleView.setText(subtitle);
    }

    void setAutoOpenCheckbox(boolean checked) {
        mAutoOpenCheckbox.setChecked(checked);
    }

    /**
     * @return Whether the "auto open" checkbox is checked.
     */
    boolean getAutoOpenEnabled() {
        return mAutoOpenCheckbox.isChecked();
    }
}
