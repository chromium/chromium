// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.hub;

import android.content.Context;
import android.graphics.drawable.Drawable;
import android.view.View;
import android.widget.Button;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

/** Util class for applying data to buttons. */
public final class ApplyButtonData {
    private ApplyButtonData() {}

    /**
     * Calls setters on the button with fields from the data. If the data is null, then all the
     * fields are nulled out and the button is hidden.
     *
     * @param buttonData Contains the information to be set.
     * @param button The button that should be updated.
     */
    public static void apply(@Nullable FullButtonData buttonData, @NonNull Button button) {
        if (buttonData == null) {
            button.setVisibility(View.GONE);
            button.setText(null);
            button.setContentDescription(null);
            button.setOnClickListener(null);
            setStartDrawable(button, null);
        } else {
            Context context = button.getContext();
            button.setVisibility(View.VISIBLE);
            button.setText(buttonData.resolveText(context));
            button.setContentDescription(buttonData.resolveContentDescription(context));
            if (buttonData.getOnPressRunnable() != null) {
                button.setOnClickListener((v) -> buttonData.getOnPressRunnable().run());
                button.setEnabled(true);
            } else {
                button.setOnClickListener(null);
                button.setEnabled(false);
            }
            setStartDrawable(button, buttonData.resolveIcon(context));
        }
    }

    private static void setStartDrawable(Button button, Drawable drawable) {
        button.setCompoundDrawablesRelativeWithIntrinsicBounds(drawable, null, null, null);
    }
}
