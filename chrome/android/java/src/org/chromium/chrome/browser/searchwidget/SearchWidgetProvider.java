// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.searchwidget;

import android.annotation.SuppressLint;
import android.app.PendingIntent;
import android.appwidget.AppWidgetManager;
import android.appwidget.AppWidgetProvider;
import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.content.SharedPreferences;
import android.net.Uri;
import android.os.Bundle;
import android.os.StrictMode;
import android.support.v4.app.ActivityOptionsCompat;
import android.text.TextUtils;
import android.view.View;
import android.widget.RemoteViews;

import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.base.ThreadUtils;
import org.chromium.base.library_loader.LibraryLoader;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.IntentHandler;
import org.chromium.chrome.browser.firstrun.FirstRunFlowSequencer;
import org.chromium.chrome.browser.locale.LocaleManager;
import org.chromium.chrome.browser.omnibox.UrlBarData;
import org.chromium.chrome.browser.search_engines.TemplateUrlServiceFactory;
import org.chromium.chrome.browser.util.IntentUtils;
import org.chromium.components.search_engines.TemplateUrl;
import org.chromium.components.search_engines.TemplateUrlService;
import org.chromium.components.search_engines.TemplateUrlService.LoadListener;
import org.chromium.components.search_engines.TemplateUrlService.TemplateUrlServiceObserver;

/**
 * Widget that lets the user search using their default search engine.
 *
 * Because this is a BroadcastReceiver, it dies immediately after it runs.  A new one is created
 * for each new broadcast.
 *
 * This class avoids loading the native library because it can be triggered at regular intervals by
 * Android when it tells widgets that they need updates.
 *
 * Methods on instances of this class called directly by Android (when a broadcast is received e.g.)
 * catch all Exceptions up to some number of times before letting them go through to allow us to get
 * a crash stack.  This is done to prevent Android from labeling the whole process as "bad" and
 * blocking taps on the widget.  See http://crbug.com/712061.
 */
public class SearchWidgetProvider extends AppWidgetProvider {
    /** Wraps up all things that a {@link SearchWidgetProvider} can request things from. */
    static class SearchWidgetProviderDelegate {
        private final Context mContext;
        private final @Nullable AppWidgetManager mManager;

        public SearchWidgetProviderDelegate(Context context) {
            mContext = context == null ? ContextUtils.getApplicationContext() : context;
            mManager = AppWidgetManager.getInstance(mContext);
        }

        /** Returns the Context to pull resources from. */
        protected Context getContext() {
            return mContext;
        }

        /** See {@link ContextUtils#getAppSharedPreferences}. */
        protected SharedPreferences getSharedPreferences() {
            return ContextUtils.getAppSharedPreferences();
        }

        /** Returns IDs for all search widgets that exist. */
        protected int[] getAllSearchWidgetIds() {
            if (mManager == null) return new int[0];
            return mManager.getAppWidgetIds(
                    new ComponentName(getContext(), SearchWidgetProvider.class.getName()));
        }

        /** See {@link AppWidgetManager#updateAppWidget}. */
        protected void updateAppWidget(int id, RemoteViews views) {
            assert mManager != null;
            mManager.updateAppWidget(id, views);
        }
    }

    /** Monitors the TemplateUrlService for changes, updating the widget when necessary. */
    private static final class SearchWidgetTemplateUrlServiceObserver
            implements LoadListener, TemplateUrlServiceObserver {
        @Override
        public void onTemplateUrlServiceLoaded() {
            TemplateUrlServiceFactory.get().unregisterLoadListener(this);
            updateCachedEngineName();
        }

        @Override
        public void onTemplateURLServiceChanged() {
            updateCachedEngineName();
        }

        private void updateCachedEngineName() {
            SearchWidgetProvider.updateCachedEngineName();
        }
    }

    static final String ACTION_START_TEXT_QUERY =
            "org.chromium.chrome.browser.searchwidget.START_TEXT_QUERY";
    static final String ACTION_START_VOICE_QUERY =
            "org.chromium.chrome.browser.searchwidget.START_VOICE_QUERY";
    static final String ACTION_UPDATE_ALL_WIDGETS =
            "org.chromium.chrome.browser.searchwidget.UPDATE_ALL_WIDGETS";

