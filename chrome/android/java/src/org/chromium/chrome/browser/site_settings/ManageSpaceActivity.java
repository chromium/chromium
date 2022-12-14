// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.site_settings;

import android.annotation.SuppressLint;
import android.app.ActivityManager;
import android.content.Context;
import android.content.DialogInterface;
import android.content.pm.ActivityInfo;
import android.content.pm.PackageManager.NameNotFoundException;
import android.content.res.Resources;
import android.os.Build;
import android.os.Bundle;
import android.os.SystemClock;
import android.text.TextUtils;
import android.text.format.Formatter;
import android.view.View;
import android.widget.Button;
import android.widget.TextView;

import androidx.annotation.VisibleForTesting;
import androidx.appcompat.app.AlertDialog;
import androidx.appcompat.app.AppCompatActivity;

import org.chromium.base.Log;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.about_settings.AboutChromeSettings;
import org.chromium.chrome.browser.init.BrowserParts;
import org.chromium.chrome.browser.init.ChromeBrowserInitializer;
import org.chromium.chrome.browser.init.EmptyBrowserParts;
import org.chromium.chrome.browser.notifications.channels.SiteChannelsManager;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.SharedPreferencesManager;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.settings.SettingsLauncherImpl;
import org.chromium.chrome.browser.ui.searchactivityutils.SearchActivityPreferencesManager;
import org.chromium.components.browser_ui.settings.SettingsLauncher;
import org.chromium.components.browser_ui.site_settings.AllSiteSettings;
import org.chromium.components.browser_ui.site_settings.SingleCategorySettings;
import org.chromium.components.browser_ui.site_settings.SiteSettingsCategory;
import org.chromium.components.browser_ui.site_settings.Website;
import org.chromium.components.browser_ui.site_settings.Website.StoredDataClearedCallback;
import org.chromium.components.browser_ui.site_settings.WebsitePermissionsFetcher;
import org.chromium.components.browser_ui.util.ConversionUtils;
import org.chromium.components.version_info.VersionInfo;

import java.util.Collection;

/**
 * This is the target activity for the "Manage Storage" button in the Android Settings UI. This is
 * configured in AndroidManifest.xml by setting android:manageSpaceActivity for the application.
 * The browser process must be started here because this Activity may be started explicitly from
 * Android settings, when Android is restoring ManageSpaceActivity after Chrome was killed, or for
 * tests.
 */
public class ManageSpaceActivity extends AppCompatActivity implements View.OnClickListener {
    private static final String TAG = "ManageSpaceActivity";

    // Do not change these constants except for the MAX entry, they are used with UMA histograms.
    private static final int OPTION_CLEAR_UNIMPORTANT = 0;
    private static final int OPTION_MANAGE_STORAGE = 1;
    private static final int OPTION_CLEAR_APP_DATA = 2;
    private static final int OPTION_MAX = 3;

    private TextView mUnimportantSiteDataSizeText;
    private TextView mSiteDataSizeText;
    private Button mClearUnimportantButton;
    private Button mManageSiteDataButton;
    private Button mClearAllDataButton;
    // Stored for testing.
    private AlertDialog mUnimportantDialog;

    private static boolean sActivityNotExportedChecked;

    private boolean mIsNativeInitialized;

