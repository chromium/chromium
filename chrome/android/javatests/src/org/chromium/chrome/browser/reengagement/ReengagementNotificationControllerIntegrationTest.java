// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.reengagement;

import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.reset;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import android.app.Notification;
import android.app.NotificationManager;
import android.content.Context;
import android.content.Intent;
import android.service.notification.StatusBarNotification;
import android.text.TextUtils;

import androidx.annotation.StringRes;
import androidx.test.core.app.ApplicationProvider;
import androidx.test.filters.MediumTest;
import androidx.test.filters.SmallTest;
import androidx.test.platform.app.InstrumentationRegistry;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.FeatureList;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.UrlUtils;
import org.chromium.chrome.browser.DefaultBrowserInfo2;
import org.chromium.chrome.browser.app.reengagement.ReengagementActivity;
import org.chromium.chrome.browser.customtabs.CustomTabActivityTestRule;
import org.chromium.chrome.browser.customtabs.CustomTabsIntentTestUtils;
import org.chromium.chrome.browser.feature_engagement.TrackerFactory;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabCreationState;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorObserver;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.R;
import org.chromium.chrome.test.util.ChromeTabUtils;
import org.chromium.components.embedder_support.util.UrlUtilities;
import org.chromium.components.feature_engagement.EventConstants;
import org.chromium.components.feature_engagement.FeatureConstants;
import org.chromium.components.feature_engagement.Tracker;
import org.chromium.content_public.common.ContentUrlConstants;

import java.util.HashMap;
import java.util.Map;

