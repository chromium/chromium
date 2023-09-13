// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.touch_to_fill.no_passkeys;

import static org.mockito.Mockito.verify;

import android.content.Context;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.mockito.quality.Strictness;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.JniMocker;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;

import java.lang.ref.WeakReference;

/** Tests for {@link NoPasskeysBottomSheetBridge} */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
@Batch(Batch.PER_CLASS)
public class NoPasskeysBottomSheetModuleTest {
    private static final long TEST_NATIVE = 42069;
    private static final String TEST_ORIGIN = "origin.com";

    @Rule
    public MockitoRule mMockitoRule = MockitoJUnit.rule().strictness(Strictness.STRICT_STUBS);
    @Rule
    public JniMocker jniMocker = new JniMocker();

    @Mock
    private NoPasskeysBottomSheetBridge.Natives mNativeMock;
    @Mock
    private BottomSheetController mBottomSheetController;
    @Mock
    private Context mContext;

    NoPasskeysBottomSheetBridge mBridge;

    @Before
    public void setUp() {
        MockitoAnnotations.openMocks(this);
        jniMocker.mock(NoPasskeysBottomSheetBridgeJni.TEST_HOOKS, mNativeMock);
        mBridge = new NoPasskeysBottomSheetBridge(TEST_NATIVE, new WeakReference<>(mContext),
                new WeakReference<>(mBottomSheetController));
    }

    @Test
    public void callNativeOnDismissAfterShow() {
        mBridge.show(TEST_ORIGIN);
        mBridge.dismiss();
        verify(mNativeMock).onDismissed(TEST_NATIVE);
    }

    @Test
    public void dismissOnlyOnce() {
        mBridge.show(TEST_ORIGIN);
        mBridge.dismiss();
        mBridge.dismiss();
        verify(mNativeMock).onDismissed(TEST_NATIVE);
    }
}
