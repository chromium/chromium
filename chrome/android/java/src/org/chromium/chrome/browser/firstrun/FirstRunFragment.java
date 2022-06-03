// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.firstrun;

import android.app.Activity;

/**
 * This interface is implemented by FRE fragments.
 */
public interface FirstRunFragment {
    /**
     * Notifies this fragment that native has been initialized.
     */
    default void onNativeInitialized() {}

    /**
     * @see Fragment#getActivity().
     */
    Activity getActivity();

    /**
     * Set the a11y focus when the fragment is shown on the screen.
     *
     * Android ViewPager cannot always assign the correct a11y focus automatically when switching
     * between pages. See https://crbug.com/1094064 for more detail.
     *
     * Note that this function can be called before views for the fragment is created. To avoid NPE,
     * it is suggested to add null checker inside this function implementation. See
     * https://crbug.com/1140174 for more detail.
     */
    void setInitialA11yFocus();

    /**
     * Convenience method to get {@link FirstRunPageDelegate}.
     */
    default FirstRunPageDelegate getPageDelegate() {
        return (FirstRunPageDelegate) getActivity();
    }

    /**
     * Reset the fragment state. This can be used when the fragment is revisited with back button.
     */
    default void reset() {}
}
