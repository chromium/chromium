// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.notifications;

import android.annotation.SuppressLint;
import android.annotation.TargetApi;
import android.app.Notification;
import android.app.PendingIntent;
import android.content.Context;
import android.content.Intent;
import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.graphics.Color;
import android.graphics.drawable.BitmapDrawable;
import android.graphics.drawable.Drawable;
import android.os.Build;
import android.support.test.InstrumentationRegistry;
import android.support.test.filters.SmallTest;
import android.util.DisplayMetrics;
import android.view.View;
import android.widget.Button;
import android.widget.ImageView;
import android.widget.LinearLayout;
import android.widget.TextView;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.DisableIf;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.MinAndroidSdkLevel;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.notifications.channels.ChannelDefinitions;
import org.chromium.content_public.browser.test.NativeLibraryTestRule;

import java.util.ArrayList;
import java.util.Arrays;

/**
 * Instrumentation unit tests for CustomNotificationBuilder.
 */
@RunWith(BaseJUnit4ClassRunner.class)
public class CustomNotificationBuilderTest {
    private static final String NOTIFICATION_TAG = "TestNotificationTag";
    private static final int NOTIFICATION_ID = 99;

    @Rule
    public NativeLibraryTestRule mActivityTestRule = new NativeLibraryTestRule();

    @Before
    public void setUp() {
        mActivityTestRule.loadNativeLibraryNoBrowserProcess();
    }

    @Test
    @SmallTest
    @Feature({"Browser", "Notifications"})
    @SuppressLint("NewApi")
    @DisableIf.Build(sdk_is_greater_than = 23, message = "crbug.com/779228")
    public void testSetAll() {
        Context context = InstrumentationRegistry.getTargetContext();

        PendingIntentProvider contentIntent = createIntent(context, "Content");
        PendingIntentProvider deleteIntent = createIntent(context, "Delete");

        Bitmap smallIcon =
                BitmapFactory.decodeResource(context.getResources(), R.drawable.ic_chrome);

        Bitmap largeIcon = createIcon(Color.RED);
        Bitmap actionIcon = createIcon(Color.WHITE);

        NotificationBuilderBase builder =
                new CustomNotificationBuilder(context)
                        .setSmallIconId(R.drawable.ic_chrome)
                        .setLargeIcon(largeIcon)
                        .setTitle("title")
                        .setBody("body")
                        .setOrigin("origin")
                        .setChannelId(ChannelDefinitions.ChannelId.SITES)
                        .setTicker("ticker")
                        .setDefaults(Notification.DEFAULT_ALL)
                        .setVibrate(new long[] {100L})
                        .setContentIntent(contentIntent)
                        .setDeleteIntent(deleteIntent)
                        .addButtonAction(actionIcon, "button",
                                createIntent(context, "ActionButtonOne").getPendingIntent())
                        .addButtonAction(actionIcon, "button",
                                createIntent(context, "ActionButtonTwo").getPendingIntent())
                        .addSettingsAction(0 /* iconId */, "settings",
                                createIntent(context, "SettingsButton").getPendingIntent());
        Notification notification = buildNotification(builder);

        assertSmallNotificationIconAsExpected(context, notification, smallIcon);
        assertLargeNotificationIconAsExpected(context, notification, largeIcon);

        View compactView = notification.contentView.apply(context, new LinearLayout(context));
        View bigView = notification.bigContentView.apply(context, new LinearLayout(context));

        Assert.assertEquals("title", getIdenticalText(R.id.title, compactView, bigView));
        Assert.assertEquals("body", getIdenticalText(R.id.body, compactView, bigView));
        Assert.assertEquals("origin", getIdenticalText(R.id.origin, compactView, bigView));

        Assert.assertEquals("title", NotificationTestUtil.getExtraTitle(notification));
        Assert.assertEquals("body", NotificationTestUtil.getExtraText(notification));
        Assert.assertEquals("origin", NotificationTestUtil.getExtraSubText(notification));
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.KITKAT_WATCH) {
            Assert.assertEquals(
                    NotificationConstants.GROUP_WEB_PREFIX + "origin", notification.getGroup());
        }

