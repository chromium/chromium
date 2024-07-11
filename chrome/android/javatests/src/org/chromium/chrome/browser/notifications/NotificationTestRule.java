// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.notifications;

import org.hamcrest.Matchers;
import org.junit.Assert;
import org.junit.runner.Description;
import org.junit.runners.model.Statement;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.chrome.browser.profiles.ProfileManager;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.components.browser_ui.modaldialog.ModalDialogView;
import org.chromium.components.browser_ui.notifications.MockNotificationManagerProxy;
import org.chromium.components.browser_ui.notifications.MockNotificationManagerProxy.NotificationEntry;
import org.chromium.components.browser_ui.site_settings.PermissionInfo;
import org.chromium.components.content_settings.ContentSettingValues;
import org.chromium.components.content_settings.ContentSettingsType;
import org.chromium.components.content_settings.SessionModel;
import org.chromium.components.embedder_support.util.UrlConstants;

import java.util.List;
import java.util.concurrent.TimeoutException;

/**
 * Base class for instrumentation tests using Web Notifications on Android.
 *
 * <p>Web Notifications are only supported on Android JellyBean and beyond.
 */
public class NotificationTestRule extends ChromeTabbedActivityTestRule {
    /** The maximum time to wait for a criteria to become valid. */
    private static final long MAX_TIME_TO_POLL_MS = 6000L;

    /** The polling interval to wait between checking for a satisfied criteria. */
    private static final long POLLING_INTERVAL_MS = 50;

    private MockNotificationManagerProxy mMockNotificationManager;

    private void setUp() {
        // The NotificationPlatformBridge must be overriden prior to the browser process starting.
        mMockNotificationManager = new MockNotificationManagerProxy();
        NotificationPlatformBridge.overrideNotificationManagerForTesting(mMockNotificationManager);
        startMainActivityWithURL(UrlConstants.NTP_URL);
        ModalDialogView.disableButtonTapProtectionForTesting();
    }

    private void tearDown() {
        NotificationPlatformBridge.overrideNotificationManagerForTesting(null);
    }

    /**
     * Sets the permission to use Web Notifications for the test HTTP server's origin to |setting|.
     */
    public void setNotificationContentSettingForOrigin(
            final @ContentSettingValues int setting, String origin) throws TimeoutException {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    // The notification content setting does not consider the embedder origin.
                    PermissionInfo notificationInfo =
                            new PermissionInfo(
                                    ContentSettingsType.NOTIFICATIONS,
                                    origin,
                                    "",
                                    /* isEmbargoed= */ false,
                                    SessionModel.DURABLE);
                    notificationInfo.setContentSetting(
                            ProfileManager.getLastUsedRegularProfile(), setting);
                });

        String permission = runJavaScriptCodeInCurrentTab("Notification.permission");
        if (setting == ContentSettingValues.ALLOW) {
            Assert.assertEquals("\"granted\"", permission);
        } else if (setting == ContentSettingValues.BLOCK) {
            Assert.assertEquals("\"denied\"", permission);
        } else {
            Assert.assertEquals("\"default\"", permission);
        }
    }

    /**
     * Waits until a notification has been displayed and then returns a NotificationEntry object to
     * the caller. Requires that only a single notification is displayed.
     *
     * @return The NotificationEntry object tracked by the MockNotificationManagerProxy.
     */
    public NotificationEntry waitForNotification() {
        waitForNotificationManagerMutation();
        List<NotificationEntry> notifications = getNotificationEntries();
        Assert.assertEquals(1, notifications.size());
        return notifications.get(0);
    }

    public List<NotificationEntry> getNotificationEntries() {
        return mMockNotificationManager.getNotifications();
    }

    /**
     * Waits for a mutation to occur in the mocked notification manager. This indicates that Chrome
     * called into Android to notify or cancel a notification.
     */
    public void waitForNotificationManagerMutation() {
        CriteriaHelper.pollUiThread(
                () -> {
                    Criteria.checkThat(
                            mMockNotificationManager.getMutationCountAndDecrement(),
                            Matchers.greaterThan(0));
                },
                MAX_TIME_TO_POLL_MS,
                POLLING_INTERVAL_MS);
    }

    /**
     * Waits until the specified number of notifications are active in the mocked
     * NotificationManager.
     */
    public void waitForNotificationCount(int count) {
        CriteriaHelper.pollUiThread(
                () -> {
                    Criteria.checkThat(getNotificationEntries().size(), Matchers.equalTo(count));
                },
                MAX_TIME_TO_POLL_MS,
                POLLING_INTERVAL_MS);
    }

    @Override
    public Statement apply(final Statement base, Description description) {
        return super.apply(
                new Statement() {
                    @Override
                    public void evaluate() throws Throwable {
                        setUp();
                        base.evaluate();
                        tearDown();
                    }
                },
                description);
    }
}