    public static final String EXTRA_START_VOICE_SEARCH =
            "org.chromium.chrome.browser.searchwidget.START_VOICE_SEARCH";

    private static final String PREF_IS_VOICE_SEARCH_AVAILABLE =
            "org.chromium.chrome.browser.searchwidget.IS_VOICE_SEARCH_AVAILABLE";
    private static final String PREF_NUM_CONSECUTIVE_CRASHES =
            "org.chromium.chrome.browser.searchwidget.NUM_CONSECUTIVE_CRASHES";
    static final String PREF_SEARCH_ENGINE_SHORTNAME =
            "org.chromium.chrome.browser.searchwidget.SEARCH_ENGINE_SHORTNAME";

    /** Number of consecutive crashes this widget will absorb before giving up. */
    private static final int CRASH_LIMIT = 3;

    private static final Object DELEGATE_LOCK = new Object();
    private static final Object OBSERVER_LOCK = new Object();

    /** The default search engine's root URL. */
    private static String sDefaultSearchEngineUrl;

    @SuppressLint("StaticFieldLeak")
    private static SearchWidgetTemplateUrlServiceObserver sObserver;
    @SuppressLint("StaticFieldLeak")
    private static SearchWidgetProviderDelegate sDelegate;

    /**
     * Creates the singleton instance of the observer that will monitor for search engine changes.
     * The native library and the browser process must have been fully loaded before calling this.
     */
    public static void initialize() {
        ThreadUtils.assertOnUiThread();
        assert LibraryLoader.getInstance().isInitialized();

        // Set up an observer to monitor for changes.
        synchronized (OBSERVER_LOCK) {
            if (sObserver != null) return;
            sObserver = new SearchWidgetTemplateUrlServiceObserver();

            TemplateUrlService service = TemplateUrlServiceFactory.get();
            service.registerLoadListener(sObserver);
            service.addObserver(sObserver);
            if (!service.isLoaded()) service.load();
        }
        int[] ids = getDelegate().getAllSearchWidgetIds();
        LocaleManager.getInstance().recordLocaleBasedSearchWidgetMetrics(
                ids != null && ids.length > 0);
    }

    /** Nukes all cached information and forces all widgets to start with a blank slate. */
    public static void reset() {
        SharedPreferences.Editor editor = getDelegate().getSharedPreferences().edit();
        editor.remove(PREF_IS_VOICE_SEARCH_AVAILABLE);
        editor.remove(PREF_SEARCH_ENGINE_SHORTNAME);
        editor.apply();

        performUpdate(null);
    }

    @Override
    public void onReceive(final Context context, final Intent intent) {
        run(new Runnable() {
            @Override
            public void run() {
                if (IntentHandler.wasIntentSenderChrome(intent)) {
                    handleAction(intent);
                } else {
                    SearchWidgetProvider.super.onReceive(context, intent);
                }
            }
        });
    }

    @Override
    public void onUpdate(final Context context, final AppWidgetManager manager, final int[] ids) {
        run(new Runnable() {
            @Override
            public void run() {
                performUpdate(ids);
            }
        });
    }

    /** Handles the intent actions to the widget. */
    @VisibleForTesting
    static void handleAction(Intent intent) {
        String action = intent.getAction();
        if (ACTION_START_TEXT_QUERY.equals(action)) {
            startSearchActivity(intent, false);
        } else if (ACTION_START_VOICE_QUERY.equals(action)) {
            startSearchActivity(intent, true);
        } else if (ACTION_UPDATE_ALL_WIDGETS.equals(action)) {
            performUpdate(null);
        } else {
            assert false;
        }
    }

    @VisibleForTesting
    static void startSearchActivity(Intent intent, boolean startVoiceSearch) {
        Log.d(SearchActivity.TAG, "Launching SearchActivity: VOICE=" + startVoiceSearch);
        Context context = getDelegate().getContext();

        // Abort if the user needs to go through First Run.
        if (FirstRunFlowSequencer.launch(context, intent, true /* requiresBroadcast */,
                    false /* preferLightweightFre */)) {
            return;
        }

        // Launch the SearchActivity.
        Intent searchIntent = new Intent();
        searchIntent.setClass(context, SearchActivity.class);
        searchIntent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        searchIntent.addFlags(Intent.FLAG_ACTIVITY_NEW_DOCUMENT);
        searchIntent.putExtra(EXTRA_START_VOICE_SEARCH, startVoiceSearch);

        Bundle optionsBundle =
                ActivityOptionsCompat.makeCustomAnimation(context, R.anim.activity_open_enter, 0)
                        .toBundle();
        IntentUtils.safeStartActivity(context, searchIntent, optionsBundle);
    }

