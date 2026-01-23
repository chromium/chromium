// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.document;

import android.app.Activity;
import android.app.ActivityManager;
import android.app.Notification;
import android.app.SearchManager;
import android.content.ActivityNotFoundException;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.os.Bundle;
import android.provider.MediaStore;
import android.text.TextUtils;

import com.google.android.material.color.DynamicColors;

import org.chromium.base.ContextUtils;
import org.chromium.base.IntentUtils;
import org.chromium.base.Log;
import org.chromium.base.PackageManagerUtils;
import org.chromium.base.TraceEvent;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.IntentHandler;
import org.chromium.chrome.browser.LaunchIntentDispatcher;
import org.chromium.chrome.browser.app.tabwindow.TabWindowManagerSingleton;
import org.chromium.chrome.browser.firstrun.FirstRunFlowSequencer;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.intents.BrowserIntentUtils;
import org.chromium.chrome.browser.multiwindow.MultiWindowUtils;
import org.chromium.chrome.browser.notifications.NotificationPlatformBridge;
import org.chromium.chrome.browser.partnercustomizations.PartnerBrowserCustomizations;
import org.chromium.chrome.browser.searchwidget.SearchActivity;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabwindow.TabWindowInfo;
import org.chromium.chrome.browser.webapps.WebappLauncherActivity;
import org.chromium.webapk.lib.common.WebApkConstants;

/**
 * Dispatches incoming intents to the appropriate activity based on the current configuration and
 * Intent fired.
 */
@NullMarked
public class ChromeLauncherActivity extends Activity {
    private static final String TAG = "ActivityDispatcher";

    @Override
    public void onCreate(@Nullable Bundle savedInstanceState) {
        // Third-party code adds disk access to Activity.onCreate. http://crbug.com/619824
        TraceEvent.begin("ChromeLauncherActivity.onCreate");
        boolean unparcelFds = ChromeFeatureList.sUnparcelIntentFileDescriptors.isEnabled();
        setIntent(IntentUtils.sanitizeIntent(getIntent(), unparcelFds));
        // Needs to be called as early as possible, to accurately capture the
        // time at which the intent was received.
        if (BrowserIntentUtils.getLaunchedRealtimeMillis(getIntent()) == -1) {
            BrowserIntentUtils.addLauncherTimestampsToIntent(getIntent());
        }
        super.onCreate(savedInstanceState);

        // TODO(crbug.com/40775606): Figure out a scalable way to apply overlays to
        // activities like this.
        applyThemeOverlays();

        dispatch();
        finish();
        TraceEvent.end("ChromeLauncherActivity.onCreate");
    }

    private void applyThemeOverlays() {
        DynamicColors.applyToActivityIfAvailable(this);
    }

    /**
     * Figure out how to route the Intent. Because this is on the critical path to startup, please
     * avoid making the pathway any more complicated than it already is. Make sure that anything you
     * add _absolutely has_ to be here.
     */
    private void dispatch() {
        Intent intent = getIntent();
        // Read partner browser customizations information asynchronously.
        // We want to initialize early because when there are no tabs to restore, we should possibly
        // show homepage, which might require reading PartnerBrowserCustomizations provider.
        PartnerBrowserCustomizations.getInstance().initializeAsync(getApplicationContext());

        int tabId = IntentHandler.getBringTabToFrontId(intent);

        // Check if a web search Intent is being handled.
        if (processWebSearchIntent(tabId, intent)) return;

        // Check if a LIVE WebappActivity has to be brought back to the foreground.  We can't
        // check for a dead WebappActivity because we don't have that information without a global
        // TabManager.  If that ever lands, code to bring back any Tab could be consolidated
        // here instead of being spread between ChromeTabbedActivity and ChromeLauncherActivity.
        // https://crbug.com/443772, https://crbug.com/522918
        if (WebappLauncherActivity.bringWebappToFront(tabId)) return;

        if (bringTabActivityToFront(tabId, intent)) return;

        // The notification settings cog on the flipped side of Notifications and in the Android
        // Settings "App Notifications" view will open us with a specific category.
        if (intent.hasCategory(Notification.INTENT_CATEGORY_NOTIFICATION_PREFERENCES)) {
            NotificationPlatformBridge.launchNotificationPreferences(intent);
            return;
        }

        // Check if we should push the user through First Run.
        if (FirstRunFlowSequencer.launch(this, intent)) return;

        // Check if we should launch a Custom Tab.
        if (LaunchIntentDispatcher.isCustomTabIntent(intent)) {
            LaunchIntentDispatcher.dispatchToCustomTabActivity(this, intent);
            return;
        }

        // b(357902796): Handle fall-back path for unbound WebAPKs.
        if (isWebApkIntent(intent) && launchWebApk(intent)) return;

        LaunchIntentDispatcher.dispatchToTabbedActivity(this, intent);
    }

