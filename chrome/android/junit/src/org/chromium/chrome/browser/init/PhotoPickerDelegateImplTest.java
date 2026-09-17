// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.init;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.when;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.UnownedUserDataHost;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.ephemeraltab.EphemeralTabCoordinator;
import org.chromium.chrome.browser.ephemeraltab.EphemeralTabCoordinatorSupplier;
import org.chromium.ui.base.WindowAndroid;

/** Unit tests for {@link PhotoPickerDelegateImpl}. */
@RunWith(BaseRobolectricTestRunner.class)
public class PhotoPickerDelegateImplTest {
    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private WindowAndroid mWindowAndroid;
    @Mock private EphemeralTabCoordinator mEphemeralTabCoordinator;

    private final UnownedUserDataHost mUserDataHost = new UnownedUserDataHost();
    private PhotoPickerDelegateImpl mDelegate;

    @Before
    public void setUp() {
        when(mWindowAndroid.getUnownedUserDataHost()).thenReturn(mUserDataHost);
        mDelegate = new PhotoPickerDelegateImpl();
    }

    @Test
    public void testShouldBlockFilePicker() {
        // No EphemeralTabCoordinatorSupplier attached -> false
        assertFalse(mDelegate.shouldBlockFilePicker(mWindowAndroid));

        // EphemeralTabCoordinator present but not opened -> false
        EphemeralTabCoordinatorSupplier.setInstanceForTesting(mEphemeralTabCoordinator);
        when(mEphemeralTabCoordinator.isOpened()).thenReturn(false);
        assertFalse(mDelegate.shouldBlockFilePicker(mWindowAndroid));

        // EphemeralTabCoordinator opened -> true
        when(mEphemeralTabCoordinator.isOpened()).thenReturn(true);
        assertTrue(mDelegate.shouldBlockFilePicker(mWindowAndroid));
    }
}
