// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.devui;

import android.annotation.SuppressLint;
import android.app.Activity;
import android.content.Context;
import android.os.Bundle;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.ArrayAdapter;
import android.widget.ListView;
import android.widget.TextView;

import androidx.annotation.MainThread;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;
import androidx.annotation.WorkerThread;

import org.chromium.android_webview.devui.util.ComponentInfo;
import org.chromium.android_webview.devui.util.ComponentsInfoLoader;
import org.chromium.base.task.AsyncTask;

import java.util.ArrayList;
import java.util.Locale;

/**
 * A fragment to show a list of WebView components.
 * <p>This feature is currently under development and therefore not exposed in the UI.
 * <p>It can be launched via the adb shell by sending an intent with fragment-id = 3.
 */
@SuppressLint("SetTextI18n")
public class ComponentsListFragment extends DevUiBaseFragment {
    private Context mContext;
    private ComponentInfoListAdapter mComponentInfoListAdapter;

    private static @Nullable Runnable sComponentInfoLoadedListener;

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

        TextView componentsSummaryView = view.findViewById(R.id.components_summary_textview);
        mComponentInfoListAdapter = new ComponentInfoListAdapter(new ArrayList<ComponentInfo>());
        updateComponentInfoList(componentsSummaryView);
        ListView componentInfoListView = view.findViewById(R.id.components_list);
        componentInfoListView.setAdapter(mComponentInfoListAdapter);
    }

    /**
     * An ArrayAdapter to show a list of {@code ComponentInfo} objects.
     * It uses custom item layout {@code R.layout.components_list_item} which has two {@code
     * TextView}; {@code text1} acts as the component name and {@code text2} as the component
     * version.
     */
    private class ComponentInfoListAdapter extends ArrayAdapter<ComponentInfo> {
        public ComponentInfoListAdapter(ArrayList<ComponentInfo> items) {
            super(mContext, R.layout.components_list_item, items);
        }

        @Override
        public View getView(int position, View view, ViewGroup parent) {
            // If the the old view is already created then reuse it, else create a new one by layout
            // inflation.
            if (view == null) {
                view = getLayoutInflater().inflate(R.layout.components_list_item, null, true);
            }

            ComponentInfo item = getItem(position);
            TextView name = view.findViewById(android.R.id.text1);
            TextView version = view.findViewById(android.R.id.text2);

            name.setText(item.getComponentName());
            if (item.getComponentVersion().equals("")) {
                version.setText("No installed versions.");
            } else {
                version.setText("Version: " + item.getComponentVersion());
            }
            return view;
        }
    }

    /**
     * Asynchronously load components info on a background thread and then update the UI when the
     * data is loaded.
     */
    private void updateComponentInfoList(TextView componentsSummaryView) {
        AsyncTask<ArrayList<ComponentInfo>> asyncTask = new AsyncTask<ArrayList<ComponentInfo>>() {
            @Override
            @WorkerThread
            protected ArrayList<ComponentInfo> doInBackground() {
                ComponentsInfoLoader componentInfoLoader = new ComponentsInfoLoader();
                ArrayList<ComponentInfo> retrievedComponentInfoList =
                        componentInfoLoader.getComponentsInfo();

                return retrievedComponentInfoList;
            }

            @Override
            protected void onPostExecute(ArrayList<ComponentInfo> retrievedComponentInfoList) {
                mComponentInfoListAdapter.clear();
                mComponentInfoListAdapter.addAll(retrievedComponentInfoList);
                componentsSummaryView.setText(String.format(
                        Locale.US, "Components (%d)", retrievedComponentInfoList.size()));
                if (sComponentInfoLoadedListener != null) sComponentInfoLoadedListener.run();
            }
        };
        asyncTask.executeOnExecutor(AsyncTask.THREAD_POOL_EXECUTOR);
    }

    /**
     * Notifies the caller when all ComponentInfo is reloaded in the ListView.
     */
    @MainThread
    @VisibleForTesting
    public static void setComponentInfoLoadedListenerForTesting(@Nullable Runnable listener) {
        sComponentInfoLoadedListener = listener;
    }
}
