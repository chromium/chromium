// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.preferences;

import android.Manifest;
import android.annotation.SuppressLint;
import android.app.Fragment;
import android.content.Intent;
import android.content.Context;
import android.content.pm.ActivityInfo;
import android.content.pm.PackageManager;
import android.content.pm.PackageManager.NameNotFoundException;
import android.content.res.Resources;
import android.graphics.BitmapFactory;
import android.nfc.NfcAdapter;
import android.os.Build;
import android.os.Bundle;
import android.os.Process;
import android.preference.Preference;
import android.preference.PreferenceFragment;
import android.preference.PreferenceFragment.OnPreferenceStartFragmentCallback;
import android.support.v7.app.AppCompatActivity;
import android.util.Log;
import android.view.Menu;
import android.view.MenuItem;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.base.VisibleForTesting;
import org.chromium.base.library_loader.ProcessInitException;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.help.HelpAndFeedback;
import org.chromium.chrome.browser.init.ChromeBrowserInitializer;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileManagerUtils;

import org.chromium.chrome.browser.preferences.PrefServiceBridge;
import org.chromium.chrome.browser.adblock.AdblockBridge;

import java.util.List;

import org.adblockplus.libadblockplus.android.AdblockEngine;
import org.adblockplus.libadblockplus.android.settings.AdblockHelper;
import org.adblockplus.libadblockplus.android.settings.AdblockSettings;
import org.adblockplus.libadblockplus.android.Utils;
import org.adblockplus.libadblockplus.android.settings.BaseSettingsFragment;
import org.adblockplus.libadblockplus.android.settings.GeneralSettingsFragment;
import org.adblockplus.libadblockplus.android.settings.AdblockSettingsStorage;
import org.adblockplus.libadblockplus.android.settings.WhitelistedDomainsSettingsFragment;

/**
 * The Chrome settings activity.
 *
 * This activity displays a single Fragment, typically a PreferenceFragment. As the user navigates
 * through settings, a separate Preferences activity is created for each screen. Thus each fragment
 * may freely modify its activity's action bar or title. This mimics the behavior of
 * android.preference.PreferenceActivity.
 */
