// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import android.content.Context;
import android.util.AttributeSet;
import android.widget.ImageButton;
import android.widget.TextView;

import androidx.annotation.Nullable;

import org.chromium.chrome.tab_ui.R;
import org.chromium.components.browser_ui.widget.MaterialCardViewNoShadow;

/**
 * A message card view for the declutter entrypoint that is laid out as a horizontal span, including
 * info text, a settings button and an expand icon.
 */
public class DeclutterMessageCardView extends MaterialCardViewNoShadow {
    private TextView mDescription;
    private ImageButton mSettingsButton;
    private ImageButton mExpandButton;

    public DeclutterMessageCardView(Context context, @Nullable AttributeSet attrs) {
        super(context, attrs);
    }

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();

        mDescription = findViewById(R.id.declutter_info_text);
        mSettingsButton = findViewById(R.id.declutter_settings);
        mExpandButton = findViewById(R.id.declutter_expand_button);
    }

    public void setDescriptionText(String template) {
        mDescription.setText(template);
    }

    public void setSettingsButtonOnClickListener(OnClickListener listener) {
        mSettingsButton.setOnClickListener(listener);
    }

    public void setExpandButtonOnClickListener(OnClickListener listener) {
        mExpandButton.setOnClickListener(listener);
    }
}
