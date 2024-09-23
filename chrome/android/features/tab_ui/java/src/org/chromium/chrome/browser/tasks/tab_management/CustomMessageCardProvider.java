// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import android.view.View;

import org.chromium.chrome.browser.tasks.tab_management.MessageCardViewProperties.MessageCardScope;
import org.chromium.chrome.browser.tasks.tab_management.TabListModel.CardProperties.ModelType;

/** An interface that retrieves and sets information on the custom message card view. */
public interface CustomMessageCardProvider {
    /** Returns the specific message type for this custom message card. */
    public int getMessageType();

    /** Retrieve the inflated custom view for this message card. */
    public View getCustomView();

    /**
     * Retrieve the desired visibility control in normal or incognito mode for the custom message
     * card view.
     */
    public @MessageCardScope int getMessageCardVisibilityControl();

    /** Retrieve the indicated card type for the custom message card view. */
    public @ModelType int getCardType();

    /**
     * Set whether this message card will be displayed in incognito mode or not.
     *
     * @param isIncognito Whether the tab switcher will open in incognito.
     */
    public void setIsIncognito(boolean isIncognito);
}
