// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.devui;

import android.app.Activity;
import android.app.AlertDialog;
import android.content.ClipData;
import android.content.ClipboardManager;
import android.content.Context;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.database.DataSetObserver;
import android.graphics.drawable.Drawable;
import android.os.Bundle;
import android.view.LayoutInflater;
import android.view.Menu;
import android.view.MenuInflater;
import android.view.MenuItem;
import android.view.View;
import android.view.ViewGroup;
import android.widget.BaseExpandableListAdapter;
import android.widget.Button;
import android.widget.ExpandableListView;
import android.widget.ImageButton;
import android.widget.ImageView;
import android.widget.TextView;

import androidx.annotation.IntDef;
import androidx.annotation.MainThread;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;
import androidx.annotation.WorkerThread;

import org.chromium.android_webview.common.DeveloperModeUtils;
import org.chromium.android_webview.common.PlatformServiceBridge;
import org.chromium.android_webview.devui.util.CrashBugUrlFactory;
import org.chromium.android_webview.devui.util.SafeIntentUtils;
import org.chromium.android_webview.devui.util.WebViewCrashInfoCollector;
import org.chromium.android_webview.nonembedded.crash.CrashInfo;
import org.chromium.android_webview.nonembedded.crash.CrashInfo.UploadState;
import org.chromium.android_webview.nonembedded.crash.CrashUploadUtil;
import org.chromium.base.BaseSwitches;
import org.chromium.base.CommandLine;
import org.chromium.base.Log;
import org.chromium.base.PackageManagerUtils;
import org.chromium.base.ResettersForTesting;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.task.AsyncTask;
import org.chromium.base.version_info.Channel;
import org.chromium.base.version_info.VersionConstants;
import org.chromium.ui.widget.Toast;

import java.util.ArrayList;
import java.util.Date;
import java.util.List;
import java.util.Locale;

/** A fragment to show a list of recent WebView crashes. */
public class CrashesListFragment extends DevUiBaseFragment {
    private static final String TAG = "WebViewDevTools";

    public static final String CRASH_BUG_DIALOG_MESSAGE =
            "This crash has already been reported to our crash system. Do you want to share more"
                    + " information, such as steps to reproduce the crash?";
    public static final String NO_WIFI_DIALOG_MESSAGE =
            "You are connected to a metered network or cellular data." + " Do you want to proceed?";
    public static final String CRASH_COLLECTION_DISABLED_ERROR_MESSAGE =
            "Crash collection is disabled. Please turn on 'Usage & diagnostics' "
                    + "from the three-dotted menu in Google settings.";
    public static final String NO_GMS_ERROR_MESSAGE =
            "Crash collection is not supported at the moment.";

    public static final String USAGE_AND_DIAGONSTICS_ACTIVITY_INTENT_ACTION =
            "com.android.settings.action.EXTRA_SETTINGS";

    // Max number of crashes to show in the crashes list.
    public static final int MAX_CRASHES_NUMBER = 20;

    private CrashListExpandableAdapter mCrashListViewAdapter;
    private Context mContext;

    private static @Nullable Runnable sCrashInfoLoadedListener;

    // These values are persisted to logs. Entries should not be renumbered and
    // numeric values should never be reused.
    @IntDef({
        CollectionState.ENABLED_BY_COMMANDLINE,
        CollectionState.ENABLED_BY_FLAG_UI,
        CollectionState.ENABLED_BY_USER_CONSENT,
        CollectionState.DISABLED_BY_USER_CONSENT,
        CollectionState.DISABLED_BY_USER_CONSENT_CANNOT_FIND_SETTINGS,
        CollectionState.DISABLED_CANNOT_USE_GMS
    })
    private @interface CollectionState {
        int ENABLED_BY_COMMANDLINE = 0;
        int ENABLED_BY_FLAG_UI = 1;
        int ENABLED_BY_USER_CONSENT = 2;
        int DISABLED_BY_USER_CONSENT = 3;
        int DISABLED_BY_USER_CONSENT_CANNOT_FIND_SETTINGS = 4;
        int DISABLED_CANNOT_USE_GMS = 5;
        int COUNT = 6;
    }

    private static void logCrashCollectionState(@CollectionState int state) {
        RecordHistogram.recordEnumeratedHistogram(
                "Android.WebView.DevUi.CrashList.CollectionState", state, CollectionState.COUNT);
    }