    @SuppressLint({"ApplySharedPref", "CommitPrefEdits"})
    @Override
    protected void onCreate(Bundle savedInstanceState) {
        ensureActivityNotExported();

        setContentView(R.layout.manage_space_activity);
        Resources r = getResources();
        setTitle(String.format(r.getString(R.string.storage_management_activity_label),
                r.getString(R.string.app_name)));

        mSiteDataSizeText = (TextView) findViewById(R.id.site_data_storage_size_text);
        mSiteDataSizeText.setText(R.string.storage_management_computing_size);
        mUnimportantSiteDataSizeText =
                (TextView) findViewById(R.id.unimportant_site_data_storage_size_text);
        mUnimportantSiteDataSizeText.setText(R.string.storage_management_computing_size);
        mManageSiteDataButton = (Button) findViewById(R.id.manage_site_data_storage);
        mClearUnimportantButton = (Button) findViewById(R.id.clear_unimportant_site_data_storage);

        // We initially disable all of our buttons except for the 'Clear All Data' button, and wait
        // until the browser is finished initializing to enable them. We want to make sure the
        // 'Clear All Data' button is enabled so users can do this even if it's taking forever for
        // the Chromium process to boot up.
        mManageSiteDataButton.setEnabled(false);
        mClearUnimportantButton.setEnabled(false);
        mManageSiteDataButton.setOnClickListener(this);
        mClearUnimportantButton.setOnClickListener(this);

        mClearAllDataButton = (Button) findViewById(R.id.clear_all_data);
        mClearAllDataButton.setOnClickListener(this);
        super.onCreate(savedInstanceState);

        BrowserParts parts = new EmptyBrowserParts() {
            @Override
            public void finishNativeInitialization() {
                ManageSpaceActivity.this.finishNativeInitialization();
            }
            @Override
            public void onStartupFailure(Exception failureCause) {
                mSiteDataSizeText.setText(R.string.storage_management_startup_failure);
                mUnimportantSiteDataSizeText.setText(R.string.storage_management_startup_failure);
            }
        };

        String productVersion =
                AboutChromeSettings.getApplicationVersion(this, VersionInfo.getProductVersion());
        String failedVersion = SharedPreferencesManager.getInstance().readString(
                ChromePreferenceKeys.SETTINGS_WEBSITE_FAILED_BUILD_VERSION, null);
        if (TextUtils.equals(failedVersion, productVersion)) {
            parts.onStartupFailure(null);
            return;
        }

        // If the native library crashes and kills the browser process, there is no guarantee
        // java-side the pref will be written before the process dies. We want to make sure we
        // don't attempt to start the browser process and have it kill chrome. This activity is
        // used to clear data for the chrome app, so it must be particularly error resistant.
        SharedPreferencesManager.getInstance().writeStringSync(
                ChromePreferenceKeys.SETTINGS_WEBSITE_FAILED_BUILD_VERSION, productVersion);

        try {
            ChromeBrowserInitializer.getInstance().handlePreNativeStartupAndLoadLibraries(parts);
            ChromeBrowserInitializer.getInstance().handlePostNativeStartup(true, parts);
        } catch (Exception e) {
            // We don't want to exit, as the user should still be able to clear all browsing data.
            Log.e(TAG, "Unable to load native library.", e);
            mSiteDataSizeText.setText(R.string.storage_management_startup_failure);
            mUnimportantSiteDataSizeText.setText(R.string.storage_management_startup_failure);
        }
    }

    /** @see BrowserParts#finishNativeInitialization */
    public void finishNativeInitialization() {
        mIsNativeInitialized = true;
        mManageSiteDataButton.setEnabled(true);
        mClearUnimportantButton.setEnabled(true);
        RecordUserAction.record("Android.ManageSpace");
        refreshStorageNumbers();
    }

    @Override
    public void onResume() {
        super.onResume();
        if (mIsNativeInitialized) refreshStorageNumbers();
    }

    @Override
    protected void onStop() {
        super.onStop();

        SharedPreferencesManager.getInstance().writeString(
                ChromePreferenceKeys.SETTINGS_WEBSITE_FAILED_BUILD_VERSION, null);
    }

    @VisibleForTesting
    public Button getClearUnimportantButton() {
        return mClearUnimportantButton;
    }

    @VisibleForTesting
    public AlertDialog getUnimportantConfirmDialog() {
        return mUnimportantDialog;
    }