public class Preferences extends AppCompatActivity implements
        OnPreferenceStartFragmentCallback,
        BaseSettingsFragment.Provider,
        GeneralSettingsFragment.Listener,
        WhitelistedDomainsSettingsFragment.Listener {

    public static final String EXTRA_SHOW_FRAGMENT = "show_fragment";
    public static final String EXTRA_SHOW_FRAGMENT_ARGUMENTS = "show_fragment_args";

    private static final String TAG = "Preferences";

    /** The current instance of Preferences in the resumed state, if any. */
    private static Preferences sResumedInstance;

    /** Whether this activity has been created for the first time but not yet resumed. */
    private boolean mIsNewlyCreated;

    private static boolean sActivityNotExportedChecked;

    @SuppressLint("InlinedApi")
    @Override
    protected void onCreate(Bundle savedInstanceState) {
        ensureActivityNotExported();

        Log.d(TAG, "Preferences.onCreate() " + this);

        // The browser process must be started here because this Activity may be started explicitly
        // from Android notifications, when Android is restoring Preferences after Chrome was
        // killed, or for tests. This should happen before super.onCreate() because it might
        // recreate a fragment, and a fragment might depend on the native library.
        try {
            ChromeBrowserInitializer.getInstance(this).handleSynchronousStartup();
        } catch (ProcessInitException e) {
            Log.e(TAG, "Failed to start browser process.", e);
            // This can only ever happen, if at all, when the activity is started from an Android
            // notification (or in tests). As such we don't want to show an error messsage to the
            // user. The application is completely broken at this point, so close it down
            // completely (not just the activity).
            System.exit(-1);
            return;
        }

        /*
        if (!AdblockHelper.get().isInit()) {
            Log.e(TAG, "AdblockHelper is not initialized!!!");

             Log.w(TAG, "Adblock: getting isolate pointer async in Thread " + java.lang.Thread.currentThread());
             long isolateProviderPtr = AdblockBridge.getInstance().getIsolateProviderNativePtr();
             Log.w(TAG, "Adblock: got isolate pointer = " + isolateProviderPtr
                 + " in thread " + Thread.currentThread());

            String basePath = getDir(
                 AdblockEngine.BASE_PATH_DIRECTORY,
                 Context.MODE_PRIVATE)
                     .getAbsolutePath();

            AdblockHelper
                .get()
                .init(this, basePath, true, AdblockHelper.PREFERENCE_NAME)
                .useV8IsolateProvider(isolateProviderPtr);
        }
        */

        // for Adblock settings we need AdblockEngine instance
        // TODO: since it's required for Adblock fragments only and it takes relatively
        // lot's of memory we need to create it for Adblock settings UI fragments only
        // WARNING: we should do it before activity is started with super.onCreate() as it adds fragment and
        // it requires ready Adblock engine instace or retaining to be done before
        Log.d(TAG, "Adblock: retaining adblock engine in " + this);

        // synchronously (blocks the UI but allows to get pointer here)
        // TODO: (improvement) do it async and wait for engine to be created
        // and passed when URL is about to load
        if (AdblockHelper.get().getProvider().retain(false)) {
            // pass FilterEngine instance pointer to C++ side
            long ptr = AdblockHelper.get().getProvider().getEngine().getFilterEngine().getNativePtr();
            android.util.Log.w(TAG, "Adblock: Notify C++ FilterEngine is created (passing pointer) " + ptr + " in thread " + Thread.currentThread());
            AdblockBridge.getInstance().setFilterEngineNativePtr(ptr);
        };

        Log.d(TAG, "Adblock: adblock engine counter = " + AdblockHelper.get().getProvider().getCounter());

        super.onCreate(savedInstanceState);

        mIsNewlyCreated = savedInstanceState == null;

        String initialFragment = getIntent().getStringExtra(EXTRA_SHOW_FRAGMENT);
        Bundle initialArguments = getIntent().getBundleExtra(EXTRA_SHOW_FRAGMENT_ARGUMENTS);

        getSupportActionBar().setDisplayHomeAsUpEnabled(true);

        // If savedInstanceState is non-null, then the activity is being
        // recreated and super.onCreate() has already recreated the fragment.
        if (savedInstanceState == null) {
            if (initialFragment == null) initialFragment = MainPreferences.class.getName();
            Fragment fragment = Fragment.instantiate(this, initialFragment, initialArguments);
            getFragmentManager().beginTransaction()
                    .replace(android.R.id.content, fragment)
                    .commit();
        }

        if (ApiCompatibilityUtils.checkPermission(
                this, Manifest.permission.NFC, Process.myPid(), Process.myUid())
                == PackageManager.PERMISSION_GRANTED) {
            // Disable Android Beam on JB and later devices.
            // In ICS it does nothing - i.e. we will send a Play Store link if NFC is used.
            NfcAdapter nfcAdapter = NfcAdapter.getDefaultAdapter(this);
            if (nfcAdapter != null) nfcAdapter.setNdefPushMessage(null, this);
        }


        Resources res = getResources();
        ApiCompatibilityUtils.setTaskDescription(this, res.getString(R.string.app_name),
                BitmapFactory.decodeResource(res, R.mipmap.app_icon),
                ApiCompatibilityUtils.getColor(res, R.color.default_primary_color));
    }

    @Override
    protected void onDestroy() {
        super.onDestroy();

        // don't forget to release Adblock engine
        Log.d(TAG, "Adblock: releasing adblock engine in " + this);
        // if it's the last instance then we need to notify C++ side (stop using it)
        if (AdblockHelper.get().getProvider().release()) {
            Log.w(TAG, "Adblock: Notify C++ side filterEngine can't be used anymore in thread " + Thread.currentThread());
            AdblockBridge.getInstance().setFilterEngineNativePtr(0L);
        };
        Log.d(TAG, "Adblock: adblock engine counter = " + AdblockHelper.get().getProvider().getCounter());
    }

    // OnPreferenceStartFragmentCallback:

    @Override
    public boolean onPreferenceStartFragment(PreferenceFragment preferenceFragment,
            Preference preference) {
        startFragment(preference.getFragment(), preference.getExtras());
        return true;
    }

    /**
     * Starts a new Preferences activity showing the desired fragment.
     *
     * @param fragmentClass The Class of the fragment to show.
     * @param args Arguments to pass to Fragment.instantiate(), or null.
     */
    public void startFragment(String fragmentClass, Bundle args) {
        Intent intent = new Intent(Intent.ACTION_MAIN);
        intent.setClass(this, getClass());
        intent.putExtra(EXTRA_SHOW_FRAGMENT, fragmentClass);
        intent.putExtra(EXTRA_SHOW_FRAGMENT_ARGUMENTS, args);
        startActivity(intent);
    }

    @Override
    public void onAttachedToWindow() {
        super.onAttachedToWindow();
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.LOLLIPOP) {
            Fragment fragment = getFragmentManager().findFragmentById(android.R.id.content);
            if (fragment instanceof PreferenceFragment && fragment.getView() != null) {
                // Set list view padding to 0 so dividers are the full width of the screen.
                fragment.getView().findViewById(android.R.id.list).setPadding(0, 0, 0, 0);
            }
        }
    }

    @Override
    protected void onResume() {
        super.onResume();

        // Prevent the user from interacting with multiple instances of Preferences at the same time
        // (e.g. in multi-instance mode on a Samsung device), which would cause many fun bugs.
        if (sResumedInstance != null && sResumedInstance.getTaskId() != getTaskId()
                && !mIsNewlyCreated) {
            // This activity was unpaused or recreated while another instance of Preferences was
            // already showing. The existing instance takes precedence.
            finish();
        } else {
            // This activity was newly created and takes precedence over sResumedInstance.
            if (sResumedInstance != null && sResumedInstance.getTaskId() != getTaskId()) {
                sResumedInstance.finish();
            }

            sResumedInstance = this;
            mIsNewlyCreated = false;
        }
    }

    @Override
    protected void onPause() {
        super.onPause();
        ProfileManagerUtils.flushPersistentDataForAllProfiles();
    }

    @Override
    protected void onStop() {
        super.onStop();
        if (sResumedInstance == this) sResumedInstance = null;
    }

    /**
     * Returns the fragment showing as this activity's main content, typically a PreferenceFragment.
     * This does not include DialogFragments or other Fragments shown on top of the main content.
     */
    @VisibleForTesting
    public Fragment getFragmentForTest() {
        return getFragmentManager().findFragmentById(android.R.id.content);
    }

    @Override
    public boolean onCreateOptionsMenu(Menu menu) {
        super.onCreateOptionsMenu(menu);
        // By default, every screen in Settings shows a "Help & feedback" menu item.
        MenuItem help = menu.add(
                Menu.NONE, R.id.menu_id_general_help, Menu.CATEGORY_SECONDARY, R.string.menu_help);
        help.setIcon(R.drawable.ic_help_and_feedback);
        return true;
    }

    @Override
    public boolean onPrepareOptionsMenu(Menu menu) {
        if (menu.size() == 1) {
            MenuItem item = menu.getItem(0);
            if (item.getIcon() != null) item.setShowAsAction(MenuItem.SHOW_AS_ACTION_IF_ROOM);
        }
        return super.onPrepareOptionsMenu(menu);
    }

    @Override
    public boolean onOptionsItemSelected(MenuItem item) {
        if (item.getItemId() == android.R.id.home) {
            finish();
            return true;
        } else if (item.getItemId() == R.id.menu_id_general_help) {
            HelpAndFeedback.getInstance(this).show(this, getString(R.string.help_context_settings),
                    Profile.getLastUsedProfile(), null);
            return true;
        }
        return super.onOptionsItemSelected(item);
    }

    private void ensureActivityNotExported() {
        if (sActivityNotExportedChecked) return;
        sActivityNotExportedChecked = true;
        try {
            ActivityInfo activityInfo = getPackageManager().getActivityInfo(getComponentName(), 0);
            // If Preferences is exported, then it's vulnerable to a fragment injection exploit:
            // http://securityintelligence.com/new-vulnerability-android-framework-fragment-injection
            if (activityInfo.exported) {
                throw new IllegalStateException("Preferences must not be exported.");
            }
        } catch (NameNotFoundException ex) {
            // Something terribly wrong has happened.
            throw new RuntimeException(ex);
        }
    }

    // Adblock provider

    @Override
    public AdblockEngine getAdblockEngine() {
        AdblockHelper.get().getProvider().waitForReady();
        return AdblockHelper.get().getProvider().getEngine();
    }

    @Override
    public AdblockSettingsStorage getAdblockSettingsStorage() {
        return AdblockHelper.get().getStorage();
    }

    // listener

    @Override
    public void onAdblockSettingsChanged(BaseSettingsFragment fragment) {
        Log.d(TAG, "AdblockHelper setting changed:\n" + fragment.getSettings().toString());

        // sending settings over JNI to C++ side
        boolean isAdblockEnabled = fragment.getSettings().isAdblockEnabled();
        PrefServiceBridge.getInstance().setAdblockEnabled(isAdblockEnabled);

        List<String> whitelistedDomains = fragment.getSettings().getWhitelistedDomains();
        String[] whitelistedDomainsArray = (whitelistedDomains != null
            ? whitelistedDomains.toArray(new String[whitelistedDomains.size()])
            : new String[0]);
        PrefServiceBridge.getInstance().setAdblockWhitelistedDomains(whitelistedDomainsArray);
    }

    @Override
    public void onWhitelistedDomainsClicked(GeneralSettingsFragment fragment) {
        insertWhitelistedFragment();
    }

    @Override
    public boolean isValidDomain(WhitelistedDomainsSettingsFragment fragment,
                               String domain,
                               AdblockSettings settings) {
        // show error here if domain is invalid
        return domain != null && domain.length() > 0;
    }

    private void insertWhitelistedFragment() {
        getFragmentManager()
            .beginTransaction()
            .replace(
                android.R.id.content,
                WhitelistedDomainsSettingsFragment.newInstance())
            .addToBackStack(WhitelistedDomainsSettingsFragment.class.getSimpleName())
            .commit();
    }
}
