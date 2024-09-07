// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.notifications;

import android.app.Notification;
import android.app.PendingIntent;
import android.content.Context;
import android.content.res.Resources;
import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.Paint;
import android.graphics.PorterDuff;
import android.graphics.PorterDuffColorFilter;
import android.graphics.drawable.Icon;
import android.os.Bundle;

import androidx.annotation.IntDef;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;
import androidx.core.app.NotificationCompat;
import androidx.core.graphics.drawable.IconCompat;

import org.chromium.components.browser_ui.notifications.NotificationMetadata;
import org.chromium.components.browser_ui.notifications.NotificationWrapper;
import org.chromium.components.browser_ui.notifications.NotificationWrapperBuilder;
import org.chromium.components.browser_ui.notifications.PendingIntentProvider;
import org.chromium.components.browser_ui.widget.RoundedIconGenerator;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;

/** Abstract base class for building a notification. Stores all given arguments for later use. */
public abstract class NotificationBuilderBase {
    protected static class Action {
        @IntDef({Type.BUTTON, Type.TEXT})
        @Retention(RetentionPolicy.SOURCE)
        public @interface Type {
            /** Regular action that triggers the provided intent when tapped. */
            int BUTTON = 0;

            /**
             * Action that triggers a remote input when tapped, for Android Wear input and inline
             * replies from Android N.
             */
            int TEXT = 1;
        }

        public int iconId;
        public Bitmap iconBitmap;
        public CharSequence title;
        public PendingIntentProvider intent;
        public @Type int type;
        public @NotificationUmaTracker.ActionType int umaActionType;

        /** If the action.type is TEXT, this corresponds to the placeholder text for the input. */
        public String placeholder;

        Action(
                int iconId,
                CharSequence title,
                PendingIntentProvider intent,
                @Type int type,
                String placeholder) {
            this(
                    iconId,
                    title,
                    intent,
                    type,
                    placeholder,
                    NotificationUmaTracker.ActionType.UNKNOWN);
        }

        Action(
                int iconId,
                CharSequence title,
                PendingIntentProvider intent,
                @Type int type,
                String placeholder,
                @NotificationUmaTracker.ActionType int umaActionType) {
            this.iconId = iconId;
            this.title = title;
            this.intent = intent;
            this.type = type;
            this.placeholder = placeholder;
            this.umaActionType = umaActionType;
        }

        Action(
                Bitmap iconBitmap,
                CharSequence title,
                PendingIntentProvider intent,
                @Type int type,
                String placeholder) {
            this.iconBitmap = iconBitmap;
            this.title = title;
            this.intent = intent;
            this.type = type;
            this.placeholder = placeholder;
            this.umaActionType = NotificationUmaTracker.ActionType.UNKNOWN;
        }
    }

    /**
     * Maximum length of CharSequence inputs to prevent excessive memory consumption. At current
     * screen sizes we display about 500 characters at most, so this is a pretty generous limit, and
     * it matches what the Notification class does.
     */
    @VisibleForTesting static final int MAX_CHARSEQUENCE_LENGTH = 5 * 1024;

    /** Background color for generated notification icons. */
    @VisibleForTesting static final int NOTIFICATION_ICON_BG_COLOR = 0xFF969696;

    /** Density-independent text size for generated notification icons. */
    @VisibleForTesting static final int NOTIFICATION_ICON_TEXT_SIZE_DP = 28;

    /**
     * The maximum number of author provided action buttons. The settings button is not part of this
     * count.
     */
    private static final int MAX_AUTHOR_PROVIDED_ACTION_BUTTONS = 2;

    private final int mLargeIconWidthPx;
    private final int mLargeIconHeightPx;
    private final RoundedIconGenerator mIconGenerator;

    protected CharSequence mTitle;
    protected CharSequence mBody;
    protected CharSequence mOrigin;
    protected String mChannelId;
    protected CharSequence mTickerText;
    protected Bitmap mImage;

    protected int mSmallIconId;
    @Nullable protected Bitmap mSmallIconBitmapForStatusBar;
    @Nullable protected Bitmap mSmallIconBitmapForContent;
    @Nullable protected Bundle mExtras;