    /** This refreshes the storage numbers by fetching all site permissions. */
    private void refreshStorageNumbers() {
        Profile profile = Profile.getLastUsedRegularProfile();
        WebsitePermissionsFetcher fetcher = new WebsitePermissionsFetcher(profile);
        fetcher.fetchPreferencesForCategory(
                SiteSettingsCategory.createFromType(profile, SiteSettingsCategory.Type.USE_STORAGE),
                new SizeCalculator());
    }

    /** Data will be cleared once we fetch all site size and important status info. */
    private void clearUnimportantData() {
        mSiteDataSizeText.setText(R.string.storage_management_computing_size);
        mUnimportantSiteDataSizeText.setText(R.string.storage_management_computing_size);
        mClearUnimportantButton.setEnabled(false);
        mManageSiteDataButton.setEnabled(false);
        UnimportantSiteDataClearer clearer = new UnimportantSiteDataClearer();
        clearer.clearData();
    }

    /** Called after we finish clearing unimportant data. Re-enables our buttons. */
    private void clearUnimportantDataDone() {
        mClearUnimportantButton.setEnabled(true);
        mManageSiteDataButton.setEnabled(true);
    }

    @Override
    public void onClick(View view) {
        if (view == mClearUnimportantButton) {
            if (mUnimportantDialog == null) {
                AlertDialog.Builder builder = new AlertDialog.Builder(this);
                builder.setPositiveButton(R.string.ok, new DialogInterface.OnClickListener() {
                    @Override
                    public void onClick(DialogInterface dialog, int id) {
                        mUnimportantDialog = null;
                        RecordHistogram.recordEnumeratedHistogram("Android.ManageSpace.ActionTaken",
                                OPTION_CLEAR_UNIMPORTANT, OPTION_MAX);
                        clearUnimportantData();
                    }
                });
                builder.setNegativeButton(R.string.cancel, null);
                builder.setTitle(R.string.storage_clear_site_storage_title);
                builder.setMessage(R.string.storage_management_clear_unimportant_dialog_text);
                mUnimportantDialog = builder.create();
            }
            mUnimportantDialog.show();
        } else if (view == mManageSiteDataButton) {
            Bundle initialArguments = new Bundle();
            initialArguments.putString(SingleCategorySettings.EXTRA_CATEGORY,
                    SiteSettingsCategory.preferenceKey(SiteSettingsCategory.Type.USE_STORAGE));
            initialArguments.putString(SingleCategorySettings.EXTRA_TITLE,
                    getString(R.string.website_settings_storage));
            RecordHistogram.recordEnumeratedHistogram(
                    "Android.ManageSpace.ActionTaken", OPTION_MANAGE_STORAGE, OPTION_MAX);
            SettingsLauncher settingsLauncher = new SettingsLauncherImpl();
            settingsLauncher.launchSettingsActivity(this, AllSiteSettings.class, initialArguments);
        } else if (view == mClearAllDataButton) {
            final ActivityManager activityManager =
                    (ActivityManager) getSystemService(Context.ACTIVITY_SERVICE);
            AlertDialog.Builder builder = new AlertDialog.Builder(this);
            builder.setPositiveButton(R.string.ok, new DialogInterface.OnClickListener() {
                @Override
                public void onClick(DialogInterface dialog, int id) {
                    if (mIsNativeInitialized) {
                        // This probably won't actually be uploaded, as android will probably kill
                        // all processes & data before it gets sent to the network.
                        RecordHistogram.recordEnumeratedHistogram("Android.ManageSpace.ActionTaken",
                                OPTION_CLEAR_APP_DATA, OPTION_MAX);
                    }

                    SearchActivityPreferencesManager.resetCachedValues();
                    if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
                        SiteChannelsManager.getInstance().deleteAllSiteChannels();
                    }
                    activityManager.clearApplicationUserData();
                }
            });
            builder.setNegativeButton(R.string.cancel, null);
            builder.setTitle(R.string.storage_management_reset_app_dialog_title);
            builder.setMessage(R.string.storage_management_reset_app_dialog_text);
            builder.create().show();
        }
    }

    private void onSiteStorageSizeCalculated(long totalSize, long unimportantSize) {
        RecordHistogram.recordCount1MHistogram("Android.ManageSpace.TotalDiskUsageMB",
                (int) ConversionUtils.bytesToMegabytes(totalSize));
        RecordHistogram.recordCount1MHistogram("Android.ManageSpace.UnimportantDiskUsageMB",
                (int) ConversionUtils.bytesToMegabytes(unimportantSize));
        mSiteDataSizeText.setText(Formatter.formatFileSize(this, totalSize));
        mUnimportantSiteDataSizeText.setText(Formatter.formatFileSize(this, unimportantSize));
    }

    private class SizeCalculator implements WebsitePermissionsFetcher.WebsitePermissionsCallback {
        @Override
        public void onWebsitePermissionsAvailable(Collection<Website> sites) {
            long siteStorageSize = 0;
            long importantSiteStorageTotal = 0;
            for (Website site : sites) {
                siteStorageSize += site.getTotalUsage();
                if (site.getLocalStorageInfo() != null
                        && site.getLocalStorageInfo().isDomainImportant()) {
                    importantSiteStorageTotal += site.getTotalUsage();
                }
            }
            onSiteStorageSizeCalculated(
                    siteStorageSize, siteStorageSize - importantSiteStorageTotal);
        }
    }

    private class UnimportantSiteDataClearer
            implements WebsitePermissionsFetcher.WebsitePermissionsCallback,
                       StoredDataClearedCallback {
        // We keep track of the number of sites waiting to be cleared, and when it reaches 0 we can
        // set our testing variable.
        private int mNumSitesClearing;
        private long mClearStartTime;

        /**
         * We fetch all the websites and clear all the non-important data. This happens
         * asynchronously, and at the end we update the UI with the new storage numbers.
         */
        public void clearData() {
            Profile profile = Profile.getLastUsedRegularProfile();
            mClearStartTime = SystemClock.elapsedRealtime();
            WebsitePermissionsFetcher fetcher = new WebsitePermissionsFetcher(profile, true);
            fetcher.fetchPreferencesForCategory(SiteSettingsCategory.createFromType(profile,
                                                        SiteSettingsCategory.Type.USE_STORAGE),
                    this);
        }

        @Override
        public void onStoredDataCleared() {
            mNumSitesClearing--;
            if (mNumSitesClearing <= 0) {
                RecordHistogram.recordTimesHistogram("Android.ManageSpace.ClearUnimportantTime",
                        SystemClock.elapsedRealtime() - mClearStartTime);
                clearUnimportantDataDone();
            }
        }

        @Override
        public void onWebsitePermissionsAvailable(Collection<Website> sites) {
            long siteStorageLeft = 0;
            for (Website site : sites) {
                if (site.getLocalStorageInfo() == null
                        || !site.getLocalStorageInfo().isDomainImportant()) {
                    mNumSitesClearing++;
                    site.clearAllStoredData(Profile.getLastUsedRegularProfile(), this);
                } else {
                    siteStorageLeft += site.getTotalUsage();
                }
            }
            if (mNumSitesClearing == 0) {
                onStoredDataCleared();
            }
            onSiteStorageSizeCalculated(siteStorageLeft, 0);
        }
    }

    // If ManageSpaceActivity is exported, then it's vulnerable to a fragment injection exploit:
    // http://securityintelligence.com/new-vulnerability-android-framework-fragment-injection
    private void ensureActivityNotExported() {
        if (sActivityNotExportedChecked) return;
        sActivityNotExportedChecked = true;
        try {
            ActivityInfo activityInfo = getPackageManager().getActivityInfo(getComponentName(), 0);
            if (activityInfo.exported) {
                throw new IllegalStateException("ManageSpaceActivity must not be exported.");
            }
        } catch (NameNotFoundException ex) {
            // Something terribly wrong has happened.
            throw new RuntimeException(ex);
        }
    }
}
