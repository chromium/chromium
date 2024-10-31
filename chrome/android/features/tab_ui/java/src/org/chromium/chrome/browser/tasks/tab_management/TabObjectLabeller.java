// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import androidx.annotation.Nullable;
import androidx.annotation.StringRes;

import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.browser_ui.widget.async_image.AsyncImageView;
import org.chromium.components.collaboration.messaging.PersistentMessage;

import java.util.Collections;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

/**
 * A partial implementation for pushing labels to to a {@link TabListNotificationHandler} for some
 * sort of tab object, like actual tabs or tab groups. Can be triggered by {{@link #showAll()}} or
 * concrete implementations may trigger of other observers.
 */
public abstract class TabObjectLabeller extends TabObjectNotificationUpdater {
    public TabObjectLabeller(
            Profile profile, TabListNotificationHandler tabListNotificationHandler) {
        super(profile, tabListNotificationHandler);
    }

    @Override
    public void showAll() {
        Map<Integer, TabCardLabelData> cardLabels = new HashMap<>();
        for (PersistentMessage message : getAllMessages()) {
            if (shouldApply(message)) {
                cardLabels.put(getTabId(message), buildLabelData(message));
            }
        }
        if (!cardLabels.isEmpty()) {
            mTabListNotificationHandler.updateTabCardLabels(cardLabels);
        }
    }

    @Override
    protected void incrementalShow(PersistentMessage message) {
        if (shouldApply(message)) {
            int tabId = getTabId(message);
            Map<Integer, TabCardLabelData> cardLabels =
                    Collections.singletonMap(tabId, buildLabelData(message));
            mTabListNotificationHandler.updateTabCardLabels(cardLabels);
        }
    }

    @Override
    protected void incrementalHide(PersistentMessage message) {
        if (shouldApply(message)) {
            int tabId = getTabId(message);
            Map<Integer, TabCardLabelData> cardLabels = Collections.singletonMap(tabId, null);
            mTabListNotificationHandler.updateTabCardLabels(cardLabels);
        }
    }

    /** If the given message should be applied or ignored. */
    protected abstract boolean shouldApply(PersistentMessage message);

    /** The resource for the text to be shown on the label. */
    protected abstract @StringRes int getTextRes(PersistentMessage message);

    /** Fetch all relevant messages that should be shown. */
    protected abstract List<PersistentMessage> getAllMessages();

    /** Return the associated tab id for a given message. */
    protected abstract int getTabId(PersistentMessage message);

    /** Returns a fetcher for the avatar image if there is one, otherwise null. */
    protected @Nullable AsyncImageView.Factory getAsyncImageFactory(PersistentMessage message) {
        return null;
    }

    private TabCardLabelData buildLabelData(PersistentMessage message) {
        @StringRes int textRes = getTextRes(message);
        AsyncImageView.Factory asyncImageFactory = getAsyncImageFactory(message);
        TextResolver textResolver = (c) -> c.getString(textRes);
        return new TabCardLabelData(
                TabCardLabelType.ACTIVITY_UPDATE, textResolver, asyncImageFactory, textResolver);
    }
}
