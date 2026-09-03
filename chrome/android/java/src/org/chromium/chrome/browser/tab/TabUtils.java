// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.app.Activity;
import android.content.Context;
import android.content.res.Configuration;
import android.content.res.Resources;
import android.graphics.Bitmap;
import android.graphics.Matrix;
import android.graphics.Point;
import android.graphics.Rect;
import android.graphics.drawable.BitmapDrawable;
import android.graphics.drawable.Drawable;
import android.util.Size;
import android.view.Display;
import android.widget.ImageView;
import android.widget.ImageView.ScaleType;

import androidx.annotation.ColorInt;
import androidx.annotation.DrawableRes;
import androidx.annotation.StringRes;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.ContextUtils;
import org.chromium.base.DeviceInfo;
import org.chromium.build.annotations.Contract;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider;
import org.chromium.chrome.browser.media.MediaCaptureDevicesDispatcherAndroid;
import org.chromium.chrome.browser.tasks.tab_management.TabUiThemeProvider;
import org.chromium.components.browser_ui.styles.SemanticColorUtils;
import org.chromium.components.browser_ui.util.AutomotiveUtils;
import org.chromium.components.browser_ui.util.DimensionCompat;
import org.chromium.components.tabs.TabAlert;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.display.DisplayAndroidManager;
import org.chromium.ui.display.DisplayUtil;

/** Collection of utility methods that operates on Tab. */
@NullMarked
public class TabUtils {
    @VisibleForTesting public static final float PORTRAIT_THUMBNAIL_ASPECT_RATIO = 0.85f;

    // Do not instantiate this class.
    private TabUtils() {}

    /**
     * @return Whether the given tab is initialized and not destroyed.
     */
    @Contract("null -> false")
    public static boolean isValid(@Nullable Tab tab) {
        return tab != null && tab.isInitialized() && !tab.isDestroyed();
    }

    /**
     * @return {@link Activity} associated with the given tab.
     */
    public static @Nullable Activity getActivity(@Nullable Tab tab) {
        WebContents webContents = tab != null ? tab.getWebContents() : null;
        if (webContents == null || webContents.isDestroyed()) return null;
        WindowAndroid window = webContents.getTopLevelNativeWindow();
        return window != null ? window.getActivity().get() : null;
    }

    /**
     * Provides an estimate of the contents size.
     *
     * The estimate is likely to be incorrect. This is not a problem, as the aim
     * is to avoid getting a different layout and resources than needed at
     * render time.
     * @param context The application context.
     * @return The estimated prerender size in pixels.
     */
    // status_bar_height is not a public framework resource, so we have to getIdentifier()
    @SuppressWarnings("DiscouragedApi")
    public static Rect estimateContentSize(Context context) {
        // The size is estimated as:
        // X = screenSizeX
        // Y = screenSizeY - top bar - bottom bar - custom tabs bar
        // The bounds rectangle includes the bottom bar and the custom tabs bar as well.
        Rect screenBounds = new Rect();
        Point screenSize = new Point();
        Display display = DisplayAndroidManager.getDefaultDisplayForContext(context);
        display.getSize(screenSize);
        Resources resources = context.getResources();
        int statusBarId = resources.getIdentifier("status_bar_height", "dimen", "android");
        try {
            screenSize.y -= resources.getDimensionPixelSize(statusBarId);
        } catch (Resources.NotFoundException e) {
            // Nothing, this is just a best effort estimate.
        }
        screenBounds.set(
                0,
                resources.getDimensionPixelSize(R.dimen.custom_tabs_control_container_height),
                screenSize.x,
                screenSize.y);
        return screenBounds;
    }

    public static @Nullable Tab fromWebContents(@Nullable WebContents webContents) {
        return TabImplJni.get().fromWebContents(webContents);
    }

    /**
     * Call when tab need to switch user agent between desktop and mobile.
     *
     * @param tab The tab to be switched the user agent.
     * @param switchToDesktop Whether switching the user agent to desktop.
     * @param caller The caller of this method.
     */
    public static void switchUserAgent(Tab tab, boolean switchToDesktop) {
        final boolean reloadOnChange = !tab.isNativePage();
        assumeNonNull(tab.getWebContents())
                .getNavigationController()
                .setUseDesktopUserAgent(
                        switchToDesktop, reloadOnChange, /* skipOnInitialNavigation= */ true);
    }

