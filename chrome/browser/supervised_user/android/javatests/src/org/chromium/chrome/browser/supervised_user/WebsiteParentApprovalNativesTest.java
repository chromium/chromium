// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.supervised_user;

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
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.superviseduser.FilteringBehavior;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.util.browser.signin.SigninTestRule;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetTestSupport;
import org.chromium.content_public.browser.WebContents;
import org.chromium.net.test.EmbeddedTestServer;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.url.GURL;

import java.util.concurrent.TimeoutException;

/**
 * Tests the local website approval flow. This test suite invokes the actual native methods and
 * allows us to execute more of the code, compared to {@link
 * org.chromium.chrome.browser.supervised_user.WebsiteParentApprovalTest} whick mocks the natives.
 * This allows us to test all histograms recorded by natives.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@DoNotBatch(
        reason =
                "Running tests in parallel can interfere with each tests setup."
                        + "The code under tests involes setting static methods and features, "
                        + "which must remain unchanged for the duration of the test.")
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class WebsiteParentApprovalNativesTest {
    // TODO(b/243916194): Expand the test coverage beyond the completion callback, up to the page
    // refresh.

    public ChromeTabbedActivityTestRule mTabbedActivityTestRule =
            new ChromeTabbedActivityTestRule();
    public SigninTestRule mSigninTestRule = new SigninTestRule();

    // Destroy TabbedActivityTestRule before SigninTestRule to remove observers of
    // FakeAccountManagerFacade.
    @Rule
    public final RuleChain mRuleChain =
            RuleChain.outerRule(mSigninTestRule).around(mTabbedActivityTestRule);

    @Rule
    public final MockitoRule mMockitoRule = MockitoJUnit.rule().strictness(Strictness.STRICT_STUBS);

    private static final String TEST_PAGE = "/chrome/test/data/android/about.html";

    private EmbeddedTestServer mTestServer;
    private String mBlockedUrl;
    private WebContents mWebContents;
    private BottomSheetController mBottomSheetController;
    private BottomSheetTestSupport mBottomSheetTestSupport;

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

        // TODO(b/243916194): Once we start consuming mParentAuthDelegateMock
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

    @Test
    @MediumTest
    public void parentApprovesLocally() {
        mockParentAuthDelegateRequestLocalAuthResponse(true);
        mTabbedActivityTestRule.loadUrl(mBlockedUrl);
        var histograms =
                HistogramWatcher.newBuilder()
                        .expectAnyRecord(
                                "FamilyLinkUser.LocalWebApprovalCompleteRequestTotalDuration")
                        .expectIntRecord("FamilyLinkUser.LocalWebApprovalResult", /* Approved= */ 0)
                        .build();

        WebsiteParentApprovalTestUtils.clickAskInPerson(mWebContents);
        WebsiteParentApprovalTestUtils.clickApprove(mBottomSheetTestSupport);

        // Delay to ensure the asynchronous code that records the histograms is executed.
        verify(mParentAuthDelegateMock, timeout(CriteriaHelper.DEFAULT_MAX_TIME_TO_POLL).times(1))
                .requestLocalAuth(any(WindowAndroid.class), any(GURL.class), any(Callback.class));

        histograms.pollInstrumentationThreadUntilSatisfied();
    }

    @Test
    @MediumTest
    public void parentRejectsLocally() {
        mockParentAuthDelegateRequestLocalAuthResponse(true);
        mTabbedActivityTestRule.loadUrl(mBlockedUrl);
        var histograms =
                HistogramWatcher.newBuilder()
                        .expectAnyRecord(
                                "FamilyLinkUser.LocalWebApprovalCompleteRequestTotalDuration")
                        .expectIntRecord("FamilyLinkUser.LocalWebApprovalResult", /* Declined= */ 1)
                        .build();

        WebsiteParentApprovalTestUtils.clickAskInPerson(mWebContents);
        WebsiteParentApprovalTestUtils.clickDoNotApprove(mBottomSheetTestSupport);

        // Delay to ensure the asynchronous code that records the histograms is executed.
        verify(mParentAuthDelegateMock, timeout(CriteriaHelper.DEFAULT_MAX_TIME_TO_POLL).times(1))
                .requestLocalAuth(any(WindowAndroid.class), any(GURL.class), any(Callback.class));

        histograms.pollInstrumentationThreadUntilSatisfied();
    }

    @Test
    @MediumTest
    public void parentAuthorizationFailure() {
        mockParentAuthDelegateRequestLocalAuthResponse(false);
        mTabbedActivityTestRule.loadUrl(mBlockedUrl);
        var histograms =
                HistogramWatcher.newSingleRecordWatcher(
                        "FamilyLinkUser.LocalWebApprovalResult", /* Cancelled= */ 2);

        WebsiteParentApprovalTestUtils.clickAskInPerson(mWebContents);

        // Delay to ensure the asynchronous code that records the histograms is executed.
        verify(mParentAuthDelegateMock, timeout(CriteriaHelper.DEFAULT_MAX_TIME_TO_POLL).times(1))
                .requestLocalAuth(any(WindowAndroid.class), any(GURL.class), any(Callback.class));

        histograms.pollInstrumentationThreadUntilSatisfied();
    }

    @Test
    @MediumTest
    public void cancelApprovalRequestIfOneAlreadyInProgress() {
        mockParentAuthDelegateRequestLocalAuthResponse(true);
        mTabbedActivityTestRule.loadUrl(mBlockedUrl);
        var histograms =
                HistogramWatcher.newBuilder()
                        .expectIntRecords(
                                "FamilyLinkUser.LocalWebApprovalResult",
                                /* Approved= */ 0,
                                /* Cancelled= */ 2)
                        .build();

        WebsiteParentApprovalTestUtils.clickAskInPerson(mWebContents);
        WebsiteParentApprovalTestUtils.clickAskInPerson(mWebContents);

        WebsiteParentApprovalTestUtils.clickApprove(mBottomSheetTestSupport);

        // Delay to ensure the asynchronous code that records the histograms is executed.
        verify(mParentAuthDelegateMock, timeout(CriteriaHelper.DEFAULT_MAX_TIME_TO_POLL).times(1))
                .requestLocalAuth(any(WindowAndroid.class), any(GURL.class), any(Callback.class));

        histograms.pollInstrumentationThreadUntilSatisfied();
    }
}
