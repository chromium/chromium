// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.password_manager.settings;

import android.app.Activity;
import android.content.Intent;
import android.net.Uri;
import android.os.Bundle;

import androidx.fragment.app.Fragment;
import androidx.fragment.app.FragmentManager;

import org.chromium.chrome.browser.profiles.Profile;

/** An interface for the implementations of {@link ExportFlow}. */
public interface ExportFlowInterface {
    /** The delegate to provide ExportFlow with essential information from the owning fragment. */
    public interface Delegate {
        /**
         * @return The activity associated with the owning fragment.
         */
        Activity getActivity();

        /**
         * @return The fragment manager associated with the owning fragment.
         */
        FragmentManager getFragmentManager();

        /**
         * @return The ID of the root view of the owning fragment.
         */
        int getViewId();

        /**
         * Runs the activity on the fragment owning the export flow.
         *
         * @param intent The intent to start an activity.
         */
        void runCreateFileOnDiskIntent(Intent intent);

        /**
         * Performs the actions that should happen after the export flow has successfully finished.
         */
        default void onExportFlowSucceeded() {}

        /** Performs the actions that should happen after the export flow has failed. */
        default void onExportFlowFailed() {}

        /** Notifies about that export flow has been canceled. */
        default void onExportFlowCanceled() {}

        /** Return the {@link Profile} associated with the passwords. */
        Profile getProfile();
    }

    /**
     * A hook to be used in the onCreate method of the owning {@link Fragment}. I restores the state
     * of the flow.
     *
     * @param savedInstanceState The {@link Bundle} passed from the fragment's onCreate method.
     * @param delegate The {@link Delegate} for this ExportFlow.
     * @param callerMetricsId The unique string, which identifies the caller. This will be used as
     *     the prefix for metrics histograms names.
     */
    public void onCreate(Bundle savedInstanceState, Delegate delegate, String callerMetricsId);

    /** Starts the password export flow. */
    public void startExporting();

    /**
     * A hook to be used in a {@link Fragment}'s onResume method. I processes the result of the
     * reauthentication.
     */
    public void onResume();

    /** Continues the export flow when password list is available. */
    public void passwordsAvailable();

    /**
     * Saves the passwords into the file (in the form of Uri) passed in.
     *
     * @param passwordsFile The file into which the passwords will be written (expected to be a file
     *         Uri).
     */
    void savePasswordsToDownloads(Uri passwordsFile);
}
