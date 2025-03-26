// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks.bar;

import android.content.Context;
import android.content.res.Resources;
import android.graphics.drawable.Drawable;

import androidx.annotation.IntDef;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.appcompat.content.res.AppCompatResources;

import org.chromium.base.ResettersForTesting;
import org.chromium.base.supplier.LazyOneshotSupplier;
import org.chromium.base.supplier.LazyOneshotSupplierImpl;
import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.browser.bookmarks.BookmarkImageFetcher;
import org.chromium.chrome.browser.bookmarks.R;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileProvider;
import org.chromium.components.bookmarks.BookmarkItem;
import org.chromium.components.prefs.PrefChangeRegistrar;
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
public class BookmarkBarUtils {

    /** Enumeration of view type identifiers for views which are rendered in the bookmark bar. */
    @IntDef({ViewType.ITEM})
    @Retention(RetentionPolicy.SOURCE)
    static @interface ViewType {
        int ITEM = 1;
    }

    /** Whether the bookmark bar feature is forcibly enabled/disabled for testing. */
    private static Boolean sFeatureEnabledForTesting;

    /** Whether the bookmark bar user setting is forcibly enabled/disabled for testing. */
    private static Boolean sSettingEnabledForTesting;

    /** Collection in which to cache bookmark bar user setting observers for testing. */
    private static Collection<PrefObserver> sSettingObserverCacheForTesting;

    private BookmarkBarUtils() {}

    /**
     * Returns whether the bookmark bar feature is currently enabled. The feature is considered to
     * be enabled when its associated feature flag is enabled and device form factor restrictions
     * are satisfied.
     *
     * @param context The context in which feature enablement should be assessed.
     * @return Whether the feature is currently enabled.
     */
    public static boolean isFeatureEnabled(@NonNull Context context) {
        if (sFeatureEnabledForTesting != null) {
            return sFeatureEnabledForTesting;
        }
        return ChromeFeatureList.sAndroidBookmarkBar.isEnabled()
                && DeviceFormFactor.isNonMultiDisplayContextOnTablet(context);
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
    public static void setSettingEnabled(@NonNull Profile profile, boolean enabled) {
        getPrefService(profile).setBoolean(Pref.SHOW_BOOKMARK_BAR, enabled);
    }

    /**
     * Toggles whether the bookmark bar user setting is currently enabled, if and only if the
     * bookmark bar feature is enabled and a non-null profile is supplied. Otherwise no-ops.
     *
     * @param context The context in which feature enablement should be assessed.
     * @param profileProviderSupplier The supplier of the profile for which to toggle the setting.
     */
    public static void toggleSettingEnabled(
            @NonNull Context context, @Nullable Supplier<ProfileProvider> profileProviderSupplier) {
        if (profileProviderSupplier == null || !isFeatureEnabled(context)) return;

        final var profileProvider = profileProviderSupplier.get();
        if (profileProvider == null) return;

        final var profile = profileProvider.getOriginalProfile();
        if (profile != null) toggleSettingEnabled(profile);
    }

    /**
     * Toggles whether the bookmark bar user setting is currently enabled.
     *
     * @param profile The profile for which the user setting should be toggled.
     */
    public static void toggleSettingEnabled(@NonNull Profile profile) {
        final var prefService = getPrefService(profile);
        prefService.setBoolean(
                Pref.SHOW_BOOKMARK_BAR, !prefService.getBoolean(Pref.SHOW_BOOKMARK_BAR));
    }

    /**
     * Registers an observer to be notified of changes to the bookmark bar user setting.
     *
     * @param registrar The registrar with which to register the observer.
     * @param observer The observer to be notified of changes.
     */
    public static void addSettingObserver(
            @NonNull PrefChangeRegistrar registrar, @NonNull PrefObserver observer) {
        registrar.addObserver(Pref.SHOW_BOOKMARK_BAR, observer);
        if (sSettingObserverCacheForTesting != null) {
            sSettingObserverCacheForTesting.add(observer);
        }
    }

    /**
     * Removes all observers from being notified of changes to the bookmark bar user setting.
     *
     * @param registrar The registrar from which to unregistar all observers.
     */
    public static void removeSettingObservers(@NonNull PrefChangeRegistrar registrar) {
        registrar.removeObserver(Pref.SHOW_BOOKMARK_BAR);
        if (sSettingObserverCacheForTesting != null) {
            sSettingObserverCacheForTesting.clear();
        }
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
    static @NonNull ListItem createListItemFor(
            @NonNull BiConsumer<BookmarkItem, Integer> clickCallback,
            @NonNull Context context,
            @NonNull BookmarkImageFetcher imageFetcher,
            @NonNull BookmarkItem item) {
        return new ListItem(
                ViewType.ITEM,
                new PropertyModel.Builder(BookmarkBarButtonProperties.ALL_KEYS)
                        .with(
                                BookmarkBarButtonProperties.CLICK_CALLBACK,
                                (metaState) -> clickCallback.accept(item, metaState))
                        .with(
                                BookmarkBarButtonProperties.ICON_SUPPLIER,
                                createIconSupplierFor(context, imageFetcher, item))
                        .with(
                                BookmarkBarButtonProperties.ICON_TINT_LIST_ID,
                                item.isFolder()
                                        ? R.color.default_icon_color_tint_list
                                        : Resources.ID_NULL)
                        .with(BookmarkBarButtonProperties.TITLE, item.getTitle())
                        .build());
    }

    private static @NonNull LazyOneshotSupplier<Drawable> createIconSupplierFor(
            @NonNull Context context,
            @NonNull BookmarkImageFetcher imageFetcher,
            @NonNull BookmarkItem item) {
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

    private static @NonNull PrefService getPrefService(@NonNull Profile profile) {
        return UserPrefs.get(profile.getOriginalProfile());
    }

    /**
     * Sets whether the bookmark bar feature is forcibly enabled/disabled for testing.
     *
     * @param enabled Whether the feature is forcibly enabled/disabled.
     */
    public static void setFeatureEnabledForTesting(@Nullable Boolean enabled) {
        sFeatureEnabledForTesting = enabled;
        ResettersForTesting.register(() -> sFeatureEnabledForTesting = null);
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
