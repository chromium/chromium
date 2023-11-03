// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.touch_to_fill.password_generation;

import static org.mockito.Mockito.verify;

import androidx.test.ext.junit.rules.ActivityScenarioRule;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.mockito.quality.Strictness;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.JniMocker;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.prefs.PrefService;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.TestActivity;
import org.chromium.ui.base.WindowAndroid;

/** Tests for {@link TouchToFillPasswordGenerationBridge} */
@RunWith(BaseRobolectricTestRunner.class)
@Batch(Batch.PER_CLASS)
public class TouchToFillPasswordGenerationBridgeTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule().strictness(Strictness.STRICT_STUBS);
    @Rule public JniMocker mJniMocker = new JniMocker();

    @Rule
    public ActivityScenarioRule<TestActivity> mActivityScenarioRule =
            new ActivityScenarioRule<>(TestActivity.class);

    @Mock private BottomSheetController mBottomSheetController;
    @Mock private TouchToFillPasswordGenerationBridge.Natives mBridgeJniMock;
    @Mock private WebContents mWebContents;
    @Mock private PrefService mPrefService;
    @Mock private WindowAndroid mWindowAndroid;

    private static final long sTestNativePointer = 1;

    private TouchToFillPasswordGenerationBridge mBridge;

    @Before
    public void setUp() {
        MockitoAnnotations.openMocks(this);
        mJniMocker.mock(TouchToFillPasswordGenerationBridgeJni.TEST_HOOKS, mBridgeJniMock);

        mActivityScenarioRule
                .getScenario()
                .onActivity(
                        activity -> {
                            mBridge =
                                    new TouchToFillPasswordGenerationBridge(
                                            sTestNativePointer,
                                            mBottomSheetController,
                                            mWindowAndroid,
                                            mWebContents,
                                            mPrefService);
                        });
    }

    @Test
    public void testOnDismissed() {
        mBridge.onDismissed(/* passwordAccepted= */ false);
        verify(mBridgeJniMock).onDismissed(sTestNativePointer, /* passwordAccepted= */ false);
    }

    @Test
    public void testOnGeneratedPasswordAccepted() {
        String generatedPassword = "password";
        mBridge.onGeneratedPasswordAccepted(generatedPassword);
        verify(mBridgeJniMock).onGeneratedPasswordAccepted(sTestNativePointer, generatedPassword);
    }

    @Test
    public void testOnGeneratedPasswordRejected() {
        mBridge.onGeneratedPasswordRejected();
        verify(mBridgeJniMock).onGeneratedPasswordRejected(sTestNativePointer);
    }
}