    /**
     * Attempts to bring the tabbed activity instance containing the given tab to the foreground.
     *
     * <p>This uses {@link ActivityManager#moveTaskToFront} to reliably switch focus between
     * activity instances in multi-window mode, which is more robust than relying on standard intent
     * delivery via {@code startActivity()}.
     *
     * @param tabId The ID of the tab to bring to the foreground.
     * @param intent The intent to pass to the activity.
     * @return {@code true} if the activity was found and successfully brought to the front, {@code
     *     false} otherwise (indicating that the standard intent path should be used as a fallback).
     */
    private boolean bringTabActivityToFront(int tabId, Intent intent) {
        if (tabId == Tab.INVALID_TAB_ID) return false;

        // Fallback to the standard intent dispatch flow if the sys-call is disabled.
        if (!ChromeFeatureList.sMoveToFrontInLaunchIntentDispatcher.isEnabled()) {
            return false;
        }

        TabWindowInfo windowInfo =
                TabWindowManagerSingleton.getInstance().getTabWindowInfoById(tabId);
        if (windowInfo == null) return false;

        return MultiWindowUtils.launchIntentInInstance(intent, windowInfo.windowId);
    }

    @SuppressWarnings(value = "UnsafeImplicitIntentLaunch")
    private boolean processWebSearchIntent(int tabId, Intent intent) {
        boolean incognito =
                intent.getBooleanExtra(IntentHandler.EXTRA_OPEN_NEW_INCOGNITO_TAB, false);
        String url = IntentHandler.getUrlFromIntent(intent);
        if (url != null || tabId != Tab.INVALID_TAB_ID || incognito) return false;

        String query = null;
        final String action = intent.getAction();
        if (Intent.ACTION_SEARCH.equals(action)
                || MediaStore.INTENT_ACTION_MEDIA_SEARCH.equals(action)) {
            query = IntentUtils.safeGetStringExtra(intent, SearchManager.QUERY);
        }
        if (TextUtils.isEmpty(query)) return false;

        Intent searchIntent = new Intent(Intent.ACTION_WEB_SEARCH);
        searchIntent.putExtra(SearchManager.QUERY, query);

        if (PackageManagerUtils.canResolveActivity(
                searchIntent, PackageManager.GET_RESOLVED_FILTER)) {
            startActivity(searchIntent);
        } else {
            // Phone doesn't have a WEB_SEARCH action handler, open Search Activity with
            // the given query.
            Intent searchActivityIntent = new Intent(Intent.ACTION_MAIN);
            searchActivityIntent.setClass(
                    ContextUtils.getApplicationContext(), SearchActivity.class);
            searchActivityIntent.putExtra(SearchManager.QUERY, query);
            startActivity(searchActivityIntent);
        }
        return true;
    }

    private static boolean isWebApkIntent(Intent intent) {
        return intent != null && intent.hasExtra(WebApkConstants.EXTRA_WEBAPK_PACKAGE_NAME);
    }

    private boolean launchWebApk(Intent intent) {
        // TODO(crbug.com/357902796): it may be possible to save 20ms or so by calling into
        // WebappLauncherActivity code directly instead of sending an intent.

        Intent webApkIntent = new Intent(WebappLauncherActivity.ACTION_START_WEBAPP);
        webApkIntent.setPackage(getPackageName());

        webApkIntent.setFlags(intent.getFlags());

        Bundle copiedExtras = intent.getExtras();
        if (copiedExtras != null) {
            webApkIntent.putExtras(copiedExtras);
        }

        try {
            startActivity(webApkIntent);
        } catch (ActivityNotFoundException e) {
            Log.w(TAG, "Unable to launch browser in WebAPK mode.");
            RecordHistogram.recordBooleanHistogram("WebApk.LaunchFromViewIntent", false);
            return false;
        }

        RecordHistogram.recordBooleanHistogram("WebApk.LaunchFromViewIntent", true);
        return true;
    }
}
