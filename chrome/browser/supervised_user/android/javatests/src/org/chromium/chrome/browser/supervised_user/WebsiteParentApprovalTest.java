// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.supervised_user;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.matcher.ViewMatchers.isCompletelyDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.isRoot;
import static androidx.test.espresso.matcher.ViewMatchers.withId;

import static org.mockito.Mockito.any;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.doNothing;
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
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.DoNotBatch;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.superviseduser.FilteringBehavior;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.util.browser.Features.EnableFeatures;
import org.chromium.chrome.test.util.browser.signin.SigninTestRule;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetTestSupport;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.test.util.DOMUtils;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.net.test.EmbeddedTestServer;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.test.util.DisableAnimationsTestRule;
import org.chromium.ui.test.util.ViewUtils;
import org.chromium.url.GURL;

import java.util.concurrent.TimeoutException;

/**
 * Tests the local website approval flow.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@DoNotBatch(reason = "Running tests in parallel can interfere with each tests setup."
                + "The code under tests involes setting static methods and features, "
                + "which must remain unchanged for the duration of the test.")
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@EnableFeatures(
        {ChromeFeatureList.LOCAL_WEB_APPROVALS, ChromeFeatureList.WEB_FILTER_INTERSTITIAL_REFRESH})
public class WebsiteParentApprovalTest {
    // TODO(b/243916194): Expand the test coverage beyond the completion callback, up to the page
    // refresh.
    // (TODO b/243916194): Expand test until the metric collection step on the native side. Requires
    // not mocking the natives so that the flow moves onto their real execution.

    public ChromeTabbedActivityTestRule mTabbedActivityTestRule =
            new ChromeTabbedActivityTestRule();
    public SigninTestRule mSigninTestRule = new SigninTestRule();

    // Destroy TabbedActivityTestRule before SigninTestRule to remove observers of
    // FakeAccountManagerFacade.
    @Rule
    public final RuleChain mRuleChain =
            RuleChain.outerRule(mSigninTestRule).around(mTabbedActivityTestRule);
    @Rule
    public final JniMocker mocker = new JniMocker();
    @Rule
    public final MockitoRule mMockitoRule = MockitoJUnit.rule().strictness(Strictness.STRICT_STUBS);
    @Rule
    public final DisableAnimationsTestRule sDisableAnimationsRule = new DisableAnimationsTestRule();

    private static final String TEST_PAGE = "/chrome/test/data/android/about.html";
    private static final String LOCAL_APPROVALS_BUTTON_NODE_ID = "local-approvals-button";

    private EmbeddedTestServer mTestServer;
    private String mBlockedUrl;
    private WebContents mWebContents;
    private BottomSheetController mBottomSheetController;
    private BottomSheetTestSupport mBottomSheetTestSupport;

    @Mock
    private WebsiteParentApproval.Natives mWebsiteParentApprovalNativesMock;
    @Mock
    private ParentAuthDelegate mParentAuthDelegateMock;

    @Before
    public void setUp() throws TimeoutException {
        mTestServer = mTabbedActivityTestRule.getEmbeddedTestServerRule().getServer();
        mBlockedUrl = mTestServer.getURL(TEST_PAGE);
        mTabbedActivityTestRule.startMainActivityOnBlankPage();
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            ChromeTabbedActivity activity = mTabbedActivityTestRule.getActivity();
            mBottomSheetController =
                    activity.getRootUiCoordinatorForTesting().getBottomSheetController();
            mBottomSheetTestSupport = new BottomSheetTestSupport(mBottomSheetController);
        });

        mSigninTestRule.addChildTestAccountThenWaitForSignin();
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            SupervisedUserSettingsBridge.setFilteringBehavior(
                    Profile.getLastUsedRegularProfile(), FilteringBehavior.BLOCK);
        });
        mWebContents = mTabbedActivityTestRule.getWebContents();

        mocker.mock(WebsiteParentApprovalJni.TEST_HOOKS, mWebsiteParentApprovalNativesMock);
        doNothing()
                .when(mWebsiteParentApprovalNativesMock)
                .fetchFavicon(any(GURL.class), any(Integer.class), any(Integer.class),
                        any(Callback.class));
        //@TODO b:243916194 : Trigger the execution of the real (void) method.
        doNothing().when(mWebsiteParentApprovalNativesMock).onCompletion(any(Integer.class));

        // @TODO b:243916194 : Once we start consuming mParentAuthDelegateMock
        // .isLocalAuthSupported we should add a mocked behaviour in this test.
        ParentAuthDelegateProvider.setInstanceForTests(mParentAuthDelegateMock);
    }

    private void mockParentAuthDelegateRequestLocalAuthResponse(boolean result) {
        doAnswer(invocation -> {
            Callback<Boolean> onCompletionCallback = invocation.getArgument(2);
            onCompletionCallback.onResult(result);
            return null;
        })
                .when(mParentAuthDelegateMock)
                .requestLocalAuth(any(WindowAndroid.class), any(GURL.class), any(Callback.class));
    }

    private void clickAskInPerson() {
        try {
            String contents =
                    DOMUtils.getNodeContents(mWebContents, LOCAL_APPROVALS_BUTTON_NODE_ID);
        } catch (TimeoutException e) {
            throw new RuntimeException("Local approval button not found");
        }
        DOMUtils.clickNodeWithJavaScript(mWebContents, LOCAL_APPROVALS_BUTTON_NODE_ID);
    }

    private void checkParentApprovalBottomSheetVisible() {
        onView(isRoot()).check(ViewUtils.waitForView(
                withId(R.id.local_parent_approval_layout), ViewUtils.VIEW_VISIBLE));
        // Ensure all animations have ended before allowing interaction with the view.
        TestThreadUtils.runOnUiThreadBlocking(
                () -> { mBottomSheetTestSupport.endAllAnimations(); });
    }

    private void clickApprove() {
        checkParentApprovalBottomSheetVisible();
        onView(withId(R.id.approve_button))
                .check(matches(isCompletelyDisplayed()))
                .perform(click());
    }

    private void clickDoNotApprove() {
        checkParentApprovalBottomSheetVisible();
        onView(withId(R.id.deny_button)).check(matches(isCompletelyDisplayed())).perform(click());
    }

    private void checkParentApprovalScreenClosedAfterClick() {
        // Ensure all animations have ended. Otherwise the following check may fail.
        TestThreadUtils.runOnUiThreadBlocking(
                () -> { mBottomSheetTestSupport.endAllAnimations(); });
        onView(isRoot()).check(ViewUtils.waitForView(withId(R.id.local_parent_approval_layout),
                ViewUtils.VIEW_INVISIBLE | ViewUtils.VIEW_GONE | ViewUtils.VIEW_NULL));
    }

    @Test
    @MediumTest
    public void parentApprovesScreenVisibilityAfterApproval() {
        mockParentAuthDelegateRequestLocalAuthResponse(true);
        mTabbedActivityTestRule.loadUrl(mBlockedUrl);

        clickAskInPerson();
        clickApprove();

        checkParentApprovalScreenClosedAfterClick();
    }

    @Test
    @MediumTest
    public void parentApprovesScreenVisibilityAfterRejection() {
        mockParentAuthDelegateRequestLocalAuthResponse(true);
        mTabbedActivityTestRule.loadUrl(mBlockedUrl);

        clickAskInPerson();
        clickDoNotApprove();

        checkParentApprovalScreenClosedAfterClick();
    }

    @Test
    @MediumTest
    public void parentApprovesLocally() {
        mockParentAuthDelegateRequestLocalAuthResponse(true);
        mTabbedActivityTestRule.loadUrl(mBlockedUrl);

        clickAskInPerson();
        clickApprove();

        verify(mWebsiteParentApprovalNativesMock,
                timeout(CriteriaHelper.DEFAULT_MAX_TIME_TO_POLL).times(1))
                .onCompletion(AndroidLocalWebApprovalFlowOutcome.APPROVED);
    }

    @Test
    @MediumTest
    public void parentRejectsLocally() {
        mockParentAuthDelegateRequestLocalAuthResponse(true);
        mTabbedActivityTestRule.loadUrl(mBlockedUrl);

        clickAskInPerson();
        clickDoNotApprove();

        verify(mWebsiteParentApprovalNativesMock,
                timeout(CriteriaHelper.DEFAULT_MAX_TIME_TO_POLL).times(1))
                .onCompletion(AndroidLocalWebApprovalFlowOutcome.REJECTED);
    }

    @Test
    @MediumTest
    public void parentAuthorizationFailure() {
        mockParentAuthDelegateRequestLocalAuthResponse(false);
        mTabbedActivityTestRule.loadUrl(mBlockedUrl);

        clickAskInPerson();

        verify(mWebsiteParentApprovalNativesMock,
                timeout(CriteriaHelper.DEFAULT_MAX_TIME_TO_POLL).times(1))
                .onCompletion(AndroidLocalWebApprovalFlowOutcome.INCOMPLETE);
    }
}
