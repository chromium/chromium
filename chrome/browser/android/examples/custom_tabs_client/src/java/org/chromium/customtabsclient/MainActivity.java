// Copyright 2015 The Chromium Authors
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
import android.graphics.Rect;
import android.media.MediaPlayer;
import android.net.Uri;
import android.os.Build;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.support.annotation.Px;
import android.text.TextUtils;
import android.util.DisplayMetrics;
import android.util.Log;
import android.util.Pair;
import android.view.LayoutInflater;
import android.view.View;
import android.view.View.OnClickListener;
import android.view.ViewGroup;
import android.widget.AdapterView;
import android.widget.ArrayAdapter;
import android.widget.Button;
import android.widget.CheckBox;
import android.widget.EditText;
import android.widget.SeekBar;
import android.widget.SeekBar.OnSeekBarChangeListener;
import android.widget.Spinner;
import android.widget.TextView;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.appcompat.app.AppCompatActivity;
import androidx.browser.customtabs.CustomTabsCallback;
import androidx.browser.customtabs.CustomTabsClient;
import androidx.browser.customtabs.CustomTabsIntent;
import androidx.browser.customtabs.CustomTabsServiceConnection;
import androidx.browser.customtabs.CustomTabsSession;

import com.google.android.material.button.MaterialButtonToggleGroup;

import org.chromium.customtabsclient.shared.CustomTabsHelper;
import org.chromium.customtabsclient.shared.ServiceConnection;
import org.chromium.customtabsclient.shared.ServiceConnectionCallback;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;

/**
 * Example client activity for using Chrome Custom Tabs.
 */