    // These values are persisted to logs. Entries should not be renumbered and
    // numeric values should never be reused.
    @IntDef({
        CrashInteraction.FORCE_UPLOAD_BUTTON,
        CrashInteraction.FORCE_UPLOAD_NO_DIALOG,
        CrashInteraction.FORCE_UPLOAD_DIALOG_METERED_NETWORK,
        CrashInteraction.FORCE_UPLOAD_DIALOG_CANCEL,
        CrashInteraction.FILE_BUG_REPORT_BUTTON,
        CrashInteraction.FILE_BUG_REPORT_DIALOG_PROCEED,
        CrashInteraction.FILE_BUG_REPORT_DIALOG_DISMISS,
        CrashInteraction.HIDE_CRASH_BUTTON
    })
    private @interface CrashInteraction {
        int FORCE_UPLOAD_BUTTON = 0;
        int FORCE_UPLOAD_NO_DIALOG = 1;
        int FORCE_UPLOAD_DIALOG_METERED_NETWORK = 2;
        int FORCE_UPLOAD_DIALOG_CANCEL = 3;
        int FILE_BUG_REPORT_BUTTON = 4;
        int FILE_BUG_REPORT_DIALOG_PROCEED = 5;
        int FILE_BUG_REPORT_DIALOG_DISMISS = 6;
        int HIDE_CRASH_BUTTON = 7;
        int COUNT = 8;
    }

