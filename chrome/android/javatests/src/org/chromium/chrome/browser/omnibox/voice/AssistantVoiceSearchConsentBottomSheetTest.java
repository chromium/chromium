// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.voice;

import androidx.test.filters.MediumTest;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetTestSupport;
import org.chromium.content_public.browser.test.util.ClickUtils;
import org.chromium.content_public.browser.test.util.TestThreadUtils;

/** Tests for AssistantVoiceSearchConsentDialog */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class AssistantVoiceSearchConsentBottomSheetTest {
    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    @Rule
    public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock
    AssistantVoiceSearchConsentUi.Observer mObserver;

    AssistantVoiceSearchConsentBottomSheet mBottomSheet;
    BottomSheetController mBottomSheetController;
    BottomSheetTestSupport mBottomSheetTestSupport;

    @Before
    public void setUp() {
        mActivityTestRule.startMainActivityOnBlankPage();

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            ChromeTabbedActivity cta = mActivityTestRule.getActivity();
            mBottomSheetController =
                    cta.getRootUiCoordinatorForTesting().getBottomSheetController();
            mBottomSheetTestSupport = new BottomSheetTestSupport(mBottomSheetController);
            mBottomSheet = createBottomSheet();
        });
    }

    @After
    public void tearDown() {}

    private void showBottomSheet() {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mBottomSheet.show(mObserver);
            mBottomSheetTestSupport.endAllAnimations();
        });
    }

    private AssistantVoiceSearchConsentBottomSheet createBottomSheet() {
        return TestThreadUtils.runOnUiThreadBlockingNoException(() -> {
            ChromeTabbedActivity cta = mActivityTestRule.getActivity();
            return new AssistantVoiceSearchConsentBottomSheet(
                    cta.getWindowAndroid().getContext().get(), mBottomSheetController);
        });
    }

    @Test
    @MediumTest
    public void testAcceptButton() {
        showBottomSheet();

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            ClickUtils.clickButton(mBottomSheet.getContentView().findViewById(R.id.button_primary));
            mBottomSheetTestSupport.endAllAnimations();
        });

        Mockito.verify(mObserver, Mockito.timeout(1000)).onConsentAccepted();
    }

    @Test
    @MediumTest
    public void testRejectButton() {
        showBottomSheet();

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            ClickUtils.clickButton(
                    mBottomSheet.getContentView().findViewById(R.id.button_secondary));
            mBottomSheetTestSupport.endAllAnimations();
        });

        Mockito.verify(mObserver, Mockito.timeout(1000)).onConsentRejected();
    }

    @Test
    @MediumTest
    public void testLearnMoreButton() {
        showBottomSheet();

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            ClickUtils.clickButton(
                    mBottomSheet.getContentView().findViewById(R.id.avs_consent_ui_learn_more));
            mBottomSheetTestSupport.endAllAnimations();
        });

        Mockito.verify(mObserver, Mockito.timeout(1000)).onLearnMoreClicked();
    }

    @Test
    @MediumTest
    public void testBackButton() {
        showBottomSheet();

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mBottomSheetTestSupport.handleBackPress();
            mBottomSheetTestSupport.endAllAnimations();
        });

        Mockito.verify(mObserver, Mockito.timeout(1000)).onConsentCanceled();
    }

    @Test
    @MediumTest
    public void testScrimTap() {
        showBottomSheet();

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mBottomSheetTestSupport.forceClickOutsideTheSheet();
            mBottomSheetTestSupport.endAllAnimations();
        });

        Mockito.verify(mObserver, Mockito.timeout(1000)).onConsentCanceled();
    }

    @Test
    @MediumTest
    public void testDismiss() {
        showBottomSheet();

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mBottomSheet.dismiss();
            mBottomSheetTestSupport.endAllAnimations();
        });

        Mockito.verifyNoMoreInteractions(mObserver);
    }

    @Test
    @MediumTest
    public void testDestroy() {
        showBottomSheet();

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mBottomSheet.destroy();
            mBottomSheetTestSupport.endAllAnimations();
        });

        Mockito.verify(mObserver, Mockito.timeout(1000)).onNonUserCancel();
    }
}
