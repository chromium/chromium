// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.android_webview.devui;

import android.Manifest.permission;
import android.app.AlertDialog;
import android.content.Context;
import android.content.Intent;
import android.content.SharedPreferences;
import android.graphics.PorterDuff;
import android.graphics.PorterDuffColorFilter;
import android.graphics.drawable.Drawable;
import android.net.Uri;
import android.os.Build;
import android.os.Build.VERSION;
import android.os.Build.VERSION_CODES;
import android.os.Bundle;
import android.provider.Settings;
import android.view.Menu;
import android.view.MenuItem;
import android.view.View;
import android.widget.LinearLayout;
import android.widget.TextView;

import androidx.activity.EdgeToEdge;
import androidx.annotation.IntDef;
import androidx.annotation.NonNull;
import androidx.annotation.VisibleForTesting;
import androidx.core.app.ActivityCompat;
import androidx.core.graphics.Insets;
import androidx.core.view.ViewCompat;
import androidx.core.view.WindowInsetsCompat;
import androidx.fragment.app.Fragment;
import androidx.fragment.app.FragmentActivity;
import androidx.fragment.app.FragmentManager;
import androidx.fragment.app.FragmentTransaction;

import org.chromium.android_webview.common.BugTrackerConstants;
import org.chromium.android_webview.devui.util.SafeIntentUtils;
import org.chromium.base.BuildInfo;
import org.chromium.base.ContextUtils;
import org.chromium.base.IntentUtils;
import org.chromium.base.ResettersForTesting;
import org.chromium.base.metrics.RecordHistogram;

import java.util.HashMap;
import java.util.Map;

/**
 * Dev UI main activity. It shows persistent errors and helps to navigate to WebView developer
 * tools.
 */
public class MainActivity extends FragmentActivity {
    private PersistentErrorView mErrorView;
    private WebViewPackageError mDifferentPackageError;
    private boolean mDifferentPackageErrorVisible;
    private boolean mSwitchFragmentOnResume;
    final Map<Integer, Integer> mFragmentIdMap = new HashMap<>();

    // Store in a variable to allow for replacement during test
    private boolean mIsAtLeastTBuild = Build.VERSION.SDK_INT >= 33;

    // Keep in sync with DeveloperUiService.java
    public static final String FRAGMENT_ID_INTENT_EXTRA = "fragment-id";
    public static final String RESET_FLAGS_INTENT_EXTRA = "reset-flags";
    public static final int FRAGMENT_ID_HOME = 0;
    public static final int FRAGMENT_ID_CRASHES = 1;
    public static final int FRAGMENT_ID_FLAGS = 2;
    public static final int FRAGMENT_ID_COMPONENTS = 3;
    public static final int FRAGMENT_ID_SAFEMODE = 4;
    public static final int FRAGMENT_ID_NETLOGS = 5;

    // These values are persisted to logs. Entries should not be renumbered and
    // numeric values should never be reused.
    @IntDef({
        MenuChoice.SWITCH_PROVIDER,
        MenuChoice.REPORT_BUG,
        MenuChoice.CHECK_UPDATES,
        MenuChoice.CRASHES_REFRESH,
        MenuChoice.ABOUT_DEVTOOLS,
        MenuChoice.COMPONENTS_UI,
        MenuChoice.COMPONENTS_UPDATE,
        MenuChoice.SAFEMODE_UI
    })
    public @interface MenuChoice {
        int SWITCH_PROVIDER = 0;
        int REPORT_BUG = 1;
        int CHECK_UPDATES = 2;
        int CRASHES_REFRESH = 3;
        int ABOUT_DEVTOOLS = 4;
        int COMPONENTS_UI = 5;
        int COMPONENTS_UPDATE = 6;
        int SAFEMODE_UI = 7;
        int COUNT = 8;
    }

    public static void logMenuSelection(@MenuChoice int selectedMenuItem) {
        RecordHistogram.recordEnumeratedHistogram(
                "Android.WebView.DevUi.MenuSelection", selectedMenuItem, MenuChoice.COUNT);
    }

