// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.subresource_filter;

import android.text.Spanned;
import android.text.style.ClickableSpan;
import android.view.View;
import android.widget.TextView;

import androidx.test.espresso.Espresso;
import androidx.test.filters.LargeTest;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.browser.MockSafeBrowsingApiHandler;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.infobar.InfoBarContainer;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabCreationState;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelObserver;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.util.ChromeTabUtils;
import org.chromium.components.browser_ui.modaldialog.TabModalPresenter;
import org.chromium.components.infobars.InfoBar;
import org.chromium.components.messages.MessageBannerProperties;
import org.chromium.components.messages.MessageDispatcher;
import org.chromium.components.messages.MessageDispatcherProvider;
import org.chromium.components.messages.MessageIdentifier;
import org.chromium.components.messages.MessageStateHandler;
import org.chromium.components.messages.MessagesTestHelper;
import org.chromium.components.safe_browsing.SafeBrowsingApiBridge;
import org.chromium.components.subresource_filter.AdsBlockedInfoBar;
import org.chromium.net.test.EmbeddedTestServer;
import org.chromium.net.test.EmbeddedTestServerRule;
import org.chromium.ui.base.DeviceFormFactor;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogProperties;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.List;
import java.util.concurrent.ExecutionException;
import java.util.concurrent.TimeoutException;

