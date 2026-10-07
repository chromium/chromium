// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management.labels;

import androidx.annotation.StringRes;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tasks.tab_management.TabListNotificationHandler;
import org.chromium.components.browser_ui.util.TextResolver;
import org.chromium.components.browser_ui.widget.async_image.AsyncImageView;
import org.chromium.components.collaboration.messaging.PersistentMessage;

import java.util.Collections;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

/**
 * A partial implementation for pushing labels to a {@link TabListNotificationHandler} for some sort
 * of tab object, like actual tabs or tab groups. Can be triggered by {@link #showAll()} or concrete
 * implementations may trigger off other observers.
 *
 * @param <KeyT> The identifier key type for the tab object.
 */
@NullMarked
public abstract class TabObjectLabeller<KeyT> extends TabObjectNotificationUpdater {
    public TabObjectLabeller(
            Profile profile, TabListNotificationHandler tabListNotificationHandler) {
        super(profile, tabListNotificationHandler);
    }

    @Override
    public void showAll() {
        Map<KeyT, TabCardLabelData> cardLabels = new HashMap<>();
        for (PersistentMessage message : getAllMessages()) {
            if (shouldApply(message)) {
                cardLabels.put(getKey(message), buildLabelData(message));
            }
        }
        if (!cardLabels.isEmpty()) {
            applyLabels(cardLabels);
        }
    }

    @Override
    protected void incrementalShow(PersistentMessage message) {
        if (shouldApply(message)) {
            KeyT key = getKey(message);
            Map<KeyT, TabCardLabelData> cardLabels =
                    Collections.singletonMap(key, buildLabelData(message));
            applyLabels(cardLabels);
        }
    }

    @Override
    protected void incrementalHide(PersistentMessage message) {
        if (shouldApply(message)) {
            KeyT key = getKey(message);
            Map<KeyT, TabCardLabelData> cardLabels = Collections.singletonMap(key, null);
            applyLabels(cardLabels);
        }
    }

    /** If the given message should be applied or ignored. */
    protected abstract boolean shouldApply(PersistentMessage message);

    /** The resource for the text to be shown on the label. */
    protected abstract @StringRes int getTextRes(PersistentMessage message);

    /** Fetch all relevant messages that should be shown. */
    protected abstract List<PersistentMessage> getAllMessages();

    /**
     * Returns the identifier key for a given message.
     *
     * @param message The {@link PersistentMessage} to extract the key from.
     * @return The identifier key for the target tab object.
     */
    protected abstract KeyT getKey(PersistentMessage message);

    /**
     * Pushes the given label map to {@link TabListNotificationHandler}.
     *
     * @param cardLabels Map of identifier keys to {@link TabCardLabelData} to apply.
     */
    protected abstract void applyLabels(Map<KeyT, TabCardLabelData> cardLabels);

    /** Returns a fetcher for the avatar image if there is one, otherwise null. */
    protected AsyncImageView.@Nullable Factory getAsyncImageFactory(PersistentMessage message) {
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