        Assert.assertEquals("ticker", notification.tickerText.toString());
        Assert.assertEquals(Notification.DEFAULT_ALL, notification.defaults);
        Assert.assertEquals(1, notification.vibrate.length);
        Assert.assertEquals(100L, notification.vibrate[0]);

        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.LOLLIPOP) {
            // Notification.publicVersion was added in Android L.
            Assert.assertNotNull(notification.publicVersion);
            Assert.assertEquals("origin",
                    Build.VERSION.SDK_INT <= Build.VERSION_CODES.M
                            ? NotificationTestUtil.getExtraTitle(notification.publicVersion)
                            : NotificationTestUtil.getExtraSubText(notification.publicVersion));
        }

        // The regular actions and the settings action are added together in the notification
        // actions array, so they can be exposed on e.g. Wear and custom lockscreens.
        Assert.assertEquals(3, NotificationTestUtil.getActions(notification).length);

        ArrayList<View> buttons = new ArrayList<>();
        bigView.findViewsWithText(buttons, "button", View.FIND_VIEWS_WITH_TEXT);
        Assert.assertEquals(2, buttons.size());
        Assert.assertEquals(
                View.VISIBLE, bigView.findViewById(R.id.button_divider).getVisibility());
        Assert.assertEquals(View.VISIBLE, bigView.findViewById(R.id.buttons).getVisibility());
    }

    @Test
    @SmallTest
    @Feature({"Browser", "Notifications"})
    @DisableIf.Build(sdk_is_greater_than = 23, message = "crbug.com/779228")
    public void testZeroActionButtons() {
        Context context = InstrumentationRegistry.getTargetContext();
        NotificationBuilderBase builder = new CustomNotificationBuilder(context).setChannelId(
                ChannelDefinitions.ChannelId.SITES);
        Notification notification = buildNotification(builder);
        View bigView = notification.bigContentView.apply(context, new LinearLayout(context));
        ArrayList<View> buttons = new ArrayList<>();
        bigView.findViewsWithText(buttons, "button", View.FIND_VIEWS_WITH_TEXT);

        // When there are no buttons the container and divider must not be shown.
        Assert.assertTrue(buttons.isEmpty());
        Assert.assertEquals(View.GONE, bigView.findViewById(R.id.button_divider).getVisibility());
        Assert.assertEquals(View.GONE, bigView.findViewById(R.id.buttons).getVisibility());
    }

    @Test
    @SmallTest
    @Feature({"Browser", "Notifications"})
    @DisableIf.Build(sdk_is_greater_than = 23, message = "crbug.com/779228")
    public void testMaxActionButtons() {
        Context context = InstrumentationRegistry.getTargetContext();
        NotificationBuilderBase builder =
                new CustomNotificationBuilder(context)
                        .setChannelId(ChannelDefinitions.ChannelId.SITES)
                        .addButtonAction(null /* iconBitmap */, "button",
                                createIntent(context, "ActionButtonOne").getPendingIntent())
                        .addButtonAction(null /* iconBitmap */, "button",
                                createIntent(context, "ActionButtonTwo").getPendingIntent());
        try {
            builder.addButtonAction(null /* iconBitmap */, "button",
                    createIntent(context, "ActionButtonThree").getPendingIntent());
            Assert.fail(
                    "This statement should not be reached as the previous statement should throw.");
        } catch (IllegalStateException e) {
            Assert.assertEquals("Cannot add more than 2 actions.", e.getMessage());
        }

        Notification notification = buildNotification(builder);
        View bigView = notification.bigContentView.apply(context, new LinearLayout(context));
        ArrayList<View> buttons = new ArrayList<>();
        bigView.findViewsWithText(buttons, "button", View.FIND_VIEWS_WITH_TEXT);

        Assert.assertEquals("There is a maximum of 2 buttons", 2, buttons.size());
    }

    @Test
    @SmallTest
    @Feature({"Browser", "Notifications"})
    @DisableIf.Build(sdk_is_greater_than = 23, message = "crbug.com/779228")
    public void largeIconShouldBePaintedWithoutChange() {
        Context context = InstrumentationRegistry.getTargetContext();

        Bitmap largeIcon = createIcon(Color.RED);

        NotificationBuilderBase builder = new CustomNotificationBuilder(context)
                                                  .setChannelId(ChannelDefinitions.ChannelId.SITES)
                                                  .setLargeIcon(largeIcon)
                                                  .setSmallIconId(R.drawable.ic_chrome);
        Notification notification = buildNotification(builder);
        assertLargeNotificationIconAsExpected(context, notification, largeIcon);
    }

    @Test
    @SmallTest
    @Feature({"Browser", "Notifications"})
    public void smallIconsArePaintedWhite() {
        Context context = InstrumentationRegistry.getTargetContext();

        Bitmap smallIcon = createIcon(Color.RED);

        NotificationBuilderBase builder = new CustomNotificationBuilder(context)
                                                  .setChannelId(ChannelDefinitions.ChannelId.SITES)
                                                  .setSmallIconForContent(smallIcon)
                                                  .setStatusBarIcon(smallIcon);
        Notification notification = buildNotification(builder);

        // Note that small icon as a Bitmap should be present on pre-M, even though it can't
        // be shown in the status bar
        assertSmallNotificationIconAsExpected(context, notification, createIcon(Color.WHITE));
    }

    @Test
    @SmallTest
    @Feature({"Browser", "Notifications"})
    public void actionIconsArePaintedWhite() {
        Context context = InstrumentationRegistry.getTargetContext();
        Bitmap actionIcon = createIcon(Color.RED);

        NotificationBuilderBase builder =
                new CustomNotificationBuilder(context)
                        .setChannelId(ChannelDefinitions.ChannelId.SITES)
                        .setSmallIconId(R.drawable.ic_chrome)
                        .addButtonAction(actionIcon, "button",
                                createIntent(context, "ActionButton").getPendingIntent());
        Notification notification = buildNotification(builder);

        Bitmap whiteIcon = createIcon(Color.WHITE);

        Assert.assertEquals(1, NotificationTestUtil.getActions(notification).length);
        View bigView = notification.bigContentView.apply(context, new LinearLayout(context));
        ImageView actionIconView = (ImageView) bigView.findViewById(R.id.button_icon);
        Bitmap actionIconBitmap = ((BitmapDrawable) actionIconView.getDrawable()).getBitmap();
        Assert.assertTrue(whiteIcon.sameAs(actionIconBitmap));
    }

    @Test
    @SmallTest
    @Feature({"Browser", "Notifications"})
    @DisableIf.Build(sdk_is_greater_than = 22)
    public void statusBarIconAsBitmapIsIgnoredIfNotSupported() {
        Context context = InstrumentationRegistry.getTargetContext();

        NotificationBuilderBase notificationBuilder = new CustomNotificationBuilder(context)
                .setStatusBarIcon(createIcon(Color.RED));

        Assert.assertFalse(notificationBuilder.hasStatusBarIconBitmap());
    }

    @Test
    @SmallTest
    @Feature({"Browser", "Notifications"})
    @DisableIf.Build(sdk_is_greater_than = 23, message = "crbug.com/779228")
    public void testCharSequenceLimits() {
        Context context = InstrumentationRegistry.getTargetContext();
        int maxLength = CustomNotificationBuilder.MAX_CHARSEQUENCE_LENGTH;
        NotificationBuilderBase builder =
                new CustomNotificationBuilder(context)
                        .setTitle(createString('a', maxLength + 1))
                        .setBody(createString('b', maxLength + 1))
                        .setOrigin(createString('c', maxLength + 1))
                        .setChannelId(ChannelDefinitions.ChannelId.SITES)
                        .setTicker(createString('d', maxLength + 1))
                        .addButtonAction(null /* iconBitmap */, createString('e', maxLength + 1),
                                createIntent(context, "ActionButtonOne").getPendingIntent());
        Notification notification = buildNotification(builder);

        View compactView = notification.contentView.apply(context, new LinearLayout(context));
        View bigView = notification.bigContentView.apply(context, new LinearLayout(context));

        Assert.assertEquals(maxLength, getIdenticalText(R.id.title, compactView, bigView).length());
        Assert.assertEquals(maxLength, getIdenticalText(R.id.body, compactView, bigView).length());
        Assert.assertEquals(
                maxLength, getIdenticalText(R.id.origin, compactView, bigView).length());
        Assert.assertEquals(maxLength, notification.tickerText.length());

        ArrayList<View> buttons = new ArrayList<>();
        bigView.findViewsWithText(buttons, createString('e', maxLength), View.FIND_VIEWS_WITH_TEXT);
        Assert.assertEquals(1, buttons.size());
        Assert.assertEquals(maxLength, ((Button) buttons.get(0)).getText().length());
    }

    @Test
    @SmallTest
    @Feature({"Browser", "Notifications"})
    @DisableIf.Build(sdk_is_greater_than = 23, message = "crbug.com/779228")
    public void testCalculateMaxBodyLines() {
        Assert.assertEquals(7, CustomNotificationBuilder.calculateMaxBodyLines(-1000.0f));
        Assert.assertEquals(7, CustomNotificationBuilder.calculateMaxBodyLines(0.5f));
        Assert.assertEquals(7, CustomNotificationBuilder.calculateMaxBodyLines(1.0f));
        Assert.assertEquals(4, CustomNotificationBuilder.calculateMaxBodyLines(2.0f));
        Assert.assertEquals(1, CustomNotificationBuilder.calculateMaxBodyLines(1000.0f));
    }

    @Test
    @SmallTest
    @Feature({"Browser", "Notifications"})
    @DisableIf.Build(sdk_is_greater_than = 23, message = "crbug.com/779228")
    public void testCalculateScaledPadding() {
        DisplayMetrics metrics = new DisplayMetrics();
        metrics.density = 10.0f;
        Assert.assertEquals(
                30, CustomNotificationBuilder.calculateScaledPadding(-1000.0f, metrics));
        Assert.assertEquals(30, CustomNotificationBuilder.calculateScaledPadding(0.5f, metrics));
        Assert.assertEquals(30, CustomNotificationBuilder.calculateScaledPadding(1.0f, metrics));
        Assert.assertEquals(20, CustomNotificationBuilder.calculateScaledPadding(1.1f, metrics));
        Assert.assertEquals(10, CustomNotificationBuilder.calculateScaledPadding(1.2f, metrics));
        Assert.assertEquals(0, CustomNotificationBuilder.calculateScaledPadding(1.3f, metrics));
        Assert.assertEquals(0, CustomNotificationBuilder.calculateScaledPadding(1000.0f, metrics));
    }

    @Test
    @SmallTest
    @Feature({"Browser", "Notifications"})
    @DisableIf.Build(sdk_is_greater_than = 23, message = "crbug.com/779228")
    public void testGeneratesLargeIconFromOriginWhenNoLargeIconProvided() {
        Context context = InstrumentationRegistry.getTargetContext();
        NotificationBuilderBase notificationBuilder =
                new CustomNotificationBuilder(context)
                        .setOrigin("https://www.google.com")
                        .setChannelId(ChannelDefinitions.ChannelId.SITES);

        Notification notification = buildNotification(notificationBuilder);

        Bitmap expectedIcon = NotificationBuilderBase.createIconGenerator(context.getResources())
                                      .generateIconForUrl("https://www.google.com");

        assertLargeNotificationIconAsExpected(context, notification, expectedIcon);
    }

    @Test
    @SmallTest
    @Feature({"Browser", "Notifications"})
    @DisableIf.Build(sdk_is_greater_than = 23, message = "crbug.com/779228")
    public void testGeneratesLargeIconFromOriginWhenLargeIconProvidedIsNull() {
        Context context = InstrumentationRegistry.getTargetContext();
        NotificationBuilderBase notificationBuilder =
                new CustomNotificationBuilder(context)
                        .setOrigin("https://www.chromium.org")
                        .setChannelId(ChannelDefinitions.ChannelId.SITES)
                        .setLargeIcon(null);

        Notification notification = buildNotification(notificationBuilder);

        Bitmap expectedIcon = NotificationBuilderBase.createIconGenerator(context.getResources())
                                      .generateIconForUrl("https://www.chromium.org");

        assertLargeNotificationIconAsExpected(context, notification, expectedIcon);
    }

    /**
     * Tests that adding a text action results in a notification action with a RemoteInput attached.
     *
     * Note that the action buttons in custom layouts will not trigger an inline reply, but we can
     * still check the notification's action properties since these are used on Android Wear.
     */
    @Test
    @MinAndroidSdkLevel(Build.VERSION_CODES.KITKAT_WATCH)
    @TargetApi(Build.VERSION_CODES.KITKAT_WATCH) // RemoteInputs were only added in KITKAT_WATCH.
    @SmallTest
    @Feature({"Browser", "Notifications"})
    @DisableIf.Build(sdk_is_greater_than = 23, message = "crbug.com/779228")
    public void testAddTextActionSetsRemoteInput() {
        Context context = InstrumentationRegistry.getTargetContext();
        NotificationBuilderBase notificationBuilder =
                new CustomNotificationBuilder(context)
                        .setChannelId(ChannelDefinitions.ChannelId.SITES)
                        .addTextAction(null, "Action Title", null, "Placeholder");

        Notification notification = buildNotification(notificationBuilder);

        Assert.assertEquals(1, notification.actions.length);
        Assert.assertEquals("Action Title", notification.actions[0].title);
        Assert.assertNotNull(notification.actions[0].getRemoteInputs());
        Assert.assertEquals(1, notification.actions[0].getRemoteInputs().length);
        Assert.assertEquals("Placeholder", notification.actions[0].getRemoteInputs()[0].getLabel());
    }

    private static void assertLargeNotificationIconAsExpected(
            Context context, Notification notification, Bitmap expectedIcon) {
        // 1. Check large icon property on the notification.

        Bitmap icon = NotificationTestUtil.getLargeIconFromNotification(context, notification);
        Assert.assertNotNull(icon);
        Assert.assertTrue(expectedIcon.sameAs(icon));

        // 2. Check the large icon in the custom layouts.

        View compactView = notification.contentView.apply(context, new LinearLayout(context));
        Drawable compactViewIcon = ((ImageView) compactView.findViewById(R.id.icon)).getDrawable();
        Assert.assertNotNull(compactViewIcon);
        Assert.assertTrue(expectedIcon.sameAs(((BitmapDrawable) compactViewIcon).getBitmap()));

        View bigView = notification.bigContentView.apply(context, new LinearLayout(context));
        Drawable bigViewIcon = ((ImageView) bigView.findViewById(R.id.icon)).getDrawable();
        Assert.assertNotNull(bigViewIcon);

        // Starts from Android O MR1, large icon can be downscaled by Android platform code.
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.O) {
            Assert.assertTrue(expectedIcon.sameAs(((BitmapDrawable) bigViewIcon).getBitmap()));
        }
    }

    private static void assertSmallNotificationIconAsExpected(
            Context context, Notification notification, Bitmap expectedIcon) {
        // 1. Check small icon property on the notification, for M+.
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.M) {
            Bitmap icon =
                    NotificationTestUtil.getBitmapFromIcon(context, notification.getSmallIcon());
            Assert.assertNotNull(icon);
            Assert.assertTrue(expectedIcon.sameAs(icon));
        }

        // 2. Check the small icon in the custom layouts.

        int smallIconId = CustomNotificationBuilder.useMaterial() ? R.id.small_icon_overlay
                                                                  : R.id.small_icon_footer;
        View compactView = notification.contentView.apply(context, new LinearLayout(context));
        Drawable compactViewIcon =
                ((ImageView) compactView.findViewById(smallIconId)).getDrawable();
        Assert.assertNotNull(compactViewIcon);
        Assert.assertTrue(expectedIcon.sameAs(((BitmapDrawable) compactViewIcon).getBitmap()));

        View bigView = notification.bigContentView.apply(context, new LinearLayout(context));
        Drawable bigViewIcon = ((ImageView) bigView.findViewById(smallIconId)).getDrawable();
        Assert.assertNotNull(bigViewIcon);
        Assert.assertTrue(expectedIcon.sameAs(((BitmapDrawable) bigViewIcon).getBitmap()));
    }

    /**
     * Finds a TextView with the given id in each of the given views, and checks that they all
     * contain the same text.
     * @param id The id to find the TextView instances with.
     * @param views The views to find the TextView instances in.
     * @return The identical text.
     */
    private CharSequence getIdenticalText(int id, View... views) {
        CharSequence result = null;
        for (View view : views) {
            TextView textView = (TextView) view.findViewById(id);
            Assert.assertNotNull(textView);
            CharSequence text = textView.getText();
            if (result == null) {
                result = text;
            } else {
                Assert.assertEquals(result, text);
            }
        }
        return result;
    }

    private static PendingIntentProvider createIntent(Context context, String action) {
        Intent intent = new Intent("CustomNotificationBuilderTest." + action);
        return PendingIntentProvider.getBroadcast(
                context, 0 /* requestCode */, intent, PendingIntent.FLAG_UPDATE_CURRENT);
    }

    private static String createString(char character, int length) {
        char[] chars = new char[length];
        Arrays.fill(chars, character);
        return new String(chars);
    }

    private static Bitmap createIcon(int color) {
        Bitmap icon = Bitmap.createBitmap(
                new int[] {color}, 1 /* width */, 1 /* height */, Bitmap.Config.ARGB_8888);
        return icon.copy(Bitmap.Config.ARGB_8888, true /* isMutable */);
    }

    private static Notification buildNotification(NotificationBuilderBase builder) {
        NotificationMetadata metadata =
                new NotificationMetadata(NotificationUmaTracker.SystemNotificationType.SITES,
                        NOTIFICATION_TAG, NOTIFICATION_ID);
        return builder.build(metadata).getNotification();
    }
}
