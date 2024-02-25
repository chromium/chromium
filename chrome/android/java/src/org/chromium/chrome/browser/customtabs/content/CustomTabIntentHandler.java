// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs.content;

import static org.chromium.chrome.browser.dependency_injection.ChromeCommonQualifiers.ACTIVITY_CONTEXT;

import android.content.Context;
import android.content.Intent;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.browser.customtabs.CustomTabsSessionToken;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.chrome.browser.browserservices.intents.BrowserServicesIntentDataProvider;
import org.chromium.chrome.browser.browserservices.intents.WebappExtras;
import org.chromium.chrome.browser.customtabs.CustomTabsConnection;
import org.chromium.chrome.browser.customtabs.features.minimizedcustomtab.CustomTabMinimizationManagerHolder;
import org.chromium.chrome.browser.dependency_injection.ActivityScope;
import org.chromium.chrome.browser.flags.ActivityType;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.net.NetworkChangeNotifier;

import javax.inject.Inject;
import javax.inject.Named;

/**
 * Handles the incoming intents: the one that starts the activity, as well as subsequent intents
 * received in onNewIntent.
 */
@ActivityScope
public class CustomTabIntentHandler {
    private final CustomTabActivityTabProvider mTabProvider;
    private final BrowserServicesIntentDataProvider mIntentDataProvider;
    private final CustomTabIntentHandlingStrategy mHandlingStrategy;
    private final IntentIgnoringCriterion mIntentIgnoringCriterion;
    private final Context mContext;
    @Nullable private Runnable mOnTabCreatedRunnable;
    private final CustomTabMinimizationManagerHolder mMinimizationManagerHolder;

    @Inject
    public CustomTabIntentHandler(
            CustomTabActivityTabProvider tabProvider,
            BrowserServicesIntentDataProvider intentDataProvider,
            CustomTabIntentHandlingStrategy handlingStrategy,
            IntentIgnoringCriterion intentIgnoringCriterion,
            @Named(ACTIVITY_CONTEXT) Context context,
            CustomTabMinimizationManagerHolder minimizationManagerHolder) {
        mTabProvider = tabProvider;
        mIntentDataProvider = intentDataProvider;
        mHandlingStrategy = handlingStrategy;
        mIntentIgnoringCriterion = intentIgnoringCriterion;
        mContext = context;
        mMinimizationManagerHolder = minimizationManagerHolder;

        observeInitialTabCreationIfNecessary();
        handleInitialIntent();
    }

    private void observeInitialTabCreationIfNecessary() {
        if (mTabProvider.getTab() != null) {
            return;
        }
        // Note that only one observer and one Runnable exists: if multiple intents arrive before
        // native init, we want to handle only the last one.
        mTabProvider.addObserver(
                new CustomTabActivityTabProvider.Observer() {
                    @Override
                    public void onInitialTabCreated(@NonNull Tab tab, @TabCreationMode int mode) {
                        if (mOnTabCreatedRunnable != null) {
                            mOnTabCreatedRunnable.run();
                            mOnTabCreatedRunnable = null;
                        }
                        mTabProvider.removeObserver(this);
                    }
                });
    }

    private void handleInitialIntent() {
        runWhenTabCreated(
                () -> {
                    if (mTabProvider.getInitialTabCreationMode() != TabCreationMode.RESTORED) {
                        mHandlingStrategy.handleInitialIntent(mIntentDataProvider);
                    } else if (mIntentDataProvider.getActivityType() == ActivityType.WEBAPP
                            && NetworkChangeNotifier.isOnline()) {
                        mTabProvider.getTab().reloadIgnoringCache();
                    }
                });
    }

    /**
     * Called from Activity#onNewIntent.
     *
     * @param intentDataProvider Data provider built from the new intent. It's different from
     * the injectable instance of {@link BrowserServicesIntentDataProvider} - that one is always
     * built from the initial intent.
     */
    public boolean onNewIntent(BrowserServicesIntentDataProvider intentDataProvider) {
        Intent intent = intentDataProvider.getIntent();
        CustomTabsSessionToken session = intentDataProvider.getSession();
        WebappExtras webappExtras = intentDataProvider.getWebappExtras();
        if (webappExtras != null) {
            // Don't navigate if the purpose of the intent was to bring the webapp to the
            // foreground.
            if (!webappExtras.shouldForceNavigation) return false;
        } else if (session == null || !session.equals(mIntentDataProvider.getSession())) {
            assert false : "New intent delivered into a Custom Tab with a different session";
            int flagsToRemove = Intent.FLAG_ACTIVITY_SINGLE_TOP | Intent.FLAG_ACTIVITY_CLEAR_TOP;
            intent.setFlags((intent.getFlags() & ~flagsToRemove) | Intent.FLAG_ACTIVITY_NEW_TASK);
            mContext.startActivity(intent);
            return false;
        }

        if (mIntentIgnoringCriterion.shouldIgnoreIntent(intent)) {
            return false;
        }

        // If we receive an intent that's reusing a session id while the tab is minimized, we should
        // close and reopen the Activity to practically 'unminimize' it. Otherwise, a navigation
        // would happen within the minimized tab, making it hard for the user to notice.
        var minimizeDelegate = mMinimizationManagerHolder.getMinimizationManager();
        if (minimizeDelegate != null && minimizeDelegate.isMinimized()) {
            var handler = CustomTabsConnection.getInstance().getEngagementSignalsHandler(session);
            if (handler != null) {
                // We're closing the Custom Tab to be reopened. Notify the Engagement Signals, so
                // that we don't send an onSessionEnded signal while the session is still alive.
                handler.notifyTabWillCloseAndReopenWithSessionReuse();
            }
            RecordHistogram.recordBooleanHistogram(
                    "CustomTabs.Minimized.ReceivedIntentReusingSession", true);
            minimizeDelegate.dismiss();
            return false;
        }

        runWhenTabCreated(() -> mHandlingStrategy.handleNewIntent(intentDataProvider));

        return true;
    }

    private void runWhenTabCreated(Runnable runnable) {
        if (mTabProvider.getTab() != null) {
            runnable.run();
        } else {
            mOnTabCreatedRunnable = runnable;
        }
    }

    /** Represents Chrome-wide rules for ignoring Intents. */
    public interface IntentIgnoringCriterion {
        /** Returns whether given intent should be ignored. */
        boolean shouldIgnoreIntent(Intent intent);
    }
}
