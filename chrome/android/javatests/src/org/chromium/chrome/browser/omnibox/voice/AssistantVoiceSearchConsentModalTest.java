// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.voice;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.matcher.ViewMatchers.withId;

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
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modaldialog.DialogDismissalCause;
import org.chromium.ui.modaldialog.ModalDialogManager;

/** Tests for AssistantVoiceSearchConsentDialog */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class AssistantVoiceSearchConsentModalTest {
    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    @Rule
    public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock
    AssistantVoiceSearchConsentUi.Observer mObserver;

    ModalDialogManager mModalDialogManager;
    AssistantVoiceSearchConsentModal mModal;

    @Before
    public void setUp() {
        mActivityTestRule.startMainActivityOnBlankPage();

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            WindowAndroid wa = mActivityTestRule.getActivity().getWindowAndroid();
            mModalDialogManager = wa.getModalDialogManager();
            mModal = new AssistantVoiceSearchConsentModal(
                    wa.getContext().get(), mModalDialogManager);
        });
    }

    @After
    public void tearDown() {}

    private void showModal() {
        TestThreadUtils.runOnUiThreadBlocking(() -> { mModal.show(mObserver); });
    }

    @Test
    @MediumTest
    public void testAcceptButton() {
        showModal();

        onView(withId(org.chromium.chrome.R.id.positive_button)).perform(click());

        Mockito.verify(mObserver, Mockito.timeout(1000)).onConsentAccepted();
    }

    @Test
    @MediumTest
    public void testRejectButton() {
        showModal();

        onView(withId(org.chromium.chrome.R.id.negative_button)).perform(click());

        Mockito.verify(mObserver, Mockito.timeout(1000)).onConsentRejected();
    }

    @Test
    @MediumTest
    public void testLearnMoreButton() {
        showModal();

        onView(withId(R.id.avs_consent_ui_learn_more)).perform(click());

        Mockito.verify(mObserver, Mockito.timeout(1000)).onLearnMoreClicked();
    }

    @Test
    @MediumTest
    public void testCancel() {
        showModal();

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mModalDialogManager.dismissAllDialogs(
                    DialogDismissalCause.NAVIGATE_BACK_OR_TOUCH_OUTSIDE);
        });

        Mockito.verify(mObserver, Mockito.timeout(1000)).onConsentCanceled();
    }

    @Test
    @MediumTest
    public void testDismiss() {
        showModal();

        TestThreadUtils.runOnUiThreadBlocking(() -> { mModal.dismiss(); });

        Mockito.verifyNoMoreInteractions(mObserver);
    }

    @Test
    @MediumTest
    public void testNonUserCancel() {
        showModal();

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mModalDialogManager.dismissAllDialogs(DialogDismissalCause.ACTION_ON_CONTENT);
        });

        Mockito.verify(mObserver, Mockito.timeout(1000)).onNonUserCancel();
    }
}
