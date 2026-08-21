// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.app.tabmodel;

import static org.chromium.base.ThreadUtils.assertOnUiThread;

import android.content.SharedPreferences;
import android.util.ArrayMap;

import org.chromium.base.task.SequencedTaskRunner;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.crypto.CipherFactory;

/**
 * Manager and entry point for {@link TabCache} storage scopes.
 *
 * <p>Follows a 3-tier architecture:
 *
 * <ul>
 *   <li><b>Tier 1 (Storage Partition Scope - {@link TabCacheDirScope}):</b> Encapsulates the
 *       underlying directory on disk, {@link SharedPreferences}, {@link SequencedTaskRunner}, and
 *       atomic clear invalidation counter for a specific arbitrary string tag.
 *   <li><b>Tier 2 (Manager - {@link TabCacheManager}):</b> Manages the global registry of shared
 *       {@link TabCacheDirScope} instances keyed by tag and serves as the factory for constructing
 *       {@link TabCache} instances.
 *   <li><b>Tier 3 (Storage Engine - {@link TabCache}):</b> The lightweight instance-level caching
 *       engine that handles asynchronous FlatBuffer serialization, zero-copy mmap deserialization,
 *       optional encryption via {@link CipherFactory}, and preloading.
 * </ul>
 */
@NullMarked
public class TabCacheManager {
    private static final ArrayMap<String, TabCacheDirScope> sDirScopes = new ArrayMap<>();

    private TabCacheManager() {}

    /**
     * Creates a {@link TabCache} instance with the given tag.
     *
     * @param tag The tag used as the cache directory and SharedPreferences name.
     * @param cipherFactory The {@link CipherFactory} used to encrypt incognito tab states, or null
     *     for unencrypted storage.
     * @return A new {@link TabCache} instance.
     */
    public static TabCache create(String tag, @Nullable CipherFactory cipherFactory) {
        assertOnUiThread();
        return new TabCache(getOrCreateDirScope(tag), cipherFactory);
    }

    /* package */ static TabCacheDirScope getOrCreateDirScope(String tag) {
        assertOnUiThread();
        TabCacheDirScope scope = sDirScopes.get(tag);
        if (scope == null) {
            scope = new TabCacheDirScope(tag);
            sDirScopes.put(tag, scope);
        }
        return scope;
    }

    /** Resets all cached scopes and clears all directories for testing. */
    public static void resetForTesting() {
        assertOnUiThread();
        for (int i = 0; i < sDirScopes.size(); i++) {
            sDirScopes.valueAt(i).clearAll();
        }
        sDirScopes.clear();
    }
}
