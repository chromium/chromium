// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp_customization.theme;

import static org.mockito.Mockito.clearInvocations;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;

/** Tests for {@link NtpThemeStateProvider}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class NtpThemeStateProviderUnitTest {
    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private NtpThemeStateProvider.Observer mObserver;

    private NtpThemeStateProvider mNtpThemeStateProvider;

    @Before
    public void setUp() {
        mNtpThemeStateProvider = NtpThemeStateProvider.getInstance();
        NtpThemeStateProvider.setInstanceForTesting(mNtpThemeStateProvider);
    }

    @After
    public void tearDown() {
        NtpThemeStateProvider.setInstanceForTesting(null);
    }

    @Test
    public void testAddRemoveObserver() {
        mNtpThemeStateProvider.addObserver(mObserver);
        mNtpThemeStateProvider.notifyApplyThemeChanges();
        verify(mObserver).applyThemeChanges();

        clearInvocations(mObserver);
        mNtpThemeStateProvider.removeObserver(mObserver);
        mNtpThemeStateProvider.notifyApplyThemeChanges();
        verify(mObserver, never()).applyThemeChanges();
    }
}
