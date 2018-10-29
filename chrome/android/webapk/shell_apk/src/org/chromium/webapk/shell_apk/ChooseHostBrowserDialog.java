// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.webapk.shell_apk;

import android.app.AlertDialog;
import android.content.Context;
import android.content.DialogInterface;
import android.content.pm.PackageManager;
import android.content.pm.ResolveInfo;
import android.content.res.Resources;
import android.graphics.drawable.Drawable;
import android.text.SpannableString;
import android.text.style.RelativeSizeSpan;
import android.view.ContextThemeWrapper;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.AdapterView;
import android.widget.ArrayAdapter;
import android.widget.ImageView;
import android.widget.ListView;
import android.widget.TextView;

import java.util.ArrayList;
import java.util.Collections;
import java.util.Comparator;
import java.util.HashSet;
import java.util.List;
import java.util.Set;

/**
 * Shows the dialog to choose a host browser to launch WebAPK. Calls the listener callback when the
 * host browser is chosen.
 */
public class ChooseHostBrowserDialog {
    /**
     * A listener which is notified when user chooses a host browser for the WebAPK, or dismiss the
     * dialog.
     */
    public interface DialogListener {
        void onHostBrowserSelected(String selectedHostBrowser);
        void onQuit();
    }

    /** Stores information about a potential host browser for the WebAPK. */
    public static class BrowserItem {
        private String mPackageName;
        private CharSequence mApplicationLabel;
        private Drawable mIcon;
        private boolean mSupportsWebApks;

        public BrowserItem(String packageName, CharSequence applicationLabel, Drawable icon,
                boolean supportsWebApks) {
            mPackageName = packageName;
            mApplicationLabel = applicationLabel;
            mIcon = icon;
            mSupportsWebApks = supportsWebApks;
        }

        /** Returns the package name of a browser. */
        public String getPackageName() {
            return mPackageName;
        }

        /** Returns the application name of a browser. */
        public CharSequence getApplicationName() {
            return mApplicationLabel;
        }

        /** Returns a drawable of the browser icon. */
        public Drawable getApplicationIcon() {
            return mIcon;
        }

        /** Returns whether the browser supports WebAPKs. */
        public boolean supportsWebApks() {
            return mSupportsWebApks;
        }
    }

    /**
     * Shows the dialog for choosing a host browser.
     * @param context The current Context.
     * @param listener The listener for the dialog.
     * @param infos The set of ResolvedInfos of the browsers that are shown on the dialog.
     * @param appName The name of the WebAPK for which the dialog is shown.
     */
    public static void show(Context context, final DialogListener listener, Set<ResolveInfo> infos,
            String appName) {
        final List<BrowserItem> browserItems =
                getBrowserInfosForHostBrowserSelection(context.getPackageManager(), infos);

        // The dialog contains:
        // 1) a title
        // 2) a description of the dialog
        // 3) a list of browsers for user to choose from
        TextView title = new TextView(context);
        title.setText(context.getString(R.string.choose_host_browser_dialog_title, appName));
        View view = LayoutInflater.from(context).inflate(R.layout.choose_host_browser_dialog, null);
        WebApkUtils.applyAlertDialogContentStyle(context, view, title);

        TextView desc = (TextView) view.findViewById(R.id.desc);
        desc.setText(R.string.choose_host_browser);

        ListView browserList = (ListView) view.findViewById(R.id.browser_list);
        browserList.setAdapter(new BrowserArrayAdapter(context, browserItems));

        // The context theme wrapper is needed for pre-L.
        AlertDialog.Builder builder = new AlertDialog.Builder(
                new ContextThemeWrapper(context, android.R.style.Theme_DeviceDefault_Light_Dialog));
        builder.setCustomTitle(title).setView(view).setNegativeButton(
                R.string.choose_host_browser_dialog_quit, new DialogInterface.OnClickListener() {
                    @Override
                    public void onClick(DialogInterface dialog, int which) {
                        listener.onQuit();
                    }
                });

        final AlertDialog dialog = builder.create();
        browserList.setOnItemClickListener(new AdapterView.OnItemClickListener() {
            @Override
            public void onItemClick(AdapterView<?> parent, View view, int position, long id) {
                BrowserItem browserItem = browserItems.get(position);
                if (browserItem.supportsWebApks()) {
                    listener.onHostBrowserSelected(browserItem.getPackageName());
                    dialog.cancel();
                }
            }
        });

        dialog.setOnDismissListener(new DialogInterface.OnDismissListener() {
            @Override
            public void onDismiss(DialogInterface dialogInterface) {
                listener.onQuit();
            }
        });
        dialog.show();
    };

