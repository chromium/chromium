// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.glic.ui;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoInteractions;
import static org.mockito.Mockito.verifyNoMoreInteractions;
import static org.mockito.Mockito.when;

import org.jni_zero.JniRawPtr;
import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.glic.GlicEnabling;
import org.chromium.chrome.browser.glic.GlicEnablingJni;
import org.chromium.chrome.browser.profiles.Profile;

/** Unit tests for {@link GlicBottomSheetComponentProvider}. */
@RunWith(BaseRobolectricTestRunner.class)
public class GlicBottomSheetComponentProviderUnitTest {
    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private Profile mProfile;
    @Mock private GlicBottomSheetComponentProvider.Natives mNativesMock;
    @Mock private GlicEnabling.Natives mGlicEnablingJniMock;

    private JniRawPtr<GlicBottomSheetComponentProvider.NativeGlicSidePanelCoordinatorAndroid>
            mNativeCoordinator;

    @Before
    public void setUp() {
        GlicBottomSheetComponentProviderJni.setInstanceForTesting(mNativesMock);
        GlicEnablingJni.setInstanceForTesting(mGlicEnablingJniMock);
    }

    @After
    public void tearDown() {
        if (mNativeCoordinator != null) {
            mNativeCoordinator.release();
            mNativeCoordinator = null;
        }
    }

    @Test
    public void testNotifyCallsNativeOnlyWithLivePointer() {
        mNativeCoordinator = spy(JniRawPtr.createForTesting(100L));
        GlicBottomSheetComponentProvider provider =
                new GlicBottomSheetComponentProvider(mProfile, mNativeCoordinator);

        provider.configureNativeFre(/* showNativeFre= */ true);
        assertTrue(provider.mShowNativeFre);
        provider.notifyNativeFreAccepted();
        assertFalse(provider.mShowNativeFre);
        verify(mNativesMock).onNativeFreAccepted(mNativeCoordinator);

        provider.destroy();
        verify(mNativeCoordinator).release();

        provider.notifyNativeFreAccepted();
        verifyNoMoreInteractions(mNativesMock);
    }

    @Test
    public void testNotifyWithNullCoordinatorIsNoOp() {
        GlicBottomSheetComponentProvider provider =
                new GlicBottomSheetComponentProvider(mProfile, /* nativeCoordinator= */ null);

        provider.configureNativeFre(/* showNativeFre= */ true);
        provider.notifyNativeFreAccepted();
        assertFalse(provider.mShowNativeFre);
        provider.destroy();

        verifyNoInteractions(mNativesMock);
    }

    @Test
    public void testIsEnterpriseAccount() {
        assertFalse(GlicEnabling.isEnterpriseAccount(null));

        when(mGlicEnablingJniMock.isEnterpriseAccount(mProfile)).thenReturn(true);
        assertTrue(GlicEnabling.isEnterpriseAccount(mProfile));

        GlicEnabling.setEnabledForTesting(true);
        assertFalse(GlicEnabling.isEnterpriseAccount(mProfile));
    }
}
