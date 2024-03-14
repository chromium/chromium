// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.reengagement;

import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoMoreInteractions;

import android.app.Activity;
import android.app.Notification;
import android.app.NotificationManager;
import android.content.Context;
import android.os.Handler;
import android.text.TextUtils;

import androidx.test.core.app.ApplicationProvider;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Shadows;
import org.robolectric.annotation.Config;
import org.robolectric.annotation.LooperMode;
import org.robolectric.shadows.ShadowNotificationManager;

import org.chromium.base.Callback;
import org.chromium.base.FeatureList;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.DefaultBrowserInfo2;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.notifications.NotificationUmaTracker;
import org.chromium.components.feature_engagement.FeatureConstants;
import org.chromium.components.feature_engagement.Tracker;

import java.util.Map;

/** Unit tests for {@link ReengagementNotificationController}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(shadows = {ShadowNotificationManager.class})
@LooperMode(LooperMode.Mode.LEGACY)
public class ReengagementNotificationControllerTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock public Tracker mTracker;

    private Context mContext;
    private ShadowNotificationManager mShadowNotificationManager;

    private class TestingReengagementNotificationController
            extends ReengagementNotificationController {
        private DefaultBrowserInfo2.DefaultInfo mInfo;

        TestingReengagementNotificationController(DefaultBrowserInfo2.DefaultInfo info) {
            super(mContext, mTracker, Activity.class);
            mInfo = info;
        }

        @Override
        protected void getDefaultBrowserInfo(Callback<DefaultBrowserInfo2.DefaultInfo> callback) {
            new Handler().post(() -> callback.onResult(mInfo));
        }
    }

    @Before
    public void setUp() throws Exception {
        FeatureList.setTestFeatures(
                Map.of(
                        ChromeFeatureList.REENGAGEMENT_NOTIFICATION,
                        true));
        mContext = ApplicationProvider.getApplicationContext();
        mShadowNotificationManager =
                Shadows.shadowOf(
                        (NotificationManager)
                                mContext.getSystemService(Context.NOTIFICATION_SERVICE));
    }

    @Test
    public void testReengagementFirstFeature() {
        ReengagementNotificationController controller =
                new TestingReengagementNotificationController(
                        createDefaultInfo(/* passesPrecondition= */ true));
        doReturn(true)
                .when(mTracker)
                .shouldTriggerHelpUI(FeatureConstants.CHROME_REENGAGEMENT_NOTIFICATION_1_FEATURE);
        doReturn(false)
                .when(mTracker)
                .shouldTriggerHelpUI(FeatureConstants.CHROME_REENGAGEMENT_NOTIFICATION_2_FEATURE);
        doReturn(false)
                .when(mTracker)
                .shouldTriggerHelpUI(FeatureConstants.CHROME_REENGAGEMENT_NOTIFICATION_3_FEATURE);
        controller.tryToReengageTheUser();

        testFeatureShowed(FeatureConstants.CHROME_REENGAGEMENT_NOTIFICATION_1_FEATURE);
    }

    @Test
    public void testReengagementSecondFeature() {
        ReengagementNotificationController controller =
                new TestingReengagementNotificationController(
                        createDefaultInfo(/* passesPrecondition= */ true));
        doReturn(false)
                .when(mTracker)
                .shouldTriggerHelpUI(FeatureConstants.CHROME_REENGAGEMENT_NOTIFICATION_1_FEATURE);
        doReturn(true)
                .when(mTracker)
                .shouldTriggerHelpUI(FeatureConstants.CHROME_REENGAGEMENT_NOTIFICATION_2_FEATURE);
        doReturn(false)
                .when(mTracker)
                .shouldTriggerHelpUI(FeatureConstants.CHROME_REENGAGEMENT_NOTIFICATION_3_FEATURE);
        controller.tryToReengageTheUser();

        testFeatureShowed(FeatureConstants.CHROME_REENGAGEMENT_NOTIFICATION_2_FEATURE);
    }

    @Test
    public void testReengagementThirdFeature() {
        ReengagementNotificationController controller =
                new TestingReengagementNotificationController(
                        createDefaultInfo(/* passesPrecondition= */ true));
        doReturn(false)
                .when(mTracker)
                .shouldTriggerHelpUI(FeatureConstants.CHROME_REENGAGEMENT_NOTIFICATION_1_FEATURE);
        doReturn(false)
                .when(mTracker)
                .shouldTriggerHelpUI(FeatureConstants.CHROME_REENGAGEMENT_NOTIFICATION_2_FEATURE);
        doReturn(true)
                .when(mTracker)
                .shouldTriggerHelpUI(FeatureConstants.CHROME_REENGAGEMENT_NOTIFICATION_3_FEATURE);
        controller.tryToReengageTheUser();

        testFeatureShowed(FeatureConstants.CHROME_REENGAGEMENT_NOTIFICATION_3_FEATURE);
    }

    @Test
    public void testReengagementTwoFeaturesMet() {
        ReengagementNotificationController controller =
                new TestingReengagementNotificationController(
                        createDefaultInfo(/* passesPrecondition= */ true));
        doReturn(false)
                .when(mTracker)
                .shouldTriggerHelpUI(FeatureConstants.CHROME_REENGAGEMENT_NOTIFICATION_1_FEATURE);
        doReturn(true)
                .when(mTracker)
                .shouldTriggerHelpUI(FeatureConstants.CHROME_REENGAGEMENT_NOTIFICATION_2_FEATURE);
        doReturn(true)
                .when(mTracker)
                .shouldTriggerHelpUI(FeatureConstants.CHROME_REENGAGEMENT_NOTIFICATION_3_FEATURE);
        controller.tryToReengageTheUser();

        testFeatureShowed(FeatureConstants.CHROME_REENGAGEMENT_NOTIFICATION_2_FEATURE);
    }

    @Test
    public void testReengagementAllFeaturesMet() {
        ReengagementNotificationController controller =
                new TestingReengagementNotificationController(
                        createDefaultInfo(/* passesPrecondition= */ true));
        doReturn(true)
                .when(mTracker)
                .shouldTriggerHelpUI(FeatureConstants.CHROME_REENGAGEMENT_NOTIFICATION_1_FEATURE);
        doReturn(true)
                .when(mTracker)
                .shouldTriggerHelpUI(FeatureConstants.CHROME_REENGAGEMENT_NOTIFICATION_2_FEATURE);
        doReturn(true)
                .when(mTracker)
                .shouldTriggerHelpUI(FeatureConstants.CHROME_REENGAGEMENT_NOTIFICATION_3_FEATURE);
        controller.tryToReengageTheUser();

        testFeatureShowed(FeatureConstants.CHROME_REENGAGEMENT_NOTIFICATION_1_FEATURE);
    }

    @Test
    public void testReengagementNoFeatures() {
        ReengagementNotificationController controller =
                new TestingReengagementNotificationController(
                        createDefaultInfo(/* passesPrecondition= */ true));
        doReturn(false)
                .when(mTracker)
                .shouldTriggerHelpUI(FeatureConstants.CHROME_REENGAGEMENT_NOTIFICATION_1_FEATURE);
        doReturn(false)
                .when(mTracker)
                .shouldTriggerHelpUI(FeatureConstants.CHROME_REENGAGEMENT_NOTIFICATION_2_FEATURE);
        doReturn(false)
                .when(mTracker)
                .shouldTriggerHelpUI(FeatureConstants.CHROME_REENGAGEMENT_NOTIFICATION_3_FEATURE);
        controller.tryToReengageTheUser();
        new Handler()
                .post(
                        () ->
                                Assert.assertEquals(
                                        0,
                                        mShadowNotificationManager.getAllNotifications().size()));
    }

    @Test
    public void testReengagementNoPreconditions() {
        ReengagementNotificationController controller =
                new TestingReengagementNotificationController(
                        createDefaultInfo(/* passesPrecondition= */ false));
        controller.tryToReengageTheUser();
        verifyNoMoreInteractions(mTracker);
        Assert.assertEquals(0, mShadowNotificationManager.getAllNotifications().size());
    }

    @Test
    public void testReengagementUnavailablePreconditions() {
        ReengagementNotificationController controller =
                new TestingReengagementNotificationController(null);
        controller.tryToReengageTheUser();
        verifyNoMoreInteractions(mTracker);
        Assert.assertEquals(0, mShadowNotificationManager.getAllNotifications().size());
    }

    private void testFeatureShowed(String feature) {
        Assert.assertEquals(
                1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "Mobile.SystemNotification.Shown", getNotificationType(feature)));

        verify(mTracker, times(1)).shouldTriggerHelpUI(feature);
        verify(mTracker, times(1)).dismissed(feature);
        Notification notification = mShadowNotificationManager.getAllNotifications().get(0);

        new Handler()
                .post(
                        () -> {
                            Assert.assertEquals(
                                    getNotificationTitle(feature),
                                    notification
                                            .extras
                                            .getCharSequence(Notification.EXTRA_TITLE)
                                            .toString());
                            Assert.assertEquals(
                                    getNotificationDescription(feature),
                                    notification
                                            .extras
                                            .getCharSequence(Notification.EXTRA_TEXT)
                                            .toString());
                        });
    }

    private @NotificationUmaTracker.SystemNotificationType int getNotificationType(String feature) {
        if (TextUtils.equals(
                feature, FeatureConstants.CHROME_REENGAGEMENT_NOTIFICATION_1_FEATURE)) {
            return NotificationUmaTracker.SystemNotificationType.CHROME_REENGAGEMENT_1;
        } else if (TextUtils.equals(
                feature, FeatureConstants.CHROME_REENGAGEMENT_NOTIFICATION_2_FEATURE)) {
            return NotificationUmaTracker.SystemNotificationType.CHROME_REENGAGEMENT_2;
        } else if (TextUtils.equals(
                feature, FeatureConstants.CHROME_REENGAGEMENT_NOTIFICATION_3_FEATURE)) {
            return NotificationUmaTracker.SystemNotificationType.CHROME_REENGAGEMENT_3;
        }

        assert false : "Invalid feature, cannot find notification type.";
        return NotificationUmaTracker.SystemNotificationType.UNKNOWN;
    }

    private String getNotificationTitle(String feature) {
        if (TextUtils.equals(
                feature, FeatureConstants.CHROME_REENGAGEMENT_NOTIFICATION_1_FEATURE)) {
            return mContext.getString(R.string.chrome_reengagement_notification_1_title);
        } else if (TextUtils.equals(
                feature, FeatureConstants.CHROME_REENGAGEMENT_NOTIFICATION_2_FEATURE)) {
            return mContext.getString(R.string.chrome_reengagement_notification_2_title);
        } else if (TextUtils.equals(
                feature, FeatureConstants.CHROME_REENGAGEMENT_NOTIFICATION_3_FEATURE)) {
            return mContext.getString(R.string.chrome_reengagement_notification_3_title);
        }

        assert false : "Invalid feature, cannot find notification title.";
        return null;
    }

    private String getNotificationDescription(String feature) {
        if (TextUtils.equals(
                feature, FeatureConstants.CHROME_REENGAGEMENT_NOTIFICATION_1_FEATURE)) {
            return mContext.getString(R.string.chrome_reengagement_notification_1_description);
        } else if (TextUtils.equals(
                feature, FeatureConstants.CHROME_REENGAGEMENT_NOTIFICATION_2_FEATURE)) {
            return mContext.getString(R.string.chrome_reengagement_notification_2_description);
        } else if (TextUtils.equals(
                feature, FeatureConstants.CHROME_REENGAGEMENT_NOTIFICATION_3_FEATURE)) {
            return mContext.getString(R.string.chrome_reengagement_notification_3_description);
        }

        assert false : "Invalid feature, cannot find notification description.";
        return null;
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
}