    // These values are persisted to logs. Entries should not be renumbered and
    // numeric values should never be reused.
    @IntDef({
        FragmentNavigation.HOME_FRAGMENT,
        FragmentNavigation.CRASHES_LIST_FRAGMENT,
        FragmentNavigation.FLAGS_FRAGMENT,
        FragmentNavigation.COMPONENTS_LIST_FRAGMENT,
        FragmentNavigation.SAFEMODE_FRAGMENT,
        FragmentNavigation.NETLOGS_FRAGMENT
    })
    private @interface FragmentNavigation {
        int HOME_FRAGMENT = 0;
        int CRASHES_LIST_FRAGMENT = 1;
        int FLAGS_FRAGMENT = 2;
        int COMPONENTS_LIST_FRAGMENT = 3;
        int SAFEMODE_FRAGMENT = 4;
        int NETLOGS_FRAGMENT = 5;
        int COUNT = 6;
    }

    private static final int NOTIFICATION_PERMISSION_REQUEST_CODE = 0;

    @VisibleForTesting
    public static final String POST_NOTIFICATIONS_PERMISSION_REQUESTED_KEY =
            "POST_NOTIFICATIONS_PERMISSION_REQUESTED";

    @VisibleForTesting
    public static final String NOTIFICATION_PERMISSION_REQUEST_MESSAGE =
            "WebView DevTools requires permission to show notifications "
                    + "in order to manage flags.";

    /**
     * Logs a navigation to a fragment. Requires a suffix from histograms.xml ("AnyMethod",
     * "FromIntent", or "NavBar") to determine which histogram to log.
     *
     * @param histogramSuffix one of the suffixes listed in histograms.xml
     * @param selectedFragmentId one of FRAGMENT_ID_HOME, FRAGMENT_ID_CRASHES, FRAGMENT_ID_FLAGS or
     *     FRAGMENT_ID_NETLOGS
     */
    private static void logFragmentNavigation(String histogramSuffix, int selectedFragmentId) {
        // Map FRAGMENT_ID_* to FragmentNavigation value (so FRAGMENT_ID_* values are permitted to
        // change in the future without messing up logs).
        @FragmentNavigation int sample;
        switch (selectedFragmentId) {
            case FRAGMENT_ID_HOME:
                sample = FragmentNavigation.HOME_FRAGMENT;
                break;
            case FRAGMENT_ID_CRASHES:
                sample = FragmentNavigation.CRASHES_LIST_FRAGMENT;
                break;
            case FRAGMENT_ID_FLAGS:
                sample = FragmentNavigation.FLAGS_FRAGMENT;
                break;
            case FRAGMENT_ID_COMPONENTS:
                sample = FragmentNavigation.COMPONENTS_LIST_FRAGMENT;
                break;
            case FRAGMENT_ID_SAFEMODE:
                sample = FragmentNavigation.SAFEMODE_FRAGMENT;
                break;
            case FRAGMENT_ID_NETLOGS:
                sample = FragmentNavigation.NETLOGS_FRAGMENT;
                break;
            default:
                sample = FragmentNavigation.HOME_FRAGMENT;
                break;
        }
        RecordHistogram.recordEnumeratedHistogram(
                "Android.WebView.DevUi.FragmentNavigation." + histogramSuffix,
                sample,
                FragmentNavigation.COUNT);
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        EdgeToEdge.enable(this);
        super.onCreate(savedInstanceState);

        setContentView(R.layout.activity_main);

        setupEdgeToEdge();

        // Let onResume handle showing the initial Fragment.
        mSwitchFragmentOnResume = true;

        mErrorView = new PersistentErrorView(this, R.id.main_error_view);
        mDifferentPackageError = new WebViewPackageError(this, mErrorView);

        // Set up bottom navigation bar:
        mFragmentIdMap.put(R.id.navigation_home, FRAGMENT_ID_HOME);
        mFragmentIdMap.put(R.id.navigation_crash_ui, FRAGMENT_ID_CRASHES);
        mFragmentIdMap.put(R.id.navigation_flags_ui, FRAGMENT_ID_FLAGS);
        mFragmentIdMap.put(R.id.navigation_net_logs_ui, FRAGMENT_ID_NETLOGS);
        LinearLayout bottomNavBar = findViewById(R.id.nav_view);
        View.OnClickListener listener =
                (View view) -> {
                    assert mFragmentIdMap.containsKey(view.getId())
                            : "Unexpected view ID: " + view.getId();
                    int fragmentId = mFragmentIdMap.get(view.getId());
                    switchFragment(fragmentId, false);
                    logFragmentNavigation("NavBar", fragmentId);
                };
        final int childCount = bottomNavBar.getChildCount();
        for (int i = 0; i < childCount; ++i) {
            View v = bottomNavBar.getChildAt(i);
            v.setOnClickListener(listener);
        }

        FragmentManager fm = getSupportFragmentManager();
        fm.registerFragmentLifecycleCallbacks(
                new FragmentManager.FragmentLifecycleCallbacks() {
                    @Override
                    public void onFragmentResumed(FragmentManager fm, Fragment f) {
                        if (!mDifferentPackageErrorVisible) {
                            if (f instanceof DevUiBaseFragment) {
                                ((DevUiBaseFragment) f).maybeShowErrorView(mErrorView);
                            }
                        }
                    }
                },
                /* recursive= */ false);

        // The boolean value doesn't matter, we only care about the total count.
        RecordHistogram.recordBooleanHistogram("Android.WebView.DevUi.AppLaunch", true);
    }

