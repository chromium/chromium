// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.privacy_sandbox;

import static org.junit.Assert.assertEquals;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.when;

import android.view.View;

import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.params.ParameterAnnotations;
import org.chromium.base.test.params.ParameterSet;
import org.chromium.base.test.params.ParameterizedRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.chrome.test.ChromeJUnit4RunnerDelegate;

import java.io.IOException;
import java.util.Arrays;
import java.util.List;

/** Tests {@link PrivacySandboxDebouncedOnClick}. */
@RunWith(ParameterizedRunner.class)
@ParameterAnnotations.UseRunnerDelegate(ChromeJUnit4RunnerDelegate.class)
@Batch(Batch.PER_CLASS)
public final class PrivacySandboxDebouncedOnClickTest {
    @ParameterAnnotations.ClassParameter
    private static final List<ParameterSet> sClassParams =
            Arrays.asList(
                    new ParameterSet().value(R.id.ack_button).name("Ack"),
                    new ParameterSet().value(R.id.settings_button).name("Settings"),
                    new ParameterSet().value(R.id.more_button).name("More"),
                    new ParameterSet().value(R.id.dropdown_element).name("Dropdown"),
                    new ParameterSet().value(R.id.no_button).name("No"),
                    new ParameterSet()
                            .value(R.id.privacy_policy_back_button)
                            .name("PrivacyPolicyBack"));

    private final View mMockView;
    private int mNumClicks;
    private final int mButtonRid;
    private final PrivacySandboxDebouncedOnClickImpl mFakeDialog;

    public PrivacySandboxDebouncedOnClickTest(int rid) {
        mMockView = mock(View.class);
        mButtonRid = rid;
        mFakeDialog =
                new PrivacySandboxDebouncedOnClickImpl(
                        "ProtectedAudienceMeasurementNoticeModal"
                                + PrivacySandboxDialogUtils.getSurfaceTypeAsString(
                                        SurfaceType.BR_APP));
    }

    @Before
    public void setUp() {
        // Set up which button to be clicked.
        when(mMockView.getId()).thenReturn(mButtonRid);
        // Reset number of clicks for accurate counting.
        mNumClicks = 0;
    }

    public class PrivacySandboxDebouncedOnClickImpl extends PrivacySandboxDebouncedOnClick {
        public PrivacySandboxDebouncedOnClickImpl(String noticeName) {
            super(noticeName);
        }

        @Override
        public void processClick(View view) {
            mNumClicks += 1;
        }
    }

    // Instantiates a concrete class which extends from the abstract class for testing.
    private void doubleClickNotice() {
        mFakeDialog.onClick(mMockView);
        mFakeDialog.onClick(mMockView);
    }

    // Waits for longer than the inputted debouncing delay passed in at the top of the file (200ms).
    private void waitPastDebouncingDelay() {
        try {
            Thread.sleep(300);
        } catch (InterruptedException e) {
            Assert.fail("Thread could not sleep: " + e.toString());
        }
    }

    @Test
    @SmallTest
    public void testDoubleClickIsDebounced() throws IOException {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    doubleClickNotice();

                    // Expect only 1 click recorded because the other is ignored.
                    assertEquals(1, mNumClicks);
                });
    }

    @Test
    @SmallTest
    public void testDoubleClickIsDebouncedThenClickIsRegistered() throws IOException {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    doubleClickNotice();

                    waitPastDebouncingDelay();

                    // Single click notice
                    mFakeDialog.onClick(mMockView);

                    // Expect 2 clicks recorded because the third click comes in > 100ms after.
                    assertEquals(2, mNumClicks);
                });
    }
}
