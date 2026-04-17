// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab_ui;

import org.chromium.build.annotations.NullMarked;

/**
 * Interface for the Tab Switcher button view, designed to be used with the Actions framework. This
 * allows the ActionButtonBinder to push state without knowing the concrete view implementation.
 *
 * <p>Note: This interface should be implemented by a {@link View} in order to be compatible with
 * the Actions framework.
 */
@NullMarked
public interface TabSwitcherButtonView {
    /** Sets the tab count with the proper drawable incognito state. */
    void setTabCount(int tabCount, boolean isIncognito);

    /** Sets whether the notification dot is visible on the button. */
    void setNotificationDotVisible(boolean showDot);

    /** Force the ripple to end so the transition looks correct when showing the tab switcher. */
    void endRippleAnimation();
}
