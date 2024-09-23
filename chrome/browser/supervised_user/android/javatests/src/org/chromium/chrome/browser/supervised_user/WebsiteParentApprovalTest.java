// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.supervised_user;

import static androidx.test.espresso.matcher.ViewMatchers.withId;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.timeout;
import static org.mockito.Mockito.verify;

import androidx.test.filters.MediumTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.RuleChain;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.mockito.quality.Strictness;

import org.chromium.base.Callback;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.DoNotBatch;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.supervised_user.android.AndroidLocalWebApprovalFlowOutcome;
import org.chromium.chrome.browser.superviseduser.FilteringBehavior;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.util.browser.signin.SigninTestRule;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetTestSupport;
import org.chromium.content_public.browser.WebContents;
import org.chromium.net.test.EmbeddedTestServer;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.test.util.ViewUtils;
import org.chromium.url.GURL;

import java.util.concurrent.TimeoutException;

/**
 * Tests the local website approval flow. This test suire mocks the natives to allows us to
 * explicitly check the actual completion callback is called. It also confirms the histograms
 * recorded by on the Android specific (Java) part. A verification flow with the actual native
 * methods is captured in {@link
 * org.chromium.chrome.browser.supervised_user.WebsiteParentApprovalNativesTest}.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@DoNotBatch(
        reason =
                "Running tests in parallel can interfere with each tests setup."
                        + "The code under tests involes setting static methods and features, "
                        + "which must remain unchanged for the duration of the test.")
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class WebsiteParentApprovalTest {
    public ChromeTabbedActivityTestRule mTabbedActivityTestRule =
            new ChromeTabbedActivityTestRule();
    public SigninTestRule mSigninTestRule = new SigninTestRule();

    // Destroy TabbedActivityTestRule before SigninTestRule to remove observers of
    // FakeAccountManagerFacade.
    @Rule
    public final RuleChain mRuleChain =
            RuleChain.outerRule(mSigninTestRule).around(mTabbedActivityTestRule);

    @Rule public final JniMocker mocker = new JniMocker();

    @Rule
    public final MockitoRule mMockitoRule = MockitoJUnit.rule().strictness(Strictness.STRICT_STUBS);

    private static final String TEST_PAGE = "/chrome/test/data/android/about.html";

    private EmbeddedTestServer mTestServer;
    private String mBlockedUrl;
    private WebContents mWebContents;
    private BottomSheetController mBottomSheetController;
    private BottomSheetTestSupport mBottomSheetTestSupport;

    @Mock private WebsiteParentApproval.Natives mWebsiteParentApprovalNativesMock;
    @Mock private ParentAuthDelegate mParentAuthDelegateMock;

    @Before
    public void setUp() throws TimeoutException {
        mTestServer = mTabbedActivityTestRule.getEmbeddedTestServerRule().getServer();
        mBlockedUrl = mTestServer.getURL(TEST_PAGE);
        mTabbedActivityTestRule.startMainActivityOnBlankPage();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    ChromeTabbedActivity activity = mTabbedActivityTestRule.getActivity();
                    mBottomSheetController =
                            activity.getRootUiCoordinatorForTesting().getBottomSheetController();
                    mBottomSheetTestSupport = new BottomSheetTestSupport(mBottomSheetController);
                });

        mSigninTestRule.addChildTestAccountThenWaitForSignin();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    SupervisedUserSettingsTestBridge.setFilteringBehavior(
                            mTabbedActivityTestRule.getProfile(/* incognito= */ false),
                            FilteringBehavior.BLOCK);
                });
        mWebContents = mTabbedActivityTestRule.getWebContents();

        mocker.mock(WebsiteParentApprovalJni.TEST_HOOKS, mWebsiteParentApprovalNativesMock);

        // @TODO b:243916194 : Once we start consuming mParentAuthDelegateMock
        // .isLocalAuthSupported we should add a mocked behaviour in this test.
        ParentAuthDelegateProvider.setInstanceForTests(mParentAuthDelegateMock);
    }

    private void mockParentAuthDelegateRequestLocalAuthResponse(boolean result) {
        doAnswer(
                        invocation -> {
                            Callback<Boolean> onCompletionCallback = invocation.getArgument(2);
                            onCompletionCallback.onResult(result);
                            return null;
                        })
                .when(mParentAuthDelegateMock)
                .requestLocalAuth(any(WindowAndroid.class), any(GURL.class), any(Callback.class));
    }

    private void checkParentApprovalScreenClosedAfterClick() {
        // Ensure all animations have ended. Otherwise the following check may fail.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mBottomSheetTestSupport.endAllAnimations();
                });
        ViewUtils.waitForViewCheckingState(
                withId(R.id.local_parent_approval_layout),
                ViewUtils.VIEW_INVISIBLE | ViewUtils.VIEW_GONE | ViewUtils.VIEW_NULL);
    }

    @Test
    @MediumTest
    public void parentApprovesScreenVisibilityAfterApproval() {
        mockParentAuthDelegateRequestLocalAuthResponse(true);
        mTabbedActivityTestRule.loadUrl(mBlockedUrl);

        WebsiteParentApprovalTestUtils.clickAskInPerson(mWebContents);
        WebsiteParentApprovalTestUtils.clickApprove(mBottomSheetTestSupport);

        checkParentApprovalScreenClosedAfterClick();
    }

    @Test
    @MediumTest
    public void parentApprovesScreenVisibilityAfterRejection() {
        mockParentAuthDelegateRequestLocalAuthResponse(true);
        mTabbedActivityTestRule.loadUrl(mBlockedUrl);

        WebsiteParentApprovalTestUtils.clickAskInPerson(mWebContents);
        WebsiteParentApprovalTestUtils.clickDoNotApprove(mBottomSheetTestSupport);

        checkParentApprovalScreenClosedAfterClick();
    }

    @Test
    @MediumTest
    public void parentApprovesLocally() {
        mockParentAuthDelegateRequestLocalAuthResponse(true);
        mTabbedActivityTestRule.loadUrl(mBlockedUrl);

        // Verify only histograms recorded in Java.
        var histogram =
                HistogramWatcher.newSingleRecordWatcher(
                        "FamilyLinkUser.LocalWebApprovalOutcome", /* APPROVED_BY_PARENT= */ 0);

        WebsiteParentApprovalTestUtils.clickAskInPerson(mWebContents);
        WebsiteParentApprovalTestUtils.clickApprove(mBottomSheetTestSupport);

        verify(
                        mWebsiteParentApprovalNativesMock,
                        timeout(CriteriaHelper.DEFAULT_MAX_TIME_TO_POLL).times(1))
                .onCompletion(AndroidLocalWebApprovalFlowOutcome.APPROVED);
        histogram.assertExpected();
    }

    @Test
    @MediumTest
    public void parentRejectsLocally() {
        mockParentAuthDelegateRequestLocalAuthResponse(true);
        mTabbedActivityTestRule.loadUrl(mBlockedUrl);

        // Verify only histograms recorded in Java.
        var histogram =
                HistogramWatcher.newSingleRecordWatcher(
                        "FamilyLinkUser.LocalWebApprovalOutcome", /* DENIED_BY_PARENT= */ 1);

        WebsiteParentApprovalTestUtils.clickAskInPerson(mWebContents);
        WebsiteParentApprovalTestUtils.clickDoNotApprove(mBottomSheetTestSupport);

        verify(
                        mWebsiteParentApprovalNativesMock,
                        timeout(CriteriaHelper.DEFAULT_MAX_TIME_TO_POLL).times(1))
                .onCompletion(AndroidLocalWebApprovalFlowOutcome.REJECTED);
        histogram.assertExpected();
    }

    @Test
    @MediumTest
    public void parentAuthorizationFailure() {
        mockParentAuthDelegateRequestLocalAuthResponse(false);
        mTabbedActivityTestRule.loadUrl(mBlockedUrl);

        WebsiteParentApprovalTestUtils.clickAskInPerson(mWebContents);

        verify(
                        mWebsiteParentApprovalNativesMock,
                        timeout(CriteriaHelper.DEFAULT_MAX_TIME_TO_POLL).times(1))
                .onCompletion(AndroidLocalWebApprovalFlowOutcome.INCOMPLETE);
    }
}
