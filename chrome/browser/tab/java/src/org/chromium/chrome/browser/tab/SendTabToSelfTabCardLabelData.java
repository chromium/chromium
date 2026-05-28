// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab;

import android.content.Context;

import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.UserData;
import org.chromium.build.annotations.NullMarked;

import java.util.Objects;

/** UserData attached to a Tab to store the sender device name for Send Tab To Self. */
@NullMarked
public class SendTabToSelfTabCardLabelData extends EmptyTabObserver implements UserData {
    private static final long EXPIRATION_MS = 5L * 24 * 60 * 60 * 1000; // 5 days

    private final Tab mTab;
    private final String mSenderDeviceName;
    private long mAdditionTimestampMs;

    /**
     * Returns the valid SendTabToSelfTabCardLabelData for the tab, removing it if expired.
     *
     * @param tab The Tab from which to retrieve the label data.
     * @return The valid SendTabToSelfTabCardLabelData object, or null if absent or expired.
     */
    public static @Nullable SendTabToSelfTabCardLabelData get(Tab tab) {
        if (tab == null || tab.getUserDataHost() == null) return null;
        SendTabToSelfTabCardLabelData data =
                tab.getUserDataHost().getUserData(SendTabToSelfTabCardLabelData.class);
        if (data == null) return null;

        if (data.isExpired()) {
            removeAndDestroy(tab, data);
            return null;
        }
        return data;
    }

    /**
     * Removes the UserData from the host and destroys the instance.
     *
     * @param tab The Tab hosting the UserData.
     * @param data The SendTabToSelfTabCardLabelData instance to remove and destroy.
     */
    private static void removeAndDestroy(Tab tab, SendTabToSelfTabCardLabelData data) {
        tab.getUserDataHost().removeUserData(SendTabToSelfTabCardLabelData.class);
        data.destroy();
    }

    /**
     * Constructs a new SendTabToSelfTabCardLabelData object and attaches it as a TabObserver.
     *
     * @param tab The Tab to which this label data is attached.
     * @param senderDeviceName The name of the device that sent the tab.
     * @param additionTimestampMs The timestamp in milliseconds when the tab was added in the
     *     background.
     */
    public SendTabToSelfTabCardLabelData(
            Tab tab, String senderDeviceName, long additionTimestampMs) {
        Objects.requireNonNull(tab);
        mTab = tab;
        mSenderDeviceName = senderDeviceName;
        mAdditionTimestampMs = additionTimestampMs;
        assert !isExpired();
        mTab.addObserver(this);
    }

    /**
     * Returns whether the label data has expired.
     *
     * @return True if the data has exceeded the 5-day expiration window, false otherwise.
     */
    private boolean isExpired() {
        return System.currentTimeMillis() - mAdditionTimestampMs > EXPIRATION_MS;
    }

    /**
     * Removes the label data, if present, upon user interaction with the tab.
     *
     * @param tab The Tab being shown.
     * @param type The type of selection event that caused the tab to be shown.
     */
    @Override
    public void onShown(Tab tab, @TabSelectionType int type) {
        if (type == TabSelectionType.FROM_USER) {
            if (tab != null && tab.getUserDataHost() != null) {
                SendTabToSelfTabCardLabelData data =
                        tab.getUserDataHost().getUserData(SendTabToSelfTabCardLabelData.class);
                if (data != null) {
                    removeAndDestroy(tab, data);
                }
            }
        }
    }

    /**
     * Cleans up observer registration when the tab is destroyed.
     *
     * @param tab The Tab being destroyed.
     */
    @Override
    public void onDestroyed(Tab tab) {
        destroy();
    }

    /** Destroys the UserData object and removes the observer from the tab. */
    @Override
    public void destroy() {
        mTab.removeObserver(this);
    }

    /**
     * Returns the localized label text to display on the tab card.
     *
     * @param context The Context used to retrieve localized string resources.
     * @return The localized label text indicating the sender device.
     */
    public String getLabelText(Context context) {
        return context.getString(
                R.string.send_tab_to_self_message_banner_subtitle, mSenderDeviceName);
    }

    /**
     * Sets the addition timestamp for testing purposes.
     *
     * @param additionTimestampMs The timestamp to set.
     */
    @VisibleForTesting
    public void setAdditionTimestampMsForTesting(long additionTimestampMs) {
        mAdditionTimestampMs = additionTimestampMs;
    }
}
