// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs.content;

import static org.chromium.chrome.browser.dependency_injection.ChromeCommonQualifiers.ACTIVITY_CONTEXT;

import android.content.Context;
import android.content.Intent;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.browser.customtabs.CustomTabsSessionToken;

import org.chromium.chrome.browser.customtabs.CustomTabIntentDataProvider;
import org.chromium.chrome.browser.dependency_injection.ActivityScope;
import org.chromium.chrome.browser.tab.Tab;

import javax.inject.Inject;
import javax.inject.Named;

/**
 * Handles the incoming intents: the one that starts the activity, as well as subsequent intents
 * received in onNewIntent.
 */
@ActivityScope
public class CustomTabIntentHandler {
    private final CustomTabActivityTabProvider mTabProvider;
    private final CustomTabIntentDataProvider mIntentDataProvider;
    private final CustomTabIntentHandlingStrategy mHandlingStrategy;
    private final IntentIgnoringCriterion mIntentIgnoringCriterion;
    private final Context mContext;
    @Nullable
    private Runnable mOnTabCreatedRunnable;

    @Inject
    public CustomTabIntentHandler(CustomTabActivityTabProvider tabProvider,
            CustomTabIntentDataProvider intentDataProvider,
            CustomTabIntentHandlingStrategy handlingStrategy,
            IntentIgnoringCriterion intentIgnoringCriterion,
            @Named(ACTIVITY_CONTEXT) Context context) {
        mTabProvider = tabProvider;
        mIntentDataProvider = intentDataProvider;
        mHandlingStrategy = handlingStrategy;
        mIntentIgnoringCriterion = intentIgnoringCriterion;
        mContext = context;

        observeInitialTabCreationIfNecessary();
        handleInitialIntent();
    }

    private void observeInitialTabCreationIfNecessary() {
        if (mTabProvider.getTab() != null) {
            return;
        }
        // Note that only one observer and one Runnable exists: if multiple intents arrive before
        // native init, we want to handle only the last one.
        mTabProvider.addObserver(new CustomTabActivityTabProvider.Observer() {
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
        runWhenTabCreated(() -> {
            if (mTabProvider.getInitialTabCreationMode() != TabCreationMode.RESTORED) {
                mHandlingStrategy.handleInitialIntent(mIntentDataProvider);
            }
        });
    }

    /**
     * Called from Activity#onNewIntent.
     *
     * @param intentDataProvider Data provider built from the new intent. It's different from
     * the injectable instance of {@link CustomTabIntentDataProvider} - that one is always built
     * from the initial intent.
     */
    public boolean onNewIntent(CustomTabIntentDataProvider intentDataProvider) {
        Intent intent = intentDataProvider.getIntent();
        CustomTabsSessionToken session = intentDataProvider.getSession();
        if (session == null || !session.equals(mIntentDataProvider.getSession())) {
            assert false : "New intent delivered into a Custom Tab with a different session";
            int flagsToRemove = Intent.FLAG_ACTIVITY_SINGLE_TOP | Intent.FLAG_ACTIVITY_CLEAR_TOP;
            intent.setFlags((intent.getFlags() & ~flagsToRemove) | Intent.FLAG_ACTIVITY_NEW_TASK);
            mContext.startActivity(intent);
            return false;
        }

        if (mIntentIgnoringCriterion.shouldIgnoreIntent(intent)) {
            return false;
        }

        runWhenTabCreated(() ->
            mHandlingStrategy.handleNewIntent(intentDataProvider)
        );

        return true;
    }

    private void runWhenTabCreated(Runnable runnable) {
        if (mTabProvider.getTab() != null) {
            runnable.run();
        } else {
            mOnTabCreatedRunnable = runnable;
        }
    }

    /**
     * Represents Chrome-wide rules for ignoring Intents.
     */
    public interface IntentIgnoringCriterion {
        /**
         * Returns whether given intent should be ignored.
         */
        boolean shouldIgnoreIntent(Intent intent);
    }
}
