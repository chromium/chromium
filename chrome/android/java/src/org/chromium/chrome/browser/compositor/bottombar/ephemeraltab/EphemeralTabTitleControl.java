// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor.bottombar.ephemeraltab;

import android.content.Context;
import android.view.ViewGroup;
import android.widget.TextView;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeFeatureList;
import org.chromium.chrome.browser.compositor.bottombar.OverlayPanel;
import org.chromium.chrome.browser.compositor.bottombar.OverlayPanelTextViewInflater;
import org.chromium.ui.resources.dynamics.DynamicResourceLoader;

/**
 * Title control showing URL for ephemeral tab.
 */
public class EphemeralTabTitleControl extends OverlayPanelTextViewInflater {
    private TextView mBarText;

    /**
     * @param panel The panel.
     * @param context The Android Context used to inflate the View.
     * @param container The container View used to inflate the View.
     * @param resourceLoader The resource loader that will handle the snapshot capturing.
     */
    public EphemeralTabTitleControl(OverlayPanel panel, Context context, ViewGroup container,
            DynamicResourceLoader resourceLoader) {
        super(panel, R.layout.ephemeral_tab_text_view, R.id.ephemeral_tab_text_view, context,
                container, resourceLoader,
                (ChromeFeatureList.isEnabled(ChromeFeatureList.OVERLAY_NEW_LAYOUT)
                                ? R.dimen.overlay_panel_end_buttons_width
                                : R.dimen.overlay_panel_padded_button_width),
                (ChromeFeatureList.isEnabled(ChromeFeatureList.OVERLAY_NEW_LAYOUT)
                                ? R.dimen.overlay_panel_end_buttons_width
                                : R.dimen.overlay_panel_padded_button_width));
        invalidate();
    }

    void setBarText(String text) {
        if (mBarText == null) inflate();
        mBarText.setText(text);
        invalidate();
    }

    // OverlayPanelTextViewInflater

    @Override
    protected TextView getTextView() {
        return mBarText;
    }

    // OverlayPanelInflater

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();
        mBarText = (TextView) getView().findViewById(R.id.ephemeral_tab_text);
    }
}