    /**
     * Get UseDesktopUserAgent setting from webContents.
     *
     * @param webContents The webContents used to retrieve UseDesktopUserAgent setting.
     * @return Whether the webContents is set to use desktop user agent.
     */
    public static boolean isUsingDesktopUserAgent(@Nullable WebContents webContents) {
        return webContents != null
                && webContents.getNavigationController().getUseDesktopUserAgent();
    }

    /**
     * Return aspect ratio for grid tab card based on form factor and orientation.
     * @param context - Context of the application.
     * @param browserControlsStateProvider - For getting browser controls height.
     * @return Aspect ratio for the grid tab card.
     */
    public static float getTabThumbnailAspectRatio(
            Context context, BrowserControlsStateProvider browserControlsStateProvider) {
        if (context.getResources().getConfiguration().orientation
                == Configuration.ORIENTATION_LANDSCAPE) {
            assert browserControlsStateProvider != null;
            int browserControlsHeightDp =
                    (browserControlsStateProvider == null)
                            ? 0
                            : Math.round(
                                    (float) browserControlsStateProvider.getTopControlsHeight()
                                            / context.getResources().getDisplayMetrics().density);
            int horizontalAutomotiveToolbarHeightDp =
                    AutomotiveUtils.getHorizontalAutomotiveToolbarHeightDp(context);
            int verticalAutomotiveToolbarWidthDp =
                    AutomotiveUtils.getVerticalAutomotiveToolbarWidthDp(context);
            DimensionCompat dimensionCompat = getDimensionCompat(context);
            float windowWidthDp = getWindowWidthDp(dimensionCompat, context);
            float windowHeightDp = getWindowHeightExcludingSystemBarsDp(dimensionCompat, context);
            // This should match the aspect ratio of a Tab's content area.
            return (windowWidthDp - verticalAutomotiveToolbarWidthDp)
                    / (windowHeightDp
                            - browserControlsHeightDp
                            - horizontalAutomotiveToolbarHeightDp);
        }
        // This is an experimentally determined value.
        return PORTRAIT_THUMBNAIL_ASPECT_RATIO;
    }

    private static float getWindowWidthDp(DimensionCompat compat, Context context) {
        return compat.getWindowWidth() / context.getResources().getDisplayMetrics().density;
    }

    private static float getWindowHeightExcludingSystemBarsDp(
            DimensionCompat compat, Context context) {
        return (compat.getWindowHeight() - compat.getNavbarHeight() - compat.getStatusBarHeight())
                / context.getResources().getDisplayMetrics().density;
    }

    private static DimensionCompat getDimensionCompat(Context context) {
        // (TODO: crbug.com/351854698) Pass activity context instead.
        Activity activity = ContextUtils.activityFromContext(context);
        assert activity != null : "Activity from context should not be null for this class.";
        return DimensionCompat.create(activity, null);
    }

    /**
     * Derive grid card height based on width, expected thumbnail aspect ratio and margins.
     *
     * @param cardWidthPx width of the card
     * @param context to derive view margins
     * @param browserControlsStateProvider - For getting browser controls height.
     * @return computed card height.
     */
    public static int deriveGridCardHeight(
            int cardWidthPx,
            Context context,
            BrowserControlsStateProvider browserControlsStateProvider) {
        float aspectRatio = getTabThumbnailAspectRatio(context, browserControlsStateProvider);
        int thumbnailHeight = (int) ((cardWidthPx - getThumbnailWidthDiff(context)) / aspectRatio);
        return thumbnailHeight + getThumbnailHeightDiff(context);
    }

    /**
     * Derive grid card width based on height, expected thumbnail aspect ratio and margins.
     *
     * @param cardHeightPx width of the card
     * @param context to derive view margins
     * @param browserControlsStateProvider - For getting browser controls height.
     * @return computed card height.
     */
    public static int deriveGridCardWidth(
            int cardHeightPx,
            Context context,
            BrowserControlsStateProvider browserControlsStateProvider) {
        float aspectRatio = getTabThumbnailAspectRatio(context, browserControlsStateProvider);
        int thumbnailWidth = (int) ((cardHeightPx - getThumbnailHeightDiff(context)) * aspectRatio);
        return thumbnailWidth + getThumbnailWidthDiff(context);
    }

