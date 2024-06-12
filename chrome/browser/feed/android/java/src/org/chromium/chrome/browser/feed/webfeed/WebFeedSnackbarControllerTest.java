// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.webfeed;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.description;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.app.Activity;
import android.content.Context;
import android.content.res.Configuration;
import android.view.View;
import android.widget.TextView;

import androidx.test.filters.SmallTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.MockitoAnnotations;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Robolectric;
import org.robolectric.RuntimeEnvironment;
import org.robolectric.annotation.Config;
import org.robolectric.shadows.ShadowLog;

import org.chromium.base.LocaleUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.browser.feature_engagement.TrackerFactory;
import org.chromium.chrome.browser.feed.FeedServiceBridge;
import org.chromium.chrome.browser.feed.FeedSurfaceTracker;
import org.chromium.chrome.browser.feed.StreamKind;
import org.chromium.chrome.browser.feed.test.R;
import org.chromium.chrome.browser.feed.v2.FeedUserActionType;
import org.chromium.chrome.browser.feed.webfeed.WebFeedSnackbarController.FeedLauncher;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileManager;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabObserver;
import org.chromium.chrome.browser.ui.messages.snackbar.Snackbar;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.components.feature_engagement.FeatureConstants;
import org.chromium.components.feature_engagement.Tracker;
import org.chromium.components.feature_engagement.TriggerDetails;
import org.chromium.ui.modaldialog.DialogDismissalCause;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogProperties;
import org.chromium.url.GURL;
import org.chromium.url.JUnitTestGURLs;

import java.util.Locale;

