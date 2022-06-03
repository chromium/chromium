// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.customtabsclient;

import android.app.ActivityManager;
import android.app.ActivityOptions;
import android.app.PendingIntent;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.content.pm.ResolveInfo;
import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.graphics.Color;
import android.media.MediaPlayer;
import android.net.Uri;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.text.TextUtils;
import android.util.Log;
import android.util.Pair;
import android.view.LayoutInflater;
import android.view.View;
import android.view.View.OnClickListener;
import android.view.ViewGroup;
import android.widget.AdapterView;
import android.widget.ArrayAdapter;
import android.widget.Button;
import android.widget.EditText;
import android.widget.Spinner;
import android.widget.TextView;

import androidx.appcompat.app.AppCompatActivity;
import androidx.browser.customtabs.CustomTabsCallback;
import androidx.browser.customtabs.CustomTabsClient;
import androidx.browser.customtabs.CustomTabsIntent;
import androidx.browser.customtabs.CustomTabsServiceConnection;
import androidx.browser.customtabs.CustomTabsSession;

import org.chromium.customtabsclient.shared.CustomTabsHelper;
import org.chromium.customtabsclient.shared.ServiceConnection;
import org.chromium.customtabsclient.shared.ServiceConnectionCallback;

import java.util.ArrayList;
import java.util.List;

/**
 * Example client activity for using Chrome Custom Tabs.
 */
