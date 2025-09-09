// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks.bar;

import android.content.Context;
import android.content.res.Resources;
import android.graphics.drawable.Drawable;
import android.view.KeyEvent;
import android.view.View;

import androidx.annotation.IntDef;
import androidx.appcompat.content.res.AppCompatResources;

import org.chromium.base.ContextUtils;
import org.chromium.base.DeviceInfo;
import org.chromium.base.ResettersForTesting;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.supplier.LazyOneshotSupplier;
import org.chromium.base.supplier.LazyOneshotSupplierImpl;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.bookmarks.BookmarkImageFetcher;
import org.chromium.chrome.browser.bookmarks.R;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.bookmarks.BookmarkItem;
import org.chromium.components.prefs.PrefChangeRegistrar.PrefObserver;
import org.chromium.components.prefs.PrefService;
import org.chromium.components.user_prefs.UserPrefs;
import org.chromium.ui.base.DeviceFormFactor;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;
import org.chromium.ui.modelutil.PropertyModel;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.Collection;
import java.util.function.BiConsumer;

/** Utilities for the bookmark bar which provides users with bookmark access from top chrome. */
@NullMarked
public class BookmarkBarUtils {

    /** Enumeration of view type identifiers for views which are rendered in the bookmark bar. */
    @IntDef({ViewType.ITEM})
    @Retention(RetentionPolicy.SOURCE)
    @interface ViewType {
        int ITEM = 1;
    }

    /**
     * Enum that defines the possible types of clicks on the Bookmark Bar. These values are
     * persisted to logs. Entries should not be renumbered and numeric values should never be
     * reused.
     */
    // LINT.IfChange(BookmarkBarClickType)
    @Retention(RetentionPolicy.SOURCE)
    @IntDef({
        BookmarkBarClickType.UNKNOWN,
        BookmarkBarClickType.BOOKMARK_BAR_URL,
        BookmarkBarClickType.BOOKMARK_BAR_FOLDER,
        BookmarkBarClickType.OVERFLOW_MENU,
        BookmarkBarClickType.ALL_BOOKMARKS,
        BookmarkBarClickType.POP_UP_URL,
        BookmarkBarClickType.POP_UP_FOLDER,
        BookmarkBarClickType.NUM_ENTRIES
    })
    public @interface BookmarkBarClickType {
        int UNKNOWN = 0;
        int BOOKMARK_BAR_URL = 1;
        int BOOKMARK_BAR_FOLDER = 2;
        int OVERFLOW_MENU = 3;
        int ALL_BOOKMARKS = 4;
        int POP_UP_URL = 5;
        int POP_UP_FOLDER = 6;
        int NUM_ENTRIES = 7;
    }

    // LINT.ThenChange(/tools/metrics/histograms/metadata/bookmarks/enums.xml:BookmarkBarClickType)

    // Histogram names:
    public static final String TOGGLED_IN_SETTINGS = "Bookmarks.BookmarkBar.ToggledInSettings";
    public static final String TOGGLED_BY_KEYBOARD_SHORTCUT =
            "Bookmarks.BookmarkBar.ToggledByKeyboardShortcut";
    public static final String BOOKMARK_BAR_CLICK = "Bookmarks.BookmarkBar.Click";

    /** Whether the bookmark bar feature is forcibly allowed/disallowed for testing. */
    private static @Nullable Boolean sActivityStateBookmarkBarCompatibleForTesting;

    /** Whether the bookmark bar feature is forcibly enabled/disabled for testing. */
    private static @Nullable Boolean sDeviceBookmarkBarCompatibleForTesting;

    /** Whether the bookmark bar feature is forcibly visible/invisible for testing. */
    private static @Nullable Boolean sBookmarkBarVisibleForTesting;

    /** Whether the bookmark bar user setting is forcibly enabled/disabled for testing. */
    private static @Nullable Boolean sSettingEnabledForTesting;

    /** Collection in which to cache bookmark bar user setting observers for testing. */
    private static @Nullable Collection<PrefObserver> sSettingObserverCacheForTesting;

    private BookmarkBarUtils() {}

