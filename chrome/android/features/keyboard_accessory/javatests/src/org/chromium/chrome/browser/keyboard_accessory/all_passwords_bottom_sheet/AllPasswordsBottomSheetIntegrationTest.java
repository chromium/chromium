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

import androidx.recyclerview.widget.RecyclerView;
import androidx.test.espresso.Espresso;
import androidx.test.filters.MediumTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentMatcher;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.keyboard_accessory.R;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.SheetState;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetControllerProvider;
import org.chromium.components.browser_ui.widget.chips.ChipView;
import org.chromium.content_public.browser.test.util.TouchCommon;

import java.util.ArrayList;
import java.util.List;

/**
 * Integration tests for the AllPasswordsBottomSheet check that the calls to the
 * AllPasswordsBottomSheet controller end up rendering a View and triggers the right native calls.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class AllPasswordsBottomSheetIntegrationTest {
    private static final String EXAMPLE_URL = "https://www.example.xyz";
    private static final Credential ANA =
            new Credential(
                    /* username= */ "ana@gmail.com",
                    /* password= */ "S3cr3t",
                    /* formattedUsername= */ "ana@gmail.com",
                    /* originUrl= */ "https://m.domain.xyz/",
                    /* isAndroidCredential= */ false,
                    /* appDisplayName= */ "",
                    /* isPlusAddressUsername= */ true);
    private static final Credential BOB =
            new Credential(
                    /* username= */ "Bob",
                    /* password= */ "*****",
                    /* formattedUsername= */ "Bob",
                    /* originUrl= */ "https://subdomain.example.xyz",
                    /* isAndroidCredential= */ false,
                    /* appDisplayName= */ "",
                    /* isPlusAddressUsername= */ false);
    private static final Credential CARL =
            new Credential(
                    /* username= */ "Carl",
                    /* password= */ "G3h3!m",
                    /* formattedUsername= */ "Carl",
                    /* originUrl= */ "https://www.origin.xyz",
                    /* isAndroidCredential= */ false,
                    /* appDisplayName= */ "",
                    /* isPlusAddressUsername= */ false);
    private static final List<Credential> TEST_CREDENTIALS = List.of(BOB, CARL, ANA);
    private static final boolean IS_PASSWORD_FIELD = true;

    private AllPasswordsBottomSheetCoordinator mCoordinator;

    private BottomSheetController mBottomSheetController;

    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    @Mock private AllPasswordsBottomSheetCoordinator.Delegate mDelegate;

    public AllPasswordsBottomSheetIntegrationTest() {
        MockitoAnnotations.initMocks(this);
    }

    @Before
    public void setUp() {
        mActivityTestRule.startMainActivityOnBlankPage();
        runOnUiThreadBlocking(
                () -> {
                    mBottomSheetController =
                            BottomSheetControllerProvider.from(
                                    mActivityTestRule.getActivity().getWindowAndroid());
                    mCoordinator = new AllPasswordsBottomSheetCoordinator();
                    mCoordinator.initialize(
                            mActivityTestRule.getActivity(),
                            mActivityTestRule.getProfile(false),
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
        pollUiThread(() -> getBottomSheetState() == SheetState.FULL);

        Espresso.pressBack();

        pollUiThread(() -> getBottomSheetState() == BottomSheetController.SheetState.HIDDEN);

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
        pollUiThread(() -> getBottomSheetState() == SheetState.FULL);

        pollUiThread(() -> getCredentialNameAt(1) != null);
        TouchCommon.singleClickView(getCredentialNameAt(1));

        pollUiThread(() -> getBottomSheetState() == BottomSheetController.SheetState.HIDDEN);
        verify(mDelegate).onCredentialSelected(argThat(matchesCredentialFillRequest(BOB, false)));
    }

    @Test
    @MediumTest
    public void testClickingUseOtherUsernameAndSelectCredentialInPasswordField() {
        runOnUiThreadBlocking(
                () -> {
                    mCoordinator.showCredentials(
                            new ArrayList<>(TEST_CREDENTIALS), IS_PASSWORD_FIELD);
                });
        pollUiThread(() -> getBottomSheetState() == SheetState.FULL);

        pollUiThread(() -> getCredentialNameAt(1) != null);
        TouchCommon.singleClickView(getCredentialNameAt(1));

        pollUiThread(() -> getBottomSheetState() == BottomSheetController.SheetState.HIDDEN);
        verify(mDelegate).onCredentialSelected(argThat(matchesCredentialFillRequest(BOB, false)));
    }

    @Test
    @MediumTest
    public void testClickingUseOtherPasswordAndSelectCredentialInUsernameField() {
        runOnUiThreadBlocking(
                () -> {
                    mCoordinator.showCredentials(
                            new ArrayList<>(TEST_CREDENTIALS), !IS_PASSWORD_FIELD);
                });
        pollUiThread(() -> getBottomSheetState() == SheetState.FULL);

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
        pollUiThread(() -> getBottomSheetState() == SheetState.FULL);

        pollUiThread(() -> getCredentialPasswordAt(1) != null);
        TouchCommon.singleClickView(getCredentialPasswordAt(1));

        pollUiThread(() -> getBottomSheetState() == BottomSheetController.SheetState.HIDDEN);
        verify(mDelegate).onCredentialSelected(argThat(matchesCredentialFillRequest(BOB, true)));
    }

    private RecyclerView getCredentials() {
        return (RecyclerView)
                mBottomSheetController
                        .getCurrentSheetContent()
                        .getContentView()
                        .findViewById(R.id.sheet_item_list);
    }

    private ChipView getCredentialNameAt(int index) {
        return ((ChipView) getCredentials().getChildAt(index).findViewById(R.id.suggestion_text));
    }

    private ChipView getCredentialPasswordAt(int index) {
        return ((ChipView) getCredentials().getChildAt(index).findViewById(R.id.password_text));
    }

    private @SheetState int getBottomSheetState() {
        return mBottomSheetController.getSheetState();
    }

    private ArgumentMatcher<CredentialFillRequest> matchesCredentialFillRequest(
            Credential expectedCredential, boolean expectedIsPasswordFillRequest) {
        return actual ->
                expectedCredential.equals(actual.getCredential())
                        && expectedIsPasswordFillRequest == actual.getRequestsToFillPassword();
    }
}