/** Integration tests for {@link ReengagementNotificationController}. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class ReengagementNotificationControllerIntegrationTest {
    @Rule
    public ChromeTabbedActivityTestRule mTabbedActivityTestRule =
            new ChromeTabbedActivityTestRule();

    @Rule
    public CustomTabActivityTestRule mCustomTabActivityTestRule = new CustomTabActivityTestRule();

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock public Tracker mTracker;

    @Before
    public void setUp() throws Exception {
        reset(mTracker);
        setReengagementNotificationEnabled(true);
        TrackerFactory.setTrackerForTests(mTracker);
        closeReengagementNotifications();
    }

    @After
    public void tearDown() {
        DefaultBrowserInfo2.clearDefaultInfoForTests();
        closeReengagementNotifications();
    }

    @Test
    @MediumTest
    @DisabledTest(message = "https://crbug.com/1464558")
    public void testReengagementNotificationSent() {
        DefaultBrowserInfo2.setDefaultInfoForTests(
                createDefaultInfo(/* passesPrecondition= */ true));
        doReturn(true)
                .when(mTracker)
                .shouldTriggerHelpUI(FeatureConstants.CHROME_REENGAGEMENT_NOTIFICATION_1_FEATURE);
        mCustomTabActivityTestRule.startCustomTabActivityWithIntent(
                CustomTabsIntentTestUtils.createMinimalCustomTabIntent(
                        ApplicationProvider.getApplicationContext(),
                        ContentUrlConstants.ABOUT_BLANK_DISPLAY_URL));
        verify(mTracker, times(1))
                .shouldTriggerHelpUI(FeatureConstants.CHROME_REENGAGEMENT_NOTIFICATION_1_FEATURE);
        verify(mTracker, times(1))
                .dismissed(FeatureConstants.CHROME_REENGAGEMENT_NOTIFICATION_1_FEATURE);

        verifyNotification(
                R.string.chrome_reengagement_notification_1_title,
                R.string.chrome_reengagement_notification_1_description);
    }

    @Test
    @MediumTest
    @DisabledTest(message = "Flaky on multiple bots, see crbug.com/1459539")
    public void testReengagementDifferentNotificationSent() {
        DefaultBrowserInfo2.setDefaultInfoForTests(
                createDefaultInfo(/* passesPrecondition= */ true));
        doReturn(true)
                .when(mTracker)
                .shouldTriggerHelpUI(FeatureConstants.CHROME_REENGAGEMENT_NOTIFICATION_2_FEATURE);
        mCustomTabActivityTestRule.startCustomTabActivityWithIntent(
                CustomTabsIntentTestUtils.createMinimalCustomTabIntent(
                        ApplicationProvider.getApplicationContext(),
                        ContentUrlConstants.ABOUT_BLANK_DISPLAY_URL));
        verify(mTracker, times(1))
                .shouldTriggerHelpUI(FeatureConstants.CHROME_REENGAGEMENT_NOTIFICATION_2_FEATURE);
        verify(mTracker, times(1))
                .dismissed(FeatureConstants.CHROME_REENGAGEMENT_NOTIFICATION_2_FEATURE);

        verifyNotification(
                R.string.chrome_reengagement_notification_2_title,
                R.string.chrome_reengagement_notification_2_description);
    }

    @Test
    @MediumTest
    @DisabledTest(message = "https://crbug.com/1464323")
    public void testReengagementNotificationNotSentDueToIPH() {
        DefaultBrowserInfo2.setDefaultInfoForTests(
                createDefaultInfo(/* passesPrecondition= */ true));
        mCustomTabActivityTestRule.startCustomTabActivityWithIntent(
                CustomTabsIntentTestUtils.createMinimalCustomTabIntent(
                        ApplicationProvider.getApplicationContext(),
                        ContentUrlConstants.ABOUT_BLANK_DISPLAY_URL));
        verifyHasNoNotifications();
        verify(mTracker, times(1))
                .shouldTriggerHelpUI(FeatureConstants.CHROME_REENGAGEMENT_NOTIFICATION_1_FEATURE);
        verify(mTracker, times(1))
                .shouldTriggerHelpUI(FeatureConstants.CHROME_REENGAGEMENT_NOTIFICATION_2_FEATURE);
        verify(mTracker, times(1))
                .shouldTriggerHelpUI(FeatureConstants.CHROME_REENGAGEMENT_NOTIFICATION_3_FEATURE);
        verify(mTracker, never())
                .dismissed(FeatureConstants.CHROME_REENGAGEMENT_NOTIFICATION_1_FEATURE);
        verify(mTracker, never())
                .dismissed(FeatureConstants.CHROME_REENGAGEMENT_NOTIFICATION_2_FEATURE);
        verify(mTracker, never())
                .dismissed(FeatureConstants.CHROME_REENGAGEMENT_NOTIFICATION_3_FEATURE);
    }

    @Test
    @MediumTest
    public void testReengagementNotificationNotSentDueToPreconditions() {
        DefaultBrowserInfo2.setDefaultInfoForTests(
                createDefaultInfo(/* passesPrecondition= */ false));
        mCustomTabActivityTestRule.startCustomTabActivityWithIntent(
                CustomTabsIntentTestUtils.createMinimalCustomTabIntent(
                        ApplicationProvider.getApplicationContext(),
                        ContentUrlConstants.ABOUT_BLANK_DISPLAY_URL));
        verifyHasNoNotifications();
        verify(mTracker, never())
                .shouldTriggerHelpUI(FeatureConstants.CHROME_REENGAGEMENT_NOTIFICATION_1_FEATURE);
        verify(mTracker, never())
                .shouldTriggerHelpUI(FeatureConstants.CHROME_REENGAGEMENT_NOTIFICATION_2_FEATURE);
        verify(mTracker, never())
                .shouldTriggerHelpUI(FeatureConstants.CHROME_REENGAGEMENT_NOTIFICATION_3_FEATURE);
        verify(mTracker, never())
                .dismissed(FeatureConstants.CHROME_REENGAGEMENT_NOTIFICATION_1_FEATURE);
        verify(mTracker, never())
                .dismissed(FeatureConstants.CHROME_REENGAGEMENT_NOTIFICATION_2_FEATURE);
        verify(mTracker, never())
                .dismissed(FeatureConstants.CHROME_REENGAGEMENT_NOTIFICATION_3_FEATURE);
    }

    @Test
    @MediumTest
    public void testReengagementNotificationNotSentDueToUnavailablePreconditions() {
        DefaultBrowserInfo2.setDefaultInfoForTests(null);
        mCustomTabActivityTestRule.startCustomTabActivityWithIntent(
                CustomTabsIntentTestUtils.createMinimalCustomTabIntent(
                        ApplicationProvider.getApplicationContext(),
                        ContentUrlConstants.ABOUT_BLANK_DISPLAY_URL));
        verifyHasNoNotifications();
        verify(mTracker, never())
                .shouldTriggerHelpUI(FeatureConstants.CHROME_REENGAGEMENT_NOTIFICATION_1_FEATURE);
        verify(mTracker, never())
                .shouldTriggerHelpUI(FeatureConstants.CHROME_REENGAGEMENT_NOTIFICATION_2_FEATURE);
        verify(mTracker, never())
                .shouldTriggerHelpUI(FeatureConstants.CHROME_REENGAGEMENT_NOTIFICATION_3_FEATURE);
        verify(mTracker, never())
                .dismissed(FeatureConstants.CHROME_REENGAGEMENT_NOTIFICATION_1_FEATURE);
        verify(mTracker, never())
                .dismissed(FeatureConstants.CHROME_REENGAGEMENT_NOTIFICATION_2_FEATURE);
        verify(mTracker, never())
                .dismissed(FeatureConstants.CHROME_REENGAGEMENT_NOTIFICATION_3_FEATURE);
    }

    @Test
    @SmallTest
    public void testEngagementTracked() {
        mTabbedActivityTestRule.startMainActivityFromLauncher();
        verify(mTracker, times(1)).notifyEvent(EventConstants.STARTED_FROM_MAIN_INTENT);
    }

    @Test
    @SmallTest
    public void testEngagementNotTracked() {
        mCustomTabActivityTestRule.startCustomTabActivityWithIntent(
                CustomTabsIntentTestUtils.createMinimalCustomTabIntent(
                        ApplicationProvider.getApplicationContext(),
                        ContentUrlConstants.ABOUT_BLANK_DISPLAY_URL));
        verify(mTracker, never()).notifyEvent(EventConstants.STARTED_FROM_MAIN_INTENT);
    }

    @Test
    @SmallTest
    @DisabledTest(message = "crbug.com/1112519 - Disabled while safety guard is in place.")
    public void testEngagementTrackedWhenDisabled() {
        setReengagementNotificationEnabled(false);
        mTabbedActivityTestRule.startMainActivityFromLauncher();
        verify(mTracker, times(1)).notifyEvent(EventConstants.STARTED_FROM_MAIN_INTENT);
    }

    @Test
    @SmallTest
    public void testEngagementNotTrackedDueToIntentOpeningTab() {
        mTabbedActivityTestRule.startMainActivityWithURL(
                UrlUtils.encodeHtmlDataUri("<html><head></head><body>foo</body></html>"));
        verify(mTracker, never()).notifyEvent(EventConstants.STARTED_FROM_MAIN_INTENT);
    }

    @Test
    @MediumTest
    public void testEngagementNotificationNotSentDueToDisabled() {
        setReengagementNotificationEnabled(false);
        DefaultBrowserInfo2.setDefaultInfoForTests(
                createDefaultInfo(/* passesPrecondition= */ true));
        mCustomTabActivityTestRule.startCustomTabActivityWithIntent(
                CustomTabsIntentTestUtils.createMinimalCustomTabIntent(
                        ApplicationProvider.getApplicationContext(),
                        ContentUrlConstants.ABOUT_BLANK_DISPLAY_URL));
        verifyHasNoNotifications();
        verify(mTracker, never())
                .shouldTriggerHelpUI(FeatureConstants.CHROME_REENGAGEMENT_NOTIFICATION_1_FEATURE);
        verify(mTracker, never())
                .shouldTriggerHelpUI(FeatureConstants.CHROME_REENGAGEMENT_NOTIFICATION_2_FEATURE);
        verify(mTracker, never())
                .shouldTriggerHelpUI(FeatureConstants.CHROME_REENGAGEMENT_NOTIFICATION_3_FEATURE);
        verify(mTracker, never())
                .dismissed(FeatureConstants.CHROME_REENGAGEMENT_NOTIFICATION_1_FEATURE);
        verify(mTracker, never())
                .dismissed(FeatureConstants.CHROME_REENGAGEMENT_NOTIFICATION_2_FEATURE);
        verify(mTracker, never())
                .dismissed(FeatureConstants.CHROME_REENGAGEMENT_NOTIFICATION_3_FEATURE);
    }

    @Test
    @MediumTest
    public void testReengagementActivity() throws Exception {
        mTabbedActivityTestRule.startMainActivityOnBlankPage();
        int initialTabCount =
                mTabbedActivityTestRule.getActivity().getTabModelSelector().getTotalTabCount();

        final CallbackHelper tabAddedCallback = new CallbackHelper();
        TabModelSelectorObserver selectorObserver =
                new TabModelSelectorObserver() {
                    @Override
                    public void onNewTabCreated(Tab tab, @TabCreationState int creationState) {
                        tabAddedCallback.notifyCalled();
                    }
                };
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mTabbedActivityTestRule
                            .getActivity()
                            .getTabModelSelector()
                            .addObserver(selectorObserver);
                });

        Intent intent =
                new Intent(ApplicationProvider.getApplicationContext(), ReengagementActivity.class);
        intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        intent.setAction(ReengagementNotificationController.LAUNCH_NTP_ACTION);
        InstrumentationRegistry.getInstrumentation().startActivitySync(intent);

        tabAddedCallback.waitForCallback(0);
        Tab tab =
                ThreadUtils.runOnUiThreadBlocking(
                        () -> mTabbedActivityTestRule.getActivity().getActivityTab());
        Assert.assertTrue(UrlUtilities.isNtpUrl(ChromeTabUtils.getUrlOnUiThread(tab)));
        Assert.assertFalse(tab.isIncognito());
        Assert.assertEquals(
                initialTabCount + 1,
                mTabbedActivityTestRule.getActivity().getTabModelSelector().getTotalTabCount());
    }

    private void verifyNotification(@StringRes int title, @StringRes int description) {
        CriteriaHelper.pollUiThread(
                () -> {
                    return findNotification(title, description);
                });
    }

    private void verifyHasNoNotifications() {
        Assert.assertFalse(hasNotifications());
    }

    private static boolean findNotification(@StringRes int title, @StringRes int description) {
        Context context = ApplicationProvider.getApplicationContext();
        StatusBarNotification[] notifications =
                ((NotificationManager) context.getSystemService(Context.NOTIFICATION_SERVICE))
                        .getActiveNotifications();

        String titleStr = context.getString(title);
        String descriptionStr = context.getString(description);

        for (StatusBarNotification notification : notifications) {
            CharSequence notifTitle =
                    notification.getNotification().extras.getCharSequence(Notification.EXTRA_TITLE);
            CharSequence notifDescription =
                    notification.getNotification().extras.getCharSequence(Notification.EXTRA_TEXT);
            if (TextUtils.equals(titleStr, notifTitle)
                    && TextUtils.equals(descriptionStr, notifDescription)) {
                return true;
            }
        }

        return false;
    }

    private static boolean hasNotifications() {
        Context context = ApplicationProvider.getApplicationContext();
        StatusBarNotification[] notifications =
                ((NotificationManager) context.getSystemService(Context.NOTIFICATION_SERVICE))
                        .getActiveNotifications();

        for (StatusBarNotification notification : notifications) {
            String tag = notification.getTag();
            if (TextUtils.equals(ReengagementNotificationController.NOTIFICATION_TAG, tag)) {
                return true;
            }
        }

        return false;
    }

    private static void closeReengagementNotifications() {
        if (!hasNotifications()) return;

        Context context = ApplicationProvider.getApplicationContext();
        ((NotificationManager) context.getSystemService(Context.NOTIFICATION_SERVICE))
                .cancel(
                        ReengagementNotificationController.NOTIFICATION_TAG,
                        ReengagementNotificationController.NOTIFICATION_ID);
    }

    private DefaultBrowserInfo2.DefaultInfo createDefaultInfo(boolean passesPrecondition) {
        int browserCount = passesPrecondition ? 2 : 1;
        return new DefaultBrowserInfo2.DefaultInfo(
                /* isChromeSystem= */ true,
                /* isChromeDefault= */ true,
                /* isDefaultSystem= */ true,
                /* hasDefault= */ true,
                browserCount,
                /* systemCount= */ 0);
    }

    private static void setReengagementNotificationEnabled(boolean enabled) {
        Map<String, Boolean> features = new HashMap<>();
        features.put(ChromeFeatureList.REENGAGEMENT_NOTIFICATION, enabled);
        // TODO(crbug.com/40142646): Remove these overrides when FeatureList#isInitialized() works
        // as expected with test values.
        features.put(ChromeFeatureList.VOICE_SEARCH_AUDIO_CAPTURE_POLICY, false);
        FeatureList.setTestFeatures(features);
    }
}
