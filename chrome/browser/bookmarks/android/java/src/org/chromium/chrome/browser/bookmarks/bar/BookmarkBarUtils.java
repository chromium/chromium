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

import org.chromium.base.supplier.LazyOneshotSupplier;
import org.chromium.base.supplier.LazyOneshotSupplierImpl;
import org.chromium.chrome.browser.bookmarks.BookmarkImageFetcher;
import org.chromium.chrome.browser.bookmarks.R;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.profiles.Profile;
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
import java.util.function.BiConsumer;

/** Utilities for the bookmark bar which provides users with bookmark access from top chrome. */
public class BookmarkBarUtils {

    /** Enumeration of view type identifiers for views which are rendered in the bookmark bar. */
    @IntDef({ViewType.ITEM})
    @Retention(RetentionPolicy.SOURCE)
    static @interface ViewType {
        int ITEM = 1;
    }

    private BookmarkBarUtils() {}

    /**
     * Returns whether the bookmark bar feature is currently enabled. The feature is considered to
     * be enabled when its associated feature flag is enabled and device form factor restrictions
     * are satisfied.
     *
     * @param context the context in which feature enablement should be assessed.
     * @return whether the feature is currently enabled.
     */
    public static boolean isFeatureEnabled(@NonNull Context context) {
        return ChromeFeatureList.sAndroidBookmarkBar.isEnabled()
                && DeviceFormFactor.isNonMultiDisplayContextOnTablet(context);
    }

    /**
     * Returns whether the bookmark bar user setting is currently enabled.
     *
     * @param profile the profile for which the user setting should be assessed.
     * @return whether the user setting is currently enabled.
     */
    public static boolean isSettingEnabled(@Nullable Profile profile) {
        return profile != null ? getPrefService(profile).getBoolean(Pref.SHOW_BOOKMARK_BAR) : false;
    }

    /**
     * Sets whether the bookmark bar user setting is currently enabled.
     *
     * @param profile the profile for which the user setting should be set.
     * @param enabled whether the user setting should be set to enabled/disabled.
     */
    public static void setSettingEnabled(@NonNull Profile profile, boolean enabled) {
        getPrefService(profile).setBoolean(Pref.SHOW_BOOKMARK_BAR, enabled);
    }

    /**
     * Toggles whether the bookmark bar user setting is currently enabled.
     *
     * @param profile the profile for which the user setting should be toggled.
     */
    public static void toggleSettingEnabled(@NonNull Profile profile) {
        final var prefService = getPrefService(profile);
        prefService.setBoolean(
                Pref.SHOW_BOOKMARK_BAR, !prefService.getBoolean(Pref.SHOW_BOOKMARK_BAR));
    }

    /**
     * Registers an observer to be notified of changes to the bookmark bar user setting.
     *
     * @param registrar the registrar with which to register the observer.
     * @param observer the observer to be notified of changes.
     */
    public static void addSettingObserver(
            @NonNull PrefChangeRegistrar registrar, @NonNull PrefObserver observer) {
        registrar.addObserver(Pref.SHOW_BOOKMARK_BAR, observer);
    }

    /**
     * Removes all observers from being notified of changes to the bookmark bar user setting.
     *
     * @param registrar the registrar from which to unregistar all observers.
     */
    public static void removeSettingObservers(@NonNull PrefChangeRegistrar registrar) {
        registrar.removeObserver(Pref.SHOW_BOOKMARK_BAR);
    }

    /**
     * Creates a list item to render in the bookmark bar for the specified bookmark item.
     *
     * @param clickCallback the callback to invoke on list item click events.
     * @param context the context in which the created list item will be rendered.
     * @param imageFetcher the image fetcher to use for rendering favicons.
     * @param item the bookmark item for which to create a renderable list item.
     * @return the created list item to render in the bookmark bar.
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
}
