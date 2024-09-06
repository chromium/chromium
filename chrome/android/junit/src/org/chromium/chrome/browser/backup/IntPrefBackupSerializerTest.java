// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.backup;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.util.Pair;

import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.components.prefs.PrefService;
import org.chromium.components.sync.internal.SyncPrefNames;

import java.util.List;

/** Unit tests for {@link IntPrefBackupSerializer}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class IntPrefBackupSerializerTest {
    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private PrefService mPrefService;

    @Test
    public void testSerializeAndDeserialize() {
        // The serializer should work with any integer value, even if the pref might have more
        // specific values in practice (e.g. given by an enum).
        // IMPORTANT: test with a large integer to make sure the 4 bytes are split and then joined
        // correctly.
        int largePrefValue = 1_234_567_890;
        when(mPrefService.getInteger(SyncPrefNames.SYNC_TO_SIGNIN_MIGRATION_STATE))
                .thenReturn(largePrefValue);

        IntPrefBackupSerializer serializer = new IntPrefBackupSerializer();
        List<Pair<String, byte[]>> serializedNamesAndValues =
                serializer.serializeAllowlistedPrefs(mPrefService);

        // Please update this expectation as well as the setInteger() ones below when a new pref is
        // added to the allowlist.
        assertEquals(serializedNamesAndValues.size(), 1);

        boolean success =
                serializer.tryDeserialize(
                        mPrefService,
                        serializedNamesAndValues.get(0).first,
                        serializedNamesAndValues.get(0).second);

        assertTrue(success);
        verify(mPrefService)
                .setInteger(SyncPrefNames.SYNC_TO_SIGNIN_MIGRATION_STATE, largePrefValue);
    }
}
