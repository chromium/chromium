// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.customtabsclient;

import android.app.ActivityManager;
import android.app.ActivityOptions;
import android.app.PendingIntent;
import android.content.Context;
import android.content.Intent;
import android.content.SharedPreferences;
import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.graphics.Color;
import android.graphics.Insets;
import android.graphics.Point;
import android.graphics.Rect;
import android.media.MediaPlayer;
import android.net.Uri;
import android.os.Build;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.os.RemoteException;
import android.text.TextUtils;
import android.util.DisplayMetrics;
import android.util.Log;
import android.util.Pair;
import android.view.Display;
import android.view.LayoutInflater;
import android.view.View;
import android.view.View.OnClickListener;
import android.view.ViewGroup;
import android.view.WindowInsets;
import android.view.WindowMetrics;
import android.widget.AdapterView;
import android.widget.ArrayAdapter;
import android.widget.AutoCompleteTextView;
import android.widget.Button;
import android.widget.CheckBox;
import android.widget.SeekBar;
import android.widget.SeekBar.OnSeekBarChangeListener;
import android.widget.Spinner;
import android.widget.TextView;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.Px;
import androidx.appcompat.app.AppCompatActivity;
import androidx.browser.customtabs.CustomTabsCallback;
import androidx.browser.customtabs.CustomTabsClient;
import androidx.browser.customtabs.CustomTabsIntent;
import androidx.browser.customtabs.CustomTabsServiceConnection;
import androidx.browser.customtabs.CustomTabsSession;
import androidx.browser.customtabs.EngagementSignalsCallback;

import com.google.android.material.button.MaterialButtonToggleGroup;

import org.chromium.customtabsclient.shared.CustomTabsHelper;
import org.chromium.customtabsclient.shared.ServiceConnection;
import org.chromium.customtabsclient.shared.ServiceConnectionCallback;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.HashSet;
import java.util.List;

/**
 * Example client activity for using Chrome Custom Tabs.
 */
