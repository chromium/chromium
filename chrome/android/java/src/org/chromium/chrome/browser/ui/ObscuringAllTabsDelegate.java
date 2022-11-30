// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui;

import android.view.View;

/**
 * An interface to pass around the ability to set a view that is obscuring all tabs on the
 * activity.
 */
public interface ObscuringAllTabsDelegate {
    /**
     * Add a view to the set of views that obscure the content of all tabs for
     * accessibility. As long as this set is nonempty, all tabs should be
     * hidden from the accessibility tree.
     *
     * @param view The view that obscures the contents of all tabs.
     */
    void addViewObscuringAllTabs(View view);

    /**
     * Remove a view that previously obscured the content of all tabs.
     *
     * @param view The view that no longer obscures the contents of all tabs.
     */
    void removeViewObscuringAllTabs(View view);

    /** @return Whether or not any views obscure all tabs. */
    boolean isViewObscuringAllTabs();
}
