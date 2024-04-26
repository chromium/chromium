// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.password_check;

import android.content.Context;
import android.content.Intent;
import android.view.MenuItem;

/** This component is responsible for handling the UI logic for the password check. */
public interface PasswordCheckComponentUi {
    /** A delegate that handles native tasks for the UI component. */
    interface Delegate {
        /**
         * Launch the UI allowing the user to edit the given credential.
         *
         * @param credential A {@link CompromisedCredential} to be edited.
         * @param context The context to launch the editing UI from.
         */
        void onEditCredential(CompromisedCredential credential, Context context);

        /**
         * Remove the given credential from the password store.
         * @param credential A {@link CompromisedCredential}.
         */
        void removeCredential(CompromisedCredential credential);
    }

    /**
     * Functional interface to start a Chrome Custom Tab for the given intent, e.g. by using {@link
     * org.chromium.chrome.browser.LaunchIntentDispatcher#createCustomTabActivityIntent}.
     * TODO(crbug.com/40134591): Remove this when the LaunchIntentDispatcher is modularized.
     */
    interface CustomTabIntentHelper {
        /**
         * @see org.chromium.chrome.browser.LaunchIntentDispatcher#createCustomTabActivityIntent
         */
        Intent createCustomTabActivityIntent(Context context, Intent intent);
    }

    /**
     * Functional interface to append trusted extras to the given intent, e.g. by using {@link
     * org.chromium.chrome.browser.IntentUtils.addTrustedIntentExtras(Intent)}.
     * TODO(crbug.com/40134591): Remove this when the IntentHandler is available in a module.
     */
    interface TrustedIntentHelper {
        /**
         * @see org.chromium.chrome.browser.IntentUtils.addTrustedIntentExtras(Intent)
         */
        void addTrustedIntentExtras(Intent intent);
    }

    /**
     * Handle the request of the user to show the help page for the Check Passwords view.
     * @param item A {@link MenuItem}.
     */
    boolean handleHelp(MenuItem item);

    /** Forwards the signal that the fragment was started. */
    void onStartFragment();

    /** Forwards the signal that the fragment is being resumed. */
    void onResumeFragment();

    /** Forwards the signal that the fragment is being destroyed. */
    void onDestroyFragment();

    /** Tears down the component when it's no longer needed. */
    void destroy();
}
