// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.password_manager;

import android.content.Context;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.BaseAdapter;
import android.widget.TextView;

import org.chromium.chrome.R;

/**
 * The adapter that populates the list popup for password generation with data. There is only
 * one item in the list, the explanation of the password generation feature.
 */
public class PasswordGenerationPopupAdapter extends BaseAdapter {
    private final Context mContext;
    private final String mExplanationText;

    /**
     * UI shows an explanation about storing passwords in Chrome.
     */
    private static final int EXPLANATION = 0;

    /**
     * There is only one type of view: EXPLANATION.
     */
    private static final int VIEW_TYPE_COUNT = 1;

    /**
     * Builds the adapter to display views using data from delegate.
     * @param context Android context.
     * @param explanationText The translated text for the explanation part of the UI.
     * @param anchorWidthInDp The width of the anchor to which the popup is attached. Used to size
     * the explanation view.
     */
    public PasswordGenerationPopupAdapter(
            Context context, String explanationText, float anchorWidthInDp) {
        mContext = context;
        mExplanationText = explanationText;
    }

    @Override
    public View getView(int position, View convertView, ViewGroup parent) {
        return convertView != null ? convertView : createExplanationView();
    }

    private View createExplanationView() {
        View view = LayoutInflater.from(mContext).inflate(
                R.layout.password_generation_popup_explanation, null);
        TextView explanation = view.findViewById(R.id.password_generation_explanation);
        explanation.setText(mExplanationText);
        return view;
    }

    @Override
    public Object getItem(int position) {
        return null;
    }
    @Override
    public long getItemId(int position) {
        return 0;
    }

    @Override
    public int getItemViewType(int position) {
        return EXPLANATION;
    }

    @Override
    public int getViewTypeCount() {
        return VIEW_TYPE_COUNT;
    }

    @Override
    public int getCount() {
        return 1;
    }

    @Override
    public boolean areAllItemsEnabled() {
        return false;
    }

    @Override
    public boolean isEnabled(int position) {
        return false;
    }
}
