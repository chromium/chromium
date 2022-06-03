// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.prefeditor;

import android.content.Context;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.ImageView;
import android.widget.TextView;

import androidx.appcompat.content.res.AppCompatResources;

import org.chromium.chrome.R;

/**
 * Helper class for creating a view with three labels and an icon.
 *
 * +--------------+------------+
 * | TOP LABEL    |            |
 * | MID LABEL    |       ICON |
 * | BOTTOM LABEL |            |
 * +--------------+------------+
 *
 * Used for showing the uneditable parts of server cards. For example:
 *
 * +--------------+------------+
 * | Visa***1234  |            |
 * | First Last   |       VISA |
 * | Exp: 12/2020 |            |
 * +--------------+------------+
 */
class EditorLabelField {
    private final View mLayout;

    /**
     * Builds a label view.
     *
     * @param context    The application context to use when creating widgets.
     * @param root       The object that provides a set of LayoutParams values for the view.
     * @param fieldModel The data model of the icon list.
     */
    public EditorLabelField(Context context, ViewGroup root, EditorFieldModel fieldModel) {
        assert fieldModel.getInputTypeHint() == EditorFieldModel.INPUT_TYPE_HINT_LABEL;

        mLayout = LayoutInflater.from(context).inflate(
                R.layout.payment_request_editor_label, root, false);

        ((TextView) mLayout.findViewById(R.id.top_label)).setText(fieldModel.getLabel());
        ((TextView) mLayout.findViewById(R.id.mid_label)).setText(fieldModel.getMidLabel());
        ((TextView) mLayout.findViewById(R.id.bottom_label)).setText(fieldModel.getBottomLabel());
        ((ImageView) mLayout.findViewById(R.id.icon))
                .setImageDrawable(AppCompatResources.getDrawable(
                        context, fieldModel.getLabelIconResourceId()));
    }

    /** @return The View containing everything. */
    public View getLayout() {
        return mLayout;
    }
}
