// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.notifications;

import static org.hamcrest.Matchers.containsInAnyOrder;

import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.Color;
import android.net.Uri;
import android.os.Looper;

import androidx.test.filters.SmallTest;

import org.hamcrest.MatcherAssert;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.ArgumentMatchers;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.MockitoAnnotations;
import org.mockito.hamcrest.MockitoHamcrest;
import org.robolectric.Shadows;
import org.robolectric.annotation.Config;

import org.chromium.base.ContextUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.browser_ui.notifications.MockNotificationManagerProxy;
import org.chromium.components.browser_ui.notifications.NotificationMetadata;
import org.chromium.components.browser_ui.notifications.NotificationWrapper;

import java.util.Arrays;
import java.util.Collections;
import java.util.List;
import java.util.stream.Collectors;

@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class NotificationSuspenderUnitTest {
    private static final String TEST_DOMAIN = "example.com";
    private static final String TEST_SUBDOMAIN = "subdomain.example.com";
    private static final String TEST_OTHER_DOMAIN = "not-example.com";
    private static final String TEST_DOMAIN_WITHOUT_NOTIFICATIONS = "no-active-notifications.com";

    private static final String TEST_ORIGIN = "https://example.com";
    private static final String TEST_ORIGIN_SUBDOMAIN = "https://subdomain.example.com";
    private static final String TEST_ORIGIN_OTHER_PORT = "https://example.com:444";
    private static final String TEST_OTHER_ORIGIN = "https://not-example.com";
    private static final String TEST_ORIGIN_HTTP = "http://example.com";
    private static final String TEST_OTHER_ORIGIN_HTTP = "http://not-example.com";

    private static final String TEST_NOTIFICATION_ID = "p#https://example.com#10";
    private static final String TEST_NOTIFICATION_ID_SAME_ORIGIN = "p#https://example.com#21";
    private static final String TEST_NOTIFICATION_ID_HTTP = "p#http://example.com#10";
    private static final String TEST_NOTIFICATION_ID_SUBDOMAIN =
            "p#https://subdomain.example.com#10";
    private static final String TEST_NOTIFICATION_ID_OTHER_DOMAIN = "p#https://not-example.com#10";
    private static final String TEST_NOTIFICATION_ID_INVALID = "p##10";

    @Rule public JniMocker mJniMocker = new JniMocker();

    @Mock private NotificationSuspender.Natives mNotificationSuspenderJniMock;
    @Mock private Profile mProfile;

    private MockNotificationManagerProxy mFakeNotificationManager;
    private NotificationSuspender mNotificationSuspender;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        mJniMocker.mock(NotificationSuspenderJni.TEST_HOOKS, mNotificationSuspenderJniMock);

        mFakeNotificationManager = new MockNotificationManagerProxy();
        mNotificationSuspender =
                new NotificationSuspender(mProfile, getContext(), mFakeNotificationManager);
    }

    private void populateTestNotifications() {
        for (String notificationId :
                new String[] {
                    TEST_NOTIFICATION_ID,
                    TEST_NOTIFICATION_ID_SAME_ORIGIN,
                    TEST_NOTIFICATION_ID_HTTP,
                    TEST_NOTIFICATION_ID_SUBDOMAIN,
                    TEST_NOTIFICATION_ID_OTHER_DOMAIN
                }) {
            NotificationBuilderBase builder = new StandardNotificationBuilder(getContext());
            mFakeNotificationManager.notify(
                    builder.build(
                            new NotificationMetadata(
                                    NotificationUmaTracker.SystemNotificationType.SITES,
                                    notificationId,
                                    NotificationPlatformBridge.PLATFORM_ID)));
        }
    }

    private String[] getActiveNotificationIds() {
        return mFakeNotificationManager.getNotifications().stream()
                .map((sbn) -> sbn.getTag())
                .collect(Collectors.toList())
                .toArray(new String[0]);
    }

    private static Context getContext() {
        return ContextUtils.getApplicationContext();
    }

    private static Bitmap createTestBitmap(int color) {
        int[] colors = {color};
        return Bitmap.createBitmap(colors, 1, 1, Bitmap.Config.ALPHA_8);
    }

    /**
     * Verifies that testSuspendNotificationsFromDomain correctly identifies the notifications
     * originating from HTTP/HTTPS versions of the specified domain.
     */
    @SmallTest
    @Test
    public void testSuspendNotificationsFromDomains_DomainsAreCorrectlyFiltered() {
        populateTestNotifications();

        mNotificationSuspender.suspendNotificationsFromDomains(
                Collections.singletonList(TEST_DOMAIN));

        // Expect the matching notifications to be stored...
        Mockito.verify(mNotificationSuspenderJniMock)
                .storeNotificationResources(
                        ArgumentMatchers.eq(mProfile),
                        /* notificationIds= */ ArgumentMatchers.eq(
                                new String[] {
                                    TEST_NOTIFICATION_ID,
                                    TEST_NOTIFICATION_ID_SAME_ORIGIN,
                                    TEST_NOTIFICATION_ID_HTTP
                                }),
                        /* origins= */ ArgumentMatchers.eq(
                                new String[] {TEST_ORIGIN, TEST_ORIGIN, TEST_ORIGIN_HTTP}),
                        /* resources= */ ArgumentMatchers.eq(
                                Collections.nCopies(9, null).toArray(new Bitmap[0])));

        // ... and cancelled, so that the rest is the following:
        Assert.assertEquals(
                new String[] {TEST_NOTIFICATION_ID_SUBDOMAIN, TEST_NOTIFICATION_ID_OTHER_DOMAIN},
                getActiveNotificationIds());
    }

    /**
     * Verifies that testSuspendNotificationsFromDomain correctly identifies the notifications
     * originating from multiple domains specified.
     */
    @SmallTest
    @Test
    public void testSuspendNotificationsFromDomains_MultipleDomains() {
        populateTestNotifications();

        mNotificationSuspender.suspendNotificationsFromDomains(
                Arrays.asList(
                        TEST_DOMAIN_WITHOUT_NOTIFICATIONS, TEST_SUBDOMAIN, TEST_OTHER_DOMAIN));

        // Expect the matching notifications to be stored...
        Mockito.verify(mNotificationSuspenderJniMock)
                .storeNotificationResources(
                        ArgumentMatchers.eq(mProfile),
                        /* notificationIds= */ ArgumentMatchers.eq(
                                new String[] {
                                    TEST_NOTIFICATION_ID_SUBDOMAIN,
                                    TEST_NOTIFICATION_ID_OTHER_DOMAIN
                                }),
                        /* origins= */ ArgumentMatchers.eq(
                                new String[] {TEST_ORIGIN_SUBDOMAIN, TEST_OTHER_ORIGIN}),
                        /* resources= */ ArgumentMatchers.eq(
                                Collections.nCopies(6, null).toArray(new Bitmap[0])));

        // ... and cancelled, so that the rest is the following:
        Assert.assertEquals(
                new String[] {
                    TEST_NOTIFICATION_ID,
                    TEST_NOTIFICATION_ID_SAME_ORIGIN,
                    TEST_NOTIFICATION_ID_HTTP
                },
                getActiveNotificationIds());
    }

    /**
     * Verifies that storeNotificationResourcesFromOrigins correctly identifies the notifications
     * originating from the given schemeful origins.
     */
    @SmallTest
    @Test
    public void testStoreNotificationResourcesFromOrigins_MultipleOrigins() {
        populateTestNotifications();

        mNotificationSuspender.storeNotificationResourcesFromOrigins(
                Arrays.asList(
                        Uri.parse(TEST_ORIGIN_SUBDOMAIN),
                        Uri.parse(TEST_ORIGIN_HTTP),
                        Uri.parse(TEST_ORIGIN_OTHER_PORT),
                        Uri.parse(TEST_OTHER_ORIGIN)),
                (notifications) -> {
                    MatcherAssert.assertThat(
                            notifications,
                            containsInAnyOrder(
                                    TEST_NOTIFICATION_ID_SUBDOMAIN,
                                    TEST_NOTIFICATION_ID_HTTP,
                                    TEST_NOTIFICATION_ID_OTHER_DOMAIN));
                });
        Shadows.shadowOf(Looper.getMainLooper()).idle();

        // Expect the matching notifications to be stored...
        Mockito.verify(mNotificationSuspenderJniMock)
                .storeNotificationResources(
                        ArgumentMatchers.eq(mProfile),
                        /* notificationIds= */ ArgumentMatchers.eq(
                                new String[] {
                                    TEST_NOTIFICATION_ID_HTTP,
                                    TEST_NOTIFICATION_ID_SUBDOMAIN,
                                    TEST_NOTIFICATION_ID_OTHER_DOMAIN
                                }),
                        /* origins= */ ArgumentMatchers.eq(
                                new String[] {
                                    TEST_ORIGIN_HTTP, TEST_ORIGIN_SUBDOMAIN, TEST_OTHER_ORIGIN
                                }),
                        /* resources= */ ArgumentMatchers.eq(
                                Collections.nCopies(9, null).toArray(new Bitmap[0])));

        // ... but not cancelled, so that the rest is still the original list.
        Assert.assertEquals(
                new String[] {
                    TEST_NOTIFICATION_ID,
                    TEST_NOTIFICATION_ID_SAME_ORIGIN,
                    TEST_NOTIFICATION_ID_HTTP,
                    TEST_NOTIFICATION_ID_SUBDOMAIN,
                    TEST_NOTIFICATION_ID_OTHER_DOMAIN
                },
                getActiveNotificationIds());
    }

    /**
     * Verifies that storeNotificationResources correctly captures the icon / badge / image; even
     * for a notification that is not presently active.
     */
    @SmallTest
    @Test
    public void testStoreNotificationResources() {
        NotificationBuilderBase builder = new StandardNotificationBuilder(getContext());
        builder.setLargeIcon(createTestBitmap(Color.RED));
        builder.setStatusBarIcon(createTestBitmap(Color.GREEN));
        builder.setImage(createTestBitmap(Color.BLUE));
        NotificationWrapper notification =
                builder.build(
                        new NotificationMetadata(
                                NotificationUmaTracker.SystemNotificationType.SITES,
                                TEST_NOTIFICATION_ID,
                                NotificationPlatformBridge.PLATFORM_ID));

        // Pass in the notification twice to verify array index arithmetic for the bitmaps.
        mNotificationSuspender.storeNotificationResources(
                Arrays.asList(notification, notification));

        // Expect the matching notifications to be stored.
        ArgumentCaptor<Bitmap[]> bitmaps = ArgumentCaptor.forClass(Bitmap[].class);
        Mockito.verify(mNotificationSuspenderJniMock)
                .storeNotificationResources(
                        ArgumentMatchers.eq(mProfile),
                        /* notificationIds= */ ArgumentMatchers.eq(
                                new String[] {TEST_NOTIFICATION_ID, TEST_NOTIFICATION_ID}),
                        /* origins= */ ArgumentMatchers.eq(new String[] {TEST_ORIGIN, TEST_ORIGIN}),
                        /* resources= */ bitmaps.capture());

        Integer[] bitmapColors =
                Arrays.stream(bitmaps.getValue())
                        .map((bitmap) -> bitmap.getPixel(0, 0))
                        .collect(Collectors.toList())
                        .toArray(new Integer[0]);
        Assert.assertEquals(
                new Integer[] {
                    Color.RED, Color.GREEN, Color.BLUE, Color.RED, Color.GREEN, Color.BLUE
                },
                bitmapColors);
    }

    /**
     * Verifies that unsuspendNotificationsFromDomain triggers restoring notifications from the
     * correct origins.
     */
    @SmallTest
    @Test
    public void testUnsuspendNotificationsFromDomains() {
        mNotificationSuspender.unsuspendNotificationsFromDomains(
                Arrays.asList(TEST_DOMAIN, TEST_OTHER_DOMAIN));

        Mockito.verify(mNotificationSuspenderJniMock)
                .reDisplayNotifications(
                        ArgumentMatchers.eq(mProfile),
                        /* origins= */ (List<String>)
                                MockitoHamcrest.argThat(
                                        containsInAnyOrder(
                                                TEST_ORIGIN,
                                                TEST_ORIGIN_HTTP,
                                                TEST_OTHER_ORIGIN,
                                                TEST_OTHER_ORIGIN_HTTP)));
    }

    /**
     * Verifies that testUnsuspendNotificationsFromOrigins triggers restoring notifications from the
     * correct origins.
     */
    @SmallTest
    @Test
    public void testUnsuspendNotificationsFromOrigins() {
        mNotificationSuspender.unsuspendNotificationsFromOrigins(
                Arrays.asList(
                        Uri.parse(TEST_ORIGIN_HTTP),
                        Uri.parse(TEST_ORIGIN_SUBDOMAIN),
                        Uri.parse(TEST_ORIGIN_OTHER_PORT),
                        Uri.parse(TEST_OTHER_ORIGIN)));

        Mockito.verify(mNotificationSuspenderJniMock)
                .reDisplayNotifications(
                        ArgumentMatchers.eq(mProfile),
                        /* origins= */ (List<String>)
                                MockitoHamcrest.argThat(
                                        containsInAnyOrder(
                                                TEST_ORIGIN_HTTP,
                                                TEST_ORIGIN_SUBDOMAIN,
                                                TEST_ORIGIN_OTHER_PORT,
                                                TEST_OTHER_ORIGIN)));
    }

    /**
     * Verifies that storeNotificationResourcesFromOrigins correctly ignores notifications with
     * invalid IDs and does not crash.
     */
    @SmallTest
    @Test
    public void testStoreNotificationResourcesFromOrigins_InvalidIdsAreIgnored() {
        for (String notificationId :
                new String[] {
                    TEST_NOTIFICATION_ID,
                    TEST_NOTIFICATION_ID_INVALID,
                    "", /* empty id */
                    null, /* null id */
                }) {
            NotificationBuilderBase builder = new StandardNotificationBuilder(getContext());
            mFakeNotificationManager.notify(
                    builder.build(
                            new NotificationMetadata(
                                    NotificationUmaTracker.SystemNotificationType.SITES,
                                    notificationId,
                                    NotificationPlatformBridge.PLATFORM_ID)));
        }

        mNotificationSuspender.storeNotificationResourcesFromOrigins(
                Collections.singletonList(Uri.parse(TEST_ORIGIN)),
                (notifications) -> {
                    MatcherAssert.assertThat(
                            notifications, containsInAnyOrder(TEST_NOTIFICATION_ID));
                });
        Shadows.shadowOf(Looper.getMainLooper()).idle();
    }
}