    protected PendingIntentProvider mContentIntent;
    protected PendingIntentProvider mDeleteIntent;
    protected @NotificationUmaTracker.ActionType int mDeleteIntentActionType =
            NotificationUmaTracker.ActionType.UNKNOWN;
    protected List<Action> mActions = new ArrayList<>(MAX_AUTHOR_PROVIDED_ACTION_BUTTONS);
    protected List<Action> mSettingsActions = new ArrayList<>(1);
    protected int mDefaults;
    protected long[] mVibratePattern;
    protected boolean mSilent;
    protected long mTimestamp;
    protected boolean mRenotify;
    protected int mPriority;
    private Bitmap mLargeIcon;
    private boolean mSuppressShowingLargeIcon;
    protected long mTimeoutAfterMs;

    public NotificationBuilderBase(Resources resources) {
        mLargeIconWidthPx =
                resources.getDimensionPixelSize(android.R.dimen.notification_large_icon_width);
        mLargeIconHeightPx =
                resources.getDimensionPixelSize(android.R.dimen.notification_large_icon_height);
        mIconGenerator = createIconGenerator(resources);
    }

    /** Combines all of the options that have been set and returns a new Notification object. */
    public abstract NotificationWrapper build(NotificationMetadata metadata);

    /** Sets the title text of the notification. */
    public NotificationBuilderBase setTitle(@Nullable CharSequence title) {
        mTitle = limitLength(title);
        return this;
    }

    /** Sets the body text of the notification. */
    public NotificationBuilderBase setBody(@Nullable CharSequence body) {
        mBody = limitLength(body);
        return this;
    }

    /** Sets the origin text of the notification. */
    public NotificationBuilderBase setOrigin(@Nullable CharSequence origin) {
        mOrigin = limitLength(origin);
        return this;
    }

    /** Sets the text that is displayed in the status bar when the notification first arrives. */
    public NotificationBuilderBase setTicker(@Nullable CharSequence tickerText) {
        mTickerText = limitLength(tickerText);
        return this;
    }

    /** Sets the content image to be prominently displayed when the notification is expanded. */
    public NotificationBuilderBase setImage(@Nullable Bitmap image) {
        mImage = image;
        return this;
    }

    /** Sets the large icon that is shown in the notification. */
    public NotificationBuilderBase setLargeIcon(@Nullable Bitmap icon) {
        mLargeIcon = icon;
        return this;
    }

    /** Sets whether to hide the large icon that would normally be shown in the notification. */
    public NotificationBuilderBase setSuppressShowingLargeIcon(boolean hideLargeIcon) {
        mSuppressShowingLargeIcon = hideLargeIcon;
        return this;
    }

    /**
     * Sets the duration after which to auto-close the notification, as if the user closed it.
     *
     * @param ms The timeout duration in milliseconds. No timeout unless positive.
     */
    public NotificationBuilderBase setTimeoutAfter(long ms) {
        mTimeoutAfterMs = ms;
        return this;
    }

    /**
     * Sets the resource id of a small icon that is shown in the notification and in the status bar.
     * Bitmaps set via {@link #setStatusBarIcon} and {@link #setSmallIconForContent} have precedence
     * over the resource id.
     */
    public NotificationBuilderBase setSmallIconId(int iconId) {
        mSmallIconId = iconId;
        return this;
    }

    /**
     * Sets the small icon that is shown in the status bar. If the platform supports using a small
     * icon bitmap, it will take precedence over one specified as a resource id.
     */
    public NotificationBuilderBase setStatusBarIcon(@Nullable Bitmap iconBitmap) {
        mSmallIconBitmapForStatusBar = applyWhiteOverlay(iconBitmap);
        return this;
    }

    /**
     * Sets the small icon that is shown in the notification. Unlike a bitmap icon in status bar,
     * this is supported on all devices. The specified Bitmap will take precedence over one
     * specified as a resource id.
     */
    public NotificationBuilderBase setSmallIconForContent(@Nullable Bitmap iconBitmap) {
        mSmallIconBitmapForContent = applyWhiteOverlay(iconBitmap);
        return this;
    }

    public NotificationBuilderBase setExtras(Bundle extras) {
        mExtras = extras;
        return this;
    }

    private static @Nullable Bitmap applyWhiteOverlay(@Nullable Bitmap icon) {
        Bitmap whitened = null;
        if (icon != null) {
            whitened = icon.copy(icon.getConfig(), /* isMutable= */ true);
            applyWhiteOverlayToBitmap(whitened);
        }
        return whitened;
    }