    /**
     * Derive thumbnail size based on parent card size.
     *
     * @param gridCardSize size of parent card.
     * @param context to derive view margins.
     * @return computed width and height of thumbnail.
     */
    public static Size deriveThumbnailSize(Size gridCardSize, Context context) {
        int thumbnailWidth = gridCardSize.getWidth() - getThumbnailWidthDiff(context);
        int thumbnailHeight = gridCardSize.getHeight() - getThumbnailHeightDiff(context);
        return new Size(thumbnailWidth, thumbnailHeight);
    }

    /**
     * Update the {@link Bitmap} and @{@link Matrix} of ImageView. The drawable is scaled by a
     * matrix to be scaled to larger of the two dimensions of {@code destinationSize}, then
     * top-center aligned.
     *
     * @param view The {@link ImageView} to update.
     * @param drawable The {@link Drawable} to set in the view and scale.
     * @param destinationSize The desired {@link Size} of the drawable.
     */
    public static void setDrawableAndUpdateImageMatrix(
            ImageView view, Drawable drawable, Size destinationSize) {
        if (DeviceInfo.isAutomotive()) {
            if (drawable instanceof BitmapDrawable bitmapDrawable) {
                Bitmap bitmap = bitmapDrawable.getBitmap();
                assert bitmap != null;
                bitmap.setDensity(
                        DisplayUtil.getUiDensityForAutomotive(
                                view.getContext(), bitmap.getDensity()));
            }
        }
        view.setImageDrawable(drawable);
        int newWidth = destinationSize == null ? 0 : destinationSize.getWidth();
        int newHeight = destinationSize == null ? 0 : destinationSize.getHeight();
        if (newWidth <= 0
                || newHeight <= 0
                || (newWidth == drawable.getIntrinsicWidth()
                        && newHeight == drawable.getIntrinsicHeight())) {
            view.setScaleType(ScaleType.FIT_CENTER);
            return;
        }

        final Matrix m = new Matrix();
        final float scale =
                Math.max(
                        (float) newWidth / drawable.getIntrinsicWidth(),
                        (float) newHeight / drawable.getIntrinsicHeight());
        m.setScale(scale, scale);

        /*
         * Bitmap is top-left aligned by default. We want to translate the image to be horizontally
         * center-aligned. |destination width - scaled width| is the width that is out of view
         * bounds. We need to translate the drawable (to left) by half of this distance.
         */
        final int xOffset = (int) ((newWidth - (drawable.getIntrinsicWidth() * scale)) / 2);
        m.postTranslate(xOffset, 0);

        view.setScaleType(ScaleType.MATRIX);
        view.setImageMatrix(m);
    }

    /** Returns whether media is being captured for a tab. */
    public static boolean isCapturingForMedia(Tab tab) {
        WebContents webContents = tab.getWebContents();
        if (webContents == null) return false;
        return MediaCaptureDevicesDispatcherAndroid.isCapturingAudio(webContents)
                || MediaCaptureDevicesDispatcherAndroid.isCapturingVideo(webContents)
                || MediaCaptureDevicesDispatcherAndroid.isCapturingTab(webContents)
                || MediaCaptureDevicesDispatcherAndroid.isCapturingWindow(webContents)
                || MediaCaptureDevicesDispatcherAndroid.isCapturingScreen(webContents);
    }

    /** Pauses media for a tab. */
    public static void pauseMedia(Tab tab) {
        WebContents webContents = tab.getWebContents();
        if (webContents != null) {
            webContents.suspendAllMediaPlayers();
            webContents.setAudioMuted(true);
        }
    }

    /**
     * Returns the {@link MediaState} corresponding to the given {@link TabAlert}.
     *
     * @param alertState The {@link TabAlert} for which to get the corresponding media state.
     * @deprecated Android is migrating from {@link MediaState} to {@link TabAlert}. Use {@link
     *     TabAlert} directly instead.
     */
    @Deprecated
    public static @MediaState int getMediaStateForAlert(@TabAlert int alertState) {
        return switch (alertState) {
            case TabAlert.AUDIO_PLAYING -> MediaState.AUDIBLE;
            case TabAlert.AUDIO_MUTING -> MediaState.MUTED;
            case TabAlert.AUDIO_RECORDING, TabAlert.MEDIA_RECORDING, TabAlert.VIDEO_RECORDING ->
                    MediaState.RECORDING;
            case TabAlert.TAB_CAPTURING, TabAlert.DESKTOP_CAPTURING -> MediaState.SHARING;
            case TabAlert.PIP_PLAYING -> MediaState.PICTURE_IN_PICTURE;
            default -> MediaState.NONE;
        };
    }

