// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.devui;

import android.annotation.SuppressLint;
import android.app.Activity;
import android.content.Context;
import android.content.Intent;
import android.os.Bundle;
import android.os.ResultReceiver;
import android.view.LayoutInflater;
import android.view.Menu;
import android.view.MenuInflater;
import android.view.MenuItem;
import android.view.View;
import android.view.ViewGroup;
import android.widget.ArrayAdapter;
import android.widget.ListView;
import android.widget.Switch;
import android.widget.TextView;

import androidx.annotation.Nullable;
import androidx.annotation.WorkerThread;

import org.chromium.android_webview.common.services.ServiceNames;
import org.chromium.android_webview.devui.util.ComponentInfo;
import org.chromium.android_webview.devui.util.ComponentsInfoLoader;
import org.chromium.android_webview.services.ComponentsProviderPathUtil;
import org.chromium.base.ThreadUtils;
import org.chromium.base.task.AsyncTask;
import org.chromium.ui.widget.Toast;

import java.io.File;
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
    private TextView mComponentsSummaryView;
    private Toast mUpdatingToast;
    private Toast mUpdatedToast;
    private boolean mOnDemandUpdate;

    private static String sComponentUpdateServiceName;
    private static @Nullable Runnable sComponentInfoLoadedListener;
    public static final String ON_DEMAND_UPDATE_REQUEST = "ON_DEMAND_UPDATE_REQUEST";
    public static final String SERVICE_FINISH_CALLBACK = "SERVICE_FINISH_CALLBACK";

    @Override
    public void onAttach(Context context) {
        super.onAttach(context);
        mContext = context;
    }

    @Override
    public void onCreate(Bundle savedInstanceState) {
        sComponentUpdateServiceName = ServiceNames.AW_COMPONENT_UPDATE_SERVICE;
        super.onCreate(savedInstanceState);
        setHasOptionsMenu(true);
    }

    @Override
    public View onCreateView(
            LayoutInflater inflater, ViewGroup container, Bundle savedInstanceState) {
        return inflater.inflate(R.layout.fragment_components_list, null);
    }

    @Override
    @SuppressWarnings({"UseSwitchCompatOrMaterialCode"})
    public void onViewCreated(View view, Bundle savedInstanceState) {
        Activity activity = (Activity) mContext;
        activity.setTitle("WebView Components");
        mComponentsSummaryView = view.findViewById(R.id.components_summary_textview);
        ListView mComponentInfoListView = view.findViewById(R.id.components_list);
        mComponentInfoListAdapter = new ComponentInfoListAdapter(new ArrayList<ComponentInfo>());
        mComponentInfoListView.setAdapter(mComponentInfoListAdapter);
        updateComponentInfoList(/* showToast= */ false);
        mUpdatingToast = Toast.makeText(mContext, "Updating Components...", Toast.LENGTH_SHORT);
        mUpdatedToast = Toast.makeText(mContext, "Components Updated!", Toast.LENGTH_SHORT);

        // On-Demand Update enabled by default
        mOnDemandUpdate = true;
        Switch onDemandUpdateToggle = (Switch) view.findViewById(R.id.on_demand_update);
        onDemandUpdateToggle.setOnCheckedChangeListener(
                (buttonView, isChecked) -> {
                    mOnDemandUpdate = isChecked;
                });
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
    public void updateComponentInfoList(Boolean showToast) {
        AsyncTask<ArrayList<ComponentInfo>> asyncTask =
                new AsyncTask<ArrayList<ComponentInfo>>() {
                    @Override
                    @WorkerThread
                    protected ArrayList<ComponentInfo> doInBackground() {
                        ComponentsInfoLoader componentInfoLoader =
                                new ComponentsInfoLoader(
                                        new File(
                                                ComponentsProviderPathUtil
                                                        .getComponentUpdateServiceDirectoryPath()));
                        ArrayList<ComponentInfo> retrievedComponentInfoList =
                                componentInfoLoader.getComponentsInfo();

                        return retrievedComponentInfoList;
                    }

                    @Override
                    protected void onPostExecute(
                            ArrayList<ComponentInfo> retrievedComponentInfoList) {
                        mComponentInfoListAdapter.clear();
                        mComponentInfoListAdapter.addAll(retrievedComponentInfoList);
                        mComponentsSummaryView.setText(
                                String.format(
                                        Locale.US,
                                        "Components (%d)",
                                        retrievedComponentInfoList.size()));

                        // show toast only if the user is viewing current fragment
                        if (showToast && ComponentsListFragment.this.isVisible()) {
                            // show toast only if it is not already showing, prevent toast spam
                            if (mUpdatedToast.getView() != null
                                    && mUpdatedToast.getView().getWindowVisibility()
                                            != View.VISIBLE) {
                                mUpdatedToast.show();
                            }
                        }

                        if (sComponentInfoLoadedListener != null) {
                            sComponentInfoLoadedListener.run();
                        }
                    }
                };
        asyncTask.executeOnExecutor(AsyncTask.THREAD_POOL_EXECUTOR);
    }

    /** Notifies the caller when all ComponentInfo is reloaded in the ListView. */
    public static void setComponentInfoLoadedListenerForTesting(@Nullable Runnable listener) {
        ThreadUtils.assertOnUiThread();
        sComponentInfoLoadedListener = listener;
    }

    public static void setComponentUpdateServiceNameForTesting(String name) {
        ThreadUtils.assertOnUiThread();
        sComponentUpdateServiceName = name;
    }

    @Override
    public void onCreateOptionsMenu(Menu menu, MenuInflater inflater) {
        inflater.inflate(R.menu.components_options_menu, menu);
    }

    @Override
    public boolean onOptionsItemSelected(MenuItem item) {
        if (item.getItemId() == R.id.options_menu_update) {
            MainActivity.logMenuSelection(MainActivity.MenuChoice.COMPONENTS_UPDATE);
            Intent intent = new Intent();
            intent.setClassName(mContext.getPackageName(), sComponentUpdateServiceName);
            intent.putExtra(
                    SERVICE_FINISH_CALLBACK,
                    new ResultReceiver(null) {
                        @Override
                        public void onReceiveResult(int resultCode, Bundle resultData) {
                            updateComponentInfoList(/* showToast= */ true);
                        }
                    });
            intent.putExtra(ON_DEMAND_UPDATE_REQUEST, mOnDemandUpdate);
            // show toast only if the user is viewing current fragment
            if (ComponentsListFragment.this.isVisible()) {
                // show toast only if it is not already showing, prevent toast spam
                if (mUpdatingToast.getView() != null
                        && mUpdatingToast.getView().getWindowVisibility() != View.VISIBLE) {
                    mUpdatingToast.show();
                }
            }
            mContext.startService(intent);
            return true;
        }
        return super.onOptionsItemSelected(item);
    }
}
