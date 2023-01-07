// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp;

import android.view.View;
import android.widget.CompoundButton;

/**
 * This interface includes methods that are shared in LegacyIncognitoDescriptionView and
 * RevampedIncognitoDescriptionView.
 */
public interface IncognitoDescriptionView {
    /**
     * Set learn more on click listener.
     * @param listener The given listener.
     */
    void setLearnMoreOnclickListener(View.OnClickListener listener);

    /**
     * Set cookie controls toggle's checked value.
     * @param enabled The value to set the toggle to.
     */
    void setCookieControlsToggle(boolean enabled);

    /**
     * Set cookie controls toggle on checked change listerner.
     * @param listener The given listener.
     */
    void setCookieControlsToggleOnCheckedChangeListener(
            CompoundButton.OnCheckedChangeListener listener);

    /**
     * Sets the cookie controls enforced state.
     * @param enforcement A CookieControlsEnforcement enum type indicating the type of
     *         enforcement policy being applied to Cookie Controls.
     */
    void setCookieControlsEnforcement(int enforcement);

    /**
     * Add click listener that redirects user to the Cookie Control Settings.
     * @param listener The given listener.
     */
    void setCookieControlsIconOnclickListener(View.OnClickListener listener);
}
