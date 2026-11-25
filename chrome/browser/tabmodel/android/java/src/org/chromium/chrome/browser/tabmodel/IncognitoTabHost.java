// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabmodel;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.profiles.Profile;

/** Implemented by entities that may host incognito tabs. */
@NullMarked
public interface IncognitoTabHost {
    /**
     * Whether there are any incognito tabs at the moment.
     *
     * @param otrProfile The off-the-record {@link Profile} to scope the check to, or {@code null}
     *     to check across all profiles.
     */
    boolean hasIncognitoTabs(@Nullable Profile otrProfile);

    /**
     * Close all incognito tabs across all profiles.
     *
     * <p>TODO(crbug.com/588361413): Scope closure to a specific {@link Profile}.
     */
    void closeAllIncognitoTabs();

    /**
     * An async version of {@link #closeAllIncognitoTabs()}, that will wait for init to finish.
     * Currently closes incognito tabs across all profiles.
     *
     * <p>TODO(crbug.com/588361413): Scope closure to a specific {@link Profile}.
     */
    void closeAllIncognitoTabsOnInit();

    /**
     * Whether there is any active incognito session at the moment for the given off-the-record
     * profile.
     *
     * @param otrProfile The off-the-record {@link Profile} to scope the check to.
     */
    boolean isActiveModel(Profile otrProfile);
}
