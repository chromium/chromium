// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.devui;

import android.app.Activity;
import android.content.Context;
import android.os.Bundle;
import android.view.LayoutInflater;
import android.view.MenuInflater;
import android.view.MenuItem;
import android.view.View;
import android.view.ViewGroup;
import android.widget.ArrayAdapter;
import android.widget.ListView;
import android.widget.PopupMenu;
import android.widget.TextView;

import androidx.annotation.NonNull;

import org.chromium.base.ContextUtils;
import org.chromium.base.ResettersForTesting;
import org.chromium.base.ThreadUtils;

import java.io.File;
import java.text.DateFormat;
import java.util.ArrayList;
import java.util.Date;
import java.util.List;
import java.util.Locale;

public class NetLogsFragment extends DevUiBaseFragment {
    private static final String TAG = "WebViewDevTools";

    private static final Long MAX_TOTAL_CAPACITY = 1000L * 1024 * 1024; // 1 GB

    private static List<File> sFileList = getAllJsonFilesInDirectory();
    private static long sTotalBytes;
    private static NetLogListAdapter sLogAdapter;

    private Context mContext;
    private View mParentView;

    @Override
    public void onAttach(Context context) {
        super.onAttach(context);
        mContext = context;
    }

    @Override
    public View onCreateView(
            LayoutInflater inflater, ViewGroup container, Bundle savedInstanceState) {
        return inflater.inflate(R.layout.fragment_net_logs, null);
    }

    @Override
    public void onViewCreated(View view, Bundle savedInstanceState) {
        Activity activity = (Activity) mContext;
        activity.setTitle("WebView Net Logs");

        updateTotalCapacityText(view.findViewById(R.id.net_logs_total_capacity));

        ListView netLogListView = view.findViewById(R.id.net_log_list);

        sLogAdapter = new NetLogListAdapter(sFileList);
        netLogListView.setAdapter(sLogAdapter);
        mParentView = view;
    }

    @Override
    public void onResume() {
        super.onResume();
        updateNetLogData();
    }

    public void updateNetLogData() {
        List<File> updatedList = getAllJsonFilesInDirectory();
        if (updatedList.size() == sFileList.size()) {
            return;
        }
        sFileList = updatedList;
        sLogAdapter.notifyDataSetChanged();
        updateTotalCapacityText(mParentView.findViewById(R.id.net_logs_total_capacity));
    }

    private static String getMegabytesFromBytes(long bytes) {
        double megabytes = (double) bytes / (MAX_TOTAL_CAPACITY / 1000);
        return String.format(Locale.US, "%.2f MB", megabytes);
    }

    private static List<File> getAllJsonFilesInDirectory() {
        sTotalBytes = 0L;
        List<File> allFiles = new ArrayList<>();
        Context dirContext = ContextUtils.getApplicationContext();
        File directory = dirContext.getFilesDir();
        if (directory.isDirectory()) {
            directory.list();
            File[] files = directory.listFiles();
            if (files != null) {
                for (File file : files) {
                    if (file.isFile() && file.getName().endsWith(".json")) {
                        allFiles.add(file);
                        sTotalBytes += file.length();
                    }
                }
            }
        }

        return allFiles;
    }

    public static Long getCreationTimeFromFileName(String fileName) {
        String pid = getProcessID(fileName);
        // Find every integer value.
        String integerName = fileName.replaceAll("[^0-9]+", "");
        return Long.parseLong(integerName.substring(pid.length()));
    }

    private static String getProcessID(String fileName) {
        String pid = "";
        for (int i = 0; i < fileName.length(); i++) {
            if (fileName.charAt(i) == '_') {
                break;
            }
            pid += fileName.charAt(i);
        }
        return pid;
    }

    public static void setFileListForTesting(@NonNull List<File> fileList) {
        ThreadUtils.assertOnUiThread();
        List<File> oldValue = sFileList;
        sFileList = fileList;
        ResettersForTesting.register(() -> sFileList = oldValue);
    }

    public static void updateFileListForTesting(File file) {
        ThreadUtils.assertOnUiThread();
        sFileList.add(file);
    }

    public static String getFilePackageName(File file) {
        // Find all instances of commas, underscore and integers.
        String fileName = file.getName().replaceAll("[0-9, _]+", "");
        return fileName.substring(0, fileName.length() - 5);
    }

    private static void updateTotalCapacityText(TextView textView) {
        String diskUsage = "Total Disk Usage: " + getMegabytesFromBytes(sTotalBytes);
        textView.setText(diskUsage);
    }

    /** Adapter to create rows of toggleable Net Log Files. */
    private class NetLogListAdapter extends ArrayAdapter<File> {
        private List<File> mItems;

        public NetLogListAdapter(List<File> files) {
            super(mContext, 0);
            mItems = files;
        }

        @Override
        public int getCount() {
            return mItems.size();
        }

        @Override
        public File getItem(int position) {
            return mItems.get(position);
        }

        @Override
        public View getView(int position, View view, ViewGroup parent) {
            if (view == null) {
                view = getLayoutInflater().inflate(R.layout.net_log_entry, null);
            }

            File file = getItem(position);

            TextView fileNameView = view.findViewById(R.id.file_name);
            fileNameView.setText(getFilePackageName(file));

            TextView fileTimeView = view.findViewById(R.id.file_time);
            DateFormat dateFormat = DateFormat.getDateTimeInstance();
            String date = dateFormat.format(new Date(getCreationTimeFromFileName(file.getName())));
            fileTimeView.setText(date);

            TextView fileCapacityView = view.findViewById(R.id.file_capacity);
            fileCapacityView.setText(getMegabytesFromBytes(file.length()));

            View.OnClickListener listener =
                    new View.OnClickListener() {
                        @Override
                        public void onClick(View clickedView) {
                            showPopupMenu(clickedView, position);
                        }
                    };

            // TODO(thomasbull): Get the listener working on one view, instead of three separate
            // subviews.
            fileNameView.setOnClickListener(listener);
            fileTimeView.setOnClickListener(listener);
            fileCapacityView.setOnClickListener(listener);
            return view;
        }

        private void showPopupMenu(View view, final int position) {
            PopupMenu popup = new PopupMenu(view.getContext(), view);
            MenuInflater inflater = popup.getMenuInflater();
            inflater.inflate(R.menu.net_log_menu, popup.getMenu());
            popup.setOnMenuItemClickListener(
                    new PopupMenu.OnMenuItemClickListener() {
                        @Override
                        public boolean onMenuItemClick(MenuItem item) {
                            int id = item.getItemId();
                            if (id == R.id.net_log_menu_delete) {
                                // TODO(thomasbull): Implement Delete
                                return true;
                            } else if (id == R.id.net_log_menu_share) {
                                // TODO(thomasbull): Implement Share
                                return true;
                            }
                            return false;
                        }
                    });
            popup.show();
        }
    }
}