    // LINT.IfChange(TabAlert)
    /**
     * Returns the {@link DrawableRes} ID for a given tab alert.
     *
     * @param alertState The {@link TabAlert} for which to get the indicator drawable.
     */
    public static @DrawableRes int getTabAlertDrawable(@TabAlert int alertState) {
        return switch (alertState) {
            case TabAlert.ACTOR_ACCESSING, TabAlert.ACTOR_WAITING_ON_USER ->
                    R.drawable.ic_arrow_selector_spark_24dp;
            case TabAlert.AUDIO_MUTING -> R.drawable.volume_off_24dp;
            case TabAlert.AUDIO_PLAYING -> R.drawable.volume_up_24dp;
            case TabAlert.AUDIO_RECORDING, TabAlert.MEDIA_RECORDING, TabAlert.VIDEO_RECORDING ->
                    R.drawable.radio_button_checked_24dp;
            case TabAlert.BLUETOOTH_CONNECTED -> R.drawable.ic_bluetooth_connected;
            case TabAlert.BLUETOOTH_SCAN_ACTIVE -> R.drawable.gm_filled_bluetooth_searching_24;
            case TabAlert.DESKTOP_CAPTURING, TabAlert.TAB_CAPTURING -> R.drawable.capture_24dp;
            case TabAlert.GLIC_ACCESSING, TabAlert.GLIC_SHARING ->
                    R.drawable.ic_screensaver_auto_24dp;
            // WebHID is unsupported on Android (services/device/hid lacks an Android driver).
            case TabAlert.HID_CONNECTED -> Resources.ID_NULL;
            case TabAlert.PIP_PLAYING -> R.drawable.picture_in_picture_24px;
            case TabAlert.SERIAL_CONNECTED -> R.drawable.gm_filled_developer_board_24;
            case TabAlert.USB_CONNECTED -> R.drawable.gm_filled_usb_24;
            case TabAlert.VR_PRESENTING_IN_HEADSET -> R.drawable.gm_filled_cardboard_24;
            default -> Resources.ID_NULL;
        };
    }

    /**
     * Returns the tint color for a given tab alert.
     *
     * @param context The {@link Context} used to retrieve color.
     * @param alertState The {@link TabAlert} for which to get the tint.
     * @param defaultTint The default tint to use.
     */
    public static @ColorInt int getTabAlertTintColor(
            Context context, @TabAlert int alertState, @ColorInt int defaultTint) {
        return switch (alertState) {
            case TabAlert.ACTOR_ACCESSING,
                    TabAlert.ACTOR_WAITING_ON_USER,
                    TabAlert.GLIC_ACCESSING,
                    TabAlert.GLIC_SHARING ->
                    SemanticColorUtils.getColorPrimary(context);
            case TabAlert.AUDIO_RECORDING, TabAlert.MEDIA_RECORDING, TabAlert.VIDEO_RECORDING ->
                    context.getColor(R.color.tab_recording_alert_color);
            case TabAlert.DESKTOP_CAPTURING, TabAlert.TAB_CAPTURING ->
                    context.getColor(R.color.tab_sharing_alert_color);
            case TabAlert.PIP_PLAYING -> context.getColor(R.color.tab_pip_alert_color);
            default -> defaultTint;
        };
    }

