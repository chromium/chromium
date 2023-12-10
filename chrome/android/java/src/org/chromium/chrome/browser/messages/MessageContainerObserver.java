// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.messages;

/** An interface for notifications about the state of the message container */
public interface MessageContainerObserver {
    /** A notification that the message container has been shown */
    void onShowMessageContainer();

    /** A notification that the message container has been hidden */
    void onHideMessageContainer();
}
