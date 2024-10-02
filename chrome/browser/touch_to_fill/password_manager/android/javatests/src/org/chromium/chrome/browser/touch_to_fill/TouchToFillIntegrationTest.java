// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.touch_to_fill;

import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.timeout;
import static org.mockito.Mockito.verify;

import static org.chromium.base.ThreadUtils.runOnUiThreadBlocking;
import static org.chromium.base.test.util.CriteriaHelper.pollUiThread;
import static org.chromium.chrome.browser.autofill.AutofillTestHelper.singleMouseClickView;

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
import org.chromium.chrome.browser.password_manager.GetLoginMatchType;
import org.chromium.chrome.browser.touch_to_fill.common.BottomSheetFocusHelper;
import org.chromium.chrome.browser.touch_to_fill.data.Credential;
import org.chromium.chrome.browser.touch_to_fill.data.WebauthnCredential;
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
    private static WebauthnCredential sCam;

    private TouchToFillComponent mTouchToFill;

    @Mock private TouchToFillComponent.Delegate mMockBridge;

    @Mock private BottomSheetFocusHelper mMockFocusHelper;

    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    private BottomSheetController mBottomSheetController;

    public TouchToFillIntegrationTest() {
        MockitoAnnotations.initMocks(this);
    }

    @Before
    public void setUp() throws InterruptedException {
        sExampleUrl = new GURL("https://www.example.xyz");
        // TODO(crbug.com/40549331): Migrate Credential to GURL.
        sAna =
                new Credential(
                        "Ana",
                        "S3cr3t",
                        "Ana",
                        sExampleUrl.getSpec(),
                        "example.xyz",
                        GetLoginMatchType.EXACT,
                        0);
        sBob =
                new Credential(
                        "Bob",
                        "*****",
                        "Bob",
                        MOBILE_URL,
                        "m.example.xyz",
                        GetLoginMatchType.PSL,
                        0);
        sCam =
                new WebauthnCredential(
                        "example.net", new byte[] {1}, new byte[] {2}, "cam@example.net");

        mActivityTestRule.startMainActivityOnBlankPage();
        runOnUiThreadBlocking(
                () -> {
                    mTouchToFill = new TouchToFillCoordinator();
                    mBottomSheetController =
                            BottomSheetControllerProvider.from(
                                    mActivityTestRule.getActivity().getWindowAndroid());
                    mTouchToFill.initialize(
                            mActivityTestRule.getActivity(),
                            mActivityTestRule.getProfile(false),
                            mBottomSheetController,
                            mMockBridge,
                            mMockFocusHelper);
                });
    }

    @Test
    @MediumTest
    public void testConsumesGenericMotionEventsToPreventMouseClicksThroughSheet() {
        runOnUiThreadBlocking(
                () -> {
                    mTouchToFill.showCredentials(
                            sExampleUrl,
                            true,
                            Collections.emptyList(),
                            Collections.singletonList(sAna),
                            /* submitCredential= */ false,
                            /* managePasskeysHidesPasswords= */ false,
                            /* showHybridPasskeyOption= */ false,
                            /* showCredManEntry= */ false);
                });
        BottomSheetTestSupport.waitForOpen(mBottomSheetController);
        assert singleMouseClickView(getCredentials());
    }

    @Test
    @MediumTest
    public void testClickingSuggestionsTriggersCallback() {
        runOnUiThreadBlocking(
                () -> {
                    mTouchToFill.showCredentials(
                            sExampleUrl,
                            true,
                            Collections.emptyList(),
                            Collections.singletonList(sAna),
                            /* submitCredential= */ false,
                            /* managePasskeysHidesPasswords= */ false,
                            /* showHybridPasskeyOption= */ false,
                            /* showCredManEntry= */ false);
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
        runOnUiThreadBlocking(
                () -> {
                    mTouchToFill.showCredentials(
                            sExampleUrl,
                            true,
                            Collections.singletonList(sCam),
                            Collections.singletonList(sAna),
                            /* submitCredential= */ false,
                            /* managePasskeysHidesPasswords= */ false,
                            /* showHybridPasskeyOption= */ false,
                            /* showCredManEntry= */ false);
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
        runOnUiThreadBlocking(
                () -> {
                    mTouchToFill.showCredentials(
                            sExampleUrl,
                            true,
                            Collections.emptyList(),
                            Collections.singletonList(sAna),
                            /* submitCredential= */ false,
                            /* managePasskeysHidesPasswords= */ false,
                            /* showHybridPasskeyOption= */ false,
                            /* showCredManEntry= */ false);
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
        runOnUiThreadBlocking(
                () -> {
                    mTouchToFill.showCredentials(
                            sExampleUrl,
                            true,
                            Collections.emptyList(),
                            Arrays.asList(sAna, sBob),
                            /* submitCredential= */ false,
                            /* managePasskeysHidesPasswords= */ false,
                            /* showHybridPasskeyOption= */ false,
                            /* showCredManEntry= */ false);
                });
        BottomSheetTestSupport.waitForOpen(mBottomSheetController);

        Espresso.pressBack();

        waitForEvent(mMockBridge).onDismissed();
        verify(mMockBridge, never()).onCredentialSelected(any());
    }

    @Test
    @MediumTest
    public void testClickingManagePasswordsTriggersCallback() {
        runOnUiThreadBlocking(
                () -> {
                    mTouchToFill.showCredentials(
                            sExampleUrl,
                            true,
                            Collections.emptyList(),
                            Collections.singletonList(sAna),
                            /* submitCredential= */ false,
                            /* managePasskeysHidesPasswords= */ false,
                            /* showHybridPasskeyOption= */ false,
                            /* showCredManEntry= */ false);
                });
        BottomSheetTestSupport.waitForOpen(mBottomSheetController);

        BottomSheetTestSupport sheetSupport = new BottomSheetTestSupport(mBottomSheetController);

        // Swipe the sheet up to its full state in order to see the 'Manage Passwords' button.
        runOnUiThreadBlocking(
                () -> {
                    sheetSupport.setSheetState(SheetState.FULL, false);
                });

        pollUiThread(() -> getManagePasswordsButton() != null);
        TouchCommon.singleClickView(getManagePasswordsButton());
        waitForEvent(mMockBridge).onManagePasswordsSelected(/* passkeysShown= */ false);
        verify(mMockBridge, never()).onDismissed();
        verify(mMockBridge, never()).onCredentialSelected(any());
    }

    @Test
    @MediumTest
    public void testClickingHybridButtonTriggersCallback() {
        runOnUiThreadBlocking(
                () -> {
                    mTouchToFill.showCredentials(
                            sExampleUrl,
                            true,
                            Collections.emptyList(),
                            Collections.singletonList(sAna),
                            /* submitCredential= */ false,
                            /* managePasskeysHidesPasswords= */ false,
                            /* showHybridPasskeyOption= */ true,
                            /* showCredManEntry= */ false);
                });
        BottomSheetTestSupport.waitForOpen(mBottomSheetController);

        BottomSheetTestSupport sheetSupport = new BottomSheetTestSupport(mBottomSheetController);

        // Swipe the sheet up to its full state in order to see the 'Use a Passkey on a Different
        // Device' button.
        runOnUiThreadBlocking(
                () -> {
                    sheetSupport.setSheetState(SheetState.FULL, false);
                });

        pollUiThread(() -> getHybridSignInButton() != null);
        TouchCommon.singleClickView(getHybridSignInButton());
        waitForEvent(mMockBridge).onHybridSignInSelected();
        verify(mMockBridge, never()).onDismissed();
        verify(mMockBridge, never()).onCredentialSelected(any());
    }

    @Test
    @MediumTest
    @SuppressLint("SetTextI18n")
    public void testDismissedIfUnableToShow() throws Exception {
        BottomSheetContent otherBottomSheetContent =
                runOnUiThreadBlocking(
                        () -> {
                            TextView highPriorityBottomSheetContentView =
                                    new TextView(mActivityTestRule.getActivity());
                            highPriorityBottomSheetContentView.setText(
                                    "Another bottom sheet content");
                            BottomSheetContent content =
                                    new BottomSheetContent() {
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
                            mBottomSheetController.requestShowContent(
                                    content, /* animate= */ false);
                            return content;
                        });
        pollUiThread(() -> getBottomSheetState() == SheetState.PEEK);
        Espresso.onView(withText("Another bottom sheet content")).check(matches(isDisplayed()));

        runOnUiThreadBlocking(
                () -> {
                    mTouchToFill.showCredentials(
                            sExampleUrl,
                            true,
                            Collections.emptyList(),
                            Arrays.asList(sAna, sBob),
                            /* submitCredential= */ false,
                            /* managePasskeysHidesPasswords= */ false,
                            /* showHybridPasskeyOption= */ false,
                            /* showCredManEntry= */ false);
                });
        waitForEvent(mMockBridge).onDismissed();
        verify(mMockBridge, never()).onCredentialSelected(any());
        Espresso.onView(withText("Another bottom sheet content")).check(matches(isDisplayed()));

        runOnUiThreadBlocking(
                () -> {
                    mBottomSheetController.hideContent(
                            otherBottomSheetContent, /* animate= */ false);
                });
        pollUiThread(() -> getBottomSheetState() == BottomSheetController.SheetState.HIDDEN);
    }

    @Test
    @MediumTest
    public void testClickingMorePasskeysTriggersCallback() {
        runOnUiThreadBlocking(
                () -> {
                    mTouchToFill.showCredentials(
                            sExampleUrl,
                            true,
                            Collections.emptyList(),
                            Collections.singletonList(sAna),
                            /* submitCredential= */ false,
                            /* managePasskeysHidesPasswords= */ false,
                            /* showHybridPasskeyOption= */ false,
                            /* showCredManEntry= */ true);
                });
        BottomSheetTestSupport.waitForOpen(mBottomSheetController);

        BottomSheetTestSupport sheetSupport = new BottomSheetTestSupport(mBottomSheetController);

        // Swipe the sheet up to its full state in order to see the 'Use a Passkey on a Different
        // Device' button.
        runOnUiThreadBlocking(() -> sheetSupport.setSheetState(SheetState.FULL, false));

        pollUiThread(() -> getMorePasskeysItem() != null);
        TouchCommon.singleClickView(getMorePasskeysItem());
        waitForEvent(mMockBridge).onShowMorePasskeysSelected();
        verify(mMockBridge, never()).onDismissed();
        verify(mMockBridge, never()).onCredentialSelected(any());
    }

    private RecyclerView getCredentials() {
        return mActivityTestRule.getActivity().findViewById(R.id.sheet_item_list);
    }

    private TextView getManagePasswordsButton() {
        return mActivityTestRule
                .getActivity()
                .findViewById(R.id.touch_to_fill_sheet_manage_passwords);
    }

    private TextView getHybridSignInButton() {
        return mActivityTestRule
                .getActivity()
                .findViewById(R.id.touch_to_fill_sheet_use_passkeys_other_device);
    }

    private TextView getMorePasskeysItem() {
        return mActivityTestRule.getActivity().findViewById(R.id.more_passkeys_label);
    }

    public static <T> T waitForEvent(T mock) {
        return verify(
                mock,
                timeout(ScalableTimeout.scaleTimeout(CriteriaHelper.DEFAULT_MAX_TIME_TO_POLL)));
    }

    private @SheetState int getBottomSheetState() {
        return mBottomSheetController.getSheetState();
    }
}