    private static void logCrashInteraction(@CrashInteraction int action) {
        RecordHistogram.recordEnumeratedHistogram(
                "Android.WebView.DevUi.CrashList.CrashInteraction", action, CrashInteraction.COUNT);
    }

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
        return inflater.inflate(R.layout.fragment_crashes_list, null);
    }

    @Override
    public void onViewCreated(View view, Bundle savedInstanceState) {
        Activity activity = (Activity) mContext;
        activity.setTitle("WebView Crashes");

        TextView crashesSummaryView = view.findViewById(R.id.crashes_summary_textview);
        mCrashListViewAdapter = new CrashListExpandableAdapter(crashesSummaryView);
        ExpandableListView crashListView = view.findViewById(R.id.crashes_list);
        crashListView.setAdapter(mCrashListViewAdapter);
    }

    @Override
    public void onResume() {
        super.onResume();
        mCrashListViewAdapter.updateCrashes();
    }

    private boolean isCrashUploadsEnabledFromCommandLine() {
        return CommandLine.getInstance().hasSwitch(BaseSwitches.ENABLE_CRASH_REPORTER_FOR_TESTING);
    }

    private boolean isCrashUploadsEnabledFromFlagsUi() {
        if (DeveloperModeUtils.isDeveloperModeEnabled(mContext.getPackageName())) {
            Boolean flagValue =
                    DeveloperModeUtils.getFlagOverrides(mContext.getPackageName())
                            .get(BaseSwitches.ENABLE_CRASH_REPORTER_FOR_TESTING);
            return Boolean.TRUE.equals(flagValue);
        }
        return false;
    }

    /** Adapter to create crashes list items from a list of CrashInfo. */
    private class CrashListExpandableAdapter extends BaseExpandableListAdapter {
        private List<CrashInfo> mCrashInfoList;

        CrashListExpandableAdapter(TextView crashesSummaryView) {
            mCrashInfoList = new ArrayList<>();

            // Update crash summary when the data changes.
            registerDataSetObserver(
                    new DataSetObserver() {
                        @Override
                        public void onChanged() {
                            crashesSummaryView.setText(
                                    String.format(
                                            Locale.US, "Crashes (%d)", mCrashInfoList.size()));
                        }
                    });
        }

        // Group View which is used as header for a crash in crashes list.
        // We show:
        //   - Icon of the app where the crash happened.
        //   - Package name of the app where the crash happened.
        //   - Time when the crash happened.
        @Override
        public View getGroupView(
                int groupPosition, boolean isExpanded, View view, ViewGroup parent) {
            // If the the old view is already created then reuse it, else create a new one by layout
            // inflation.
            if (view == null) {
                view = getLayoutInflater().inflate(R.layout.crashes_list_item_header, null);
            }

            CrashInfo crashInfo = (CrashInfo) getGroup(groupPosition);

            ImageView packageIcon = view.findViewById(R.id.crash_package_icon);
            String packageName = crashInfo.getCrashKey(CrashInfo.APP_PACKAGE_NAME_KEY);
            if (packageName == null) {
                // This can happen if crash log file where we keep crash info is cleared but other
                // log files like upload logs still exist.
                packageName = "unknown app";
                packageIcon.setImageResource(android.R.drawable.sym_def_app_icon);
            } else {
                try {
                    Drawable icon = mContext.getPackageManager().getApplicationIcon(packageName);
                    packageIcon.setImageDrawable(icon);
                } catch (PackageManager.NameNotFoundException e) {
                    // This can happen if the app was uninstalled after the crash was recorded.
                    packageIcon.setImageResource(android.R.drawable.sym_def_app_icon);
                }
            }
            setTwoLineListItemText(
                    view.findViewById(R.id.crash_header),
                    packageName,
                    new Date(crashInfo.captureTime).toString());
            return view;
        }

        // Child View where more info about the crash is shown:
        //    - Crash report upload status.
        @Override
        public View getChildView(
                int groupPosition,
                final int childPosition,
                boolean isLastChild,
                View view,
                ViewGroup parent) {
            // If the the old view is already created then reuse it, else create a new one by layout
            // inflation.
            if (view == null) {
                view = getLayoutInflater().inflate(R.layout.crashes_list_item_body, null);
            }

            CrashInfo crashInfo = (CrashInfo) getChild(groupPosition, childPosition);

            // Upload info
            String uploadState = uploadStateString(crashInfo.uploadState);
            View uploadInfoView = view.findViewById(R.id.upload_status);
            if (crashInfo.uploadState == UploadState.UPLOADED) {
                final String uploadInfo =
                        new Date(crashInfo.uploadTime).toString() + "\nID: " + crashInfo.uploadId;
                uploadInfoView.setOnLongClickListener(
                        v -> {
                            ClipboardManager clipboard =
                                    (ClipboardManager)
                                            mContext.getSystemService(Context.CLIPBOARD_SERVICE);
                            ClipData clip = ClipData.newPlainText("upload info", uploadInfo);
                            clipboard.setPrimaryClip(clip);
                            // Show a toast that the text has been copied.
                            Toast.makeText(mContext, "Copied upload info", Toast.LENGTH_SHORT)
                                    .show();
                            return true;
                        });
                setTwoLineListItemText(uploadInfoView, uploadState, uploadInfo);
            } else {
                setTwoLineListItemText(uploadInfoView, uploadState, null);
            }

            Button bugButton = view.findViewById(R.id.crash_report_button);
            // Report button is only clickable if the crash report is uploaded.
            if (crashInfo.uploadState == UploadState.UPLOADED) {
                bugButton.setEnabled(true);
                bugButton.setOnClickListener(
                        v -> {
                            logCrashInteraction(CrashInteraction.FILE_BUG_REPORT_BUTTON);
                            buildCrashBugDialog(crashInfo).show();
                        });
            } else {
                bugButton.setEnabled(false);
            }

            Button uploadButton = view.findViewById(R.id.crash_upload_button);
            if (crashInfo.uploadState == UploadState.SKIPPED
                    || crashInfo.uploadState == UploadState.PENDING) {
                uploadButton.setVisibility(View.VISIBLE);
                uploadButton.setOnClickListener(
                        v -> {
                            if (!CrashUploadUtil.isNetworkUnmetered(mContext)) {
                                new AlertDialog.Builder(mContext)
                                        .setTitle("Network Warning")
                                        .setMessage(NO_WIFI_DIALOG_MESSAGE)
                                        .setPositiveButton(
                                                "Upload",
                                                (dialog, id) -> {
                                                    logCrashInteraction(
                                                            CrashInteraction
                                                                    .FORCE_UPLOAD_DIALOG_METERED_NETWORK);
                                                    attemptUploadCrash(crashInfo.localId);
                                                })
                                        .setNegativeButton(
                                                "Cancel",
                                                (dialog, id) -> {
                                                    logCrashInteraction(
                                                            CrashInteraction
                                                                    .FORCE_UPLOAD_DIALOG_CANCEL);
                                                    dialog.dismiss();
                                                })
                                        .create()
                                        .show();
                            } else {
                                logCrashInteraction(CrashInteraction.FORCE_UPLOAD_NO_DIALOG);
                                attemptUploadCrash(crashInfo.localId);
                            }
                        });
            } else {
                uploadButton.setVisibility(View.GONE);
            }

            ImageButton hideButton = view.findViewById(R.id.crash_hide_button);
            hideButton.setOnClickListener(
                    v -> {
                        logCrashInteraction(CrashInteraction.HIDE_CRASH_BUTTON);
                        crashInfo.isHidden = true;
                        WebViewCrashInfoCollector.updateCrashLogFileWithNewCrashInfo(crashInfo);
                        updateCrashes();
                    });

            return view;
        }

        private void attemptUploadCrash(String crashLocalId) {
            // Attempt uploading the file asynchronously, upload is not guaranteed.
            CrashUploadUtil.tryUploadCrashDumpWithLocalId(mContext, crashLocalId);
            // Update the uploadState to be PENDING_USER_REQUESTED or UPLOADED.
            updateCrashes();
        }

        @Override
        public boolean isChildSelectable(int groupPosition, int childPosition) {
            return true;
        }

        @Override
        public Object getGroup(int groupPosition) {
            return mCrashInfoList.get(groupPosition);
        }

        @Override
        public Object getChild(int groupPosition, int childPosition) {
            return mCrashInfoList.get(groupPosition);
        }

        @Override
        public long getGroupId(int groupPosition) {
            // Hash code of local id is unique per crash info object.
            return ((CrashInfo) getGroup(groupPosition)).localId.hashCode();
        }

        @Override
        public long getChildId(int groupPosition, int childPosition) {
            // Child ID refers to a piece of info in a particular crash report. It is stable
            // since we don't change the order we show this info in runtime and currently we show
            // all information in only one child item.
            return childPosition;
        }

        @Override
        public boolean hasStableIds() {
            // Stable IDs mean both getGroupId and getChildId return stable IDs for each group and
            // child i.e: an ID always refers to the same object. See getGroupId and getChildId for
            // why the IDs are stable.
            return true;
        }

        @Override
        public int getChildrenCount(int groupPosition) {
            // Crash info is shown in one child item.
            return 1;
        }

        @Override
        public int getGroupCount() {
            return mCrashInfoList.size();
        }

        /**
         * Asynchronously load crash info on a background thread and then update the UI when the
         * data is loaded.
         */
        public void updateCrashes() {
            AsyncTask<List<CrashInfo>> asyncTask =
                    new AsyncTask<List<CrashInfo>>() {
                        @Override
                        @WorkerThread
                        protected List<CrashInfo> doInBackground() {
                            WebViewCrashInfoCollector crashCollector =
                                    new WebViewCrashInfoCollector();
                            // Only show crashes from the same WebView channel, which usually means
                            // the same package.
                            List<CrashInfo> crashes =
                                    crashCollector.loadCrashesInfo(
                                            crashInfo -> {
                                                @Channel
                                                int channel = getCrashInfoChannel(crashInfo);
                                                // Always show the crash if the channel is unknown
                                                // (to handle missing channel info for example for
                                                // crashes from older versions).
                                                return channel == Channel.DEFAULT
                                                        || channel == VersionConstants.CHANNEL;
                                            });
                            if (crashes.size() > MAX_CRASHES_NUMBER) {
                                return crashes.subList(0, MAX_CRASHES_NUMBER);
                            }
                            return crashes;
                        }

                        @Override
                        protected void onPostExecute(List<CrashInfo> result) {
                            mCrashInfoList = result;
                            notifyDataSetChanged();
                            if (sCrashInfoLoadedListener != null) sCrashInfoLoadedListener.run();
                        }
                    };
            asyncTask.executeOnExecutor(AsyncTask.THREAD_POOL_EXECUTOR);
        }
    }

    @VisibleForTesting
    public static String uploadStateString(UploadState uploadState) {
        if (uploadState == null) return null;
        switch (uploadState) {
            case UPLOADED:
                return "Uploaded";
            case PENDING:
            case PENDING_USER_REQUESTED:
                return "Pending upload";
            case SKIPPED:
                return "Skipped upload";
        }
        return null;
    }

    @Channel
    private static int getCrashInfoChannel(@NonNull CrashInfo c) {
        switch (c.getCrashKeyOrDefault(CrashInfo.WEBVIEW_CHANNEL_KEY, "default")) {
            case "canary":
                return Channel.CANARY;
            case "dev":
                return Channel.DEV;
            case "beta":
                return Channel.BETA;
            case "stable":
                return Channel.STABLE;
            default:
                return Channel.DEFAULT;
        }
    }

    // Helper method to find and set text for two line list item. If a null String is passed, the
    // relevant TextView will be hidden.
    private static void setTwoLineListItemText(
            @NonNull View view, @Nullable String title, @Nullable String subtitle) {
        TextView titleView = view.findViewById(android.R.id.text1);
        TextView subtitleView = view.findViewById(android.R.id.text2);
        if (titleView != null) {
            titleView.setVisibility(View.VISIBLE);
            titleView.setText(title);
        } else {
            titleView.setVisibility(View.GONE);
        }
        if (subtitle != null) {
            subtitleView.setVisibility(View.VISIBLE);
            subtitleView.setText(subtitle);
        } else {
            subtitleView.setVisibility(View.GONE);
        }
    }

    @Override
    void maybeShowErrorView(PersistentErrorView errorView) {
        // Check if crash collection is enabled and show or hide the error message.
        // Firstly, check for the flag value in commandline, since it doesn't require any IPCs.
        // Then check for flags value in the DeveloperUi ContentProvider (it involves an IPC but
        // it's guarded by quick developer mode check). Finally check the GMS service since it
        // is the slowest check.
        if (isCrashUploadsEnabledFromCommandLine()) {
            logCrashCollectionState(CollectionState.ENABLED_BY_COMMANDLINE);
            errorView.hide();
        } else if (isCrashUploadsEnabledFromFlagsUi()) {
            logCrashCollectionState(CollectionState.ENABLED_BY_FLAG_UI);
            errorView.hide();
        } else {
            PlatformServiceBridge.getInstance()
                    .queryMetricsSetting(
                            enabled -> {
                                if (Boolean.TRUE.equals(enabled)) {
                                    logCrashCollectionState(
                                            CollectionState.ENABLED_BY_USER_CONSENT);
                                    errorView.hide();
                                } else {
                                    buildCrashConsentError(errorView);
                                    errorView.show();
                                }
                            });
        }
    }

    private void buildCrashConsentError(PersistentErrorView errorView) {
        if (PlatformServiceBridge.getInstance().canUseGms()) {
            errorView.setText(CRASH_COLLECTION_DISABLED_ERROR_MESSAGE);
            // Open Google Settings activity, "Usage & diagnostics" activity is not exported and
            // cannot be opened directly.
            Intent settingsIntent = new Intent(USAGE_AND_DIAGONSTICS_ACTIVITY_INTENT_ACTION);
            // Show a button to open GMS settings activity only if it exists.
            if (PackageManagerUtils.canResolveActivity(settingsIntent)) {
                logCrashCollectionState(CollectionState.DISABLED_BY_USER_CONSENT);
                errorView.setActionButton(
                        "Open Settings", v -> mContext.startActivity(settingsIntent));
            } else {
                logCrashCollectionState(
                        CollectionState.DISABLED_BY_USER_CONSENT_CANNOT_FIND_SETTINGS);
                Log.e(TAG, "Cannot find GMS settings activity");
            }
        } else {
            logCrashCollectionState(CollectionState.DISABLED_CANNOT_USE_GMS);
            errorView.setText(NO_GMS_ERROR_MESSAGE);
        }
    }

    private AlertDialog buildCrashBugDialog(CrashInfo crashInfo) {
        AlertDialog.Builder dialogBuilder = new AlertDialog.Builder(mContext);
        dialogBuilder.setMessage(CRASH_BUG_DIALOG_MESSAGE);
        dialogBuilder.setPositiveButton(
                "Provide more info",
                (dialog, id) -> {
                    logCrashInteraction(CrashInteraction.FILE_BUG_REPORT_DIALOG_PROCEED);
                    SafeIntentUtils.startActivityOrShowError(
                            mContext,
                            new CrashBugUrlFactory(crashInfo).getReportIntent(),
                            SafeIntentUtils.NO_BROWSER_FOUND_ERROR);
                });
        dialogBuilder.setNegativeButton(
                "Dismiss",
                (dialog, id) -> {
                    logCrashInteraction(CrashInteraction.FILE_BUG_REPORT_DIALOG_DISMISS);
                    dialog.dismiss();
                });
        return dialogBuilder.create();
    }

    /** Notifies the caller when all CrashInfo is reloaded in the ListView. */
    @MainThread
    public static void setCrashInfoLoadedListenerForTesting(@Nullable Runnable listener) {
        sCrashInfoLoadedListener = listener;
        ResettersForTesting.register(() -> sCrashInfoLoadedListener = null);
    }

    @Override
    public void onCreateOptionsMenu(Menu menu, MenuInflater inflater) {
        inflater.inflate(R.menu.crashes_options_menu, menu);
    }

    @Override
    public boolean onOptionsItemSelected(MenuItem item) {
        if (item.getItemId() == R.id.options_menu_refresh) {
            MainActivity.logMenuSelection(MainActivity.MenuChoice.CRASHES_REFRESH);
            mCrashListViewAdapter.updateCrashes();
            return true;
        }
        return super.onOptionsItemSelected(item);
    }
}