    /**
     * Returns true if the current state is compatible with the Bookmark Bar. The Bookmark Bar
     * requires certain device types, as well as certain activity states, e.g. window sizes. It may
     * be true that a device supports the Bookmark Bar and so the feature is exposed to the user for
     * this session, but, the user could be in a state where interaction with, or parts of, the
     * Bookmark Bar should be disabled.
     *
     * <p>Check this value in cases such as responding to user actions that interact with the
     * Bookmark Bar.
     *
     * <p>See {@link #isDeviceBookmarkBarCompatible(Context)} and {@link
     * #isWindowBookmarkBarCompatible(Context)}.
     *
     * @param context The context in which activity state compatibility should be assessed.
     * @return Whether the current activity state supports the Bookmark Bar.
     */
    public static boolean isActivityStateBookmarkBarCompatible(Context context) {
        if (sActivityStateBookmarkBarCompatibleForTesting != null) {
            return sActivityStateBookmarkBarCompatibleForTesting;
        }
        return isDeviceBookmarkBarCompatible(context) && isWindowBookmarkBarCompatible(context);
    }

    /**
     * Returns true if the device is compatible with, and can support, the Bookmark Bar, and
     * therefore if the feature should be exposed to the user. If true, user flows such as keyboard
     * shortcuts, IPH, settings toggles, device policies, etc should be present. This value should
     * always return the same value for a device.
     *
     * <p>Check this value when determining which user actions to expose to users for the Bookmark
     * Bar.
     *
     * <p>See {@link #isWindowBookmarkBarCompatible(Context)} and {@link
     * #isActivityStateBookmarkBarCompatible(Context)}.
     *
     * <p>Note: This also checks the feature flag for simplicity for clients.
     *
     * @param context The context in which device compatibility should be assessed.
     * @return Whether the device supports the Bookmark Bar.
     */
    public static boolean isDeviceBookmarkBarCompatible(Context context) {
        if (sDeviceBookmarkBarCompatibleForTesting != null) {
            return sDeviceBookmarkBarCompatibleForTesting;
        }
        return ChromeFeatureList.sAndroidBookmarkBar.isEnabled()
                && DeviceFormFactor.isNonMultiDisplayContextOnTablet(context);
    }

    /**
     * Returns true if the current activity window is compatible with the Bookmark Bar. The Bookmark
     * Bar is disabled for narrow windows, so the window size needs to be of sufficient width for
     * the Bookmark Bar to be displayed. The current requirement is a width >= 412dp, see {@link
     * //chrome/android/java/res/values-w412dp/bools.xml}. This value is not constant for a device,
     * and can change based on user interactions.
     *
     * <p>Note: There is no reasonable use-case to check this in isolation, so it is private.
     *
     * @param context The context in which window compatibility should be assessed.
     * @return Whether the window supports the Bookmark Bar.
     */
    private static boolean isWindowBookmarkBarCompatible(Context context) {
        return context.getResources().getBoolean(R.bool.bookmark_bar_allowed);
    }

    /**
     * Returns true if the Bookmark Bar currently visible. The feature is visible when it is allowed
     * in the given context, and the show bookmark bar UserPref is enabled for the current user.
     * When on tablets, we do not use the UserPref and instead use the device preference.
     *
     * @param context The context in which compatibility should be assessed.
     * @param profile The profile for which the user UserPref should be assessed.
     * @return Whether the Bookmark Bar is currently visible.
     */
    public static boolean isBookmarkBarVisible(Context context, @Nullable Profile profile) {
        if (sBookmarkBarVisibleForTesting != null) {
            return sBookmarkBarVisibleForTesting;
        }

        if (!isActivityStateBookmarkBarCompatible(context)) {
            return false;
        }

        // On Desktop, we sync with the UserPrefs, but on tablets we use a local device preference.
        return DeviceInfo.isDesktop()
                ? isUserPrefsShowBookmarksBarEnabled(profile)
                : isDevicePrefShowBookmarksBarEnabled();
    }

    // UserPrefs methods - used on Desktop.

    /**
     * Returns whether the bookmark bar should be shown based on the current user's UserPrefs. Note:
     * This is synced across devices for the user's profile.
     *
     * @param profile The profile for which the UserPref should be assessed.
     * @return The user's current preference for showing the bookmark bar.
     */
    public static boolean isUserPrefsShowBookmarksBarEnabled(@Nullable Profile profile) {
        if (sSettingEnabledForTesting != null) {
            return sSettingEnabledForTesting;
        }
        return profile != null ? getPrefService(profile).getBoolean(Pref.SHOW_BOOKMARK_BAR) : false;
    }

