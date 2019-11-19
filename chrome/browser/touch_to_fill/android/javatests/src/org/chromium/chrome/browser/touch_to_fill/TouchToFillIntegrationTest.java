// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.touch_to_fill;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.timeout;
import static org.mockito.Mockito.verify;

import static org.chromium.content_public.browser.test.util.CriteriaHelper.pollUiThread;
import static org.chromium.content_public.browser.test.util.TestThreadUtils.runOnUiThreadBlocking;

import android.support.test.espresso.Espresso;
import android.support.test.filters.MediumTest;
import android.support.v7.widget.RecyclerView;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.RetryOnFailure;
import org.chromium.base.test.util.ScalableTimeout;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.browser.touch_to_fill.data.Credential;
import org.chromium.chrome.browser.widget.bottomsheet.BottomSheetController;
import org.chromium.chrome.browser.widget.bottomsheet.BottomSheetController.SheetState;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.content_public.browser.test.util.CriteriaHelper;
import org.chromium.content_public.browser.test.util.TouchCommon;

import java.util.Arrays;
import java.util.Collections;

/**
 * Integration tests for the Touch To Fill component check that the calls to the Touch To Fill API
 * end up rendering a View.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@RetryOnFailure
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class TouchToFillIntegrationTest {
    private static final String EXAMPLE_URL = "https://www.example.xyz";
    private static final String MOBILE_URL = "https://m.example.xyz";
    private static final Credential ANA =
            new Credential("Ana", "S3cr3t", "Ana", EXAMPLE_URL, false);
    private static final Credential BOB = new Credential("Bob", "*****", "Bob", MOBILE_URL, true);

    private final TouchToFillComponent mTouchToFill = new TouchToFillCoordinator();

    @Mock
    private TouchToFillComponent.Delegate mMockBridge;

    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    public TouchToFillIntegrationTest() {
        MockitoAnnotations.initMocks(this);
    }

    @Before
    public void setUp() throws InterruptedException {
        mActivityTestRule.startMainActivityOnBlankPage();
        runOnUiThreadBlocking(() -> {
            mTouchToFill.initialize(mActivityTestRule.getActivity(),
                    mActivityTestRule.getActivity().getBottomSheetController(), mMockBridge);
        });
    }

    @Test
    @MediumTest
    @DisabledTest(message = "crbug.com/1012221")
    public void testClickingSuggestionsTriggersCallback() {
        runOnUiThreadBlocking(() -> {
            mTouchToFill.showCredentials(EXAMPLE_URL, true, Collections.singletonList(ANA));
        });
        pollUiThread(() -> getBottomSheetState() == BottomSheetController.SheetState.HALF);

        pollUiThread(() -> getCredentials().getChildAt(0) != null);
        TouchCommon.singleClickView(getCredentials().getChildAt(0));

        waitForEvent(mMockBridge).onCredentialSelected(ANA);
        verify(mMockBridge).fetchFavicon(eq(ANA.getOriginUrl()), EXAMPLE_URL, anyInt(), any());
        verify(mMockBridge, never()).onDismissed();
    }

    @Test
    @MediumTest
    public void testBackDismissesAndCallsCallback() {
        runOnUiThreadBlocking(() -> {
            mTouchToFill.showCredentials(EXAMPLE_URL, true, Arrays.asList(ANA, BOB));
        });
        pollUiThread(() -> getBottomSheetState() == BottomSheetController.SheetState.HALF);

        Espresso.pressBack();

        waitForEvent(mMockBridge).onDismissed();
        verify(mMockBridge, never()).onCredentialSelected(any());
    }

    private RecyclerView getCredentials() {
        return mActivityTestRule.getActivity().findViewById(R.id.sheet_item_list);
    }

    public static <T> T waitForEvent(T mock) {
        return verify(mock,
                timeout(ScalableTimeout.scaleTimeout(CriteriaHelper.DEFAULT_MAX_TIME_TO_POLL)));
    }

    private @SheetState int getBottomSheetState() {
        return mActivityTestRule.getActivity().getBottomSheetController().getSheetState();
    }
}