/**
 * End to end tests of SubresourceFilter ad filtering on Android.
 *
 * <p>Since these tests take a while to set up (averaging 12 seconds between activity startup and
 * ruleset publishing), prefer to limit the number of test cases where possible.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public final class SubresourceFilterTest {
    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    @Rule public EmbeddedTestServerRule mTestServerRule = new EmbeddedTestServerRule();

    private EmbeddedTestServer mTestServer;

    private static final String PAGE_WITH_JPG =
            "/chrome/test/data/android/subresource_filter/page-with-img.html";
    private static final String LEARN_MORE_PAGE =
            "https://support.google.com/chrome/?p=blocked_ads";
    private static final String METADATA_FOR_ENFORCEMENT =
            "{\"matches\":[{\"threat_type\":\"13\",\"sf_bas\":\"\"}]}";
    private static final String METADATA_FOR_WARNING =
            "{\"matches\":[{\"threat_type\":\"13\",\"sf_bas\":\"warn\"}]}";

    private void createAndPublishRulesetDisallowingSuffix(String suffix) {
        TestRulesetPublisher publisher = new TestRulesetPublisher();
        ThreadUtils.runOnUiThreadBlocking(
                () -> publisher.createAndPublishRulesetDisallowingSuffixForTesting(suffix));

        // This takes an average of 6 seconds but can range anywhere from 2-10 seconds on occasion.
        // Since we are testing startup events, ensuring that they fire, this delay is unavoidable.
        // Increase the timeout to 15 seconds to remove flakiness.
        CriteriaHelper.pollUiThread(
                publisher::isPublished, 15000L, CriteriaHelper.DEFAULT_POLLING_INTERVAL);
    }

    @Before
    public void setUp() throws Exception {
        mTestServer = mTestServerRule.getServer();
        SafeBrowsingApiBridge.setSafeBrowsingApiHandler(new MockSafeBrowsingApiHandler());
        mActivityTestRule.startMainActivityOnBlankPage();

        // Disallow all jpgs.
        createAndPublishRulesetDisallowingSuffix(".jpg");
    }

    @After
    public void tearDown() {
        MockSafeBrowsingApiHandler.clearMockResponses();
        SafeBrowsingApiBridge.clearHandlerForTesting();
    }

    @Test
    @LargeTest
    public void resourceNotFiltered() throws Exception {
        String url = mTestServer.getURL(PAGE_WITH_JPG);
        mActivityTestRule.loadUrl(url);

        String loaded = mActivityTestRule.runJavaScriptCodeInCurrentTab("imgLoaded");
        Assert.assertEquals("true", loaded);

        // Check that the infobar is not showing.
        List<InfoBar> infoBars = mActivityTestRule.getInfoBars();
        CriteriaHelper.pollUiThread(() -> infoBars.isEmpty());
    }

    @Test
    @LargeTest
    @DisableFeatures(ChromeFeatureList.MESSAGES_FOR_ANDROID_ADS_BLOCKED)
    public void resourceFilteredClose_InfobarUI() throws Exception {
        String url = mTestServer.getURL(PAGE_WITH_JPG);
        Assert.assertFalse(loadPageWithBlockableContentAndTestIfBlocked(url, false));

        // Check that the infobar is showing.
        List<InfoBar> infoBars = mActivityTestRule.getInfoBars();
        CriteriaHelper.pollUiThread(() -> infoBars.size() == 1);
        AdsBlockedInfoBar infobar = (AdsBlockedInfoBar) infoBars.get(0);

        // Click the link once to expand it.
        ThreadUtils.runOnUiThreadBlocking(infobar::onLinkClicked);

        // Check the checkbox and press the button to reload.
        ThreadUtils.runOnUiThreadBlocking(() -> infobar.onCheckedChanged(null, true));

        // Think better of it and just close the infobar.
        ThreadUtils.runOnUiThreadBlocking(infobar::onCloseButtonClicked);
        Tab tab = mActivityTestRule.getActivity().getActivityTab();
        CriteriaHelper.pollUiThread(() -> !InfoBarContainer.get(tab).hasInfoBars());
    }

    @Test
    @LargeTest
    @DisableFeatures(ChromeFeatureList.MESSAGES_FOR_ANDROID_ADS_BLOCKED)
    public void resourceFilteredClickLearnMore_InfobarUI() throws Exception {
        String url = mTestServer.getURL(PAGE_WITH_JPG);
        Assert.assertFalse(loadPageWithBlockableContentAndTestIfBlocked(url, false));

        Tab originalTab = mActivityTestRule.getActivity().getActivityTab();
        CallbackHelper tabCreatedCallback = new CallbackHelper();
        TabModel tabModel = mActivityTestRule.getActivity().getTabModelSelector().getCurrentModel();
        ThreadUtils.runOnUiThreadBlocking(
                () ->
                        tabModel.addObserver(
                                new TabModelObserver() {
                                    @Override
                                    public void didAddTab(
                                            Tab tab,
                                            @TabLaunchType int type,
                                            @TabCreationState int creationState,
                                            boolean markedForSelection) {
                                        if (tab.getUrl().getSpec().equals(LEARN_MORE_PAGE)) {
                                            tabCreatedCallback.notifyCalled();
                                        }
                                    }
                                }));

        // Check that the infobar is showing.
        List<InfoBar> infoBars = mActivityTestRule.getInfoBars();
        CriteriaHelper.pollUiThread(() -> infoBars.size() == 1);
        AdsBlockedInfoBar infobar = (AdsBlockedInfoBar) infoBars.get(0);

        // Click the link once to expand it.
        ThreadUtils.runOnUiThreadBlocking(infobar::onLinkClicked);

        // Click again to navigate, which should spawn a new tab.
        ThreadUtils.runOnUiThreadBlocking(infobar::onLinkClicked);

        // Wait for the tab to be added with the correct URL. Note, do not wait for this URL to be
        // loaded since it is not controlled by the test instrumentation. Just waiting for the
        // navigation to start should be OK though.
        tabCreatedCallback.waitForCallback("Never received tab created event", 0);

        // The infobar should not be removed on the original tab.
        CriteriaHelper.pollUiThread(() -> InfoBarContainer.get(originalTab).hasInfoBars());
    }

    @Test
    @LargeTest
    @Restriction(DeviceFormFactor.PHONE)
    @EnableFeatures({ChromeFeatureList.MESSAGES_FOR_ANDROID_ADS_BLOCKED})
    public void resourceFilteredClickLearnMore_MessagesUI_ReshowDialogOnPhoneOnBackPress()
            throws Exception {
        testResourceFilteredClickLearnMore_MessagesUIFlow();
    }

    @Test
    @LargeTest
    @Restriction(DeviceFormFactor.TABLET)
    @EnableFeatures({ChromeFeatureList.MESSAGES_FOR_ANDROID_ADS_BLOCKED})
    public void resourceFilteredClickLearnMore_MessagesUI_ReshowDialogOnTabletOnBackPress()
            throws Exception {
        testResourceFilteredClickLearnMore_MessagesUIFlow();
    }

    @Test
    @LargeTest
    @DisableFeatures(ChromeFeatureList.MESSAGES_FOR_ANDROID_ADS_BLOCKED)
    public void resourceFilteredReload_InfobarUI() throws Exception {
        String url = mTestServer.getURL(PAGE_WITH_JPG);
        Assert.assertFalse(loadPageWithBlockableContentAndTestIfBlocked(url, false));

        // Check that the infobar is showing.
        List<InfoBar> infoBars = mActivityTestRule.getInfoBars();
        CriteriaHelper.pollUiThread(() -> infoBars.size() == 1);
        AdsBlockedInfoBar infobar = (AdsBlockedInfoBar) infoBars.get(0);

        // Click the link once to expand it.
        ThreadUtils.runOnUiThreadBlocking(infobar::onLinkClicked);

        // Check the checkbox and press the button to reload.
        ThreadUtils.runOnUiThreadBlocking(() -> infobar.onCheckedChanged(null, true));
        ThreadUtils.runOnUiThreadBlocking(() -> infobar.onButtonClicked(true));

        Assert.assertTrue(verifyPageReloadedWithOriginalContent(url));
    }

    @Test
    @LargeTest
    @EnableFeatures({ChromeFeatureList.MESSAGES_FOR_ANDROID_ADS_BLOCKED})
    public void resourceFilteredReload_MessagesUI() throws Exception {
        String url = mTestServer.getURL(PAGE_WITH_JPG);
        Assert.assertFalse(loadPageWithBlockableContentAndTestIfBlocked(url, false));

        // Check that the Ads Blocked message is showing and get the active message.
        PropertyModel message = verifyAndGetAdsBlockedMessage();

        // Trigger the Ads Blocked dialog and simulate the dialog positive button click.
        PropertyModel adsBlockedDialog = createAdsBlockedDialog(message);
        ModalDialogProperties.Controller dialogController =
                adsBlockedDialog.get(ModalDialogProperties.CONTROLLER);
        ThreadUtils.runOnUiThreadBlocking(
                () ->
                        dialogController.onClick(
                                adsBlockedDialog, ModalDialogProperties.ButtonType.POSITIVE));

        Assert.assertTrue(verifyPageReloadedWithOriginalContent(url));
    }

    @Test
    @LargeTest
    public void resourceNotFilteredWithWarning() throws Exception {
        String url = mTestServer.getURL(PAGE_WITH_JPG);
        Assert.assertTrue(loadPageWithBlockableContentAndTestIfBlocked(url, true));

        // Check that the infobar is not showing.
        List<InfoBar> infoBars = mActivityTestRule.getInfoBars();
        CriteriaHelper.pollUiThread(() -> infoBars.isEmpty());
    }

    private void testResourceFilteredClickLearnMore_MessagesUIFlow()
            throws TimeoutException, ExecutionException, InterruptedException {
        String url = mTestServer.getURL(PAGE_WITH_JPG);
        Assert.assertFalse(loadPageWithBlockableContentAndTestIfBlocked(url, false));

        CallbackHelper tabCreatedCallback = new CallbackHelper();
        TabModel tabModel = mActivityTestRule.getActivity().getTabModelSelector().getCurrentModel();
        ThreadUtils.runOnUiThreadBlocking(
                () ->
                        tabModel.addObserver(
                                new TabModelObserver() {
                                    @Override
                                    public void didAddTab(
                                            Tab tab,
                                            @TabLaunchType int type,
                                            @TabCreationState int creationState,
                                            boolean markedForSelection) {
                                        if (tab.getUrl().getSpec().equals(LEARN_MORE_PAGE)) {
                                            tabCreatedCallback.notifyCalled();
                                        }
                                    }
                                }));

        // Check that the Ads Blocked message is showing and get the active message.
        PropertyModel message = verifyAndGetAdsBlockedMessage();

        int currentTabCreatedCallbackCount = tabCreatedCallback.getCallCount();

        // Trigger the Ads Blocked dialog and simulate the "Learn more" link click.
        createAdsBlockedDialog(message);
        View dialogView =
                ((TabModalPresenter)
                                mActivityTestRule
                                        .getActivity()
                                        .getModalDialogManager()
                                        .getCurrentPresenterForTest())
                        .getDialogContainerForTest();
        TextView messageView = dialogView.findViewById(R.id.message_paragraph_1);
        Spanned spannedMessage = (Spanned) messageView.getText();
        ClickableSpan[] spans =
                spannedMessage.getSpans(0, spannedMessage.length(), ClickableSpan.class);
        Assert.assertEquals(
                "Ads Blocked dialog message text must have only 1 ClickableSpan.", 1, spans.length);
        ThreadUtils.runOnUiThreadBlocking(() -> spans[0].onClick(messageView));

        // Wait for the tab to be added with the correct URL. Note, do not wait for this URL to be
        // loaded since it is not controlled by the test instrumentation. Just waiting for the
        // navigation to start should be OK though.
        tabCreatedCallback.waitForCallback(
                "Never received tab created event", currentTabCreatedCallbackCount);

        // Press the back button to go to the original tab where the dialog was shown.
        Espresso.pressBack();

        CriteriaHelper.pollUiThread(
                () -> {
                    // Verify that the dialog is re-shown on the original tab.
                    return mActivityTestRule
                                    .getActivity()
                                    .getModalDialogManager()
                                    .getCurrentDialogForTest()
                            != null;
                },
                "The dialog should be re-shown on navigation to the original tab.");
    }

    private boolean loadPageWithBlockableContentAndTestIfBlocked(String url, boolean isForWarning)
            throws TimeoutException {
        int[] threatAttribute =
                isForWarning
                        ? new int[] {MockSafeBrowsingApiHandler.THREAT_ATTRIBUTE_CANARY_CODE}
                        : new int[0];
        MockSafeBrowsingApiHandler.addMockResponse(
                url, MockSafeBrowsingApiHandler.BETTER_ADS_VIOLATION_CODE, threatAttribute);
        mActivityTestRule.loadUrl(url);
        return Boolean.parseBoolean(mActivityTestRule.runJavaScriptCodeInCurrentTab("imgLoaded"));
    }

    private PropertyModel verifyAndGetAdsBlockedMessage() throws ExecutionException {
        MessageDispatcher messageDispatcher =
                ThreadUtils.runOnUiThreadBlocking(
                        () ->
                                MessageDispatcherProvider.from(
                                        mActivityTestRule.getActivity().getWindowAndroid()));
        List<MessageStateHandler> messages =
                MessagesTestHelper.getEnqueuedMessages(
                        messageDispatcher, MessageIdentifier.ADS_BLOCKED);
        return MessagesTestHelper.getCurrentMessage(messages.get(0));
    }

    private PropertyModel createAdsBlockedDialog(PropertyModel message) {
        // Simulate the message secondary button click.
        Runnable secondaryActionCallback = message.get(MessageBannerProperties.ON_SECONDARY_ACTION);
        ThreadUtils.runOnUiThreadBlocking(secondaryActionCallback);

        // Retrieve the Ads Blocked dialog.
        ModalDialogManager modalDialogManager =
                mActivityTestRule.getActivity().getModalDialogManager();
        return modalDialogManager.getCurrentDialogForTest();
    }

    private boolean verifyPageReloadedWithOriginalContent(String url) throws TimeoutException {
        Tab tab = mActivityTestRule.getActivity().getActivityTab();
        ChromeTabUtils.waitForTabPageLoaded(tab, url);

        CriteriaHelper.pollUiThread(() -> !InfoBarContainer.get(tab).hasInfoBars());

        // Reloading should allowlist the site, so resources should no longer be filtered.
        return Boolean.parseBoolean(mActivityTestRule.runJavaScriptCodeInCurrentTab("imgLoaded"));
    }
}
