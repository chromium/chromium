// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.share;

import android.graphics.drawable.Icon;

import java.util.List;

/** Helper class used to deliver custom action to {@link ShareHelper}. */
public class ChromeCustomShareAction {
    /** Provider interface that wants to provide Share Actions. */
    public interface Provider {
        /**
         * Get a map of actions to the parcel to be used as custom actions sent to Android share
         * sheet.
         *
         * The key for the map is a string identifier of an action; if a custom action is chosen in
         * share sheet, the corresponding runnable will be called.
         */
        List<ChromeCustomShareAction> getCustomActions();
    }

    /** Identifier for the custom action. */
    public final String key;

    /** Icon used for the custom action. */
    public final Icon icon;

    /** Label used below the icon for custom actions. */
    public final String label;

    /** Action when custom action is selected. */
    public final Runnable runnable;

    /**
     * @param key Identifier for the custom action.
     * @param icon Icon used for the custom action.
     * @param label Label used below the icon for custom actions.
     * @param runnable Action when custom action is selected.
     */
    public ChromeCustomShareAction(String key, Icon icon, String label, Runnable runnable) {
        this.key = key;
        this.icon = icon;
        this.label = label;
        this.runnable = runnable;
    }
}