    /**
     * Sets the status bar icon for a notification that will be displayed by a different app. This
     * is safe to use for any app.
     *
     * @param iconId An iconId for a resource in the package that will display the notification.
     * @param iconBitmap The decoded bitmap. Depending on the device we need either id or bitmap.
     */
    public NotificationBuilderBase setStatusBarIconForRemoteApp(
            int iconId, @Nullable Bitmap iconBitmap) {
        // The small icon has to be from the resources of the app whose context
        // is passed to the Notification.Builder constructor. Thus we can't use iconId directly,
        // and instead use the decoded Bitmap.
        setStatusBarIcon(iconBitmap);
        return this;
    }

    /**
     * Sets the small icon to be shown inside a notification that will be displayed by a different
     * app. This is safe to use for any app.
     */
    public NotificationBuilderBase setContentSmallIconForRemoteApp(@Nullable Bitmap bitmap) {
        setSmallIconForContent(bitmap);
        return this;
    }

    /** Returns whether a there is a small icon bitmap to show in the status bar. */
    public boolean hasStatusBarIconBitmap() {
        return mSmallIconBitmapForStatusBar != null;
    }

    /** Returns whether a there is a small icon bitmap to show in the notification. */
    public boolean hasSmallIconForContent() {
        return mSmallIconBitmapForContent != null;
    }

    /** Sets the PendingIntent to send when the notification is clicked. */
    public NotificationBuilderBase setContentIntent(@Nullable PendingIntentProvider intent) {
        mContentIntent = intent;
        return this;
    }

    /**
     * Sets the PendingIntent to send when the notification is cleared by the user directly from the
     * notification panel.
     */
    public NotificationBuilderBase setDeleteIntent(@Nullable PendingIntentProvider intent) {
        mDeleteIntent = intent;
        return this;
    }

    /**
     * Sets the PendingIntent to send when the notification is cleared by the user directly from the
     * notification panel.
     *
     * <p>Records the intent in UMA as a special action instead of a dismissal.
     *
     * @param actionType The `ActionType` to record in UMA.
     */
    public NotificationBuilderBase setDeleteIntent(
            @Nullable PendingIntentProvider intent,
            @NotificationUmaTracker.ActionType int actionType) {
        mDeleteIntent = intent;
        mDeleteIntentActionType = actionType;
        return this;
    }

    /** Sets the channel id of the notification. */
    public NotificationBuilderBase setChannelId(String channelId) {
        mChannelId = channelId;
        return this;
    }

    /**
     * Adds an action to the notification, displayed as a button adjacent to the notification
     * content.
     */
    public NotificationBuilderBase addButtonAction(
            @Nullable Bitmap iconBitmap,
            @Nullable CharSequence title,
            PendingIntentProvider intent) {
        addAuthorProvidedAction(iconBitmap, title, intent, Action.Type.BUTTON, null);
        return this;
    }

    /**
     * Adds an action to the notification, displayed as a button adjacent to the notification
     * content, which when tapped will trigger a remote input. This enables Android Wear input and,
     * from Android N, displays a text box within the notification for inline replies.
     */
    public NotificationBuilderBase addTextAction(
            @Nullable Bitmap iconBitmap,
            @Nullable CharSequence title,
            PendingIntentProvider intent,
            String placeholder) {
        addAuthorProvidedAction(iconBitmap, title, intent, Action.Type.TEXT, placeholder);
        return this;
    }

    private void addAuthorProvidedAction(
            @Nullable Bitmap iconBitmap,
            @Nullable CharSequence title,
            PendingIntentProvider intent,
            @Action.Type int actionType,
            @Nullable String placeholder) {
        if (mActions.size() == MAX_AUTHOR_PROVIDED_ACTION_BUTTONS) {
            throw new IllegalStateException(
                    "Cannot add more than " + MAX_AUTHOR_PROVIDED_ACTION_BUTTONS + " actions.");
        }
        if (iconBitmap != null) {
            applyWhiteOverlayToBitmap(iconBitmap);
        }
        mActions.add(new Action(iconBitmap, limitLength(title), intent, actionType, placeholder));
    }

