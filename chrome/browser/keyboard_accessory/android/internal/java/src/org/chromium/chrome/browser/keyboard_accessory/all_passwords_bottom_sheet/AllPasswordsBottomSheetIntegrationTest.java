// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.keyboard_accessory.all_passwords_bottom_sheet;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.argThat;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;

import static org.chromium.base.ThreadUtils.runOnUiThreadBlocking;
import static org.chromium.base.test.util.CriteriaHelper.pollUiThread;
import static org.chromium.build.NullUtil.assertNonNull;
import static org.chromium.build.NullUtil.assumeNonNull;
import static org.chromium.chrome.browser.keyboard_accessory.all_passwords_bottom_sheet.AllPasswordsBottomSheetTestHelper.ANA;
import static org.chromium.chrome.browser.keyboard_accessory.all_passwords_bottom_sheet.AllPasswordsBottomSheetTestHelper.TEST_CREDENTIALS;
import static org.chromium.chrome.browser.keyboard_accessory.all_passwords_bottom_sheet.AllPasswordsBottomSheetTestHelper.createBottomSheetController;
import static org.chromium.components.browser_ui.bottomsheet.BottomSheetTestSupport.waitForState;

import androidx.recyclerview.widget.RecyclerView;
import androidx.test.filters.MediumTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentMatcher;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.BaseActivityTestRule;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.DoNotBatch;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.keyboard_accessory.R;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetContent;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.SheetState;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetTestSupport;
import org.chromium.components.browser_ui.widget.chips.ChipView;
import org.chromium.content_public.browser.test.util.TouchCommon;
import org.chromium.ui.test.util.BlankUiTestActivity;

import java.util.ArrayList;

/**
 * Integration tests for the AllPasswordsBottomSheet check that the calls to the
 * AllPasswordsBottomSheet controller end up rendering a View and triggers the right native calls.
 */
@DoNotBatch(reason = "Batching causes tests to be flaky.")
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class AllPasswordsBottomSheetIntegrationTest {
    private static final String EXAMPLE_URL = "https://www.example.xyz";
    private static final boolean IS_PASSWORD_FIELD = true;

    private BottomSheetController mBottomSheetController;
    private AllPasswordsBottomSheetCoordinator mCoordinator;
    private BottomSheetTestSupport mBottomSheetSupport;

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Rule
    public BaseActivityTestRule<BlankUiTestActivity> mActivityTestRule =
            new BaseActivityTestRule<>(BlankUiTestActivity.class);

    @Mock private Profile mProfile;
    @Mock private AllPasswordsBottomSheetCoordinator.Delegate mDelegate;

    @Before
    public void setUp() {
        mActivityTestRule.launchActivity(null);
        runOnUiThreadBlocking(
                () -> {
                    mBottomSheetController =
                            createBottomSheetController(mActivityTestRule.getActivity());
                    mBottomSheetSupport = new BottomSheetTestSupport(mBottomSheetController);
                    mCoordinator = new AllPasswordsBottomSheetCoordinator();
                    mCoordinator.initialize(
                            mActivityTestRule.getActivity(),
                            mProfile,
                            mBottomSheetController,
                            mDelegate,
                            EXAMPLE_URL);
                });
    }

    @Test
    @MediumTest
    public void testClickingUseOtherUsernameAndPressBack() {
        runOnUiThreadBlocking(
                () -> {
                    mCoordinator.showCredentials(
                            new ArrayList<>(TEST_CREDENTIALS), !IS_PASSWORD_FIELD);
                });
        waitForState(mBottomSheetController, SheetState.FULL);

        runOnUiThreadBlocking(mBottomSheetSupport::handleBackPress);

        waitForState(mBottomSheetController, SheetState.HIDDEN);

        verify(mDelegate).onDismissed();
    }

    @Test
    @MediumTest
    public void testClickingUseOtherUsernameAndSelectCredentialInUsernameField() {
        runOnUiThreadBlocking(
                () -> {
                    mCoordinator.showCredentials(
                            new ArrayList<>(TEST_CREDENTIALS), !IS_PASSWORD_FIELD);
                });
        waitForState(mBottomSheetController, SheetState.FULL);

        pollUiThread(() -> getCredentialNameAt(0) != null);
        TouchCommon.singleClickView(getCredentialNameAt(0));

        waitForState(mBottomSheetController, SheetState.HIDDEN);
        verify(mDelegate).onCredentialSelected(argThat(matchesCredentialFillRequest(ANA, false)));
    }

    @Test
    @MediumTest
    public void testClickingUseOtherUsernameAndSelectCredentialInPasswordField() {
        runOnUiThreadBlocking(
                () -> {
                    mCoordinator.showCredentials(
                            new ArrayList<>(TEST_CREDENTIALS), IS_PASSWORD_FIELD);
                });
        waitForState(mBottomSheetController, SheetState.FULL);

        pollUiThread(() -> getCredentialNameAt(0) != null);
        TouchCommon.singleClickView(getCredentialNameAt(0));

        waitForState(mBottomSheetController, SheetState.HIDDEN);
        verify(mDelegate).onCredentialSelected(argThat(matchesCredentialFillRequest(ANA, false)));
    }

    @Test
    @MediumTest
    public void testClickingUseOtherPasswordAndSelectCredentialInUsernameField() {
        runOnUiThreadBlocking(
                () -> {
                    mCoordinator.showCredentials(
                            new ArrayList<>(TEST_CREDENTIALS), !IS_PASSWORD_FIELD);
                });
        waitForState(mBottomSheetController, SheetState.FULL);

        pollUiThread(() -> getCredentialPasswordAt(1) != null);
        TouchCommon.singleClickView(getCredentialPasswordAt(1));

        verify(mDelegate, never()).onCredentialSelected(any());
    }

    @Test
    @MediumTest
    public void testClickingUseOtherPasswordAndSelectCredentialInPasswordField() {
        runOnUiThreadBlocking(
                () -> {
                    mCoordinator.showCredentials(
                            new ArrayList<>(TEST_CREDENTIALS), IS_PASSWORD_FIELD);
                });
        waitForState(mBottomSheetController, SheetState.FULL);

        pollUiThread(() -> getCredentialPasswordAt(0) != null);
        TouchCommon.singleClickView(getCredentialPasswordAt(0));

        waitForState(mBottomSheetController, SheetState.HIDDEN);
        verify(mDelegate).onCredentialSelected(argThat(matchesCredentialFillRequest(ANA, true)));
    }

    private RecyclerView getCredentials() {
        BottomSheetContent content = assumeNonNull(mBottomSheetController).getCurrentSheetContent();
        assertNonNull(content);
        return (RecyclerView) content.getContentView().findViewById(R.id.sheet_item_list);
    }

    private ChipView getCredentialNameAt(int index) {
        return ((ChipView) getCredentials().getChildAt(index).findViewById(R.id.suggestion_text));
    }

    private ChipView getCredentialPasswordAt(int index) {
        return ((ChipView) getCredentials().getChildAt(index).findViewById(R.id.password_text));
    }

    private ArgumentMatcher<CredentialFillRequest> matchesCredentialFillRequest(
            Credential expectedCredential, boolean expectedIsPasswordFillRequest) {
        return actual ->
                expectedCredential.equals(actual.getCredential())
                        && expectedIsPasswordFillRequest == actual.getRequestsToFillPassword();
    }
}
