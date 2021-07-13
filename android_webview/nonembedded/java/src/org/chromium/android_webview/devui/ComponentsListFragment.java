// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.devui;

import android.app.Activity;
import android.content.Context;
import android.os.Bundle;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.ArrayAdapter;
import android.widget.ListView;
import android.widget.TextView;

import java.util.ArrayList;
import java.util.Locale;

/**
 * A fragment to show a list of WebView components.
 * <p>This feature is currently under development and therefore not exposed in the UI.
 * <p>It can be launched via the adb shell by sending an intent with fragment-id = 3
 */
public class ComponentsListFragment extends DevUiBaseFragment {
    private Context mContext;

    @Override
    public void onAttach(Context context) {
        super.onAttach(context);
        mContext = context;
    }

    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setHasOptionsMenu(true);
    }

    @Override
    public View onCreateView(
            LayoutInflater inflater, ViewGroup container, Bundle savedInstanceState) {
        return inflater.inflate(R.layout.fragment_components_list, null);
    }

    @Override
    public void onViewCreated(View view, Bundle savedInstanceState) {
        Activity activity = (Activity) mContext;
        activity.setTitle("WebView Components");

        ArrayList<String> componentsInfo = new ArrayList<String>();

        TextView componentsSummaryView = view.findViewById(R.id.components_summary_textview);
        componentsSummaryView.setText(
                String.format(Locale.US, "Components (%d)", componentsInfo.size()));

        ListView componentsListView = view.findViewById(R.id.components_list);

        ArrayAdapter<String> componentsInfoAdapter =
                new ArrayAdapter<>(mContext, R.layout.components_list_item, componentsInfo);

        componentsListView.setAdapter(componentsInfoAdapter);
    }
}