public class MainActivity
        extends AppCompatActivity implements OnClickListener, ServiceConnectionCallback {
    private static final String TAG = "CustomTabsClientExample";
    private static final String TOOLBAR_COLOR = "#ef6c00";

    private EditText mEditText;
    private CustomTabsSession mCustomTabsSession;
    private CustomTabsClient mClient;
    private CustomTabsServiceConnection mConnection;
    private String mPackageNameToBind;
    private Button mConnectButton;
    private Button mWarmupButton;
    private Button mMayLaunchButton;
    private Button mLaunchButton;
    private Button mLaunchIncognitoButton;
    private MediaPlayer mMediaPlayer;

    /**
     * Once per second, asks the framework for the process importance, and logs any change.
     */
    private Runnable mLogImportance = new Runnable() {
        private int mPreviousImportance = -1;
        private boolean mPreviousServiceInUse;
        private Handler mHandler = new Handler(Looper.getMainLooper());

        @Override
        public void run() {
            ActivityManager.RunningAppProcessInfo state =
                    new ActivityManager.RunningAppProcessInfo();
            ActivityManager.getMyMemoryState(state);
            int importance = state.importance;
            boolean serviceInUse = state.importanceReasonCode
                    == ActivityManager.RunningAppProcessInfo.REASON_SERVICE_IN_USE;
            if (importance != mPreviousImportance || serviceInUse != mPreviousServiceInUse) {
                mPreviousImportance = importance;
                mPreviousServiceInUse = serviceInUse;
                String message = "New importance = " + importance;
                if (serviceInUse) message += " (Reason: Service in use)";
                Log.w(TAG, message);
            }
            mHandler.postDelayed(this, 1000);
        }
    };

    private static class NavigationCallback extends CustomTabsCallback {
        @Override
        public void onNavigationEvent(int navigationEvent, Bundle extras) {
            Log.w(TAG, "onNavigationEvent: Code = " + navigationEvent);
        }
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.main);
        mEditText = (EditText) findViewById(R.id.edit);
        mConnectButton = (Button) findViewById(R.id.connect_button);
        mWarmupButton = (Button) findViewById(R.id.warmup_button);
        mMayLaunchButton = (Button) findViewById(R.id.may_launch_button);
        mLaunchButton = (Button) findViewById(R.id.launch_button);
        mLaunchIncognitoButton = findViewById(R.id.launch_incognito_button);
        Spinner spinner = (Spinner) findViewById(R.id.spinner);
        mEditText.requestFocus();
        mConnectButton.setOnClickListener(this);
        mWarmupButton.setOnClickListener(this);
        mMayLaunchButton.setOnClickListener(this);
        mLaunchButton.setOnClickListener(this);
        mLaunchIncognitoButton.setOnClickListener(this);
        mMediaPlayer = MediaPlayer.create(this, R.raw.amazing_grace);
        findViewById(R.id.register_twa_service).setOnClickListener(this);

        Intent activityIntent = new Intent(Intent.ACTION_VIEW, Uri.parse("http://www.example.com"));
        PackageManager pm = getPackageManager();
        List<ResolveInfo> resolvedActivityList = pm.queryIntentActivities(
                activityIntent, PackageManager.MATCH_ALL);
        List<Pair<String, String>> packagesSupportingCustomTabs = new ArrayList<>();
        for (ResolveInfo info : resolvedActivityList) {
            Intent serviceIntent = new Intent();
            serviceIntent.setAction("android.support.customtabs.action.CustomTabsService");
            serviceIntent.setPackage(info.activityInfo.packageName);
            if (pm.resolveService(serviceIntent, 0) != null) {
                packagesSupportingCustomTabs.add(
                        Pair.create(info.loadLabel(pm).toString(), info.activityInfo.packageName));
            }
        }

        final ArrayAdapter<Pair<String, String>> adapter = new ArrayAdapter<Pair<String, String>>(
                this, 0, packagesSupportingCustomTabs) {
            @Override
            public View getView(int position, View convertView, ViewGroup parent) {
                View view = convertView;
                if (view == null) {
                    view = LayoutInflater.from(MainActivity.this).inflate(
                            android.R.layout.simple_list_item_2, parent, false);
                }
                Pair<String, String> data = getItem(position);
                ((TextView) view.findViewById(android.R.id.text1)).setText(data.first);
                ((TextView) view.findViewById(android.R.id.text2)).setText(data.second);
                return view;
            }

            @Override
            public View getDropDownView(int position, View convertView, ViewGroup parent) {
                return getView(position, convertView, parent);
            }
        };
        spinner.setAdapter(adapter);
        spinner.setOnItemSelectedListener(new AdapterView.OnItemSelectedListener() {
            @Override
            public void onItemSelected(AdapterView<?> parent, View view, int position, long id) {
                Pair<String, String> item = adapter.getItem(position);
                if (TextUtils.isEmpty(item.second)) {
                    onNothingSelected(parent);
                    return;
                }
                mPackageNameToBind = item.second;
            }

            @Override
            public void onNothingSelected(AdapterView<?> parent) {
                mPackageNameToBind = null;
            }
        });

        mLogImportance.run();
    }

    @Override
    protected void onDestroy() {
        unbindCustomTabsService();
        super.onDestroy();
    }

    private CustomTabsSession getSession() {
        if (mClient == null) {
            mCustomTabsSession = null;
        } else if (mCustomTabsSession == null) {
            mCustomTabsSession = mClient.newSession(new NavigationCallback());
            SessionHelper.setCurrentSession(mCustomTabsSession);
        }
        return mCustomTabsSession;
    }

    private void bindCustomTabsService() {
        if (mClient != null) return;
        if (TextUtils.isEmpty(mPackageNameToBind)) {
            mPackageNameToBind = CustomTabsHelper.getPackageNameToUse(this);
            if (mPackageNameToBind == null) return;
        }
        mConnection = new ServiceConnection(this);
        boolean ok = CustomTabsClient.bindCustomTabsService(this, mPackageNameToBind, mConnection);
        if (ok) {
            mConnectButton.setEnabled(false);
        } else {
            mConnection = null;
        }
    }

    private void unbindCustomTabsService() {
        if (mConnection == null) return;
        unbindService(mConnection);
        mClient = null;
        mCustomTabsSession = null;
    }

    @Override
    public void onClick(View v) {
        String url = mEditText.getText().toString();
        int viewId = v.getId();

        if (viewId == R.id.connect_button) {
            bindCustomTabsService();
        } else if (viewId == R.id.warmup_button) {
            boolean success = false;
            if (mClient != null) success = mClient.warmup(0);
            if (!success) mWarmupButton.setEnabled(false);
        } else if (viewId == R.id.may_launch_button) {
            CustomTabsSession session = getSession();
            boolean success = false;
            if (mClient != null) success = session.mayLaunchUrl(Uri.parse(url), null, null);
            if (!success) mMayLaunchButton.setEnabled(false);
        } else if (viewId == R.id.launch_button || viewId == R.id.launch_incognito_button) {
            CustomTabsSession session = getSession();
            CustomTabsIntent.Builder builder = new CustomTabsIntent.Builder(session);
            builder.setToolbarColor(Color.parseColor(TOOLBAR_COLOR)).setShowTitle(true);
            prepareMenuItems(builder);
            prepareActionButton(builder);
            if (session != null) prepareBottombar(builder);
            builder.setStartAnimations(this, R.anim.slide_in_right, R.anim.slide_out_left);
            builder.setExitAnimations(this, R.anim.slide_in_left, R.anim.slide_out_right);
            builder.setCloseButtonIcon(
                    BitmapFactory.decodeResource(getResources(), R.drawable.ic_arrow_back));
            CustomTabsIntent customTabsIntent = builder.build();
            // NOTE: opening in incognito may be restricted. This assumes it is not.
            customTabsIntent.intent.putExtra(
                    "com.google.android.apps.chrome.EXTRA_OPEN_NEW_INCOGNITO_TAB",
                    viewId == R.id.launch_incognito_button);
            if (session != null) {
                CustomTabsHelper.addKeepAliveExtra(this, customTabsIntent.intent);
            } else {
                if (!TextUtils.isEmpty(mPackageNameToBind)) {
                    customTabsIntent.intent.setPackage(mPackageNameToBind);
                }
            }
            customTabsIntent.launchUrl(this, Uri.parse(url));
        }
    }

    private void prepareMenuItems(CustomTabsIntent.Builder builder) {
        Intent menuIntent = new Intent();
        menuIntent.setClass(getApplicationContext(), this.getClass());
        // Optional animation configuration when the user clicks menu items.
        Bundle menuBundle = ActivityOptions.makeCustomAnimation(this, android.R.anim.slide_in_left,
                android.R.anim.slide_out_right).toBundle();
        PendingIntent pi = PendingIntent.getActivity(getApplicationContext(), 0, menuIntent, 0,
                menuBundle);
        builder.addMenuItem("Menu entry 1", pi);
    }

    private void prepareActionButton(CustomTabsIntent.Builder builder) {
        // An example intent that sends an email.
        Intent actionIntent = new Intent(Intent.ACTION_SEND);
        actionIntent.setType("*/*");
        actionIntent.putExtra(Intent.EXTRA_EMAIL, "example@example.com");
        actionIntent.putExtra(Intent.EXTRA_SUBJECT, "example");
        PendingIntent pi = PendingIntent.getActivity(this, 0, actionIntent, 0);
        Bitmap icon = BitmapFactory.decodeResource(getResources(), R.drawable.ic_share);
        builder.setActionButton(icon, "send email", pi, true);
    }

    private void prepareBottombar(CustomTabsIntent.Builder builder) {
        BottomBarManager.setMediaPlayer(mMediaPlayer);
        builder.setSecondaryToolbarViews(BottomBarManager.createRemoteViews(this, true),
                BottomBarManager.getClickableIDs(), BottomBarManager.getOnClickPendingIntent(this));
    }

    @Override
    public void onServiceConnected(CustomTabsClient client) {
        mClient = client;
        mConnectButton.setEnabled(false);
        mWarmupButton.setEnabled(true);
        mMayLaunchButton.setEnabled(true);
        mLaunchButton.setEnabled(true);
        mLaunchIncognitoButton.setEnabled(true);
    }

    @Override
    public void onServiceDisconnected() {
        mConnectButton.setEnabled(true);
        mWarmupButton.setEnabled(false);
        mMayLaunchButton.setEnabled(false);
        mLaunchButton.setEnabled(false);
        mLaunchIncognitoButton.setEnabled(false);
        mClient = null;
    }
}
