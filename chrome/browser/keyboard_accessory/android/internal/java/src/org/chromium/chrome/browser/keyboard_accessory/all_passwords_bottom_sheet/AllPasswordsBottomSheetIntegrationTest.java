// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.keyboard_accessory.all_passwords_bottom_sheet;

import static org.junit.Assert.assertEquals;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.argThat;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import static org.chromium.build.NullUtil.assertNonNull;
import static org.chromium.build.NullUtil.assumeNonNull;
import static org.chromium.chrome.browser.keyboard_accessory.all_passwords_bottom_sheet.AllPasswordsBottomSheetTestHelper.ANA;
import static org.chromium.chrome.browser.keyboard_accessory.all_passwords_bottom_sheet.AllPasswordsBottomSheetTestHelper.TEST_CREDENTIALS;
import static org.chromium.chrome.browser.keyboard_accessory.all_passwords_bottom_sheet.AllPasswordsBottomSheetTestHelper.createBottomSheetController;

import android.app.Activity;
import android.view.View;

import androidx.recyclerview.widget.RecyclerView;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentMatcher;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Robolectric;
import org.robolectric.android.controller.ActivityController;
import org.robolectric.shadows.ShadowLooper;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.autofill.helpers.FaviconHelper;
import org.chromium.chrome.browser.keyboard_accessory.R;
import org.chromium.chrome.browser.profiles.TestProfile;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetContent;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.SheetState;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetTestSupport;
import org.chromium.components.browser_ui.widget.chips.ChipView;
import org.chromium.components.url_formatter.UrlFormatter;
import org.chromium.components.url_formatter.UrlFormatterJni;
import org.chromium.ui.base.TestActivity;
import org.chromium.url.GURL;

import java.util.ArrayList;

/**
 * Integration tests for the AllPasswordsBottomSheet check that the calls to the
 * AllPasswordsBottomSheet controller end up rendering a View and triggers the right native calls.
 */
@RunWith(BaseRobolectricTestRunner.class)
public class AllPasswordsBottomSheetIntegrationTest {
    private static final String EXAMPLE_URL = "https://www.example.xyz";
    private static final boolean IS_PASSWORD_FIELD = true;

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    private final TestProfile mProfile = TestProfile.createRegular();

    private @Mock AllPasswordsBottomSheetCoordinator.Delegate mDelegate;
    private @Mock FaviconHelper mFaviconHelper;
    private @Mock UrlFormatter.Natives mUrlFormatterJniMock;

    private BottomSheetController mBottomSheetController;
    private AllPasswordsBottomSheetCoordinator mCoordinator;
    private BottomSheetTestSupport mBottomSheetSupport;

    @Before
    public void setUp() {
        UrlFormatterJni.setInstanceForTesting(mUrlFormatterJniMock);
        when(mUrlFormatterJniMock.formatUrlForSecurityDisplay(any(), anyInt()))
                .thenAnswer(inv -> inv.<GURL>getArgument(0).getHost());
        FaviconHelper.setCreationStrategy((context, profile) -> mFaviconHelper);

        ActivityController<TestActivity> controller = Robolectric.buildActivity(TestActivity.class);
        controller.get().setTheme(R.style.Theme_BrowserUI_DayNight);
        Activity activity = controller.setup().get();
        mBottomSheetController = createBottomSheetController(activity);
        mBottomSheetSupport = new BottomSheetTestSupport(mBottomSheetController);
        mCoordinator = new AllPasswordsBottomSheetCoordinator();
        mCoordinator.initialize(activity, mProfile, mBottomSheetController, mDelegate, EXAMPLE_URL);
    }

    @Test
    public void testClickingUseOtherUsernameAndPressBack() {
        mCoordinator.showCredentials(new ArrayList<>(TEST_CREDENTIALS), !IS_PASSWORD_FIELD);
        waitForSheetState(SheetState.FULL);

        mBottomSheetSupport.handleBackPress();

        waitForSheetState(SheetState.HIDDEN);

        verify(mDelegate).onDismissed();
    }

    @Test
    public void testClickingUseOtherUsernameAndSelectCredentialInUsernameField() {
        mCoordinator.showCredentials(new ArrayList<>(TEST_CREDENTIALS), !IS_PASSWORD_FIELD);
        waitForSheetState(SheetState.FULL);

        layoutCredentials();
        getCredentialNameAt(0).performClick();

        waitForSheetState(SheetState.HIDDEN);
        verify(mDelegate).onCredentialSelected(argThat(matchesCredentialFillRequest(ANA, false)));
    }

    @Test
    public void testClickingUseOtherUsernameAndSelectCredentialInPasswordField() {
        mCoordinator.showCredentials(new ArrayList<>(TEST_CREDENTIALS), IS_PASSWORD_FIELD);
        waitForSheetState(SheetState.FULL);

        layoutCredentials();
        getCredentialNameAt(0).performClick();

        waitForSheetState(SheetState.HIDDEN);
        verify(mDelegate).onCredentialSelected(argThat(matchesCredentialFillRequest(ANA, false)));
    }

    @Test
    public void testClickingUseOtherPasswordAndSelectCredentialInUsernameField() {
        mCoordinator.showCredentials(new ArrayList<>(TEST_CREDENTIALS), !IS_PASSWORD_FIELD);
        waitForSheetState(SheetState.FULL);

        layoutCredentials();
        getCredentialPasswordAt(1).performClick();

        verify(mDelegate, never()).onCredentialSelected(any());
    }

    @Test
    public void testClickingUseOtherPasswordAndSelectCredentialInPasswordField() {
        mCoordinator.showCredentials(new ArrayList<>(TEST_CREDENTIALS), IS_PASSWORD_FIELD);
        waitForSheetState(SheetState.FULL);

        layoutCredentials();
        getCredentialPasswordAt(0).performClick();

        waitForSheetState(SheetState.HIDDEN);
        verify(mDelegate).onCredentialSelected(argThat(matchesCredentialFillRequest(ANA, true)));
    }

    private void waitForSheetState(@SheetState int state) {
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();
        mBottomSheetSupport.endAllAnimations();
        assertEquals(state, mBottomSheetController.getSheetState());
    }

    private void layoutCredentials() {
        RecyclerView recyclerView = getCredentials();
        recyclerView.measure(
                View.MeasureSpec.makeMeasureSpec(1080, View.MeasureSpec.EXACTLY),
                View.MeasureSpec.makeMeasureSpec(1920, View.MeasureSpec.EXACTLY));
        recyclerView.layout(0, 0, 1080, 1920);
    }

    private RecyclerView getCredentials() {
        BottomSheetContent content = assumeNonNull(mBottomSheetController).getCurrentSheetContent();
        assertNonNull(content);
        return content.getContentView().findViewById(R.id.sheet_item_list);
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
