// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks.bar;

import android.content.Context;
import android.content.res.Resources;
import android.graphics.drawable.Drawable;

import androidx.annotation.IntDef;
import androidx.appcompat.content.res.AppCompatResources;

import org.chromium.base.ResettersForTesting;
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
    static @interface ViewType {
        int ITEM = 1;
    }

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
     * Returns true if the Bookmark Bar currently visible. The feature is visible when it is allowed
     * in the given context and the user setting for the given profile is enabled.
     *
     * @param context The context in which compatibility should be assessed.
     * @param profile The profile for which the user setting should be assessed.
     * @return Whether the Bookmark Bar is currently visible.
     */
    public static boolean isBookmarkBarVisible(Context context, @Nullable Profile profile) {
        if (sBookmarkBarVisibleForTesting != null) {
            return sBookmarkBarVisibleForTesting;
        }
        return isActivityStateBookmarkBarCompatible(context) && isSettingEnabled(profile);
    }

    /**
     * Returns whether the bookmark bar user setting is currently enabled.
     *
     * @param profile The profile for which the user setting should be assessed.
     * @return Whether the user setting is currently enabled.
     */
    public static boolean isSettingEnabled(@Nullable Profile profile) {
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
    public static void setSettingEnabled(Profile profile, boolean enabled) {
        getPrefService(profile).setBoolean(Pref.SHOW_BOOKMARK_BAR, enabled);
    }

    /**
     * Toggles whether the bookmark bar user setting is currently enabled.
     *
     * @param profile The profile for which the user setting should be toggled.
     */
    public static void toggleSettingEnabled(Profile profile) {
        final var prefService = getPrefService(profile);
        prefService.setBoolean(
                Pref.SHOW_BOOKMARK_BAR, !prefService.getBoolean(Pref.SHOW_BOOKMARK_BAR));
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
        PropertyModel.Builder modelBuilder =
                new PropertyModel.Builder(BookmarkBarButtonProperties.ALL_KEYS)
                        .with(
                                BookmarkBarButtonProperties.CLICK_CALLBACK,
                                (metaState) -> clickCallback.accept(item, metaState))
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