    /**
     * Sets whether the bookmark bar user setting is currently enabled.
     *
     * @param profile The profile for which the user setting should be set.
     * @param enabled Whether the user setting should be set to enabled/disabled.
     */
    public static void setUserPrefsShowBookmarksBar(
            Profile profile, boolean enabled, boolean fromKeyboardShortcut) {
        RecordHistogram.recordBooleanHistogram(
                fromKeyboardShortcut ? TOGGLED_BY_KEYBOARD_SHORTCUT : TOGGLED_IN_SETTINGS, enabled);
        getPrefService(profile).setBoolean(Pref.SHOW_BOOKMARK_BAR, enabled);
    }

    /**
     * Toggles the value of the show bookmarks bar UserPref for the current user.
     *
     * @param profile The profile for which the UserPref should be toggled.
     */
    public static void toggleUserPrefsShowBookmarksBar(
            Profile profile, boolean fromKeyboardShortcut) {
        setUserPrefsShowBookmarksBar(
                profile,
                !getPrefService(profile).getBoolean(Pref.SHOW_BOOKMARK_BAR),
                fromKeyboardShortcut);
    }

    // Device preferences methods - used on tablets.

    /**
     * Returns whether or not the bookmark bar should be shown based on the local device
     * preferences. This is only used on tablets, where bookmarks bar does not sync with the user's
     * desktop preference, but is instead stored locally on device.
     *
     * <p>Note: When a user has not previously set the device preference, the default return value
     * is currently controlled by a FeatureParam for testing.
     *
     * @return Whether or not the bookmarks bar should be shown based on device preference.
     */
    public static boolean isDevicePrefShowBookmarksBarEnabled() {
        // If a user has set the show bookmarks bar setting explicitly, then we will use that value.
        // If the user has never set the preference, then we will return a default, which is
        // currently controlled with a FeatureParam.
        return hasUserSetDevicePrefShowBookmarksBar()
                ? ContextUtils.getAppSharedPreferences()
                        .getBoolean(BookmarkBarConstants.BOOKMARK_BAR_SHOW_BOOKMARK_BAR, false)
                : ChromeFeatureList.sAndroidBookmarkBarShowBookmarkBar.getValue();
    }

    /**
     * Set whether the bookmark bar should be shown at a device preferences level. This is only used
     * on tablets, where bookmarks bar does not sync with the user's desktop preference, but is
     * instead stored locally on the device.
     *
     * @param enabled The new device preference for enabling the bookmark bar.
     */
    public static void setDevicePrefShowBookmarksBar(
            boolean enabled, boolean fromKeyboardShortcut) {
        RecordHistogram.recordBooleanHistogram(
                fromKeyboardShortcut ? TOGGLED_BY_KEYBOARD_SHORTCUT : TOGGLED_IN_SETTINGS, enabled);
        ContextUtils.getAppSharedPreferences()
                .edit()
                .putBoolean(BookmarkBarConstants.BOOKMARK_BAR_SHOW_BOOKMARK_BAR, enabled)
                .apply();
    }

    /**
     * Returns true when the user has previously set the visibility of the bookmarks bar explicitly
     * at the device preference level. This is only used on tablets, where bookmarks bar does not
     * sync with the user's desktop preference, but is instead stored locally on the device.
     *
     * @return Whether the user has set show bookmarks bar device preference manually.
     */
    public static boolean hasUserSetDevicePrefShowBookmarksBar() {
        return ContextUtils.getAppSharedPreferences()
                .contains(BookmarkBarConstants.BOOKMARK_BAR_SHOW_BOOKMARK_BAR);
    }

    /**
     * Toggles the value of the show bookmarks bar device preference, this is stored locally and
     * only used on tablets.
     */
    public static void toggleDevicePrefShowBookmarksBar(boolean fromKeyboardShortcut) {
        setDevicePrefShowBookmarksBar(!isDevicePrefShowBookmarksBarEnabled(), fromKeyboardShortcut);
    }

    // Histogram recording methods.

    public static void recordClick(@BookmarkBarClickType int clickType) {
        RecordHistogram.recordEnumeratedHistogram(
                BOOKMARK_BAR_CLICK, clickType, BookmarkBarClickType.NUM_ENTRIES);
    }

    // Helper methods.