    private void switchFragment(int chosenFragmentId, boolean onResume) {
        DevUiBaseFragment fragment = null;
        switch (chosenFragmentId) {
            case FRAGMENT_ID_HOME:
                fragment = new HomeFragment();
                break;
            case FRAGMENT_ID_CRASHES:
                fragment = new CrashesListFragment();
                break;
            case FRAGMENT_ID_FLAGS:
                boolean needPermissionCheck = needToRequestPostNotificationPermission();
                if (needPermissionCheck) {
                    // Spawn the request permission check on top of the new fragment
                    requestPostNotificationPermission();
                }

                boolean shouldResetFlags = false;
                // The flag reset is checked on resume so that
                // it can only be triggered by a new intent.
                if (onResume) {
                    shouldResetFlags =
                            IntentUtils.safeGetBooleanExtra(
                                    getIntent(), RESET_FLAGS_INTENT_EXTRA, false);
                }
                // Enable the UI if we don't need a permission check
                fragment = new FlagsFragment(!needPermissionCheck, shouldResetFlags);
                break;
            case FRAGMENT_ID_COMPONENTS:
                fragment = new ComponentsListFragment();
                break;
            case FRAGMENT_ID_SAFEMODE:
                fragment = new SafeModeFragment();
                break;
            case FRAGMENT_ID_NETLOGS:
                fragment = new NetLogsFragment();
                break;
            default:
                chosenFragmentId = FRAGMENT_ID_HOME;
                fragment = new HomeFragment();
                break;
        }
        assert fragment != null;
        logFragmentNavigation("AnyMethod", chosenFragmentId);

        // Switch fragments
        FragmentManager fm = getSupportFragmentManager();
        FragmentTransaction transaction = fm.beginTransaction();
        transaction.replace(R.id.content_fragment, fragment);
        transaction.commit();

        // Update the bottom toolbar
        LinearLayout bottomNavBar = findViewById(R.id.nav_view);
        final int childCount = bottomNavBar.getChildCount();
        for (int i = 0; i < childCount; ++i) {
            View view = bottomNavBar.getChildAt(i);
            assert mFragmentIdMap.containsKey(view.getId()) : "Unexpected view ID: " + view.getId();
            int fragmentId = mFragmentIdMap.get(view.getId());
            assert view instanceof TextView : "Bottom bar must have TextViews as direct children";
            TextView textView = (TextView) view;

            boolean isSelectedFragment = chosenFragmentId == fragmentId;
            textView.setTextAppearance(
                    isSelectedFragment
                            ? R.style.SelectedNavigationButton
                            : R.style.UnselectedNavigationButton);
            int color =
                    isSelectedFragment
                            ? getColor(R.color.navigation_selected)
                            : getColor(R.color.navigation_unselected);
            for (Drawable drawable : textView.getCompoundDrawables()) {
                if (drawable != null) {
                    drawable.mutate();
                    drawable.setColorFilter(
                            new PorterDuffColorFilter(color, PorterDuff.Mode.SRC_IN));
                }
            }
        }
    }

