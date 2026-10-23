// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.touch_to_fill.password_generation;

import static org.mockito.Mockito.verify;

import androidx.test.ext.junit.rules.ActivityScenarioRule;

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
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.prefs.PrefService;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.TestActivity;
import org.chromium.ui.base.WindowAndroid;

/** Tests for {@link TouchToFillPasswordGenerationBridge} */
@RunWith(BaseRobolectricTestRunner.class)
public class TouchToFillPasswordGenerationBridgeTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Rule
    public ActivityScenarioRule<TestActivity> mActivityScenarioRule =
            new ActivityScenarioRule<>(TestActivity.class);

    @Mock private BottomSheetController mBottomSheetController;
    @Mock private TouchToFillPasswordGenerationBridge.Natives mBridgeJniMock;
    @Mock private WebContents mWebContents;
    @Mock private PrefService mPrefService;
    @Mock private WindowAndroid mWindowAndroid;

    private JniRawPtr<
                    TouchToFillPasswordGenerationBridge
                            .NativeTouchToFillPasswordGenerationBridgeImpl>
            mTestNativePointer;

    private TouchToFillPasswordGenerationBridge mBridge;

    @Before
    public void setUp() {
        TouchToFillPasswordGenerationBridgeJni.setInstanceForTesting(mBridgeJniMock);
        mTestNativePointer = JniRawPtr.createForTesting(1L);

        mActivityScenarioRule
                .getScenario()
                .onActivity(
                        activity -> {
                            mBridge =
                                    new TouchToFillPasswordGenerationBridge(
                                            mTestNativePointer,
                                            mBottomSheetController,
                                            mWindowAndroid,
                                            mWebContents,
                                            mPrefService);
                        });
    }

    @After
    public void tearDown() {
        mTestNativePointer.release();
    }

    @Test
    public void testOnDismissed() {
        mBridge.onDismissed(/* passwordAccepted= */ false);
        verify(mBridgeJniMock).onDismissed(mTestNativePointer, /* passwordAccepted= */ false);
    }

    @Test
    public void testOnGeneratedPasswordAccepted() {
        String generatedPassword = "password";
        mBridge.onGeneratedPasswordAccepted(generatedPassword);
        verify(mBridgeJniMock).onGeneratedPasswordAccepted(mTestNativePointer, generatedPassword);
    }

    @Test
    public void testOnGeneratedPasswordRejected() {
        mBridge.onGeneratedPasswordRejected();
        verify(mBridgeJniMock).onGeneratedPasswordRejected(mTestNativePointer);
    }
}