    private static void performUpdate(int[] ids) {
        SearchWidgetProviderDelegate delegate = getDelegate();

        if (ids == null) ids = delegate.getAllSearchWidgetIds();
        if (ids.length == 0) return;

        SharedPreferences prefs = delegate.getSharedPreferences();
        boolean isVoiceSearchAvailable = getCachedVoiceSearchAvailability(prefs);
        String engineName = getCachedEngineName(prefs);

        for (int id : ids) {
            RemoteViews views = createWidgetViews(
                    delegate.getContext(), id, engineName, isVoiceSearchAvailable);
            delegate.updateAppWidget(id, views);
        }
    }

    private static RemoteViews createWidgetViews(
            Context context, int id, String engineName, boolean isVoiceSearchAvailable) {
        RemoteViews views =
                new RemoteViews(context.getPackageName(), R.layout.search_widget_template);

        // Clicking on the widget fires an Intent back at this BroadcastReceiver, allowing control
        // over how the Activity is animated when it starts up.
        Intent textIntent = createStartQueryIntent(context, ACTION_START_TEXT_QUERY, id);
        views.setOnClickPendingIntent(R.id.text_container,
                PendingIntent.getBroadcast(
                        context, 0, textIntent, PendingIntent.FLAG_UPDATE_CURRENT));

        // If voice search is available, clicking on the microphone triggers a voice query.
        if (isVoiceSearchAvailable) {
            Intent voiceIntent = createStartQueryIntent(context, ACTION_START_VOICE_QUERY, id);
            views.setOnClickPendingIntent(R.id.microphone_icon,
                    PendingIntent.getBroadcast(
                            context, 0, voiceIntent, PendingIntent.FLAG_UPDATE_CURRENT));
            views.setViewVisibility(R.id.microphone_icon, View.VISIBLE);
        } else {
            views.setViewVisibility(R.id.microphone_icon, View.GONE);
        }

        // Update what string is displayed by the widget.
        String text = TextUtils.isEmpty(engineName) || !shouldShowFullString()
                ? context.getString(R.string.search_widget_default)
                : context.getString(R.string.search_with_product, engineName);
        views.setCharSequence(R.id.title, "setHint", text);

        return views;
    }

    /** Creates a trusted Intent that lets the user begin performing queries. */
    private static Intent createStartQueryIntent(Context context, String action, int widgetId) {
        Intent intent = new Intent(action, Uri.parse(String.valueOf(widgetId)));
        intent.setClass(context, SearchWidgetProvider.class);
        IntentHandler.addTrustedIntentExtras(intent);
        return intent;
    }

    /** Caches whether or not a voice search is possible. */
    static void updateCachedVoiceSearchAvailability(boolean isVoiceSearchAvailable) {
        SharedPreferences prefs = getDelegate().getSharedPreferences();
        if (getCachedVoiceSearchAvailability(prefs) != isVoiceSearchAvailable) {
            prefs.edit().putBoolean(PREF_IS_VOICE_SEARCH_AVAILABLE, isVoiceSearchAvailable).apply();
            performUpdate(null);
        }
    }

    /** Attempts to update the cached search engine name. */
    public static void updateCachedEngineName() {
        ThreadUtils.assertOnUiThread();
        if (!LibraryLoader.getInstance().isInitialized()) return;

        // Getting an instance of the TemplateUrlService requires that the native library be
        // loaded, but the TemplateUrlService also itself needs to be initialized.
        TemplateUrlService service = TemplateUrlServiceFactory.get();
        if (!service.isLoaded()) return;

        // Update the URL that we show for zero-suggest.
        TemplateUrl dseTemplateUrl = service.getDefaultSearchEngineTemplateUrl();
        String engineName = null;
        if (dseTemplateUrl != null) {
            String searchEngineUrl =
                    service.getSearchEngineUrlFromTemplateUrl(dseTemplateUrl.getKeyword());
            UrlBarData urlBarData = UrlBarData.forUrl(searchEngineUrl);
            sDefaultSearchEngineUrl =
                    urlBarData.displayText
                            .subSequence(urlBarData.originStartIndex, urlBarData.originEndIndex)
                            .toString();
            engineName = dseTemplateUrl.getShortName();
        }

        updateCachedEngineName(engineName);
    }