    @Override
    protected void onNewIntent(Intent intent) {
        super.onNewIntent(intent);
        // Store the Intent so we can switch Fragments in onResume (which is called next). Only need
        // to switch Fragment if the Intent specifies to do so.
        setIntent(intent);
        mSwitchFragmentOnResume = IntentUtils.safeHasExtra(intent, FRAGMENT_ID_INTENT_EXTRA);
    }

    @Override
    protected void onResume() {
        super.onResume();

        // Check package status in onResume() to hide/show the error message if the user
        // changes WebView implementation from system settings and then returns back to the
        // activity.
        mDifferentPackageErrorVisible = mDifferentPackageError.showMessageIfDifferent();

        // Don't change Fragment unless we have a new Intent, since the user might just be coming
        // back to this through the task switcher.
        if (!mSwitchFragmentOnResume) return;

        // Ensure we only switch the first time we see a new Intent.
        mSwitchFragmentOnResume = false;

        // Default to HomeFragment if not specified.
        int fragmentId = FRAGMENT_ID_HOME;
        // FRAGMENT_ID_INTENT_EXTRA is an optional extra to specify which fragment to open. At the
        // moment, it's specified only by DeveloperUiService (so make sure these constants stay in
        // sync).
        fragmentId = IntentUtils.safeGetIntExtra(getIntent(), FRAGMENT_ID_INTENT_EXTRA, fragmentId);
        switchFragment(fragmentId, true);
        logFragmentNavigation("FromIntent", fragmentId);
    }

    @Override
    public boolean onCreateOptionsMenu(Menu menu) {
        getMenuInflater().inflate(R.menu.options_menu, menu);
        if (!WebViewPackageError.canAccessWebViewProviderDeveloperSetting()) {
            MenuItem item = menu.findItem(R.id.options_menu_switch_provider);
            item.setVisible(false);
        }
        return super.onCreateOptionsMenu(menu);
    }