    /**
     * Creates a list item to render in the bookmark bar for the specified bookmark item.
     *
     * @param clickCallback The callback to invoke on list item click events.
     * @param context The context in which the created list item will be rendered.
     * @param imageFetcher The image fetcher to use for rendering favicons.
     * @param item The bookmark item for which to create a renderable list item.
     * @return The created list item to render in the bookmark bar.
     */
    static ListItem createListItemFor(
            BiConsumer<BookmarkItem, Integer> clickCallback,
            Context context,
            @Nullable BookmarkImageFetcher imageFetcher,
            BookmarkItem item) {

        View.OnKeyListener keyListener =
                (v, keyCode, event) -> {
                    // Check whether the Enter key is released.
                    if (event.getAction() == KeyEvent.ACTION_UP
                            && keyCode == KeyEvent.KEYCODE_ENTER) {
                        // clickCallback is an object that represents
                        // BookmarkBarMediator#onBookmarkItemClick.
                        clickCallback.accept(item, event.getMetaState());
                        // Returning true handles the event, avoids triggering a normal click
                        // (double action).
                        return true;
                    }
                    // We do not handle other keys.
                    return false;
                };

        PropertyModel.Builder modelBuilder =
                new PropertyModel.Builder(BookmarkBarButtonProperties.ALL_KEYS)
                        .with(
                                BookmarkBarButtonProperties.CLICK_CALLBACK,
                                (metaState) -> clickCallback.accept(item, metaState))
                        .with(BookmarkBarButtonProperties.KEY_LISTENER, keyListener)
                        .with(
                                BookmarkBarButtonProperties.ICON_TINT_LIST_ID,
                                item.isFolder()
                                        ? R.color.default_icon_color_tint_list
                                        : Resources.ID_NULL)
                        .with(BookmarkBarButtonProperties.TITLE, item.getTitle());
        if (imageFetcher != null) {
            modelBuilder.with(
                    BookmarkBarButtonProperties.ICON_SUPPLIER,
                    createIconSupplierFor(context, imageFetcher, item));
        }
        return new ListItem(ViewType.ITEM, modelBuilder.build());
    }

    private static LazyOneshotSupplier<Drawable> createIconSupplierFor(
            Context context, BookmarkImageFetcher imageFetcher, BookmarkItem item) {
        if (item.isFolder()) {
            return LazyOneshotSupplier.fromSupplier(
                    () ->
                            AppCompatResources.getDrawable(
                                    context, R.drawable.ic_folder_outline_24dp));
        }
        return new LazyOneshotSupplierImpl<>() {
            @Override
            public void doSet() {
                imageFetcher.fetchFaviconForBookmark(item, this::set);
            }
        };
    }

    private static PrefService getPrefService(Profile profile) {
        return UserPrefs.get(profile.getOriginalProfile());
    }

    // ForTesting methods.

    /**
     * Sets whether the bookmark bar feature is forcibly allowed/disallowed for testing.
     *
     * @param allowed Whether the feature is forcibly allowed/disallowed.
     */
    public static void setActivityStateBookmarkBarCompatibleForTesting(@Nullable Boolean allowed) {
        sActivityStateBookmarkBarCompatibleForTesting = allowed;
        ResettersForTesting.register(() -> sActivityStateBookmarkBarCompatibleForTesting = null);
    }

    /**
     * Sets whether the bookmark bar feature is forcibly enabled/disabled for testing.
     *
     * @param enabled Whether the feature is forcibly enabled/disabled.
     */
    public static void setDeviceBookmarkBarCompatibleForTesting(@Nullable Boolean enabled) {
        sDeviceBookmarkBarCompatibleForTesting = enabled;
        ResettersForTesting.register(() -> sDeviceBookmarkBarCompatibleForTesting = null);
    }

    /**
     * Sets whether the bookmark bar feature is forcibly visible/invisible for testing.
     *
     * @param visible Whether the feature is forcibly visible/invisible.
     */
    public static void setBookmarkBarVisibleForTesting(@Nullable Boolean visible) {
        sBookmarkBarVisibleForTesting = visible;
        ResettersForTesting.register(() -> sBookmarkBarVisibleForTesting = null);
    }

    /**
     * Sets whether the bookmark bar user setting is forcibly enabled/disabled for testing.
     *
     * @param enabled Whether the user setting is forcibly enabled/disabled.
     */
    public static void setSettingEnabledForTesting(@Nullable Boolean enabled) {
        sSettingEnabledForTesting = enabled;
        ResettersForTesting.register(() -> sSettingEnabledForTesting = null);
    }

    /**
     * Sets the collection in which to cache bookmark bar user setting observers for testing.
     *
     * @param cache The collection in which to cache observers.
     */
    public static void setSettingObserverCacheForTesting(@Nullable Collection<PrefObserver> cache) {
        sSettingObserverCacheForTesting = cache;
        ResettersForTesting.register(() -> sSettingObserverCacheForTesting = null);
    }
}