public class MainActivity
        extends AppCompatActivity implements OnClickListener, ServiceConnectionCallback {
    private static final String TAG = "CustomTabsClientExample";

    /**
     * Minimal height the bottom sheet CCT should show is half of the display height.
     */
    private static final float MINIMAL_HEIGHT_RATIO = 0.5f;
    private static final int ACTIVITY_HEIGHT_FIXED = 2;

    private EditText mEditText;
    private CustomTabsSession mCustomTabsSession;
    private CustomTabsClient mClient;
    private CustomTabsServiceConnection mConnection;
    private String mPackageNameToBind;
    private String mToolbarColor;
    private Button mConnectButton;
    private Button mWarmupButton;
    private Button mLaunchButton;
    private Button mLaunchIncognitoButton;
    private Button mLaunchPartialHeightCctButton;
    private MediaPlayer mMediaPlayer;
    private MaterialButtonToggleGroup mCloseButtonPositionToggle;
    private MaterialButtonToggleGroup mCloseButtonIcon;
    private MaterialButtonToggleGroup mThemeButton;
    private TextView mToolbarCornerRadiusLabel;
    private SeekBar mToolbarCornerRadiusSlider;
    private CheckBox mBottomToolbarCheckbox;
    private CheckBox mPcctResizableCheckbox;
    private CheckBox mShowTitleCheckbox;
    private CheckBox mUrlHidingCheckbox;
    private TextView mPcctInitialHeightLabel;
    private SeekBar mPcctInitialHeightSlider;
    private Spinner mPackageSpinner;
    private Spinner mColorSpinner;
    private @Px int mMaxHeight;
    private @Px int mInitialHeight;

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

        @Override
        public void extraCallback(@NonNull String callbackName, @Nullable Bundle args) {
            if (args == null) return;

            // CustomTabsConnection#ON_VERTICAL_SCROLL_EVENT_CALLBACK
            if (callbackName.equals("onVerticalScrollEvent")) {
                // CustomTabsConnection#ON_VERTICAL_SCROLL_EVENT_IS_DIRECTION_UP_EXTRA
                Log.w(TAG,
                        "onVerticalScrollEvent: isDirectionUp = "
                                + args.getBoolean("isDirectionUp"));

                // CustomTabsConnection#ON_GREATEST_SCROLL_PERCENTAGE_INCREASED_CALLBACK
            } else if (callbackName.equals("onGreatestScrollPercentageIncreased")) {
                // CustomTabsConnection#ON_GREATEST_SCROLL_PERCENTAGE_INCREASED_PERCENTAGE_EXTRA
                Log.w(TAG,
                        "onGreatestScrollPercentageIncreased: scrollPercentage = "
                                + args.getInt("scrollPercentage"));
            }
        }
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.main);
        mEditText = (EditText) findViewById(R.id.edit);
        mEditText.requestFocus();
        initializePackageSpinner();
        initializeColorSpinner();
        initializeToggles();
        mMediaPlayer = MediaPlayer.create(this, R.raw.amazing_grace);
        findViewById(R.id.register_twa_service).setOnClickListener(this);
        initializeCornerRadiusSlider();
        initializeHeightSlider();
        initializeCheckBoxes();
        initializeButtons();
        mLogImportance.run();
    }

    private void initializePackageSpinner() {
        mPackageSpinner = (Spinner) findViewById(R.id.package_spinner);
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
        mPackageSpinner.setAdapter(adapter);
        mPackageSpinner.setOnItemSelectedListener(new AdapterView.OnItemSelectedListener() {
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
    }

    private void initializeColorSpinner() {
        mColorSpinner = (Spinner) findViewById(R.id.color_spinner);
        HashMap<String, String> colors = new HashMap<String, String>();
        colors.put("Default", "");
        colors.put("Orange", "#ef6c00");
        colors.put("Red", "#c63d3c");
        colors.put("Green", "#369f3d");
        colors.put("Blue", "#3d3bad");
        final ArrayAdapter<String> colorAdapter = new ArrayAdapter<String>(
                this, 0, colors.keySet().toArray(new String[0])) {
            @Override
            public View getView(int position, View convertView, ViewGroup parent) {
                View view = convertView;
                if (view == null) {
                    view = LayoutInflater.from(MainActivity.this)
                                   .inflate(android.R.layout.simple_list_item_2, parent, false);
                }
                String data = getItem(position);
                ((TextView) view.findViewById(android.R.id.text1)).setText(data);
                return view;
            }
            @Override
            public View getDropDownView(int position, View convertView, ViewGroup parent) {
                return getView(position, convertView, parent);
            }
        };
        mColorSpinner.setAdapter(colorAdapter);
        mColorSpinner.setOnItemSelectedListener(new AdapterView.OnItemSelectedListener() {
            @Override
            public void onItemSelected(AdapterView<?> parent, View view, int position, long id) {
                String item = colorAdapter.getItem(position);
                if (TextUtils.isEmpty(item)) {
                    onNothingSelected(parent);
                    return;
                }
                mToolbarColor = colors.get(item);
            }
            @Override
            public void onNothingSelected(AdapterView<?> parent) {}
        });
    }

    private void initializeToggles() {
        mThemeButton = findViewById(R.id.theme_toggle);
        mThemeButton.check(R.id.system_button);
        mCloseButtonPositionToggle = findViewById(R.id.close_button_position_toggle);
        mCloseButtonPositionToggle.check(R.id.start_button);
        mCloseButtonIcon = findViewById(R.id.close_button_icon_toggle);
        mCloseButtonIcon.check(R.id.x_button);
    }

    private void initializeCornerRadiusSlider() {
        mToolbarCornerRadiusLabel = findViewById(R.id.corner_radius_slider_label);
        mToolbarCornerRadiusSlider = findViewById(R.id.corner_radius_slider);
        mToolbarCornerRadiusLabel.setText(
                getString(R.string.dp_template, mToolbarCornerRadiusSlider.getProgress()));
        mToolbarCornerRadiusSlider.setOnSeekBarChangeListener(new OnSeekBarChangeListener() {
            @Override
            public void onProgressChanged(SeekBar seekBar, int progress, boolean fromUser) {
                mToolbarCornerRadiusLabel.setText(getString(R.string.dp_template, progress));
            }

            @Override
            public void onStartTrackingTouch(SeekBar seekBar) {}

            @Override
            public void onStopTrackingTouch(SeekBar seekBar) {}
        });
    }

    private void initializeCheckBoxes() {
        mPcctResizableCheckbox = findViewById(R.id.pcct_resizable_checkbox);
        mBottomToolbarCheckbox = findViewById(R.id.bottom_toolbar_checkbox);
        mShowTitleCheckbox = findViewById(R.id.show_title_checkbox);
        mUrlHidingCheckbox = findViewById(R.id.url_hiding_checkbox);
    }

    private void initializeButtons() {
        mConnectButton = (Button) findViewById(R.id.connect_button);
        mWarmupButton = (Button) findViewById(R.id.warmup_button);
        mLaunchButton = (Button) findViewById(R.id.launch_button);
        mLaunchIncognitoButton = findViewById(R.id.launch_incognito_button);
        mLaunchPartialHeightCctButton = findViewById(R.id.launch_pcct_button);
        mConnectButton.setOnClickListener(this);
        mWarmupButton.setOnClickListener(this);
        mLaunchButton.setOnClickListener(this);
        mLaunchIncognitoButton.setOnClickListener(this);
        mLaunchPartialHeightCctButton.setOnClickListener(this);
    }

    private void initializeHeightSlider() {
        mMaxHeight = getMaximumPossibleHeight();
        mInitialHeight = (int) (mMaxHeight * MINIMAL_HEIGHT_RATIO);
        mPcctInitialHeightSlider = findViewById(R.id.pcct_initial_height_slider);
        mPcctInitialHeightLabel = findViewById(R.id.pcct_initial_height_slider_label);
        mPcctInitialHeightSlider.setMax(mMaxHeight);
        mPcctInitialHeightSlider.setProgress(mInitialHeight);
        mPcctInitialHeightLabel.setText(getString(R.string.px_template, mInitialHeight));
        mPcctInitialHeightSlider.setOnSeekBarChangeListener(new OnSeekBarChangeListener() {
            @Override
            public void onProgressChanged(SeekBar seekBar, int progress, boolean fromUser) {
                mPcctInitialHeightLabel.setText(getString(R.string.px_template, progress));
            }
            @Override
            public void onStartTrackingTouch(SeekBar seekBar) {}
            @Override
            public void onStopTrackingTouch(SeekBar seekBar) {}
        });
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

        // @CloseButtonPosition
        int closeButtonPosition =
                mCloseButtonPositionToggle.getCheckedButtonId() == R.id.end_button ? 2 : 1;

        if (viewId == R.id.connect_button) {
            bindCustomTabsService();
        } else if (viewId == R.id.warmup_button) {
            boolean success = false;
            if (mClient != null) success = mClient.warmup(0);
            if (!success) mWarmupButton.setEnabled(false);
        } else if (viewId == R.id.launch_button || viewId == R.id.launch_incognito_button) {
            CustomTabsSession session = getSession();
            CustomTabsIntent.Builder builder = new CustomTabsIntent.Builder(session);
            prepareAesthetics(builder, /*isPcct=*/ false);
            prepareMenuItems(builder);
            prepareActionButton(builder);
            if (session != null && mBottomToolbarCheckbox.isChecked()) prepareBottombar(builder);
            CustomTabsIntent customTabsIntent = builder.build();
            // NOTE: opening in incognito may be restricted. This assumes it is not.
            customTabsIntent.intent.putExtra(
                    "com.google.android.apps.chrome.EXTRA_OPEN_NEW_INCOGNITO_TAB",
                    viewId == R.id.launch_incognito_button);
            customTabsIntent.intent.putExtra(
                    "androidx.browser.customtabs.extra.CLOSE_BUTTON_POSITION", closeButtonPosition);
            configSessionConnection(session, customTabsIntent);
            customTabsIntent.launchUrl(this, Uri.parse(url));
        } else if (viewId == R.id.launch_pcct_button) {
            CustomTabsSession session = getSession();
            CustomTabsIntent.Builder builder = new CustomTabsIntent.Builder(session);
            prepareAesthetics(builder, /*isPcct=*/true);
            prepareMenuItems(builder);
            prepareActionButton(builder);
            CustomTabsIntent customTabsIntent = builder.build();
            configSessionConnection(session, customTabsIntent);
            customTabsIntent.intent.putExtra(
                    "androidx.browser.customtabs.extra.CLOSE_BUTTON_POSITION", closeButtonPosition);
            int toolbarCornerRadiusDp = mToolbarCornerRadiusSlider.getProgress();
            int toolbarCornerRadiusPx =
                    Math.round(toolbarCornerRadiusDp * getResources().getDisplayMetrics().density);
            customTabsIntent.intent.putExtra(
                    "androidx.browser.customtabs.extra.TOOLBAR_CORNER_RADIUS_IN_PIXEL",
                    toolbarCornerRadiusPx);

            int pcctInitialHeightPx = mPcctInitialHeightSlider.getProgress();

            if (pcctInitialHeightPx != 0) {
                customTabsIntent.intent.putExtra(
                        "androidx.browser.customtabs.extra.INITIAL_ACTIVITY_HEIGHT_IN_PIXEL",
                        pcctInitialHeightPx);
            }

            if (!mPcctResizableCheckbox.isChecked()) {
                customTabsIntent.intent.putExtra(
                        "androidx.browser.customtabs.extra.ACTIVITY_RESIZE_BEHAVIOR",
                        ACTIVITY_HEIGHT_FIXED);
            }

            customTabsIntent.launchUrl(this, Uri.parse(url));
        }
    }

    private void prepareAesthetics(CustomTabsIntent.Builder builder, boolean isPcct) {
        boolean urlHiding = mUrlHidingCheckbox.isChecked();
        boolean showTitle = mShowTitleCheckbox.isChecked();
        int closeButton = mCloseButtonIcon.getCheckedButtonId();
        int colorScheme = CustomTabsIntent.COLOR_SCHEME_SYSTEM;
        if (mThemeButton.getCheckedButtonId() == R.id.light_button) {
            colorScheme = CustomTabsIntent.COLOR_SCHEME_LIGHT;
        }
        if (mThemeButton.getCheckedButtonId() == R.id.dark_button) {
            colorScheme = CustomTabsIntent.COLOR_SCHEME_DARK;
        }

        if (!TextUtils.isEmpty(mToolbarColor)) {
            builder.setToolbarColor(Color.parseColor(mToolbarColor));
        }
        builder.setShowTitle(showTitle);
        builder.setColorScheme(colorScheme);
        builder.setUrlBarHidingEnabled(urlHiding);
        if (isPcct) {
            builder.setStartAnimations(this, R.anim.slide_in_up, R.anim.slide_out_bottom);
            builder.setExitAnimations(this, R.anim.slide_in_bottom, R.anim.slide_out_up);
        } else {
            builder.setStartAnimations(this, R.anim.slide_in_right, R.anim.slide_out_left);
            builder.setExitAnimations(this, R.anim.slide_in_left, R.anim.slide_out_right);
        }

        if (closeButton == R.id.check_button) {
            builder.setCloseButtonIcon(
                    BitmapFactory.decodeResource(getResources(), R.drawable.baseline_check_white));
        } else if (closeButton == R.id.back_button) {
            builder.setCloseButtonIcon(
                    BitmapFactory.decodeResource(getResources(), R.drawable.ic_arrow_back));
        } else {
            builder.setCloseButtonIcon(
                    BitmapFactory.decodeResource(getResources(), R.drawable.baseline_close_white));
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
        Bitmap icon = BitmapFactory.decodeResource(getResources(), R.drawable.baseline_send_white);
        builder.setActionButton(icon, "send email", pi, true);
    }

    private void prepareBottombar(CustomTabsIntent.Builder builder) {
        BottomBarManager.setMediaPlayer(mMediaPlayer);
        builder.setSecondaryToolbarColor(Color.parseColor(mToolbarColor));
        builder.setSecondaryToolbarViews(BottomBarManager.createRemoteViews(this, true),
                BottomBarManager.getClickableIDs(), BottomBarManager.getOnClickPendingIntent(this));
    }

    private void configSessionConnection(
            CustomTabsSession session, CustomTabsIntent customTabsIntent) {
        if (session != null) {
            CustomTabsHelper.addKeepAliveExtra(this, customTabsIntent.intent);
        } else {
            if (!TextUtils.isEmpty(mPackageNameToBind)) {
                customTabsIntent.intent.setPackage(mPackageNameToBind);
            }
        }
    }

    @Override
    public void onServiceConnected(CustomTabsClient client) {
        mClient = client;
        mConnectButton.setEnabled(false);
        mWarmupButton.setEnabled(true);
        mLaunchButton.setEnabled(true);
        mLaunchIncognitoButton.setEnabled(true);
    }

    @Override
    public void onServiceDisconnected() {
        mConnectButton.setEnabled(true);
        mWarmupButton.setEnabled(false);
        mLaunchButton.setEnabled(false);
        mLaunchIncognitoButton.setEnabled(false);
        mClient = null;
    }

    private @Px int getMaximumPossibleHeight() {
        @Px
        int res = 0;
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
            Rect rect = this.getWindowManager().getMaximumWindowMetrics().getBounds();
            res = Math.max(rect.width(), rect.height());
        } else {
            DisplayMetrics displayMetrics = new DisplayMetrics();
            this.getWindowManager().getDefaultDisplay().getRealMetrics(displayMetrics);
            res = Math.max(displayMetrics.widthPixels, displayMetrics.heightPixels);
        }
        return res;
    }
}