    @Override
    public boolean onOptionsItemSelected(MenuItem item) {
        if (item.getItemId() == R.id.options_menu_switch_provider) {
            logMenuSelection(MenuChoice.SWITCH_PROVIDER);
            SafeIntentUtils.startActivityOrShowError(
                    this,
                    new Intent(Settings.ACTION_WEBVIEW_SETTINGS),
                    SafeIntentUtils.WEBVIEW_SETTINGS_ERROR);
            return true;
        } else if (item.getItemId() == R.id.options_menu_report_bug) {
            logMenuSelection(MenuChoice.REPORT_BUG);
            Uri reportUri =
                    new Uri.Builder()
                            .scheme("https")
                            .authority("issues.chromium.org")
                            .path("/issues/new")
                            .appendQueryParameter(
                                    "component", BugTrackerConstants.COMPONENT_MOBILE_WEBVIEW)
                            .appendQueryParameter(
                                    "template", BugTrackerConstants.DEFAULT_WEBVIEW_TEMPLATE)
                            .appendQueryParameter("priority", "P3")
                            .appendQueryParameter("type", "BUG")
                            .appendQueryParameter(
                                    "customFields", BugTrackerConstants.OS_FIELD + ":Android")
                            .build();
            SafeIntentUtils.startActivityOrShowError(
                    this,
                    new Intent(Intent.ACTION_VIEW, reportUri),
                    SafeIntentUtils.NO_BROWSER_FOUND_ERROR);
            return true;
        } else if (item.getItemId() == R.id.options_menu_check_updates) {
            logMenuSelection(MenuChoice.CHECK_UPDATES);
            try {
                Uri marketUri =
                        new Uri.Builder()
                                .scheme("market")
                                .authority("details")
                                .appendQueryParameter("id", this.getPackageName())
                                .build();
                startActivity(new Intent(Intent.ACTION_VIEW, marketUri));
            } catch (Exception e) {
                Uri marketUri =
                        new Uri.Builder()
                                .scheme("https")
                                .authority("play.google.com")
                                .path("/store/apps/details")
                                .appendQueryParameter("id", this.getPackageName())
                                .build();
                SafeIntentUtils.startActivityOrShowError(
                        this,
                        new Intent(Intent.ACTION_VIEW, marketUri),
                        SafeIntentUtils.NO_BROWSER_FOUND_ERROR);
            }
            return true;
        } else if (item.getItemId() == R.id.options_menu_about_devui) {
            logMenuSelection(MenuChoice.ABOUT_DEVTOOLS);
            Uri uri =
                    Uri.parse(
                            "https://chromium.googlesource.com/chromium/src/+/HEAD/android_webview/docs/developer-ui.md");
            SafeIntentUtils.startActivityOrShowError(
                    this,
                    new Intent(Intent.ACTION_VIEW, uri),
                    SafeIntentUtils.NO_BROWSER_FOUND_ERROR);
            return true;
        } else if (item.getItemId() == R.id.options_menu_components) {
            logMenuSelection(MenuChoice.COMPONENTS_UI);
            switchFragment(FRAGMENT_ID_COMPONENTS, false);
            return true;
        } else if (item.getItemId() == R.id.options_menu_safe_mode) {
            logMenuSelection(MenuChoice.SAFEMODE_UI);
            switchFragment(FRAGMENT_ID_SAFEMODE, false);
            return true;
        }
        return super.onOptionsItemSelected(item);
    }

    @VisibleForTesting
    public boolean needToRequestPostNotificationPermission() {
        if (!mIsAtLeastTBuild) {
            return false;
        }
        // Check if we already requested the permission. If we did, we don't need to request
        // it again, even if no permission was given.
        return !getAlreadyRequestedNotificationPermissionPreference();
    }

    private boolean getAlreadyRequestedNotificationPermissionPreference() {
        return getSharedPreferences()
                .getBoolean(POST_NOTIFICATIONS_PERMISSION_REQUESTED_KEY, false);
    }

    private void requestPostNotificationPermission() {
        AlertDialog.Builder builder = new AlertDialog.Builder(this);
        builder.setMessage(NOTIFICATION_PERMISSION_REQUEST_MESSAGE);
        builder.setPositiveButton(
                "Ok",
                (dialogInterface, i) -> {
                    ActivityCompat.requestPermissions(
                            this,
                            new String[] {permission.POST_NOTIFICATIONS},
                            NOTIFICATION_PERMISSION_REQUEST_CODE);
                });
        builder.setNegativeButton("Cancel", (dialogInterface, i) -> {});
        builder.create().show();
    }

    @Override
    public void onRequestPermissionsResult(
            int requestCode, @NonNull String[] permissions, @NonNull int[] grantResults) {
        super.onRequestPermissionsResult(requestCode, permissions, grantResults);
        if (requestCode == NOTIFICATION_PERMISSION_REQUEST_CODE && grantResults.length > 0) {
            // We don't actually care about the result, just that we got a result.
            // The service will still work.
            // Save the fact that we have received the permission callback.
            registerPostNotificationRequested();
            // Reset the UI to enable input fields.
            switchFragment(FRAGMENT_ID_FLAGS, false);
        }
    }

    private void registerPostNotificationRequested() {
        getSharedPreferences()
                .edit()
                .putBoolean(POST_NOTIFICATIONS_PERMISSION_REQUESTED_KEY, true)
                .apply();
    }