    /** Returns a list of BrowserItem for all of the installed browsers. */
    private static List<BrowserItem> getBrowserInfosForHostBrowserSelection(
            PackageManager packageManager, Set<ResolveInfo> resolveInfos) {
        List<BrowserItem> browsers = new ArrayList<>();
        List<String> browsersSupportingWebApk = HostBrowserUtils.getBrowsersSupportingWebApk();
        Set<String> packages = new HashSet<>();

        for (ResolveInfo info : resolveInfos) {
            if (packages.contains(info.activityInfo.packageName)) continue;
            packages.add(info.activityInfo.packageName);

            browsers.add(new BrowserItem(info.activityInfo.packageName,
                    info.loadLabel(packageManager), info.loadIcon(packageManager),
                    browsersSupportingWebApk.contains(info.activityInfo.packageName)));
        }

        if (browsers.size() <= 1) return browsers;

        Collections.sort(browsers, new Comparator<BrowserItem>() {
            @Override
            public int compare(BrowserItem a, BrowserItem b) {
                if (a.mSupportsWebApks == b.mSupportsWebApks) {
                    return a.getPackageName().compareTo(b.getPackageName());
                }
                return a.mSupportsWebApks ? -1 : 1;
            }
        });

        return browsers;
    }

    /** Item adaptor for the list of browsers. */
    private static class BrowserArrayAdapter extends ArrayAdapter<BrowserItem> {
        private List<BrowserItem> mBrowsers;
        private Context mContext;
        private static final float UNSUPPORTED_ICON_OPACITY = 0.26f;
        private static final float SUPPORTED_ICON_OPACITY = 1f;

        public BrowserArrayAdapter(Context context, List<BrowserItem> browsers) {
            super(context, R.layout.host_browser_list_item, browsers);
            mContext = context;
            mBrowsers = browsers;
        }

        @Override
        public View getView(int position, View convertView, ViewGroup parent) {
            if (convertView == null) {
                convertView = LayoutInflater.from(mContext).inflate(
                        R.layout.host_browser_list_item, parent, false);
            }

            Resources res = mContext.getResources();
            ImageView icon = (ImageView) convertView.findViewById(R.id.browser_icon);
            TextView name = (TextView) convertView.findViewById(R.id.browser_name);
            WebApkUtils.setPaddingInPixel(
                    name, res.getDimensionPixelSize(R.dimen.list_column_padding), 0, 0, 0);

            BrowserItem item = mBrowsers.get(position);
            name.setEnabled(item.supportsWebApks());
            if (item.supportsWebApks()) {
                name.setText(item.getApplicationName());
                name.setTextColor(WebApkUtils.getColor(res, R.color.black_alpha_87));
                icon.setAlpha(SUPPORTED_ICON_OPACITY);
            } else {
                String text = mContext.getString(R.string.host_browser_item_not_supporting_webapks,
                        item.getApplicationName());
                SpannableString spannableName = new SpannableString(text);
                float descriptionProportion = res.getDimension(R.dimen.text_size_medium_dense)
                        / res.getDimension(R.dimen.text_size_large);
                spannableName.setSpan(new RelativeSizeSpan(descriptionProportion),
                        item.getApplicationName().length() + 1, spannableName.length(), 0);
                name.setText(spannableName);
                name.setSingleLine(false);
                name.setTextColor(WebApkUtils.getColor(res, R.color.black_alpha_38));
                icon.setAlpha(UNSUPPORTED_ICON_OPACITY);
            }
            icon.setImageDrawable(item.getApplicationIcon());
            icon.setEnabled(item.supportsWebApks());
            return convertView;
        }

        @Override
        public boolean isEnabled(int position) {
            return mBrowsers.get(position).supportsWebApks();
        }
    }
}
