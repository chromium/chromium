// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.password_entry_edit;

import android.os.Bundle;
import androidx.preference.PreferenceFragmentCompat;

/**
 * This class is responsible for rendering the edit fragment where users can edit a saved password.
 */
public class CredentialEditFragmentView extends PreferenceFragmentCompat {
    private ComponentStateDelegate mComponentStateDelegate;

    // TODO(crbug.com/1178519): The coordinator should be made a LifecycleObserver instead.
    interface ComponentStateDelegate {
        /**
         * Signals that the component is no longer needed.
         */
        void onDestroy();
    }

    /**
     * Sets the delegate that handles view events which affect the state of the component
     *
     * @param componentStateDelegate The delegate handling the view events.
     **/
    void setComponentStateDelegate(ComponentStateDelegate componentStateDelegate) {
        mComponentStateDelegate = componentStateDelegate;
    }

    @Override
    public void onCreatePreferences(Bundle bundle, String rootKey) {
        getActivity().setTitle(R.string.password_entry_viewer_edit_stored_password_action_title);
    }

    @Override
    public void onDestroy() {
        super.onDestroy();
        if (getActivity().isFinishing() && mComponentStateDelegate != null) {
            mComponentStateDelegate.onDestroy();
        }
    }

    void dismiss() {
        getActivity().finish();
    }
}