    /**
     * Adds an action to the notification for performing a settings related action, such as opening
     * the settings screen or revoking the permission in one tap.
     */
    public NotificationBuilderBase addSettingsAction(
            int iconId,
            @Nullable CharSequence title,
            PendingIntentProvider intent,
            @NotificationUmaTracker.ActionType int umaActionType) {
        mSettingsActions.add(
                new Action(
                        iconId,
                        limitLength(title),
                        intent,
                        Action.Type.BUTTON,
                        null,
                        umaActionType));
        return this;
    }

    /**
     * Sets the default notification options that will be used.
     *
     * <p>The value should be one or more of the following fields combined with bitwise-or: {@link
     * Notification#DEFAULT_SOUND}, {@link Notification#DEFAULT_VIBRATE}, {@link
     * Notification#DEFAULT_LIGHTS}.
     *
     * <p>For all default values, use {@link Notification#DEFAULT_ALL}.
     */
    public NotificationBuilderBase setDefaults(int defaults) {
        mDefaults = defaults;
        return this;
    }

    /** Sets the vibration pattern to use. */
    public NotificationBuilderBase setVibrate(long[] pattern) {
        mVibratePattern = Arrays.copyOf(pattern, pattern.length);
        return this;
    }

    /** Sets whether this notification should be silent. */
    public NotificationBuilderBase setSilent(boolean silent) {
        mSilent = silent;
        return this;
    }

    /**
     * Sets the priority of the notification (if set to private, overrides |setDefaults| and
     * |setVibrate|)
     */
    public NotificationBuilderBase setPriority(int priority) {
        mPriority = priority;
        return this;
    }

    /** Sets the timestamp at which the event of the notification took place. */
    public NotificationBuilderBase setTimestamp(long timestamp) {
        mTimestamp = timestamp;
        return this;
    }

    /** Sets the behavior for when the notification is replaced. */
    public NotificationBuilderBase setRenotify(boolean renotify) {
        mRenotify = renotify;
        return this;
    }

    /**
     * Gets the large icon for the notification.
     *
     * <p>If a large icon was supplied to the builder, returns this icon, scaled to an appropriate
     * size if necessary.
     *
     * <p>If no large icon was supplied then returns a default icon based on the notification
     * origin.
     *
     * <p>See {@link NotificationBuilderBase#ensureNormalizedIcon} for more details.
     */
    protected Bitmap getNormalizedLargeIcon() {
        if (mSuppressShowingLargeIcon) {
            return null;
        }
        return ensureNormalizedIcon(mLargeIcon, mOrigin);
    }

    /**
     * Ensures the availability of an icon for the notification.
     *
     * <p>If |icon| is a valid, non-empty Bitmap, the bitmap will be scaled to be of an appropriate
     * size for the current Android device. Otherwise, a default icon will be created based on the
     * origin the notification is being displayed for.
     *
     * @param icon The developer-provided icon they intend to use for the notification.
     * @param origin The origin the notification is being displayed for.
     * @return An appropriately sized icon to use for the notification.
     */
    @VisibleForTesting
    public Bitmap ensureNormalizedIcon(Bitmap icon, CharSequence origin) {
        if (icon == null || icon.getWidth() == 0) {
            return origin != null
                    ? mIconGenerator.generateIconForUrl(origin.toString(), true)
                    : null;
        }
        if (icon.getWidth() > mLargeIconWidthPx || icon.getHeight() > mLargeIconHeightPx) {
            return Bitmap.createScaledBitmap(
                    icon, mLargeIconWidthPx, mLargeIconHeightPx, false /* not filtered */);
        }
        return icon;
    }

    /**
     * Creates a public version of the notification to be displayed in sensitive contexts, such as
     * on the lockscreen, displaying just the site origin and badge or generated icon.
     */
    protected Notification createPublicNotification(Context context) {
        NotificationWrapperBuilder builder =
                NotificationWrapperBuilderFactory.createNotificationWrapperBuilder(mChannelId)
                        .setContentText(context.getString(R.string.notification_hidden_text))
                        .setSmallIcon(R.drawable.ic_chrome)
                        .setSubText(mOrigin);

        // Use the badge if provided and SDK supports it, else use a generated icon.
        if (mSmallIconBitmapForStatusBar != null) {
            // The Icon class was added in Android M.
            Bitmap publicIcon =
                    mSmallIconBitmapForStatusBar.copy(
                            mSmallIconBitmapForStatusBar.getConfig(), true);
            builder.setSmallIcon(Icon.createWithBitmap(publicIcon));
        }
        return builder.build();
    }

