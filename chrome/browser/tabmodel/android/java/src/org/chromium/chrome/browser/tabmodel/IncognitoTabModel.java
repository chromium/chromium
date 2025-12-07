// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabmodel;

import org.chromium.build.annotations.NullMarked;

/** A {@link TabModel} which also emits events relevant to incognito tabs. */
@NullMarked
public interface IncognitoTabModel extends TabModel {
    /**
     * Subscribes an {@link IncognitoTabModelObserver} to be notified about incognito events.
     *
     * @param observer The observer to be subscribed.
     */
    void addIncognitoObserver(IncognitoTabModelObserver observer);

    /**
     * Unsubscribes an {@link IncognitoTabModelObserver}.
     * @param observer The observer to be unsubscribed.
     */
    void removeIncognitoObserver(IncognitoTabModelObserver observer);
}