    /**
     * Updates the name of the user's default search engine that is cached in SharedPreferences.
     * Caching it in SharedPreferences prevents us from having to load the native library and the
     * TemplateUrlService whenever the widget is updated.
     */
    static void updateCachedEngineName(String engineName) {
        SharedPreferences prefs = getDelegate().getSharedPreferences();

        if (!shouldShowFullString()) engineName = null;

        if (!TextUtils.equals(getCachedEngineName(prefs), engineName)) {
            prefs.edit().putString(PREF_SEARCH_ENGINE_SHORTNAME, engineName).apply();
            performUpdate(null);
        }
    }

    /** Updates the number of consecutive crashes this widget has absorbed. */
    @SuppressLint({"ApplySharedPref", "CommitPrefEdits"})
    static void updateNumConsecutiveCrashes(int newValue) {
        SharedPreferences prefs = getDelegate().getSharedPreferences();
        if (getNumConsecutiveCrashes(prefs) == newValue) return;

        SharedPreferences.Editor editor = prefs.edit();
        if (newValue == 0) {
            editor.remove(PREF_NUM_CONSECUTIVE_CRASHES);
        } else {
            editor.putInt(PREF_NUM_CONSECUTIVE_CRASHES, newValue);
        }

        // This metric is committed synchronously because it relates to crashes.
        StrictMode.ThreadPolicy oldPolicy = StrictMode.allowThreadDiskWrites();
        try {
            editor.commit();
        } finally {
            StrictMode.setThreadPolicy(oldPolicy);
        }
    }

    private static boolean getCachedVoiceSearchAvailability(SharedPreferences prefs) {
        return prefs.getBoolean(PREF_IS_VOICE_SEARCH_AVAILABLE, true);
    }

    private static String getCachedEngineName(SharedPreferences prefs) {
        return prefs.getString(PREF_SEARCH_ENGINE_SHORTNAME, null);
    }

    @VisibleForTesting
    static int getNumConsecutiveCrashes(SharedPreferences prefs) {
        return prefs.getInt(PREF_NUM_CONSECUTIVE_CRASHES, 0);
    }

    private static SearchWidgetProviderDelegate getDelegate() {
        synchronized (DELEGATE_LOCK) {
            if (sDelegate == null) sDelegate = new SearchWidgetProviderDelegate(null);
        }
        return sDelegate;
    }

    @VisibleForTesting
    static void run(Runnable runnable) {
        try {
            runnable.run();
            updateNumConsecutiveCrashes(0);
        } catch (Exception e) {
            int numCrashes = getNumConsecutiveCrashes(getDelegate().getSharedPreferences()) + 1;
            updateNumConsecutiveCrashes(numCrashes);

            if (numCrashes < CRASH_LIMIT) {
                // Absorb the crash.
                Log.e(SearchActivity.TAG,
                        "Absorbing exception caught when attempting to launch widget.", e);
            } else {
                // Too many crashes have happened consecutively.  Let Android handle it.
                throw e;
            }
        }
    }

    static boolean shouldShowFullString() {
        boolean freIsNotNecessary = !FirstRunFlowSequencer.checkIfFirstRunIsNecessary(null, false);
        boolean noNeedToCheckForSearchDialog =
                !LocaleManager.getInstance().needToCheckForSearchEnginePromo();
        return freIsNotNecessary && noNeedToCheckForSearchDialog;
    }

    /** Sets an {@link SearchWidgetProviderDelegate} to interact with. */
    @VisibleForTesting
    static void setActivityDelegateForTest(SearchWidgetProviderDelegate delegate) {
        assert sDelegate == null;
        sDelegate = delegate;
    }

    /** See {@link #sDefaultSearchEngineUrl}. */
    static String getDefaultSearchEngineUrl() {
        // TODO(yusufo): Get rid of this.
        return sDefaultSearchEngineUrl;
    }
}
