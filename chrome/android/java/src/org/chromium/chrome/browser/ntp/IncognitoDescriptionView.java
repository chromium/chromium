// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp;

import android.view.View;
import android.widget.CompoundButton;

/**
 * This interface includes method that are shared in LegacyIncognitoDescriptionView and
 * RevampedIncognitoDescriptionView.
 */
public interface IncognitoDescriptionView {
    /**
     * Set NTP Header
     * @param newTabPageHeader
     */
    void setNewTabHeader(String newTabPageHeader);

    /**
     * Set learn more on click listerner.
     * @param listener The given listener.
     */
    void setLearnMoreOnclickListener(View.OnClickListener listener);

    /**
     * Adjust the Cookie Controls Card.
     * @param showCard A boolean indicating if the card should be visible or not.
     */
    default void showCookieControlsCard(boolean showCard) {}

    /**
     * Set cookie controls toggle's checked value.
     * @param enabled The value to set the toggle to.
     */
    default void setCookieControlsToggle(boolean enabled) {}

    /**
     * Set cookie controls toggle on checked change listerner.
     * @param listener The given listener.
     */
    default void setCookieControlsToggleOnCheckedChangeListener(
            CompoundButton.OnCheckedChangeListener listener) {}

    /**
     * Sets the cookie controls enforced state.
     * @param enforcement A CookieControlsEnforcement enum type indicating the type of
     *         enforcement policy being applied to Cookie Controls.
     */
    default void setCookieControlsEnforcement(int enforcement) {}

    /**
     * Set cookie controls icon on click listener.
     * @param listener The given listener.
     */
    default void setCookieControlsIconOnclickListener(View.OnClickListener listener) {}
}