/** Tests {@link WebFeedSnackbarController}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(
        manifest = Config.NONE,
        shadows = {})
@DisableFeatures(ChromeFeatureList.FEED_FOLLOW_UI_UPDATE)
@SmallTest
public final class WebFeedSnackbarControllerTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();
    @Rule public JniMocker mJniMocker = new JniMocker();

    private static final GURL sTestUrl = JUnitTestGURLs.EXAMPLE_URL;
    private static final GURL sFaviconUrl = JUnitTestGURLs.RED_1;
    private static final String sTitle = "Example Title";
    private static final byte[] sFollowId = new byte[] {1, 2, 3};

    @Mock private FeedLauncher mFeedLauncher;
    @Mock private Tracker mTracker;
    @Mock public WebFeedBridge.Natives mWebFeedBridgeJniMock;
    @Mock public FeedServiceBridge.Natives mFeedServideBridgeJniMock;
    private Context mContext;
    @Mock private Profile mProfile;
    private ModalDialogManager mDialogManager =
            new ModalDialogManager(Mockito.mock(ModalDialogManager.Presenter.class), 0);
    @Mock private SnackbarManager mSnackbarManager;
    private WebFeedSnackbarController mWebFeedSnackbarController;
    @Mock private Tab mTab;
    // The current observer of Tab.
    private TabObserver mTabObserver;

    @Captor private ArgumentCaptor<Snackbar> mSnackbarCaptor;
    @Captor private ArgumentCaptor<WebFeedBridge.WebFeedPageInformation> mPageInformationCaptor;

    @Before
    public void setUp() {
        // Print logs to stdout.
        ShadowLog.stream = System.out;

        // Set default locale to other country in order not to make
        // FeedFeatures.isFeedFollowUiUpdateEnabled always return true.
        Configuration config = new Configuration();
        config.setLocale(new Locale("tl", "PH"));
        LocaleUtils.setDefaultLocalesFromConfiguration(config);

        ProfileManager.setLastUsedProfileForTesting(mProfile);
        MockitoAnnotations.initMocks(this);
        mJniMocker.mock(WebFeedBridge.getTestHooksForTesting(), mWebFeedBridgeJniMock);
        mJniMocker.mock(FeedServiceBridge.getTestHooksForTesting(), mFeedServideBridgeJniMock);
        mContext = Robolectric.setupActivity(Activity.class);
        when(mTracker.shouldTriggerHelpUI(FeatureConstants.IPH_WEB_FEED_POST_FOLLOW_DIALOG_FEATURE))
                .thenReturn(false);
        when(mTracker.shouldTriggerHelpUIWithSnooze(
                        FeatureConstants.IPH_WEB_FEED_POST_FOLLOW_DIALOG_FEATURE))
                .thenReturn(new TriggerDetails(false, false));
        when(mTab.getOriginalUrl()).thenReturn(sTestUrl);
        Mockito.doAnswer(
                        invocationOnMock -> {
                            mTabObserver = (TabObserver) invocationOnMock.getArgument(0);
                            return null;
                        })
                .when(mTab)
                .addObserver(any());
        Mockito.doAnswer(
                        invocationOnMock -> {
                            mTabObserver = null;
                            return null;
                        })
                .when(mTab)
                .removeObserver(any());
        TrackerFactory.setTrackerForTests(mTracker);

        mWebFeedSnackbarController =
                new WebFeedSnackbarController(
                        RuntimeEnvironment.application,
                        mFeedLauncher,
                        mDialogManager,
                        mSnackbarManager);
    }

    @Test
    public void showSnackbarForFollow_successful_withMetadata() {
        mWebFeedSnackbarController.showPostFollowHelp(
                mTab,
                getSuccessfulFollowResult(),
                sFollowId,
                sTestUrl,
                sTitle,
                WebFeedBridge.CHANGE_REASON_WEB_PAGE_MENU);

        verify(mSnackbarManager).showSnackbar(mSnackbarCaptor.capture());
        Snackbar snackbar = mSnackbarCaptor.getValue();
        assertEquals(
                "Snackbar should be for successful follow.",
                Snackbar.UMA_WEB_FEED_FOLLOW_SUCCESS,
                snackbar.getIdentifierForTesting());
        assertEquals(
                "Snackbar message should be for successful follow with title from metadata.",
                mContext.getString(
                        R.string.web_feed_follow_success_snackbar_message,
                        getSuccessfulFollowResult().metadata.title),
                snackbar.getTextForTesting());
    }

    @Test
    public void showPostSuccessfulFollowHelp_ShowsSnackbar_FromForYouFeed() {
        mWebFeedSnackbarController.showPostSuccessfulFollowHelp(
                sTitle, true, StreamKind.FOR_YOU, null, null);

        assertFalse("Dialog should not be showing.", mDialogManager.isShowing());
        verify(mSnackbarManager).showSnackbar(mSnackbarCaptor.capture());
        Snackbar snackbar = mSnackbarCaptor.getValue();
        assertEquals(
                "Snackbar should be for successful follow.",
                Snackbar.UMA_WEB_FEED_FOLLOW_SUCCESS,
                snackbar.getIdentifierForTesting());
        assertEquals(
                "Snackbar message should be for successful follow with title from metadata.",
                mContext.getString(
                        R.string.web_feed_follow_success_snackbar_message,
                        getSuccessfulFollowResult().metadata.title),
                snackbar.getTextForTesting());
        assertEquals(
                "Snackbar action should be for going to the Following feed.",
                mContext.getString(
                        R.string.web_feed_follow_success_snackbar_action_go_to_following),
                snackbar.getActionText());
    }

    @Test
    public void showPostSuccessfulFollowHelp_ShowsSnackbar_FromFollowingFeed() {
        mWebFeedSnackbarController.showPostSuccessfulFollowHelp(
                sTitle, true, StreamKind.FOLLOWING, mTab, sTestUrl);

        assertFalse("Dialog should not be showing.", mDialogManager.isShowing());
        verify(mSnackbarManager).showSnackbar(mSnackbarCaptor.capture());
        Snackbar snackbar = mSnackbarCaptor.getValue();
        assertEquals(
                "Snackbar should be for successful follow.",
                Snackbar.UMA_WEB_FEED_FOLLOW_SUCCESS,
                snackbar.getIdentifierForTesting());
        assertEquals(
                "Snackbar message should be for successful follow with title from metadata.",
                mContext.getString(
                        R.string.web_feed_follow_success_snackbar_message,
                        getSuccessfulFollowResult().metadata.title),
                snackbar.getTextForTesting());
        assertEquals(
                "Snackbar action should be for refreshing the contents of the feed.",
                mContext.getString(R.string.web_feed_follow_success_snackbar_action_refresh),
                snackbar.getActionText());
    }

    @Test
    public void showSnackbarForFollow_successful_noMetadata() {
        WebFeedBridge.FollowResults followResults =
                new WebFeedBridge.FollowResults(
                        WebFeedSubscriptionRequestStatus.SUCCESS, /* metadata= */ null);

        mWebFeedSnackbarController.showPostFollowHelp(
                mTab,
                followResults,
                sFollowId,
                sTestUrl,
                sTitle,
                WebFeedBridge.CHANGE_REASON_WEB_PAGE_MENU);

        verify(mSnackbarManager).showSnackbar(mSnackbarCaptor.capture());
        Snackbar snackbar = mSnackbarCaptor.getValue();
        assertEquals(
                "Snackbar should be for successful follow.",
                Snackbar.UMA_WEB_FEED_FOLLOW_SUCCESS,
                snackbar.getIdentifierForTesting());
        assertEquals(
                "Snackbar message should be for successful follow with title from input.",
                mContext.getString(R.string.web_feed_follow_success_snackbar_message, sTitle),
                snackbar.getTextForTesting());

        // Simulate a navigation, the snackbar is dismissed.
        mTabObserver.onPageLoadStarted(mTab, JUnitTestGURLs.URL_2);
        verify(mSnackbarManager).dismissSnackbars(eq(snackbar.getController()));
    }

    @Test
    public void showSnackbarForFollow_correctDuration() {
        WebFeedBridge.FollowResults followResults =
                new WebFeedBridge.FollowResults(
                        WebFeedSubscriptionRequestStatus.SUCCESS, /* metadata= */ null);

        mWebFeedSnackbarController.showPostFollowHelp(
                mTab,
                followResults,
                sFollowId,
                sTestUrl,
                sTitle,
                WebFeedBridge.CHANGE_REASON_WEB_PAGE_MENU);

        verify(mSnackbarManager).showSnackbar(mSnackbarCaptor.capture());
        assertEquals(
                "Snackbar duration for follow should be correct.",
                WebFeedSnackbarController.SNACKBAR_DURATION_MS,
                mSnackbarCaptor.getValue().getDuration());
    }

    @Test
    public void showPromoDialogForFollow_successful_active() {
        when(mTracker.shouldTriggerHelpUI(FeatureConstants.IPH_WEB_FEED_POST_FOLLOW_DIALOG_FEATURE))
                .thenReturn(true);
        when(mTracker.shouldTriggerHelpUIWithSnooze(
                        FeatureConstants.IPH_WEB_FEED_POST_FOLLOW_DIALOG_FEATURE))
                .thenReturn(new TriggerDetails(true, false));
        WebFeedBridge.FollowResults followResults = getSuccessfulFollowResult();

        mWebFeedSnackbarController.showPostFollowHelp(
                mTab,
                followResults,
                sFollowId,
                sTestUrl,
                sTitle,
                WebFeedBridge.CHANGE_REASON_WEB_PAGE_MENU);

        View currentDialog =
                mDialogManager.getCurrentDialogForTest().get(ModalDialogProperties.CUSTOM_VIEW);
        verify(mSnackbarManager, times(0)).showSnackbar(any());
        assertTrue("Dialog should be showing.", mDialogManager.isShowing());
        assertEquals(
                "Dialog title should be correct for active follow.",
                mContext.getString(R.string.web_feed_post_follow_dialog_title, sTitle),
                ((TextView) currentDialog.findViewById(R.id.web_feed_dialog_title)).getText());
        assertEquals(
                "Dialog details should be for active follow.",
                mContext.getString(
                        R.string.web_feed_post_follow_dialog_stories_ready_description, sTitle),
                ((TextView) currentDialog.findViewById(R.id.web_feed_dialog_details)).getText());
    }

    @Test
    public void showPostSuccessfulFollowHelp_Dialog_FromForYouFeed() {
        when(mTracker.shouldTriggerHelpUI(FeatureConstants.IPH_WEB_FEED_POST_FOLLOW_DIALOG_FEATURE))
                .thenReturn(true);
        when(mTracker.shouldTriggerHelpUIWithSnooze(
                        FeatureConstants.IPH_WEB_FEED_POST_FOLLOW_DIALOG_FEATURE))
                .thenReturn(new TriggerDetails(true, false));

        mWebFeedSnackbarController.showPostSuccessfulFollowHelp(
                sTitle, true, StreamKind.FOR_YOU, mTab, sTestUrl);

        View currentDialog =
                mDialogManager.getCurrentDialogForTest().get(ModalDialogProperties.CUSTOM_VIEW);
        assertTrue("Dialog should be showing.", mDialogManager.isShowing());
        // TODO(b/243676323): figure out how to test the positive_button label, which is out of the
        // currentDialog hierarchy.

        // No snackbar should be shown after the dialog is closed.
        mDialogManager.dismissAllDialogs(DialogDismissalCause.POSITIVE_BUTTON_CLICKED);
        verify(mSnackbarManager, times(0)).showSnackbar(any());
    }

    @Test
    public void showPostSuccessfulFollowHelp_DialogAndSnackbar_FromFollowingFeed() {
        when(mTracker.shouldTriggerHelpUI(FeatureConstants.IPH_WEB_FEED_POST_FOLLOW_DIALOG_FEATURE))
                .thenReturn(true);
        when(mTracker.shouldTriggerHelpUIWithSnooze(
                        FeatureConstants.IPH_WEB_FEED_POST_FOLLOW_DIALOG_FEATURE))
                .thenReturn(new TriggerDetails(true, false));

        mWebFeedSnackbarController.showPostSuccessfulFollowHelp(
                sTitle, true, StreamKind.FOLLOWING, mTab, sTestUrl);

        verify(mSnackbarManager, times(0)).showSnackbar(any());
        View currentDialog =
                mDialogManager.getCurrentDialogForTest().get(ModalDialogProperties.CUSTOM_VIEW);
        assertTrue("Dialog should be showing.", mDialogManager.isShowing());
        // TODO(b/243676323): figure out how to test the positive_button label, which is out of the
        // currentDialog hierarchy.

        // A snackbar should be shown when the dialog is closed, offering the refresh action.
        mDialogManager.dismissAllDialogs(DialogDismissalCause.POSITIVE_BUTTON_CLICKED);
        verify(mSnackbarManager).showSnackbar(mSnackbarCaptor.capture());
        Snackbar snackbar = mSnackbarCaptor.getValue();
        assertEquals(
                "Snackbar should be for successful follow.",
                Snackbar.UMA_WEB_FEED_FOLLOW_SUCCESS,
                snackbar.getIdentifierForTesting());
        assertEquals(
                "Snackbar message should be for successful follow with title from metadata.",
                mContext.getString(
                        R.string.web_feed_follow_success_snackbar_message,
                        getSuccessfulFollowResult().metadata.title),
                snackbar.getTextForTesting());
        assertEquals(
                "Snackbar action should be for refreshing the contents of the feed.",
                mContext.getString(R.string.web_feed_follow_success_snackbar_action_refresh),
                snackbar.getActionText());
    }

    @Test
    public void showPromoDialogForFollow_successful_notActive() {
        when(mTracker.shouldTriggerHelpUI(FeatureConstants.IPH_WEB_FEED_POST_FOLLOW_DIALOG_FEATURE))
                .thenReturn(true);
        when(mTracker.shouldTriggerHelpUIWithSnooze(
                        FeatureConstants.IPH_WEB_FEED_POST_FOLLOW_DIALOG_FEATURE))
                .thenReturn(new TriggerDetails(true, false));
        WebFeedBridge.FollowResults followResults =
                new WebFeedBridge.FollowResults(
                        WebFeedSubscriptionRequestStatus.SUCCESS,
                        new WebFeedBridge.WebFeedMetadata(
                                sFollowId,
                                sTitle,
                                sTestUrl,
                                WebFeedSubscriptionRequestStatus.SUCCESS,
                                WebFeedAvailabilityStatus.INACTIVE,
                                /* isRecommended= */ false,
                                sFaviconUrl));

        mWebFeedSnackbarController.showPostFollowHelp(
                mTab,
                followResults,
                sFollowId,
                sTestUrl,
                sTitle,
                WebFeedBridge.CHANGE_REASON_WEB_PAGE_MENU);

        View currentDialog =
                mDialogManager.getCurrentDialogForTest().get(ModalDialogProperties.CUSTOM_VIEW);
        verify(mSnackbarManager, times(0)).showSnackbar(any());
        assertTrue("Dialog should be showing.", mDialogManager.isShowing());
        assertEquals(
                "Dialog title should be correct for inactive follow.",
                mContext.getString(R.string.web_feed_post_follow_dialog_title, sTitle),
                ((TextView) currentDialog.findViewById(R.id.web_feed_dialog_title)).getText());
        assertEquals(
                "Dialog details should be for inactive follow.",
                mContext.getString(
                        R.string.web_feed_post_follow_dialog_stories_not_ready_description, sTitle),
                ((TextView) currentDialog.findViewById(R.id.web_feed_dialog_details)).getText());
    }

    @Test
    public void showPromoDialogForFollow_successful_noMetadata() {
        when(mTracker.shouldTriggerHelpUI(FeatureConstants.IPH_WEB_FEED_POST_FOLLOW_DIALOG_FEATURE))
                .thenReturn(true);
        when(mTracker.shouldTriggerHelpUIWithSnooze(
                        FeatureConstants.IPH_WEB_FEED_POST_FOLLOW_DIALOG_FEATURE))
                .thenReturn(new TriggerDetails(true, false));
        WebFeedBridge.FollowResults followResults =
                new WebFeedBridge.FollowResults(
                        WebFeedSubscriptionRequestStatus.SUCCESS, /* metadata= */ null);

        mWebFeedSnackbarController.showPostFollowHelp(
                mTab,
                followResults,
                sFollowId,
                sTestUrl,
                sTitle,
                WebFeedBridge.CHANGE_REASON_WEB_PAGE_MENU);

        View currentDialog =
                mDialogManager.getCurrentDialogForTest().get(ModalDialogProperties.CUSTOM_VIEW);
        verify(mSnackbarManager, times(0)).showSnackbar(any());
        assertTrue("Dialog should be showing.", mDialogManager.isShowing());
        assertEquals(
                "Dialog title should be correct.",
                mContext.getString(R.string.web_feed_post_follow_dialog_title, sTitle),
                ((TextView) currentDialog.findViewById(R.id.web_feed_dialog_title)).getText());
        assertEquals(
                "Dialog details should be for active follow.",
                mContext.getString(
                        R.string.web_feed_post_follow_dialog_stories_not_ready_description, sTitle),
                ((TextView) currentDialog.findViewById(R.id.web_feed_dialog_details)).getText());
    }

    @Test
    public void showSnackbarForFollow_noId_unsuccessful() {
        mWebFeedSnackbarController.showPostFollowHelp(
                mTab,
                failureFollowResults(),
                "".getBytes(),
                sTestUrl,
                sTitle,
                WebFeedBridge.CHANGE_REASON_WEB_PAGE_MENU);

        verify(mSnackbarManager).showSnackbar(mSnackbarCaptor.capture());
        Snackbar snackbar = mSnackbarCaptor.getValue();
        assertEquals(
                "Snackbar should be for unsuccessful follow.",
                Snackbar.UMA_WEB_FEED_FOLLOW_FAILURE,
                snackbar.getIdentifierForTesting());
        assertEquals(
                "Snackbar message should be for unsuccessful follow.",
                mContext.getString(R.string.web_feed_follow_generic_failure_snackbar_message),
                snackbar.getTextForTesting());
        assertEquals(
                mContext.getString(R.string.web_feed_generic_failure_snackbar_action),
                snackbar.getActionText());

        // Click follow try again button.
        snackbar.getController().onAction(null);
        verify(
                        mWebFeedBridgeJniMock,
                        description(
                                "FollowFromUrl should be called on follow try again when ID is not "
                                        + "available."))
                .followWebFeed(
                        mPageInformationCaptor.capture(),
                        eq(WebFeedBridge.CHANGE_REASON_WEB_PAGE_MENU),
                        any());
        assertEquals(sTestUrl, mPageInformationCaptor.getValue().mUrl);
        assertEquals(mTab, mPageInformationCaptor.getValue().mTab);
        verify(mFeedServideBridgeJniMock)
                .reportOtherUserAction(
                        StreamKind.UNKNOWN, FeedUserActionType.TAPPED_FOLLOW_TRY_AGAIN_ON_SNACKBAR);
    }

    @Test
    public void showSnackbarForFollow_tryAgain_dismissedAfterNavigation() {
        mWebFeedSnackbarController.showPostFollowHelp(
                mTab,
                failureFollowResults(),
                "".getBytes(),
                sTestUrl,
                sTitle,
                WebFeedBridge.CHANGE_REASON_WEB_PAGE_MENU);

        verify(mSnackbarManager).showSnackbar(mSnackbarCaptor.capture());
        Snackbar snackbar = mSnackbarCaptor.getValue();
        assertEquals(
                "Snackbar should be for unsuccessful follow.",
                Snackbar.UMA_WEB_FEED_FOLLOW_FAILURE,
                snackbar.getIdentifierForTesting());
        assertEquals(
                "Snackbar message should be for unsuccessful follow.",
                mContext.getString(R.string.web_feed_follow_generic_failure_snackbar_message),
                snackbar.getTextForTesting());

        // Simulate a navigation, the snackbar is dismissed.
        mTabObserver.onPageLoadStarted(mTab, JUnitTestGURLs.URL_2);
        verify(mSnackbarManager).dismissSnackbars(eq(snackbar.getController()));
    }

    @Test
    public void showSnackbarForFollow_tryAgain_dismissedAfterTabHidden() {
        mWebFeedSnackbarController.showPostFollowHelp(
                mTab,
                failureFollowResults(),
                "".getBytes(),
                sTestUrl,
                sTitle,
                WebFeedBridge.CHANGE_REASON_WEB_PAGE_MENU);

        verify(mSnackbarManager).showSnackbar(mSnackbarCaptor.capture());
        Snackbar snackbar = mSnackbarCaptor.getValue();
        assertEquals(
                "Snackbar should be for unsuccessful follow.",
                Snackbar.UMA_WEB_FEED_FOLLOW_FAILURE,
                snackbar.getIdentifierForTesting());
        assertEquals(
                "Snackbar message should be for unsuccessful follow.",
                mContext.getString(R.string.web_feed_follow_generic_failure_snackbar_message),
                snackbar.getTextForTesting());

        // Hide tab, the snackbar is dismissed.
        mTabObserver.onHidden(mTab, 0);
        verify(mSnackbarManager).dismissSnackbars(eq(snackbar.getController()));
    }

    @Test
    public void showSnackbarForFollow_followFailureAfterNavigation_showsNoTryAgainAction() {
        // mTab reports sTestUrl as the current page. Show a follow-failure snackbar for a different
        // page.
        mWebFeedSnackbarController.showPostFollowHelp(
                mTab,
                failureFollowResults(),
                "".getBytes(),
                JUnitTestGURLs.URL_2,
                sTitle,
                WebFeedBridge.CHANGE_REASON_WEB_PAGE_MENU);

        verify(mSnackbarManager).showSnackbar(mSnackbarCaptor.capture());

        assertEquals(
                "Snackbar should have no action", null, mSnackbarCaptor.getValue().getActionText());
    }

    @Test
    public void showSnackbarForFollow_withId_unsuccessful() {
        WebFeedBridge.FollowResults followResults = failureFollowResults();

        mWebFeedSnackbarController.showPostFollowHelp(
                mTab,
                followResults,
                sFollowId,
                sTestUrl,
                sTitle,
                WebFeedBridge.CHANGE_REASON_WEB_PAGE_MENU);

        verify(mSnackbarManager).showSnackbar(mSnackbarCaptor.capture());
        Snackbar snackbar = mSnackbarCaptor.getValue();
        assertEquals(
                "Snackbar should be for unsuccessful follow.",
                Snackbar.UMA_WEB_FEED_FOLLOW_FAILURE,
                snackbar.getIdentifierForTesting());
        assertEquals(
                "Snackbar message should be for unsuccessful follow.",
                mContext.getString(R.string.web_feed_follow_generic_failure_snackbar_message),
                snackbar.getTextForTesting());

        // Click follow try again button.
        snackbar.getController().onAction(null);
        verify(
                        mWebFeedBridgeJniMock,
                        description(
                                "FollowFromId should be called on follow try again when ID is"
                                        + " available."))
                .followWebFeedById(
                        eq(sFollowId),
                        /* isDurable= */ eq(false),
                        eq(WebFeedBridge.CHANGE_REASON_WEB_PAGE_MENU),
                        any());
        verify(mFeedServideBridgeJniMock)
                .reportOtherUserAction(
                        StreamKind.UNKNOWN, FeedUserActionType.TAPPED_FOLLOW_TRY_AGAIN_ON_SNACKBAR);
    }

    @Test
    public void showSnackbarForUnfollow_successful() {
        mWebFeedSnackbarController.showSnackbarForUnfollow(
                WebFeedSubscriptionRequestStatus.SUCCESS,
                sFollowId,
                mTab,
                sTestUrl,
                sTitle,
                WebFeedBridge.CHANGE_REASON_WEB_PAGE_MENU);

        verify(mSnackbarManager).showSnackbar(mSnackbarCaptor.capture());
        Snackbar snackbar = mSnackbarCaptor.getValue();
        assertEquals(
                "Snackbar should be for successful unfollow.",
                Snackbar.UMA_WEB_FEED_UNFOLLOW_SUCCESS,
                snackbar.getIdentifierForTesting());

        // Click refollow button.
        snackbar.getController().onAction(null);
        verify(mWebFeedBridgeJniMock, description("Follow should be called on refollow."))
                .followWebFeedById(
                        eq(sFollowId),
                        /* isDurable= */ eq(false),
                        eq(WebFeedBridge.CHANGE_REASON_WEB_PAGE_MENU),
                        any());
        verify(mFeedServideBridgeJniMock)
                .reportOtherUserAction(
                        StreamKind.UNKNOWN,
                        FeedUserActionType.TAPPED_REFOLLOW_AFTER_UNFOLLOW_ON_SNACKBAR);
    }

    @Test
    public void showSnackbarForUnfollow_successful_nourl() {
        mWebFeedSnackbarController.showSnackbarForUnfollow(
                WebFeedSubscriptionRequestStatus.SUCCESS,
                sFollowId,
                null,
                null,
                sTitle,
                WebFeedBridge.CHANGE_REASON_WEB_PAGE_MENU);

        verify(mSnackbarManager).showSnackbar(mSnackbarCaptor.capture());
        Snackbar snackbar = mSnackbarCaptor.getValue();
        assertEquals(
                "Snackbar should be for successful unfollow.",
                Snackbar.UMA_WEB_FEED_UNFOLLOW_SUCCESS,
                snackbar.getIdentifierForTesting());

        // Click refollow button.
        snackbar.getController().onAction(null);
        verify(mWebFeedBridgeJniMock, description("Follow should be called on refollow."))
                .followWebFeedById(
                        eq(sFollowId),
                        /* isDurable= */ eq(false),
                        eq(WebFeedBridge.CHANGE_REASON_WEB_PAGE_MENU),
                        any());
        verify(mFeedServideBridgeJniMock)
                .reportOtherUserAction(
                        StreamKind.UNKNOWN,
                        FeedUserActionType.TAPPED_REFOLLOW_AFTER_UNFOLLOW_ON_SNACKBAR);
    }

    @Test
    public void showSnackbarForUnfollow_unsuccessful() {
        mWebFeedSnackbarController.showSnackbarForUnfollow(
                WebFeedSubscriptionRequestStatus.FAILED_OFFLINE,
                sFollowId,
                mTab,
                sTestUrl,
                sTitle,
                WebFeedBridge.CHANGE_REASON_WEB_PAGE_MENU);

        verify(mSnackbarManager).showSnackbar(mSnackbarCaptor.capture());
        Snackbar snackbar = mSnackbarCaptor.getValue();
        assertEquals(
                "Snackbar should be for unsuccessful unfollow.",
                Snackbar.UMA_WEB_FEED_UNFOLLOW_FAILURE,
                snackbar.getIdentifierForTesting());

        // Click unfollow try again button.
        snackbar.getController().onAction(null);
        verify(
                        mWebFeedBridgeJniMock,
                        description("Unfollow should be called on unfollow try again."))
                .unfollowWebFeed(
                        eq(sFollowId),
                        /* isDurable= */ eq(false),
                        eq(WebFeedBridge.CHANGE_REASON_WEB_PAGE_MENU),
                        any());
        verify(mFeedServideBridgeJniMock)
                .reportOtherUserAction(
                        StreamKind.UNKNOWN,
                        FeedUserActionType.TAPPED_UNFOLLOW_TRY_AGAIN_ON_SNACKBAR);
    }

    @Test
    public void showSnackbarForUnfollow_unsuccessful_nourl() {
        mWebFeedSnackbarController.showSnackbarForUnfollow(
                WebFeedSubscriptionRequestStatus.FAILED_OFFLINE,
                sFollowId,
                null,
                null,
                sTitle,
                WebFeedBridge.CHANGE_REASON_WEB_PAGE_MENU);

        verify(mSnackbarManager).showSnackbar(mSnackbarCaptor.capture());
        Snackbar snackbar = mSnackbarCaptor.getValue();
        assertEquals(
                "Snackbar should be for unsuccessful unfollow.",
                Snackbar.UMA_WEB_FEED_UNFOLLOW_FAILURE,
                snackbar.getIdentifierForTesting());

        // Click unfollow try again button.
        snackbar.getController().onAction(null);
        verify(
                        mWebFeedBridgeJniMock,
                        description("Unfollow should be called on unfollow try again."))
                .unfollowWebFeed(
                        eq(sFollowId),
                        /* isDurable= */ eq(false),
                        eq(WebFeedBridge.CHANGE_REASON_WEB_PAGE_MENU),
                        any());
        verify(mFeedServideBridgeJniMock)
                .reportOtherUserAction(
                        StreamKind.UNKNOWN,
                        FeedUserActionType.TAPPED_UNFOLLOW_TRY_AGAIN_ON_SNACKBAR);
    }

    @Test
    public void showSnackbarForUnfollow_correctDuration() {
        mWebFeedSnackbarController.showSnackbarForUnfollow(
                WebFeedSubscriptionRequestStatus.SUCCESS,
                sFollowId,
                mTab,
                sTestUrl,
                sTitle,
                WebFeedBridge.CHANGE_REASON_WEB_PAGE_MENU);

        verify(mSnackbarManager).showSnackbar(mSnackbarCaptor.capture());
        Snackbar snackbar = mSnackbarCaptor.getValue();
        assertEquals(
                "Snackbar duration for unfollow should be correct.",
                WebFeedSnackbarController.SNACKBAR_DURATION_MS,
                snackbar.getDuration());
    }

    @Test
    public void postFollowSnackbarIsDismissedUponFeedSurfaceOpened() {
        mWebFeedSnackbarController.showPostFollowHelp(
                mTab,
                getSuccessfulFollowResult(),
                sFollowId,
                sTestUrl,
                sTitle,
                WebFeedBridge.CHANGE_REASON_WEB_PAGE_MENU);
        verify(mSnackbarManager).showSnackbar(mSnackbarCaptor.capture());
        Snackbar snackbar = mSnackbarCaptor.getValue();

        FeedSurfaceTracker.getInstance().surfaceOpened();
        verify(mSnackbarManager).dismissSnackbars(eq(mSnackbarCaptor.getValue().getController()));
    }

    private WebFeedBridge.FollowResults getSuccessfulFollowResult() {
        return new WebFeedBridge.FollowResults(
                WebFeedSubscriptionRequestStatus.SUCCESS,
                new WebFeedBridge.WebFeedMetadata(
                        sFollowId,
                        sTitle,
                        sTestUrl,
                        WebFeedSubscriptionStatus.SUBSCRIBED,
                        WebFeedAvailabilityStatus.ACTIVE,
                        /* isRecommended= */ true,
                        sFaviconUrl));
    }

    private WebFeedBridge.FollowResults failureFollowResults() {
        return new WebFeedBridge.FollowResults(
                WebFeedSubscriptionRequestStatus.FAILED_UNKNOWN_ERROR, null);
    }
}
