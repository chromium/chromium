// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.extensions.api.messaging;

import org.chromium.base.ThreadUtils;
import org.chromium.base.lifetime.Destroyable;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileKeyedMap;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.Map;

/**
 * Profile-scoped manager for Android Native Messaging. Maintains a map of package names to
 * ServiceConnections.
 */
@NullMarked
public class NativeMessagingManager implements Destroyable, NativeMessagingConnection.Observer {
    private static @Nullable ProfileKeyedMap<NativeMessagingManager> sProfileMap;

    private final Map<String, NativeMessagingConnection> mConnections = new HashMap<>();

    /** Return the {@link NativeMessagingManager} associated with the passed in {@link Profile}. */
    public static NativeMessagingManager getForProfile(Profile profile) {
        ThreadUtils.assertOnUiThread();
        if (sProfileMap == null) {
            sProfileMap =
                    ProfileKeyedMap.createMapOfDestroyables(
                            ProfileKeyedMap.ProfileSelection.REDIRECTED_TO_ORIGINAL);
        }
        return sProfileMap.getForProfile(profile, NativeMessagingManager::new);
    }

    private NativeMessagingManager(Profile profile) {}

    @Override
    public void destroy() {
        ThreadUtils.assertOnUiThread();
        // Snapshot to a list so mConnections.remove() inside onUnbound doesn't break iteration:
        for (NativeMessagingConnection connection : new ArrayList<>(mConnections.values())) {
            connection.unbind();
        }
        mConnections.clear();
    }

    // NativeMessagingConnection.Observer
    @Override
    public void onUnbound(String packageName) {
        mConnections.remove(packageName);
    }

    // Connects an extension to a native Android app.
    public @Nullable String connect(String packageName, String extensionId) {
        ThreadUtils.assertOnUiThread();
        NativeMessagingConnection connection = mConnections.get(packageName);
        if (connection == null) {
            connection = new NativeMessagingConnection(packageName, this);
            if (!connection.isBound()) {
                return "Error: Unable to connect to " + packageName;
            }

            mConnections.put(packageName, connection);
        }

        return null;
    }

    @Nullable NativeMessagingConnection getConnectionForTesting(String packageName) {
        return mConnections.get(packageName);
    }
}
