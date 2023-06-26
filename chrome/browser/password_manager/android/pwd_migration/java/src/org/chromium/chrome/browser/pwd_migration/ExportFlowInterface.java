// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.pwd_migration;

import android.app.Activity;
import android.os.Bundle;

import androidx.fragment.app.FragmentManager;

/**
 * An interface for the implementations of {@link ExportFlow}.
 */
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
    }

    /**
     * A hook to be used in the onCreate method of the owning {@link Fragment}. I restores the state
     * of the flow.
     *
     * @param savedInstanceState The {@link Bundle} passed from the fragment's onCreate
     * method.
     * @param delegate The {@link Delegate} for this ExportFlow.
     */
    public void onCreate(Bundle savedInstanceState, Delegate delegate);

    /**
     * Starts the password export flow.
     */
    public void startExporting();

    /**
     * A hook to be used in a {@link Fragment}'s onResume method. I processes the result of the
     * reauthentication.
     */
    public void onResume();
}