    private static @Nullable CharSequence limitLength(@Nullable CharSequence input) {
        if (input == null) {
            return input;
        }
        if (input.length() > MAX_CHARSEQUENCE_LENGTH) {
            return input.subSequence(0, MAX_CHARSEQUENCE_LENGTH);
        }
        return input;
    }

    /**
     * Sets the small icon on {@code builder} using a {@code Bitmap} if a non-null bitmap is
     * provided and the API level is high enough, otherwise the resource id is used.
     *
     * @param iconBitmap should be used only on devices that support bitmap icons.
     */
    protected static void setStatusBarIcon(
            NotificationWrapperBuilder builder, int iconId, @Nullable Bitmap iconBitmap) {
        if (iconBitmap != null) {
            builder.setSmallIcon(Icon.createWithBitmap(iconBitmap));
        } else {
            builder.setSmallIcon(iconId);
        }
    }

    /**
     * Adds an action to {@code builder} using a {@code Bitmap} if a bitmap is provided and the API
     * level is high enough, otherwise a resource id is used.
     */
    @SuppressWarnings("deprecation") // For addAction(NotificationCompat.Action)
    protected static void addActionToBuilder(NotificationWrapperBuilder builder, Action action) {
        NotificationCompat.Action.Builder actionBuilder = getActionBuilder(action);
        if (action.type == Action.Type.TEXT) {
            assert action.placeholder != null;
            actionBuilder.addRemoteInput(
                    new androidx.core.app.RemoteInput.Builder(NotificationConstants.KEY_TEXT_REPLY)
                            .setLabel(action.placeholder)
                            .build());
        }

        if (action.umaActionType == NotificationUmaTracker.ActionType.UNKNOWN) {
            builder.addAction(actionBuilder.build());
        } else {
            builder.addAction(
                    actionBuilder.build(),
                    action.intent.getFlags(),
                    action.umaActionType,
                    /* requestCode= */ 0);
        }
    }

    /**
     * Sets the notification group for the builder, determined by the origin provided. Note, after
     * this notification is built and posted, a further summary notification must be posted for
     * notifications in the group to appear grouped in the notification shade.
     */
    static void setGroupOnBuilder(NotificationWrapperBuilder builder, CharSequence origin) {
        if (origin == null) return;
        builder.setGroup(NotificationConstants.GROUP_WEB_PREFIX + origin);
        // TODO(crbug.com/40498483) Post a group summary notification.
        // Notifications with the same group will only actually be stacked if we post a group
        // summary notification. Calling setGroup at least prevents them being autobundled with
        // all Chrome notifications on N though (see crbug.com/674015).
    }

    private static NotificationCompat.Action.Builder getActionBuilder(Action action) {
        PendingIntent intent = action.intent.getPendingIntent();
        if (action.iconBitmap != null) {
            IconCompat icon = IconCompat.createWithBitmap(action.iconBitmap);
            return new NotificationCompat.Action.Builder(icon, action.title, intent);
        }
        return new NotificationCompat.Action.Builder(action.iconId, action.title, intent);
    }

    /**
     * Paints {@code bitmap} white. This processing should be performed if the Android system
     * expects a bitmap to be white, and the bitmap is not already known to be white. The bitmap
     * must be mutable.
     */
    static void applyWhiteOverlayToBitmap(Bitmap bitmap) {
        Paint paint = new Paint();
        paint.setColorFilter(new PorterDuffColorFilter(Color.WHITE, PorterDuff.Mode.SRC_ATOP));
        Canvas canvas = new Canvas(bitmap);
        canvas.drawBitmap(bitmap, 0, 0, paint);
    }

    @VisibleForTesting
    static RoundedIconGenerator createIconGenerator(Resources resources) {
        int largeIconWidthPx =
                resources.getDimensionPixelSize(android.R.dimen.notification_large_icon_width);
        int largeIconHeightPx =
                resources.getDimensionPixelSize(android.R.dimen.notification_large_icon_height);
        float density = resources.getDisplayMetrics().density;
        int cornerRadiusPx = Math.min(largeIconWidthPx, largeIconHeightPx) / 2;
        return new RoundedIconGenerator(
                largeIconWidthPx,
                largeIconHeightPx,
                cornerRadiusPx,
                NOTIFICATION_ICON_BG_COLOR,
                NOTIFICATION_ICON_TEXT_SIZE_DP * density);
    }
}