public class MainActivity
        extends AppCompatActivity implements OnClickListener, ServiceConnectionCallback {
    private static final String TAG = "CustomTabsClientExample";
    private static final String DEFAULT_URL = "https://www.google.com";
    private static final String SHARED_PREF_BACKGROUND_INTERACT = "BackgroundInteract";
    private static final String SHARED_PREF_BOTTOM_TOOLBAR = "BottomToolbar";
    private static final String SHARED_PREF_CCT = "Cct";
    private static final String SHARED_PREF_CLOSE_ICON = "CloseIcon";
    private static final String SHARED_PREF_CLOSE_POSITION = "ClosePosition";
    private static final String SHARED_PREF_SIDE_SHEET_POSITION = "SideSheetPosition";
    private static final String SHARED_PREF_SIDE_SHEET_ANIMATION = "SideSheetAnimation";
    private static final String SHARED_PREF_COLOR = "Color";
    private static final String SHARED_PREF_DECORATION = "Decoration";
    private static final String SHARED_PREF_HEIGHT = "Height";
    private static final String SHARED_PREF_WIDTH = "Width";
    private static final String SHARED_PREF_BREAKPOINT = "Breakpoint";
    private static final String SHARED_PREF_PROGRESS = "Progress";
    private static final String SHARED_PREF_HEIGHT_RESIZABLE = "HeightResizable";
    private static final String SHARED_PREF_SITES = "Sites";
    private static final String SHARED_PREF_SHOW_TITLE = "ShowTitle";
    private static final String SHARED_PREF_THEME = "Theme";
    private static final String SHARED_PREF_URL_HIDING = "UrlHiding";
    private static final String SHARED_PREF_FORCE_ENGAGEMENT_SIGNALS = "ForceEngagementSignals";
    private static final String SHARED_PREF_SIDE_SHEET_MAX_BUTTON = "SideSheetMaxButton";
    private static final String SHARED_PREF_SIDE_SHEET_ROUNDED_CORNER = "RoundedCorner";
    private static final int CLOSE_ICON_X = 0;
    private static final int CLOSE_ICON_BACK = 1;
    private static final int CLOSE_ICON_CHECK = 2;
    private static final int UNCHECKED = 0;
    private static final int CHECKED = 1;
    private static final int ACTIVITY_HEIGHT_FIXED = 2;
    private static final int BACKGROUND_INTERACT_OFF_VALUE = 2;

    /** Extra that enables the maximization button on the side sheet Custom Tab toolbar. */
    public static final String EXTRA_ACTIVITY_SIDE_SHEET_ENABLE_MAXIMIZATION =
            "androidx.browser.customtabs.extra.ACTIVITY_SIDE_SHEET_ENABLE_MAXIMIZATION";

    /**
     * Minimal height the bottom sheet CCT should show is half of the display height.
     */
    private static final float MINIMAL_HEIGHT_RATIO = 0.5f;
    /**
     * Minimal height the side sheet CCT should show is a third of the display height.
     */
    private static final float MINIMAL_WIDTH_RATIO = 0.33f;
    private static final int DEFAULT_BREAKPOINT = 840;
    private AutoCompleteTextView mEditUrl;
    private CustomTabsSession mCustomTabsSession;
    private CustomTabsClient mClient;
    private CustomTabsServiceConnection mConnection;
    private String mPackageNameToBind;
    private String mPackageTitle;
    private String mToolbarColor;
    private String mColorName;
    private String mCctType;
    private Button mConnectButton;
    private Button mDisconnectButton;
    private Button mMayLaunchButton;
    private Button mWarmupButton;
    private Button mLaunchButton;
    private MediaPlayer mMediaPlayer;
    private MaterialButtonToggleGroup mCloseButtonPositionToggle;
    private MaterialButtonToggleGroup mCloseButtonIcon;
    private MaterialButtonToggleGroup mDecorationType;
    private MaterialButtonToggleGroup mThemeButton;
    private MaterialButtonToggleGroup mSideSheetPositionToggle;
    private MaterialButtonToggleGroup mSideSheetAnimationToggle;
    private TextView mToolbarCornerRadiusLabel;
    private SeekBar mToolbarCornerRadiusSlider;
    private CheckBox mBottomToolbarCheckbox;
    private CheckBox mPcctHeightResizableCheckbox;
    private CheckBox mShowTitleCheckbox;
    private CheckBox mUrlHidingCheckbox;
    private CheckBox mBackgroundInteractCheckbox;
    private CheckBox mForceEngagementSignalsCheckbox;
    private CheckBox mSideSheetMaxButtonCheckbox;
    private CheckBox mSideSheetRoundedCornerCheckbox;
    private TextView mPcctBreakpointLabel;
    private SeekBar mPcctBreakpointSlider;
    private TextView mPcctInitialHeightLabel;
    private SeekBar mPcctInitialHeightSlider;
    private SeekBar mPcctInitialWidthSlider;
    private TextView mPcctInitialWidthLabel;
    private SharedPreferences mSharedPref;
    private CustomTabsPackageHelper mCustomTabsPackageHelper;
    private @Px int mMaxHeight;
    private @Px int mInitialHeight;
    private @Px int mMaxWidth;
    private @Px int mInitialWidth;

    public static final int ACTIVITY_SIDE_SHEET_POSITION_DEFAULT = 0;
    public static final int ACTIVITY_SIDE_SHEET_POSITION_START = 1;
    public static final int ACTIVITY_SIDE_SHEET_POSITION_END = 2;

    public static final String EXTRA_ACTIVITY_SIDE_SHEET_POSITION =
            "androidx.browser.customtabs.extra.ACTIVITY_SIDE_SHEET_POSITION";

    public static final int ACTIVITY_SIDE_SHEET_DECORATION_TYPE_DEFAULT = 0;
    public static final int ACTIVITY_SIDE_SHEET_DECORATION_TYPE_NONE = 1;
    public static final int ACTIVITY_SIDE_SHEET_DECORATION_TYPE_SHADOW = 2;
    public static final int ACTIVITY_SIDE_SHEET_DECORATION_TYPE_DIVIDER = 3;

    public static final String EXTRA_ACTIVITY_SIDE_SHEEET_DECORATION_TYPE =
            "androidx.browser.customtabs.extra.ACTIVITY_SIDE_SHEET_DECORATION_TYPE";

    public static final int ACTIVITY_SIDE_SHEET_SLIDE_IN_DEFAULT = 0;
    public static final int ACTIVITY_SIDE_SHEET_SLIDE_IN_FROM_BOTTOM = 1;
    public static final int ACTIVITY_SIDE_SHEET_SLIDE_IN_FROM_SIDE = 2;

    public static final String EXTRA_ACTIVITY_SIDE_SHEET_SLIDE_IN_BEHAVIOR =
            "androidx.browser.customtabs.extra.ACTIVITY_SIDE_SHEET_SLIDE_IN_BEHAVIOR";

    public static final int ACTIVITY_SIDE_SHEET_ROUNDED_CORNERS_DEFAULT = 0;
    public static final int ACTIVITY_SIDE_SHEET_ROUNDED_CORNERS_NONE = 1;
    public static final int ACTIVITY_SIDE_SHEET_ROUNDED_CORNERS_TOP = 2;

    public static final String EXTRA_ACTIVITY_SIDE_SHEET_ROUNDED_CORNERS_POSITION =
            "androidx.browser.customtabs.extra.ACTIVITY_SIDE_SHEET_ROUNDED_CORNERS_POSITION";

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
        public void onActivityResized(int height, int width, Bundle extras) {
            Log.w(TAG, "onActivityResized: height = " + height + " width: " + width);
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
                // CustomTabsConnection#ON_RESIZED_CALLBACK
            } else if (callbackName.equals("onResized")) {
                // CustomTabsConnection#ON_RESIZED_SIZE_EXTRA
                Log.w(TAG, "onResized: size = " + args.getInt("size"));
                // CustomTabsConnection#ON_ACTIVITY_LAYOUT_CALLBACK
            } else if (callbackName.equals("onActivityLayout")) {
                Log.w(TAG,
                        "onActivityLayout:"
                                // CustomTabsConnection#ON_ACTIVITY_LAYOUT_LEFT_EXTRA
                                + " left = "
                                + args.getInt("left")
                                // CustomTabsConnection#ON_ACTIVITY_LAYOUT_TOP_EXTRA
                                + " top = "
                                + args.getInt("top")
                                // CustomTabsConnection#ON_ACTIVITY_LAYOUT_RIGHT_EXTRA
                                + " right = "
                                + args.getInt("right")
                                // CustomTabsConnection#ON_ACTIVITY_LAYOUT_BOTTOM_EXTRA
                                + " bottom = "
                                + args.getInt("bottom")
                                // CustomTabsConnection#ON_ACTIVITY_LAYOUT_STATE_EXTRA
                                + " state = " + args.getInt("state"));
            }
        }
    }

    private class EngagementCallback implements EngagementSignalsCallback {
        @Override
        public void onVerticalScrollEvent(boolean isDirectionUp, Bundle extras) {
            Log.w(TAG,
                    "EngagementSignalsCallback#onVerticalScrollEvent: isDirectionUp = "
                            + isDirectionUp);
        }

        @Override
        public void onGreatestScrollPercentageIncreased(int scrollPercentage, Bundle extras) {
            Log.w(TAG,
                    "EngagementSignalsCallback#onGreatestScrollPercentageIncreased: "
                            + "scrollPercentage = " + scrollPercentage);
        }

        @Override
        public void onSessionEnded(boolean didUserInteract, Bundle extras) {
            Log.w(TAG,
                    "EngagementSignalsCallback#onSessionEnded: didUserInteract = "
                            + didUserInteract);
        }
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.main);
        mSharedPref = getPreferences(Context.MODE_PRIVATE);
        mMediaPlayer = MediaPlayer.create(this, R.raw.amazing_grace);
        mCustomTabsPackageHelper = new CustomTabsPackageHelper(this, mSharedPref);
        initializeUrlEditTextView();
        initializePackageSpinner();
        initializeColorSpinner();
        initializeToggles();
        initializeCornerRadiusSlider();
        initializeHeightSlider();
        initializeWidthSlider();
        initializeBreakpointSlider();
        initializeCheckBoxes();
        initializeCctSpinner();
        initializeButtons();
        mLogImportance.run();
    }

    private void initializeUrlEditTextView() {
        // Populate the dropdown menu with most recently used URLs up to 5.
        String recent = "";
        ArrayList<String> urlsDropdown = new ArrayList<>();
        HashSet<String> stringSet = (HashSet<String>) mSharedPref.getStringSet(SHARED_PREF_SITES, null);
        if (stringSet != null) {
            for (String site : stringSet) {
                // We use prefixes with numbers on the StringSet in order to track the ordering
                if (site.charAt(0) == '1') {
                    recent = site.substring(1);
                } else {
                    urlsDropdown.add(site.substring(1));
                }
            }
        }

        mEditUrl = findViewById(R.id.autocomplete_url);
        mEditUrl.setText(urlsDropdown.size() > 0 ? recent : DEFAULT_URL);
        mEditUrl.requestFocus();
        ArrayAdapter<String> adapter =
                new ArrayAdapter<>(this, android.R.layout.simple_list_item_1, urlsDropdown);
        mEditUrl.setAdapter(adapter);
        mEditUrl.setOnClickListener(v -> mEditUrl.showDropDown());
    }

    private void initializePackageSpinner() {
        Spinner packageSpinner = findViewById(R.id.package_spinner);
        List<Pair<String, String>> packagesSupportingCustomTabs =
                mCustomTabsPackageHelper.getCustomTabsSupportingPackages();
        ArrayAdapter<Pair<String, String>> adapter = new ArrayAdapter<>(
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

        packageSpinner.setAdapter(adapter);
        packageSpinner.setOnItemSelectedListener(new AdapterView.OnItemSelectedListener() {
            @Override
            public void onItemSelected(AdapterView<?> parent, View view, int position, long id) {
                Pair<String, String> item = adapter.getItem(position);
                if (TextUtils.isEmpty(item.second)) {
                    onNothingSelected(parent);
                    return;
                }
                mPackageTitle = item.first;
                mPackageNameToBind = item.second;
            }

            @Override
            public void onNothingSelected(AdapterView<?> parent) {
                mPackageNameToBind = null;
            }
        });
    }

    private void initializeColorSpinner() {
        Spinner colorSpinner = (Spinner) findViewById(R.id.color_spinner);
        HashMap<String, String> colors = new HashMap<String, String>();
        colors.put("Default", "");
        colors.put("Orange", "#ef6c00");
        colors.put("Red", "#c63d3c");
        colors.put("Green", "#369f3d");
        colors.put("Blue", "#3d3bad");

        // Check if there is a saved color preference which needs to be moved to the default/0 position
        String prefColor = mSharedPref.getString(SHARED_PREF_COLOR, "");
        String[] colorsArr = colors.keySet().toArray(new String[0]);
        for (int i = 0; i < colorsArr.length; i++) {
            if (colorsArr[i].equals(prefColor)) {
                colorsArr[i] = colorsArr[0];
                colorsArr[0] = prefColor;
                break;
            }
        }

        final ArrayAdapter<String> colorAdapter = new ArrayAdapter<String>(this, 0, colorsArr) {
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
        colorSpinner.setAdapter(colorAdapter);
        colorSpinner.setOnItemSelectedListener(new AdapterView.OnItemSelectedListener() {
            @Override
            public void onItemSelected(AdapterView<?> parent, View view, int position, long id) {
                String item = colorAdapter.getItem(position);
                if (TextUtils.isEmpty(item)) {
                    onNothingSelected(parent);
                    return;
                }
                mColorName = item;
                mToolbarColor = colors.get(item);
            }

            @Override
            public void onNothingSelected(AdapterView<?> parent) {}
        });
    }

    private void initializeToggles() {
        mThemeButton = findViewById(R.id.theme_toggle);
        if (mSharedPref.getInt(SHARED_PREF_THEME, CustomTabsIntent.COLOR_SCHEME_SYSTEM)
                == CustomTabsIntent.COLOR_SCHEME_SYSTEM) {
            mThemeButton.check(R.id.system_button);
        } else if (mSharedPref.getInt(SHARED_PREF_THEME, -1)
                == CustomTabsIntent.COLOR_SCHEME_LIGHT) {
            mThemeButton.check(R.id.light_button);
        } else {
            mThemeButton.check(R.id.dark_button);
        }

        mCloseButtonPositionToggle = findViewById(R.id.close_button_position_toggle);
        int buttonType = mSharedPref.getInt(SHARED_PREF_CLOSE_POSITION,
                CustomTabsIntent.CLOSE_BUTTON_POSITION_START)
                == CustomTabsIntent.CLOSE_BUTTON_POSITION_START ? R.id.start_button : R.id.end_button;
        mCloseButtonPositionToggle.check(buttonType);

        mCloseButtonIcon = findViewById(R.id.close_button_icon_toggle);
        if (mSharedPref.getInt(SHARED_PREF_CLOSE_ICON, CLOSE_ICON_X) == CLOSE_ICON_X) {
            mCloseButtonIcon.check(R.id.x_button);
        } else if (mSharedPref.getInt(SHARED_PREF_CLOSE_ICON, -1) == CLOSE_ICON_BACK) {
            mCloseButtonIcon.check(R.id.back_button);
        } else {
            mCloseButtonIcon.check(R.id.check_button);
        }

        mSideSheetPositionToggle = findViewById(R.id.side_sheet_position_toggle);
        int sideSheetPositionType = mSharedPref.getInt(SHARED_PREF_SIDE_SHEET_POSITION,
                                            ACTIVITY_SIDE_SHEET_POSITION_END)
                        == ACTIVITY_SIDE_SHEET_POSITION_START
                ? R.id.side_sheet_start_button
                : R.id.side_sheet_end_button;
        mSideSheetPositionToggle.check(sideSheetPositionType);
        mSideSheetAnimationToggle = findViewById(R.id.side_sheet_animation_toggle);
        int sideSheetAnimationType = mSharedPref.getInt(SHARED_PREF_SIDE_SHEET_ANIMATION,
                                             ACTIVITY_SIDE_SHEET_SLIDE_IN_FROM_SIDE)
                        == ACTIVITY_SIDE_SHEET_SLIDE_IN_FROM_SIDE
                ? R.id.side_sheet_side_button
                : R.id.side_sheet_bottom_button;
        mSideSheetAnimationToggle.check(sideSheetAnimationType);

        mDecorationType = findViewById(R.id.decoration_type_toggle);
        if (mSharedPref.getInt(SHARED_PREF_DECORATION, ACTIVITY_SIDE_SHEET_DECORATION_TYPE_SHADOW)
                == ACTIVITY_SIDE_SHEET_DECORATION_TYPE_SHADOW) {
            mDecorationType.check(R.id.decoration_type_shadow_button);
        } else if (mSharedPref.getInt(
                           SHARED_PREF_DECORATION, ACTIVITY_SIDE_SHEET_DECORATION_TYPE_SHADOW)
                == ACTIVITY_SIDE_SHEET_DECORATION_TYPE_DIVIDER) {
            mDecorationType.check(R.id.decoration_type_divider_button);
        } else {
            mDecorationType.check(R.id.decoration_type_none_button);
        }
    }

    private void initializeCornerRadiusSlider() {
        mToolbarCornerRadiusLabel = findViewById(R.id.corner_radius_slider_label);
        mToolbarCornerRadiusSlider = findViewById(R.id.corner_radius_slider);
        int savedProgress = mSharedPref.getInt(SHARED_PREF_PROGRESS, -1);
        if (savedProgress != -1) mToolbarCornerRadiusSlider.setProgress(savedProgress);

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
        mPcctHeightResizableCheckbox = findViewById(R.id.pcct_height_resizable_checkbox);
        mPcctHeightResizableCheckbox.setChecked(
                mSharedPref.getInt(SHARED_PREF_HEIGHT_RESIZABLE, CHECKED) == CHECKED);
        mBottomToolbarCheckbox = findViewById(R.id.bottom_toolbar_checkbox);
        mBottomToolbarCheckbox.setChecked(
                mSharedPref.getInt(SHARED_PREF_BOTTOM_TOOLBAR, UNCHECKED) == CHECKED);
        mShowTitleCheckbox = findViewById(R.id.show_title_checkbox);
        mShowTitleCheckbox.setChecked(mSharedPref.getInt(SHARED_PREF_SHOW_TITLE, CHECKED) == CHECKED);
        mUrlHidingCheckbox = findViewById(R.id.url_hiding_checkbox);
        mUrlHidingCheckbox.setChecked(mSharedPref.getInt(SHARED_PREF_URL_HIDING, CHECKED) == CHECKED);
        mBackgroundInteractCheckbox = findViewById(R.id.background_interact_checkbox);
        mBackgroundInteractCheckbox.setChecked(
                mSharedPref.getInt(SHARED_PREF_BACKGROUND_INTERACT, CHECKED) == CHECKED);
        mForceEngagementSignalsCheckbox = findViewById(R.id.force_engagement_signals_checkbox);
        mForceEngagementSignalsCheckbox.setChecked(
                mSharedPref.getInt(SHARED_PREF_FORCE_ENGAGEMENT_SIGNALS, CHECKED) == CHECKED);
        mSideSheetMaxButtonCheckbox = findViewById(R.id.side_sheet_max_button_checkbox);
        mSideSheetMaxButtonCheckbox.setChecked(
                mSharedPref.getInt(SHARED_PREF_SIDE_SHEET_MAX_BUTTON, CHECKED) == CHECKED);
        mSideSheetRoundedCornerCheckbox = findViewById(R.id.side_sheet_rounded_corner_checkbox);
        mSideSheetRoundedCornerCheckbox.setChecked(
                mSharedPref.getInt(SHARED_PREF_SIDE_SHEET_ROUNDED_CORNER, CHECKED) == CHECKED);
    }

    private void initializeCctSpinner() {
        Spinner cctSpinner = (Spinner) findViewById(R.id.cct_spinner);
        String[] cctOptions = new String[] {"CCT", "Partial CCT", "Incognito CCT"};
        String prefCct = mSharedPref.getString(SHARED_PREF_CCT, "");
        for (int i = 0; i < cctOptions.length; i++) {
            if (cctOptions[i].equals(prefCct)) {
                cctOptions[i] = cctOptions[0];
                cctOptions[0] = prefCct;
                break;
            }
        }

        final ArrayAdapter<String> cctAdapter = new ArrayAdapter<String>(this, 0, cctOptions) {
            @Override
            public View getView(int position, View convertView, ViewGroup parent) {
                View view = convertView;
                if (view == null) {
                    view = LayoutInflater.from(MainActivity.this)
                                   .inflate(android.R.layout.simple_list_item_2, parent, false);
                }
                ((TextView) view.findViewById(android.R.id.text1)).setText(getItem(position));
                return view;
            }

            @Override
            public View getDropDownView(int position, View convertView, ViewGroup parent) {
                return getView(position, convertView, parent);
            }
        };
        cctSpinner.setAdapter(cctAdapter);
        cctSpinner.setOnItemSelectedListener(new AdapterView.OnItemSelectedListener() {
            @Override
            public void onItemSelected(AdapterView<?> parent, View view, int position, long id) {
                String item = cctAdapter.getItem(position);
                if (TextUtils.isEmpty(item)) {
                    onNothingSelected(parent);
                    return;
                }
                mCctType = item;
                if (mDisconnectButton.isEnabled()) {
                    unbindCustomTabsService();
                }
            }

            @Override
            public void onNothingSelected(AdapterView<?> parent) {}
        });
    }

    private void initializeButtons() {
        mConnectButton = (Button) findViewById(R.id.connect_button);
        mDisconnectButton = (Button) findViewById(R.id.disconnect_button);
        mWarmupButton = (Button) findViewById(R.id.warmup_button);
        mMayLaunchButton = (Button) findViewById(R.id.may_launch_button);
        mLaunchButton = (Button) findViewById(R.id.launch_button);
        mConnectButton.setOnClickListener(this);
        mDisconnectButton.setOnClickListener(this);
        mWarmupButton.setOnClickListener(this);
        mMayLaunchButton.setOnClickListener(this);
        mLaunchButton.setOnClickListener(this);
        findViewById(R.id.test_asm_button).setOnClickListener(this);
    }

    private void initializeHeightSlider() {
        mMaxHeight = getMaximumPossibleSizePx();
        mInitialHeight = (int) (mMaxHeight * MINIMAL_HEIGHT_RATIO);
        mPcctInitialHeightSlider = findViewById(R.id.pcct_initial_height_slider);
        mPcctInitialHeightLabel = findViewById(R.id.pcct_initial_height_slider_label);
        mPcctInitialHeightSlider.setMax(mMaxHeight);
        int sharedHeight = mSharedPref.getInt(SHARED_PREF_HEIGHT, -1);
        mPcctInitialHeightSlider.setProgress(sharedHeight != -1 ? sharedHeight : mInitialHeight);
        mPcctInitialHeightLabel.setText(
                getString(R.string.px_template, mPcctInitialHeightSlider.getProgress()));
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

    private void initializeWidthSlider() {
        mMaxWidth = getMaximumPossibleSizePx();
        mInitialWidth = (int) (mMaxWidth * MINIMAL_WIDTH_RATIO);
        mPcctInitialWidthSlider = findViewById(R.id.pcct_initial_width_slider);
        mPcctInitialWidthLabel = findViewById(R.id.pcct_initial_width_slider_label);
        mPcctInitialWidthSlider.setMax(mMaxWidth);
        int sharedWidth = mSharedPref.getInt(SHARED_PREF_WIDTH, -1);
        mPcctInitialWidthSlider.setProgress(sharedWidth != -1 ? sharedWidth : mInitialWidth);
        mPcctInitialWidthLabel.setText(
                getString(R.string.px_template, mPcctInitialWidthSlider.getProgress()));
        mPcctInitialWidthSlider.setOnSeekBarChangeListener(new OnSeekBarChangeListener() {
            @Override
            public void onProgressChanged(SeekBar seekBar, int progress, boolean fromUser) {
                mPcctInitialWidthLabel.setText(getString(R.string.px_template, progress));
            }

            @Override
            public void onStartTrackingTouch(SeekBar seekBar) {}

            @Override
            public void onStopTrackingTouch(SeekBar seekBar) {}
        });
    }

    private void initializeBreakpointSlider() {
        int maxBreakpointDp =
                (int) (getMaximumPossibleSizePx() / getResources().getDisplayMetrics().density);
        mPcctBreakpointSlider = findViewById(R.id.pcct_breakpoint_slider);
        mPcctBreakpointLabel = findViewById(R.id.pcct_breakpoint_slider_label);
        mPcctBreakpointSlider.setMax(maxBreakpointDp);
        int sharedBreakpoint = mSharedPref.getInt(SHARED_PREF_BREAKPOINT, -1);
        mPcctBreakpointSlider.setProgress(
                sharedBreakpoint != -1 ? sharedBreakpoint : DEFAULT_BREAKPOINT);
        mPcctBreakpointLabel.setText(
                getString(R.string.dp_template, mPcctBreakpointSlider.getProgress()));
        mPcctBreakpointSlider.setOnSeekBarChangeListener(new OnSeekBarChangeListener() {
            @Override
            public void onProgressChanged(SeekBar seekBar, int progress, boolean fromUser) {
                mPcctBreakpointLabel.setText(getString(R.string.dp_template, progress));
            }

            @Override
            public void onStartTrackingTouch(SeekBar seekBar) {}

            @Override
            public void onStopTrackingTouch(SeekBar seekBar) {}
        });
    }

    @Override
    protected void onDestroy() {
        mMediaPlayer.release();
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
        mCustomTabsPackageHelper.saveLastUsedPackage(mPackageTitle);
        if (ok) {
            mConnectButton.setEnabled(false);
            mWarmupButton.setEnabled(true);
        } else {
            mConnection = null;
        }
    }

    private void unbindCustomTabsService() {
        initializeUrlEditTextView();
        if (mConnection == null) return;

        unbindService(mConnection);
        mClient = null;
        mCustomTabsSession = null;
        mConnectButton.setEnabled(true);
        mDisconnectButton.setEnabled(false);
        mWarmupButton.setEnabled(false);
    }

    @Override
    public void onClick(View v) {
        String url = mEditUrl.getText().toString();
        HashSet<String> savedUrlSet;

        int viewId = v.getId();
        SharedPreferences.Editor editor = mSharedPref.edit();

        if (viewId == R.id.connect_button) {
            if (mSharedPref.getStringSet(SHARED_PREF_SITES, null) != null) {
                savedUrlSet = (HashSet<String>) mSharedPref.getStringSet(SHARED_PREF_SITES, null);
                boolean duplicate = false;
                for (String s : savedUrlSet) {
                    if (s.substring(1).equals(url)) {
                        duplicate = true;
                    }
                }
                if (!duplicate) {
                    String[] savedUrlArr = savedUrlSet.toArray(new String[5]);
                    if (!TextUtils.isEmpty(url)) {
                        // Populate new entry into array - 3 steps:
                        //  1. Found empty spot in array, just add new url and STOP
                        //  2. Array full, replace oldest entry with newest
                        //  3. Increment position of other entries
                        for (int i = 0; i < savedUrlArr.length; i++) {
                            if (savedUrlArr[i] == null) {
                                savedUrlArr[i] = "1" + url;
                                break;
                            } else if (savedUrlArr[i].substring(0, 1).equals("5")) {
                                savedUrlArr[i] = "1" + url;
                            } else {
                                int position = Integer.parseInt(savedUrlArr[i].substring(0, 1));
                                savedUrlArr[i] = (position + 1) + savedUrlArr[i].substring(1);
                            }
                        }
                        savedUrlSet.clear();
                        for (String entry : savedUrlArr) {
                            if (entry != null) savedUrlSet.add(entry);
                        }
                        editor.remove(SHARED_PREF_SITES);
                        editor.apply();
                        editor.putStringSet(SHARED_PREF_SITES, savedUrlSet);
                        editor.apply();
                    }
                }
            } else {
                // TODO(1369795) Refactor the way ordering is stored so it's not mixed with URLs
                savedUrlSet = new HashSet<String>();
                if (!TextUtils.isEmpty(url)) {
                    savedUrlSet.add("1" + url);
                    editor.putStringSet(SHARED_PREF_SITES, savedUrlSet);
                    editor.apply();
                }
            }
            bindCustomTabsService();
        } else if (viewId == R.id.disconnect_button) {
            unbindCustomTabsService();
        } else if (viewId == R.id.warmup_button) {
            boolean success = false;
            if (mClient != null) success = mClient.warmup(0);
            if (!success) mWarmupButton.setEnabled(false);
        } else if (viewId == R.id.may_launch_button) {
            CustomTabsSession session = getSession();
            boolean success = false;
            if (mClient != null) success = session.mayLaunchUrl(Uri.parse(url), null, null);
            if (!success) mMayLaunchButton.setEnabled(false);
        } else if (viewId == R.id.test_asm_button) {
            launchCct(url, editor);
            new Handler().postDelayed(() -> launchCct("https://abc.xyz", editor), 5000);
        } else if (viewId == R.id.launch_button) {
            launchCct(url, editor);
        }
    }

    private void launchCct(String url, SharedPreferences.Editor editor) {
        CustomTabsSession session = getSession();
        CustomTabsIntent.Builder builder = new CustomTabsIntent.Builder(session);
        prepareMenuItems(builder);
        prepareActionButton(builder);
        boolean isPCCT = mCctType.equals("Partial CCT");
        prepareAesthetics(builder, isPCCT);
        CustomTabsIntent customTabsIntent = builder.build();

        // @CloseButtonPosition
        int closeButtonPosition = mCloseButtonPositionToggle.getCheckedButtonId() == R.id.end_button
                ? CustomTabsIntent.CLOSE_BUTTON_POSITION_END
                : CustomTabsIntent.CLOSE_BUTTON_POSITION_START;
        int sideSheetPosition =
                mSideSheetPositionToggle.getCheckedButtonId() == R.id.side_sheet_end_button
                ? ACTIVITY_SIDE_SHEET_POSITION_END
                : ACTIVITY_SIDE_SHEET_POSITION_START;
        int sideSheetAnimation =
                mSideSheetAnimationToggle.getCheckedButtonId() == R.id.side_sheet_side_button
                ? ACTIVITY_SIDE_SHEET_SLIDE_IN_FROM_SIDE
                : ACTIVITY_SIDE_SHEET_SLIDE_IN_FROM_BOTTOM;
        int decorationType = ACTIVITY_SIDE_SHEET_DECORATION_TYPE_SHADOW;
        if (mDecorationType.getCheckedButtonId() == R.id.decoration_type_divider_button) {
            decorationType = ACTIVITY_SIDE_SHEET_DECORATION_TYPE_DIVIDER;
        } else if (mDecorationType.getCheckedButtonId() == R.id.decoration_type_none_button) {
            decorationType = ACTIVITY_SIDE_SHEET_DECORATION_TYPE_NONE;
        }

        if (isPCCT) {
            editor.putString(SHARED_PREF_CCT, "Partial CCT");
            int toolbarCornerRadiusDp = mToolbarCornerRadiusSlider.getProgress();
            int toolbarCornerRadiusPx =
                    Math.round(toolbarCornerRadiusDp * getResources().getDisplayMetrics().density);
            customTabsIntent.intent.putExtra(
                    "androidx.browser.customtabs.extra.CLOSE_BUTTON_POSITION", closeButtonPosition);
            customTabsIntent.intent.putExtra(
                    "androidx.browser.customtabs.extra.TOOLBAR_CORNER_RADIUS_IN_PIXEL",
                    toolbarCornerRadiusPx);
            int pcctInitialHeightPx = mPcctInitialHeightSlider.getProgress();
            if (pcctInitialHeightPx != 0) {
                customTabsIntent.intent.putExtra(
                        "androidx.browser.customtabs.extra.INITIAL_ACTIVITY_HEIGHT_IN_PIXEL",
                        pcctInitialHeightPx);
            }
            int pcctInitialWidthPx = mPcctInitialWidthSlider.getProgress();
            if (pcctInitialWidthPx != 0) {
                customTabsIntent.intent.putExtra(
                        "androidx.browser.customtabs.extra.INITIAL_ACTIVITY_WIDTH_PX",
                        pcctInitialWidthPx);
            }
            int pcctBreakpointDp = mPcctBreakpointSlider.getProgress();
            customTabsIntent.intent.putExtra(
                    "androidx.browser.customtabs.extra.ACTIVITY_SIDE_SHEET_BREAKPOINT_DP",
                    pcctBreakpointDp);
            if (mSideSheetMaxButtonCheckbox.isChecked()) {
                customTabsIntent.intent.putExtra(
                        EXTRA_ACTIVITY_SIDE_SHEET_ENABLE_MAXIMIZATION, true);
            }
            if (!mPcctHeightResizableCheckbox.isChecked()) {
                customTabsIntent.intent.putExtra(
                        "androidx.browser.customtabs.extra.ACTIVITY_HEIGHT_RESIZE_BEHAVIOR",
                        ACTIVITY_HEIGHT_FIXED);
            }
            if (!mBackgroundInteractCheckbox.isChecked()) {
                customTabsIntent.intent.putExtra(
                        "androidx.browser.customtabs.extra.ENABLE_BACKGROUND_INTERACTION",
                        BACKGROUND_INTERACT_OFF_VALUE);
            }
            if (mSideSheetRoundedCornerCheckbox.isChecked()) {
                customTabsIntent.intent.putExtra(EXTRA_ACTIVITY_SIDE_SHEET_ROUNDED_CORNERS_POSITION,
                        ACTIVITY_SIDE_SHEET_ROUNDED_CORNERS_TOP);
            }
            customTabsIntent.intent.putExtra(EXTRA_ACTIVITY_SIDE_SHEET_POSITION, sideSheetPosition);
            customTabsIntent.intent.putExtra(
                    EXTRA_ACTIVITY_SIDE_SHEET_SLIDE_IN_BEHAVIOR, sideSheetAnimation);
            customTabsIntent.intent.putExtra(
                    EXTRA_ACTIVITY_SIDE_SHEEET_DECORATION_TYPE, decorationType);
        } else {
            editor.putString(
                    SHARED_PREF_CCT, mCctType.equals("Incognito CCT") ? "Incognito CCT" : "CCT");
            if (session != null && mBottomToolbarCheckbox.isChecked()) {
                prepareBottombar(builder);
                Intent broadcastIntent = new Intent(this, BottomBarManager.SwipeUpReceiver.class);
                PendingIntent pi = PendingIntent.getBroadcast(
                        this, 0, broadcastIntent, PendingIntent.FLAG_MUTABLE);
                customTabsIntent.intent.putExtra(
                        "androidx.browser.customtabs.extra.SECONDARY_TOOLBAR_SWIPE_UP_ACTION", pi);
            }
            // NOTE: opening in incognito may be restricted. This assumes it is not.
            customTabsIntent.intent.putExtra(
                    "com.google.android.apps.chrome.EXTRA_OPEN_NEW_INCOGNITO_TAB",
                    mCctType.equals("Incognito CCT"));
            customTabsIntent.intent.putExtra(
                    "androidx.browser.customtabs.extra.CLOSE_BUTTON_POSITION", closeButtonPosition);
        }
        if (mForceEngagementSignalsCheckbox.isChecked()) {
            // NOTE: this may not work because this app is not a trusted 1st party app,
            // and CCT requires that for this feature currently.
            // Set the command-line-flag --cct-client-firstparty-override to fake 1st-party!
            customTabsIntent.intent.putStringArrayListExtra(
                    "org.chromium.chrome.browser.customtabs.EXPERIMENTS_ENABLE",
                    new ArrayList<String>(
                            List.of("CCTRealTimeEngagementSignals", "CCTBrandTransparency")));
        }
        configSessionConnection(session, customTabsIntent);
        customTabsIntent.launchUrl(this, Uri.parse(url));

        editor.putInt(SHARED_PREF_HEIGHT, mPcctInitialHeightSlider.getProgress());
        editor.putInt(SHARED_PREF_WIDTH, mPcctInitialWidthSlider.getProgress());
        editor.putInt(SHARED_PREF_BREAKPOINT, mPcctBreakpointSlider.getProgress());
        editor.putInt(SHARED_PREF_PROGRESS, mToolbarCornerRadiusSlider.getProgress());
        int toolbarCheck =
                session != null && mBottomToolbarCheckbox.isChecked() ? CHECKED : UNCHECKED;
        editor.putInt(SHARED_PREF_BOTTOM_TOOLBAR, toolbarCheck);
        editor.putInt(SHARED_PREF_CLOSE_POSITION, closeButtonPosition);
        editor.putInt(SHARED_PREF_SIDE_SHEET_POSITION, sideSheetPosition);
        editor.putInt(SHARED_PREF_SIDE_SHEET_ANIMATION, sideSheetAnimation);
        editor.putInt(SHARED_PREF_HEIGHT_RESIZABLE,
                mPcctHeightResizableCheckbox.isChecked() ? CHECKED : UNCHECKED);
        editor.putInt(SHARED_PREF_SIDE_SHEET_MAX_BUTTON,
                mSideSheetMaxButtonCheckbox.isChecked() ? CHECKED : UNCHECKED);
        editor.putInt(SHARED_PREF_SIDE_SHEET_ROUNDED_CORNER,
                mSideSheetRoundedCornerCheckbox.isChecked() ? CHECKED : UNCHECKED);
        editor.putInt(SHARED_PREF_DECORATION, decorationType);
        editor.apply();
    }

    private void prepareAesthetics(CustomTabsIntent.Builder builder, boolean isPcct) {
        SharedPreferences.Editor editor = mSharedPref.edit();
        boolean urlHiding = mUrlHidingCheckbox.isChecked();
        if (urlHiding) {
            editor.putInt(SHARED_PREF_URL_HIDING, CHECKED);
        } else {
            editor.putInt(SHARED_PREF_URL_HIDING, UNCHECKED);
        }
        if (mForceEngagementSignalsCheckbox.isChecked()) {
            editor.putInt(SHARED_PREF_FORCE_ENGAGEMENT_SIGNALS, CHECKED);
        } else {
            editor.putInt(SHARED_PREF_FORCE_ENGAGEMENT_SIGNALS, UNCHECKED);
        }
        boolean backgroundInteract = mBackgroundInteractCheckbox.isChecked();
        if (backgroundInteract) {
            editor.putInt(SHARED_PREF_BACKGROUND_INTERACT, CHECKED);
        } else {
            editor.putInt(SHARED_PREF_BACKGROUND_INTERACT, UNCHECKED);
        }
        boolean showTitle = mShowTitleCheckbox.isChecked();
        if (showTitle) {
            editor.putInt(SHARED_PREF_SHOW_TITLE, CHECKED);
        } else {
            editor.putInt(SHARED_PREF_SHOW_TITLE, UNCHECKED);
        }
        int closeButton = mCloseButtonIcon.getCheckedButtonId();
        int colorScheme = CustomTabsIntent.COLOR_SCHEME_SYSTEM;
        if (mThemeButton.getCheckedButtonId() == R.id.light_button) {
            colorScheme = CustomTabsIntent.COLOR_SCHEME_LIGHT;
            editor.putInt(SHARED_PREF_THEME, CustomTabsIntent.COLOR_SCHEME_LIGHT);
        } else if (mThemeButton.getCheckedButtonId() == R.id.dark_button) {
            colorScheme = CustomTabsIntent.COLOR_SCHEME_DARK;
            editor.putInt(SHARED_PREF_THEME, CustomTabsIntent.COLOR_SCHEME_DARK);
        } else {
            editor.putInt(SHARED_PREF_THEME, CustomTabsIntent.COLOR_SCHEME_SYSTEM);
        }
        if (!TextUtils.isEmpty(mToolbarColor)) {
            builder.setToolbarColor(Color.parseColor(mToolbarColor));
        }
        editor.putString(SHARED_PREF_COLOR, mColorName);
        builder.setShowTitle(showTitle)
                .setColorScheme(colorScheme)
                .setUrlBarHidingEnabled(urlHiding);
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
            editor.putInt(SHARED_PREF_CLOSE_ICON, CLOSE_ICON_CHECK);
        } else if (closeButton == R.id.back_button) {
            builder.setCloseButtonIcon(
                    BitmapFactory.decodeResource(getResources(), R.drawable.ic_arrow_back));
            editor.putInt(SHARED_PREF_CLOSE_ICON, CLOSE_ICON_BACK);
        } else {
            builder.setCloseButtonIcon(
                    BitmapFactory.decodeResource(getResources(), R.drawable.baseline_close_white));
            editor.putInt(SHARED_PREF_CLOSE_ICON, CLOSE_ICON_X);
        }
        editor.apply();
    }

    private void prepareMenuItems(CustomTabsIntent.Builder builder) {
        Intent menuIntent = new Intent();
        menuIntent.setClass(getApplicationContext(), this.getClass());
        // Optional animation configuration when the user clicks menu items.
        Bundle menuBundle = ActivityOptions.makeCustomAnimation(this, android.R.anim.slide_in_left,
                android.R.anim.slide_out_right).toBundle();
        PendingIntent pi = PendingIntent.getActivity(
                getApplicationContext(), 0, menuIntent, PendingIntent.FLAG_MUTABLE, menuBundle);
        builder.addMenuItem("Menu entry 1", pi);
    }

    private void prepareActionButton(CustomTabsIntent.Builder builder) {
        // An example intent that sends an email.
        Intent actionIntent = new Intent(Intent.ACTION_SEND);
        actionIntent.setType("*/*");
        actionIntent.putExtra(Intent.EXTRA_EMAIL, "example@example.com");
        actionIntent.putExtra(Intent.EXTRA_SUBJECT, "example");
        PendingIntent pi =
                PendingIntent.getActivity(this, 0, actionIntent, PendingIntent.FLAG_IMMUTABLE);
        Bitmap icon = BitmapFactory.decodeResource(getResources(), R.drawable.baseline_send_white);
        builder.setActionButton(icon, "send email", pi, true);
    }

    private void prepareBottombar(CustomTabsIntent.Builder builder) {
        BottomBarManager.setMediaPlayer(mMediaPlayer);
        Intent broadcastIntent = new Intent(this, BottomBarManager.class);
        PendingIntent pi =
                PendingIntent.getBroadcast(this, 0, broadcastIntent, PendingIntent.FLAG_MUTABLE);
        builder.setSecondaryToolbarViews(BottomBarManager.createRemoteViews(this, true),
                BottomBarManager.getClickableIDs(), pi);
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
        mDisconnectButton.setEnabled(true);
        mMayLaunchButton.setEnabled(true);
        try {
            if (getSession().isEngagementSignalsApiAvailable(Bundle.EMPTY)) {
                getSession().setEngagementSignalsCallback(new EngagementCallback(), Bundle.EMPTY);
            }
        } catch (RemoteException e) {
            Log.w(TAG, "The Service died while responding to the request.", e);
        } catch (UnsupportedOperationException e) {
            Log.w(TAG, "Engagement Signals API isn't supported by the browser.", e);
        }
    }

    @Override
    public void onServiceDisconnected() {
        mConnectButton.setEnabled(true);
        mWarmupButton.setEnabled(false);
        mMayLaunchButton.setEnabled(false);
        mClient = null;
    }

    private @Px int getMaximumPossibleSizePx() {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
            WindowMetrics windowMetrics = getWindowManager().getCurrentWindowMetrics();
            Insets navbarInsets = windowMetrics.getWindowInsets().getInsets(
                    WindowInsets.Type.navigationBars() | WindowInsets.Type.displayCutout());
            int navbarWidth = navbarInsets.left + navbarInsets.right;
            Rect windowBounds = getWindowManager().getCurrentWindowMetrics().getBounds();
            int width = windowBounds.width() - navbarWidth;
            int height = windowMetrics.getBounds().height();
            return Math.max(width, height);
        } else {
            Display display = getWindowManager().getDefaultDisplay();
            Point size = new Point();
            display.getSize(size);
            int width = size.x;

            DisplayMetrics displayMetrics = new DisplayMetrics();
            if (isInMultiWindowMode()) {
                display.getMetrics(displayMetrics);
            } else {
                display.getRealMetrics(displayMetrics);
            }
            int height = displayMetrics.heightPixels;
            return Math.max(width, height);
        }
    }
}
