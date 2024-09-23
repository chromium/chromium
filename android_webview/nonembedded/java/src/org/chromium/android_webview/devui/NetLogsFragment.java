// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.devui;

import android.app.Activity;
import android.content.Context;
import android.content.Intent;
import android.net.Uri;
import android.os.Bundle;
import android.view.LayoutInflater;
import android.view.MenuInflater;
import android.view.MenuItem;
import android.view.View;
import android.view.ViewGroup;
import android.widget.ArrayAdapter;
import android.widget.Button;
import android.widget.ListView;
import android.widget.PopupMenu;
import android.widget.TextView;

import androidx.annotation.NonNull;
import androidx.core.content.FileProvider;

import org.chromium.android_webview.services.AwNetLogService;
import org.chromium.base.Log;
import org.chromium.base.ResettersForTesting;
import org.chromium.base.ThreadUtils;
import org.chromium.ui.widget.Toast;

import java.io.File;
import java.text.DateFormat;
import java.util.ArrayList;
import java.util.Collections;
import java.util.Date;
import java.util.HashMap;
import java.util.List;
import java.util.Locale;

public class NetLogsFragment extends DevUiBaseFragment {
    private static final String TAG = "WebViewDevTools";

    private static final Long MAX_TOTAL_CAPACITY = 1000L * 1024 * 1024; // 1 GB

    private static List<File> sFileList = getAllNetLogFilesInDir();
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

        Button deleteAllNetLogsButton = view.findViewById(R.id.delete_all_net_logs_button);
        deleteAllNetLogsButton.setOnClickListener(
                (View flagButton) -> {
                    deleteAllNetLogs();
                });

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
        List<File> updatedList = getAllNetLogFilesInDir();
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

    private static List<File> getAllNetLogFilesInDir() {
        List<File> allFiles = new ArrayList<>();
        sTotalBytes = 0L;
        File directory = AwNetLogService.getNetLogFileDirectory();
        for (File file : directory.listFiles()) {
            allFiles.add(file);
            sTotalBytes += file.length();
        }

        return sortFilesForDisplay(allFiles);
    }

    public static List<File> sortFilesForDisplay(List<File> fileList) {
        List<Long> timeList = new ArrayList<Long>();
        HashMap<Long, File> fileMap = new HashMap<Long, File>();
        for (int i = 0; i < fileList.size(); i++) {
            Long creationTime =
                    AwNetLogService.getCreationTimeFromFileName(fileList.get(i).getName());
            fileMap.put(creationTime, fileList.get(i));
            timeList.add(creationTime);
        }
        List<File> sortedFileList = new ArrayList<File>();
        Collections.sort(timeList);
        for (int i = timeList.size() - 1; i >= 0; i--) {
            sortedFileList.add(fileMap.get(timeList.get(i)));
        }
        return sortedFileList;
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

    private static void deleteAllNetLogs() {
        ArrayList<File> filesToDelete = new ArrayList<>(sFileList);
        for (File file : filesToDelete) {
            deleteNetLogFile(file);
        }
    }

    private static void deleteNetLogFile(File file) {
        if (file.exists()) {
            long capacity = file.length();
            boolean deleted = file.delete();
            if (deleted) {
                sFileList.remove(file);
                sLogAdapter.remove(file);
                sLogAdapter.notifyDataSetChanged();
                sTotalBytes -= capacity;
            } else {
                Log.w(TAG, "Failed to delete file: " + file.getAbsolutePath());
            }
        }
    }

    public static String getFilePackageName(File file) {
        String[] fileAttributes = file.getName().split("_", 3);
        String fileText = fileAttributes[2];
        return fileText.substring(0, (fileText.length() - 5));
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
            String date =
                    dateFormat.format(
                            new Date(AwNetLogService.getCreationTimeFromFileName(file.getName())));
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
                            File file = getItem(position);
                            if (id == R.id.net_log_menu_delete) {
                                deleteNetLogFile(file);
                                return true;
                            } else if (id == R.id.net_log_menu_share) {
                                shareFile(file);
                                return true;
                            }
                            return false;
                        }
                    });
            popup.show();
        }

        public void shareFile(File file) {
            try {
                Uri contentUri =
                        FileProvider.getUriForFile(
                                mContext, mContext.getPackageName() + ".net_logs_provider", file);
                Intent intent = new Intent(Intent.ACTION_SEND);
                intent.setType("application/json");
                intent.putExtra(Intent.EXTRA_STREAM, contentUri);
                intent.addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION);

                startActivity(Intent.createChooser(intent, "Share JSON File"));
            } catch (Exception e) {
                Toast.makeText(mContext, "Error sharing net log file", Toast.LENGTH_LONG).show();
                Log.e(TAG, "Error sharing net log file:", e);
            }
        }
    }
}