    /**
     * Get the SharedPreferences for this activity.
     *
     * Uses {@link ContextUtils#getApplicationContext()} to facilitate mocking out the preferences
     * by tests, but otherwise accesses the same file as the {@link #getPreferences(int)} method
     * when passing {@link Context#MODE_PRIVATE}.
     * @return Private preferences for this activity
     */
    private static SharedPreferences getSharedPreferences() {
        return ContextUtils.getApplicationContext()
                .getSharedPreferences(MainActivity.class.getCanonicalName(), Context.MODE_PRIVATE);
    }

    /**
     * Handle window insets for edge_to_edge behaviour. This is backwards compatible until Android
     * 10 (API level 29) — first android version to support all edge-to-edge behaviour. Anything
     * below that, the value of the insets would typically be zero. This is because the system UI
     * elements are not considered as insets in those cases — they are always part of the layout.
     *
     * <p>We are doing two things:
     *
     * <ol>
     *   <li>Extend app content all the way to the system edges, drawing behind the system bars
     *       (status and navigation bar). This would cause visual overlaps.
     *   <li>So, we handle the insets and add padding accordingly to protect the critical elements
     *       (buttons, textviews...) not overlap with the system bars.
     * </ol>
     */
    private void setupEdgeToEdge() {
        // Disable platform enforced contrast between navbar and app content, allowing for a
        // transparent navbar.
        if (VERSION.SDK_INT >= VERSION_CODES.Q) {
            getWindow().setNavigationBarContrastEnforced(false);
        }

        // Ensure the header view does not overlap with the status bar by adjusting its top padding
        // based on the system window insets (i.e., the space occupied by system status bar).
        ViewCompat.setOnApplyWindowInsetsListener(
                findViewById(R.id.header),
                (v, windowInsets) -> {
                    Insets insets = windowInsets.getInsets(WindowInsetsCompat.Type.systemBars());
                    v.setPadding(
                            v.getPaddingLeft(),
                            insets.top,
                            v.getPaddingRight(),
                            v.getPaddingBottom());

                    // By returning the windowInsets object, we allow the insets to be passed down
                    // to the child views. We want this so that the custom bottom navbar, which is a
                    // child of this element, to receive the insets and adjust its layout
                    // accordingly (see below).
                    return windowInsets;
                });

        // Same as above, but add bottom padding to it instead so that the TextViews inside of
        // the bottom navbar won't overlap with system navbar.
        ViewCompat.setOnApplyWindowInsetsListener(
                findViewById(R.id.nav_view),
                (v, windowInsets) -> {
                    Insets insets = windowInsets.getInsets(WindowInsetsCompat.Type.systemBars());
                    v.setPadding(
                            v.getPaddingLeft(),
                            v.getPaddingTop(),
                            v.getPaddingRight(),
                            insets.bottom);

                    // By returning WindowInsetsCompat.CONSUMED, we are indicating that we have
                    // handled the insets for this view, and they should not be passed down to child
                    // views (The three TextViews).
                    return WindowInsetsCompat.CONSUMED;
                });
    }

    /**
     * Override whether or not the Activity is running on a T+ build of Android.
     *
     * <p>This method has been introduced to avoid mocking out {@link BuildInfo#isAtLeastT()}.
     *
     * @param isAtLeastT Whether the running Android version is at least T.
     */
    public void setIsAtLeastTBuildForTesting(boolean isAtLeastT) {
        var oldValue = mIsAtLeastTBuild;
        mIsAtLeastTBuild = isAtLeastT;
        ResettersForTesting.register(() -> mIsAtLeastTBuild = oldValue);
    }

    /**
     * Update the preferences for {@link MainActivity} to indicate that the app has already
     * requested permission to show popups.
     */
    public static void markPopupPermissionRequestedInPrefsForTesting() {
        getSharedPreferences()
                .edit()
                .putBoolean(POST_NOTIFICATIONS_PERMISSION_REQUESTED_KEY, true)
                .apply();
        ResettersForTesting.register(MainActivity::clearSharedPrefsForTesting);
    }

    /** Clear preferences for {@link MainActivity} for testing purposes. */
    public static void clearSharedPrefsForTesting() {
        getSharedPreferences().edit().clear().apply();
    }
}