    /**
     * Returns the {@link StringRes} ID for the tooltip / accessibility description of a tab alert.
     *
     * @param alertState The {@link TabAlert} for which to get the description.
     */
    public static @StringRes int getTabAlertDescriptionRes(@TabAlert int alertState) {
        return switch (alertState) {
            case TabAlert.ACTOR_ACCESSING, TabAlert.ACTOR_WAITING_ON_USER ->
                    R.string.tooltip_tab_alert_state_actor_accessing;
            case TabAlert.AUDIO_MUTING -> R.string.tooltip_tab_alert_state_audio_muting;
            case TabAlert.AUDIO_PLAYING -> R.string.tooltip_tab_alert_state_audio_playing;
            case TabAlert.AUDIO_RECORDING -> R.string.tooltip_tab_alert_state_audio_recording;
            case TabAlert.BLUETOOTH_CONNECTED ->
                    R.string.tooltip_tab_alert_state_bluetooth_connected;
            case TabAlert.BLUETOOTH_SCAN_ACTIVE ->
                    R.string.tooltip_tab_alert_state_bluetooth_scan_active;
            case TabAlert.DESKTOP_CAPTURING -> R.string.tooltip_tab_alert_state_desktop_capturing;
            case TabAlert.GLIC_ACCESSING -> R.string.tooltip_tab_alert_state_glic_accessing;
            case TabAlert.GLIC_SHARING -> R.string.tooltip_tab_alert_state_glic_sharing;
            // WebHID is unsupported on Android (see getTabAlertDrawable above).
            case TabAlert.HID_CONNECTED -> Resources.ID_NULL;
            case TabAlert.MEDIA_RECORDING -> R.string.tooltip_tab_alert_state_media_recording;
            case TabAlert.PIP_PLAYING -> R.string.tooltip_tab_alert_state_pip_playing;
            case TabAlert.SERIAL_CONNECTED -> R.string.tooltip_tab_alert_state_serial_connected;
            case TabAlert.TAB_CAPTURING -> R.string.tooltip_tab_alert_state_tab_capturing;
            case TabAlert.USB_CONNECTED -> R.string.tooltip_tab_alert_state_usb_connected;
            case TabAlert.VIDEO_RECORDING -> R.string.tooltip_tab_alert_state_video_recording;
            case TabAlert.VR_PRESENTING_IN_HEADSET ->
                    R.string.tooltip_tab_alert_state_vr_presenting;
            default -> Resources.ID_NULL;
        };
    }

    // LINT.ThenChange(/components/tabs/public/tab_alert.h)

    // LINT.IfChange(TabAlertPriority)
    /** The maximum alert priority value returned by {@link #getTabAlertPriority(int)}. */
    public static final int MAX_TAB_ALERT_PRIORITY = 17;

    /**
     * Returns the priority of a given tab alert (higher number = higher priority to show).
     *
     * @param alertState The {@link TabAlert} for which to get the priority.
     * @return The priority integer, or -1 if {@code alertState} is {@link TabAlert#NONE} or
     *     unknown.
     */
    public static int getTabAlertPriority(@TabAlert int alertState) {
        return switch (alertState) {
            case TabAlert.DESKTOP_CAPTURING -> MAX_TAB_ALERT_PRIORITY;
            case TabAlert.TAB_CAPTURING -> 16;
            case TabAlert.MEDIA_RECORDING -> 15;
            case TabAlert.AUDIO_RECORDING -> 14;
            case TabAlert.VIDEO_RECORDING -> 13;
            case TabAlert.BLUETOOTH_CONNECTED -> 12;
            case TabAlert.BLUETOOTH_SCAN_ACTIVE -> 11;
            case TabAlert.USB_CONNECTED -> 10;
            case TabAlert.HID_CONNECTED -> 9;
            case TabAlert.SERIAL_CONNECTED -> 8;
            case TabAlert.ACTOR_WAITING_ON_USER -> 7;
            case TabAlert.ACTOR_ACCESSING -> 6;
            case TabAlert.GLIC_ACCESSING -> 5;
            case TabAlert.GLIC_SHARING -> 4;
            case TabAlert.VR_PRESENTING_IN_HEADSET -> 3;
            case TabAlert.PIP_PLAYING -> 2;
            case TabAlert.AUDIO_MUTING -> 1;
            case TabAlert.AUDIO_PLAYING -> 0;
            default -> -1;
        };
    }

    // LINT.ThenChange(//chrome/browser/ui/tabs/alert/tab_alert_controller.cc:TabAlertPriority)

    private static int getThumbnailHeightDiff(Context context) {
        final int tabGridCardMargin = (int) TabUiThemeProvider.getTabGridCardMargin(context);
        final int thumbnailMargin =
                (int) context.getResources().getDimension(R.dimen.tab_grid_card_thumbnail_margin);
        int heightMargins = (2 * tabGridCardMargin) + thumbnailMargin;
        final int titleHeight =
                (int) context.getResources().getDimension(R.dimen.tab_grid_card_header_height);
        return titleHeight + heightMargins;
    }

    private static int getThumbnailWidthDiff(Context context) {
        final int tabGridCardMargin = (int) TabUiThemeProvider.getTabGridCardMargin(context);
        final int thumbnailMargin =
                (int) context.getResources().getDimension(R.dimen.tab_grid_card_thumbnail_margin);
        return 2 * (tabGridCardMargin + thumbnailMargin);
    }
}
