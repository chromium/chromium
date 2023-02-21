// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.touch_to_fill;

import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.timeout;
import static org.mockito.Mockito.verify;

import static org.chromium.base.test.util.CriteriaHelper.pollUiThread;
import static org.chromium.content_public.browser.test.util.TestThreadUtils.runOnUiThreadBlocking;

import android.annotation.SuppressLint;
import android.view.View;
import android.widget.TextView;

import androidx.annotation.Nullable;
import androidx.recyclerview.widget.RecyclerView;
import androidx.test.espresso.Espresso;
import androidx.test.filters.MediumTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;

import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.ScalableTimeout;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.touch_to_fill.data.Credential;
import org.chromium.chrome.browser.touch_to_fill.data.WebAuthnCredential;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetContent;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.SheetState;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetControllerProvider;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetTestSupport;
import org.chromium.content_public.browser.test.util.TouchCommon;
import org.chromium.url.GURL;

import java.util.Arrays;
import java.util.Collections;

/**
 * Integration tests for the Touch To Fill component check that the calls to the Touch To Fill API
 * end up rendering a View.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@Batch(Batch.PER_CLASS)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class TouchToFillIntegrationTest {
    private static GURL sExampleUrl;
    private static final String MOBILE_URL = "https://m.example.xyz";
    private static Credential sAna;
    private static Credential sBob;
    private static WebAuthnCredential sCam;

    private TouchToFillComponent mTouchToFill;

    @Mock
    private TouchToFillComponent.Delegate mMockBridge;

    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    private BottomSheetController mBottomSheetController;

    public TouchToFillIntegrationTest() {
        MockitoAnnotations.initMocks(this);
    }

    @Before
    public void setUp() throws InterruptedException {
        sExampleUrl = new GURL("https://www.example.xyz");
        // TODO(https://crbug.com/783819): Migrate Credential to GURL.
        sAna = new Credential("Ana", "S3cr3t", "Ana", sExampleUrl.getSpec(), false, false, 0);
        sBob = new Credential("Bob", "*****", "Bob", MOBILE_URL, true, false, 0);
        sCam = new WebAuthnCredential("cam@example.net", "12345");

        mActivityTestRule.startMainActivityOnBlankPage();
        runOnUiThreadBlocking(() -> {
            mTouchToFill = new TouchToFillCoordinator();
            mBottomSheetController = BottomSheetControllerProvider.from(
                    mActivityTestRule.getActivity().getWindowAndroid());
            mTouchToFill.initialize(
                    mActivityTestRule.getActivity(), mBottomSheetController, mMockBridge);
        });
    }

    @Test
    @MediumTest
    public void testClickingSuggestionsTriggersCallback() {
        runOnUiThreadBlocking(() -> {
            mTouchToFill.showCredentials(sExampleUrl, true, Collections.emptyList(),
                    Collections.singletonList(sAna), false);
        });
        BottomSheetTestSupport.waitForOpen(mBottomSheetController);

        pollUiThread(() -> getCredentials().getChildAt(1) != null);
        TouchCommon.singleClickView(getCredentials().getChildAt(1));

        waitForEvent(mMockBridge).onCredentialSelected(sAna);
        verify(mMockBridge, never()).onDismissed();
    }

    @Test
    @MediumTest
    public void testClickingWebAuthnCredentialTriggersCallback() {
        runOnUiThreadBlocking(() -> {
            mTouchToFill.showCredentials(sExampleUrl, true, Collections.singletonList(sCam),
                    Collections.singletonList(sAna), false);
        });
        BottomSheetTestSupport.waitForOpen(mBottomSheetController);

        pollUiThread(() -> getCredentials().getChildAt(1) != null);
        TouchCommon.singleClickView(getCredentials().getChildAt(1));

        waitForEvent(mMockBridge).onWebAuthnCredentialSelected(sCam);
        verify(mMockBridge, never()).onDismissed();
    }

    @Test
    @MediumTest
    public void testClickingButtonTriggersCallback() {
        runOnUiThreadBlocking(() -> {
            mTouchToFill.showCredentials(sExampleUrl, true, Collections.emptyList(),
                    Collections.singletonList(sAna), false);
        });
        BottomSheetTestSupport.waitForOpen(mBottomSheetController);

        pollUiThread(() -> getCredentials().getChildAt(2) != null);
        TouchCommon.singleClickView(getCredentials().getChildAt(2));

        waitForEvent(mMockBridge).onCredentialSelected(sAna);
        verify(mMockBridge, never()).onDismissed();
    }

    @Test
    @MediumTest
    public void testBackDismissesAndCallsCallback() {
        runOnUiThreadBlocking(() -> {
            mTouchToFill.showCredentials(
                    sExampleUrl, true, Collections.emptyList(), Arrays.asList(sAna, sBob), false);
        });
        BottomSheetTestSupport.waitForOpen(mBottomSheetController);

        Espresso.pressBack();

        waitForEvent(mMockBridge).onDismissed();
        verify(mMockBridge, never()).onCredentialSelected(any());
    }

    @Test
    @MediumTest
    public void testClickingManagePasswordsTriggersCallback() {
        runOnUiThreadBlocking(() -> {
            mTouchToFill.showCredentials(sExampleUrl, true, Collections.emptyList(),
                    Collections.singletonList(sAna), false);
        });
        BottomSheetTestSupport.waitForOpen(mBottomSheetController);

        BottomSheetTestSupport sheetSupport = new BottomSheetTestSupport(mBottomSheetController);

        // Swipe the sheet up to it's full state in order to see the 'Manage Passwords' button.
        runOnUiThreadBlocking(() -> { sheetSupport.setSheetState(SheetState.FULL, false); });

        pollUiThread(() -> getManagePasswordsButton() != null);
        TouchCommon.singleClickView(getManagePasswordsButton());
        waitForEvent(mMockBridge).onManagePasswordsSelected();
        verify(mMockBridge, never()).onDismissed();
        verify(mMockBridge, never()).onCredentialSelected(any());
    }

    @Test
    @MediumTest
    @SuppressLint("SetTextI18n")
    public void testDismissedIfUnableToShow() throws Exception {
        BottomSheetContent otherBottomSheetContent = runOnUiThreadBlocking(() -> {
            TextView highPriorityBottomSheetContentView =
                    new TextView(mActivityTestRule.getActivity());
            highPriorityBottomSheetContentView.setText("Another bottom sheet content");
            BottomSheetContent content = new BottomSheetContent() {
                @Override
                public View getContentView() {
                    return highPriorityBottomSheetContentView;
                }

                @Nullable
                @Override
                public View getToolbarView() {
                    return null;
                }

                @Override
                public int getVerticalScrollOffset() {
                    return 0;
                }

                @Override
                public void destroy() {}

                @Override
                public int getPriority() {
                    return ContentPriority.HIGH;
                }

                @Override
                public boolean swipeToDismissEnabled() {
                    return false;
                }

                @Override
                public int getSheetContentDescriptionStringId() {
                    return 0;
                }

                @Override
                public int getSheetHalfHeightAccessibilityStringId() {
                    return 0;
                }

                @Override
                public int getSheetFullHeightAccessibilityStringId() {
                    return 0;
                }

                @Override
                public int getSheetClosedAccessibilityStringId() {
                    return 0;
                }
            };
            mBottomSheetController.requestShowContent(content, /* animate = */ false);
            return content;
        });
        pollUiThread(() -> getBottomSheetState() == SheetState.PEEK);
        Espresso.onView(withText("Another bottom sheet content")).check(matches(isDisplayed()));

        runOnUiThreadBlocking(() -> {
            mTouchToFill.showCredentials(
                    sExampleUrl, true, Collections.emptyList(), Arrays.asList(sAna, sBob), false);
        });
        waitForEvent(mMockBridge).onDismissed();
        verify(mMockBridge, never()).onCredentialSelected(any());
        Espresso.onView(withText("Another bottom sheet content")).check(matches(isDisplayed()));

        runOnUiThreadBlocking(() -> {
            mBottomSheetController.hideContent(otherBottomSheetContent, /* animate = */ false);
        });
        pollUiThread(() -> getBottomSheetState() == BottomSheetController.SheetState.HIDDEN);
    }

    private RecyclerView getCredentials() {
        return mActivityTestRule.getActivity().findViewById(
                org.chromium.chrome.browser.touch_to_fill.common.R.id.sheet_item_list);
    }

    private TextView getManagePasswordsButton() {
        return mActivityTestRule.getActivity().findViewById(
                R.id.touch_to_fill_sheet_manage_passwords);
    }

    public static <T> T waitForEvent(T mock) {
        return verify(mock,
                timeout(ScalableTimeout.scaleTimeout(CriteriaHelper.DEFAULT_MAX_TIME_TO_POLL)));
    }

    private @SheetState int getBottomSheetState() {
        return mBottomSheetController.getSheetState();
    }
}
