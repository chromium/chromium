// Copyright 2017 The Chromium Authors
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
import android.os.Bundle;
import android.text.TextUtils;
import android.view.View;
import android.widget.RemoteViews;

import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;
import androidx.core.app.ActivityOptionsCompat;

import org.chromium.base.ContextUtils;
import org.chromium.base.IntentUtils;
import org.chromium.base.Log;
import org.chromium.base.shared_preferences.SharedPreferencesManager;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.firstrun.FirstRunFlowSequencer;
import org.chromium.chrome.browser.locale.LocaleManager;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;
import org.chromium.chrome.browser.ui.searchactivityutils.SearchActivityClient;
import org.chromium.chrome.browser.ui.searchactivityutils.SearchActivityExtras.IntentOrigin;
import org.chromium.chrome.browser.ui.searchactivityutils.SearchActivityExtras.SearchType;
import org.chromium.chrome.browser.ui.searchactivityutils.SearchActivityPreferencesManager;
import org.chromium.chrome.browser.ui.searchactivityutils.SearchActivityPreferencesManager.SearchActivityPreferences;

import java.util.function.Consumer;

/**
 * Widget that lets the user search using their default search engine.
 *
 * <p>Because this is a BroadcastReceiver, it dies immediately after it runs. A new one is created
 * for each new broadcast.
 *
 * <p>This class avoids loading the native library because it can be triggered at regular intervals
 * by Android when it tells widgets that they need updates.
 *
 * <p>Methods on instances of this class called directly by Android (when a broadcast is received
 * e.g.) catch all Exceptions up to some number of times before letting them go through to allow us
 * to get a crash stack. This is done to prevent Android from labeling the whole process as "bad"
 * and blocking taps on the widget. See http://crbug.com/712061.
 */
public class SearchWidgetProvider extends AppWidgetProvider {
    /** Wraps up all things that a {@link SearchWidgetProvider} can request things from. */
    static class SearchWidgetProviderDelegate implements Consumer<SearchActivityPreferences> {
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

        /** Returns the {@link SharedPreferencesManager} to store prefs. */
        protected SharedPreferencesManager getChromeSharedPreferences() {
            return ChromeSharedPreferences.getInstance();
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

        @Override
        public void accept(SearchActivityPreferences prefs) {
            performUpdate(null, prefs);
        }
    }

    public static final String EXTRA_FROM_SEARCH_WIDGET =
            "org.chromium.chrome.browser.searchwidget.FROM_SEARCH_WIDGET";

    /** Number of consecutive crashes this widget will absorb before giving up. */
    private static final int CRASH_LIMIT = 3;

    private static final Object DELEGATE_LOCK = new Object();

    @SuppressLint("StaticFieldLeak")
    private static SearchWidgetProviderDelegate sDelegate;

    public static void initialize() {
        SearchActivityPreferencesManager.addObserver(getDelegate());
    }

    @Override
    public void onUpdate(final Context context, final AppWidgetManager manager, final int[] ids) {
        run(
                new Runnable() {
                    @Override
                    public void run() {
                        performUpdate(ids, null);
                    }
                });
    }

    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    public static PendingIntent createIntent(Context context, boolean startVoiceSearch) {
        SearchActivityClient client = new SearchActivityClientImpl();
        // Launch the SearchActivity.
        Intent searchIntent =
                client.createIntent(
                        context,
                        IntentOrigin.SEARCH_WIDGET,
                        null,
                        startVoiceSearch ? SearchType.VOICE : SearchType.TEXT);

        searchIntent.putExtra(EXTRA_FROM_SEARCH_WIDGET, true);

        Bundle optionsBundle =
                ActivityOptionsCompat.makeCustomAnimation(context, R.anim.activity_open_enter, 0)
                        .toBundle();
        return PendingIntent.getActivity(
                context,
                0,
                searchIntent,
                PendingIntent.FLAG_UPDATE_CURRENT
                        | IntentUtils.getPendingIntentMutabilityFlag(false),
                optionsBundle);
    }

    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    public static void performUpdate(int[] ids, SearchActivityPreferences prefs) {
        SearchWidgetProviderDelegate delegate = getDelegate();
        if (ids == null) ids = delegate.getAllSearchWidgetIds();
        if (prefs == null) prefs = SearchActivityPreferencesManager.getCurrent();

        for (int id : ids) {
            RemoteViews views =
                    createWidgetViews(
                            delegate.getContext(),
                            id,
                            prefs.searchEngineName,
                            prefs.voiceSearchAvailable);
            delegate.updateAppWidget(id, views);
        }
    }

    private static RemoteViews createWidgetViews(
            Context context, int id, String engineName, boolean isVoiceSearchAvailable) {
        RemoteViews views =
                new RemoteViews(context.getPackageName(), R.layout.search_widget_template);

        views.setOnClickPendingIntent(R.id.text_container, createIntent(context, false));
        views.setOnClickPendingIntent(R.id.microphone_icon, createIntent(context, true));
        views.setViewVisibility(
                R.id.microphone_icon, isVoiceSearchAvailable ? View.VISIBLE : View.GONE);

        // Update what string is displayed by the widget.
        String text =
                TextUtils.isEmpty(engineName) || !shouldShowFullString()
                        ? context.getString(R.string.search_widget_default)
                        : context.getString(R.string.search_with_product, engineName);
        views.setCharSequence(R.id.title, "setHint", text);

        return views;
    }

    /** Updates the number of consecutive crashes this widget has absorbed. */
    @SuppressLint({"ApplySharedPref", "CommitPrefEdits"})
    static void updateNumConsecutiveCrashes(int newValue) {
        SharedPreferencesManager prefs = getDelegate().getChromeSharedPreferences();
        if (getNumConsecutiveCrashes(prefs) == newValue) return;

        // This metric is committed synchronously because it relates to crashes.
        prefs.writeIntSync(ChromePreferenceKeys.SEARCH_WIDGET_NUM_CONSECUTIVE_CRASHES, newValue);
    }

    @VisibleForTesting
    static int getNumConsecutiveCrashes(SharedPreferencesManager prefs) {
        return prefs.readInt(ChromePreferenceKeys.SEARCH_WIDGET_NUM_CONSECUTIVE_CRASHES);
    }

    private static SearchWidgetProviderDelegate getDelegate() {
        synchronized (DELEGATE_LOCK) {
            if (sDelegate == null) {
                sDelegate = new SearchWidgetProviderDelegate(null);
            }
        }
        return sDelegate;
    }

    @VisibleForTesting
    static void run(Runnable runnable) {
        try {
            runnable.run();
            updateNumConsecutiveCrashes(0);
        } catch (Exception e) {
            int numCrashes =
                    getNumConsecutiveCrashes(getDelegate().getChromeSharedPreferences()) + 1;
            updateNumConsecutiveCrashes(numCrashes);

            if (numCrashes < CRASH_LIMIT) {
                // Absorb the crash.
                Log.e(
                        SearchActivity.TAG,
                        "Absorbing exception caught when attempting to launch widget.",
                        e);
            } else {
                // Too many crashes have happened consecutively.  Let Android handle it.
                throw e;
            }
        }
    }

    static boolean shouldShowFullString() {
        boolean freIsNotNecessary = !FirstRunFlowSequencer.checkIfFirstRunIsNecessary(false, false);
        boolean noNeedToCheckForSearchDialog =
                !LocaleManager.getInstance().needToCheckForSearchEnginePromo();
        return freIsNotNecessary && noNeedToCheckForSearchDialog;
    }

    /** Sets an {@link SearchWidgetProviderDelegate} to interact with. */
    static void setActivityDelegateForTest(SearchWidgetProviderDelegate delegate) {
        assert sDelegate == null;
        sDelegate = delegate;
    }
}
