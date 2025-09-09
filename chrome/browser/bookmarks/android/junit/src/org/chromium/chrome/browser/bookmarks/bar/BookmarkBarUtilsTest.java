// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks.bar;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.any;
import static org.mockito.Mockito.anyBoolean;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.eq;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.content.Context;
import android.content.res.ColorStateList;
import android.graphics.drawable.Drawable;
import android.view.KeyEvent;
import android.view.LayoutInflater;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.appcompat.content.res.AppCompatResources;
import androidx.test.ext.junit.rules.ActivityScenarioRule;
import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.mockito.stubbing.Answer;
import org.robolectric.Robolectric;
import org.robolectric.RuntimeEnvironment;
import org.robolectric.Shadows;
import org.robolectric.annotation.Config;
import org.robolectric.shadows.ShadowDrawable;

import org.chromium.base.Callback;
import org.chromium.base.FeatureOverrides;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.bookmarks.BookmarkImageFetcher;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileProvider;
import org.chromium.chrome.test.OverrideContextWrapperTestRule;
import org.chromium.components.bookmarks.BookmarkItem;
import org.chromium.components.prefs.PrefService;
import org.chromium.components.user_prefs.UserPrefsJni;
import org.chromium.content_public.browser.test.util.TouchCommon;
import org.chromium.ui.base.DeviceFormFactor;
import org.chromium.ui.base.TestActivity;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

import java.util.Objects;
import java.util.concurrent.atomic.AtomicBoolean;
import java.util.function.BiConsumer;

/** Unit tests for {@link BookmarkBarUtils}. */
@RunWith(BaseRobolectricTestRunner.class)
@EnableFeatures(ChromeFeatureList.ANDROID_BOOKMARK_BAR)
public class BookmarkBarUtilsTest {

    private static final String PHONE_QUALIFIER =
            "sw" + (DeviceFormFactor.MINIMUM_TABLET_WIDTH_DP - 1) + "dp";
    private static final String TABLET_QUALIFIER =
            "sw" + DeviceFormFactor.MINIMUM_TABLET_WIDTH_DP + "dp";

    @Rule
    public final ActivityScenarioRule<TestActivity> mActivityScenarioRule =
            new ActivityScenarioRule<>(TestActivity.class);

    @Rule
    public OverrideContextWrapperTestRule mOverrideContextRule =
            new OverrideContextWrapperTestRule();

    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private BiConsumer<BookmarkItem, Integer> mClickCallback;
    @Mock private Drawable mFavicon;
    @Mock private BookmarkImageFetcher mImageFetcher;
    @Mock private BookmarkItem mItem;
    @Mock private PrefService mPrefService;
    @Mock private Profile mProfile;
    @Mock private ProfileProvider mProfileProvider;
    @Mock private UserPrefsJni mUserPrefsJni;

    private final AtomicBoolean mSetting = new AtomicBoolean();

    private ObservableSupplierImpl<ProfileProvider> mProfileProviderSupplier;

    @Before
    public void setUp() {
        doAnswer(runCallbackWithValueAtIndex(mSetting::set, 1))
                .when(mPrefService)
                .setBoolean(eq(Pref.SHOW_BOOKMARK_BAR), anyBoolean());

        when(mPrefService.getBoolean(Pref.SHOW_BOOKMARK_BAR)).thenAnswer(i -> mSetting.get());
        when(mProfile.getOriginalProfile()).thenReturn(mProfile);
        when(mProfileProvider.getOriginalProfile()).thenReturn(mProfile);
        when(mUserPrefsJni.get(mProfile)).thenReturn(mPrefService);

        UserPrefsJni.setInstanceForTesting(mUserPrefsJni);

        mProfileProviderSupplier = new ObservableSupplierImpl<>(mProfileProvider);

        // Explicitly override FeatureParam for consistency.
        FeatureOverrides.Builder overrides = FeatureOverrides.newBuilder();
        overrides =
                overrides.param(ChromeFeatureList.ANDROID_BOOKMARK_BAR, "show_bookmark_bar", true);
        overrides.apply();
    }

    @After
    public void tearDown() {
        UserPrefsJni.setInstanceForTesting(null);
        mOverrideContextRule.setIsDesktop(false);
    }

    @Test
    @SmallTest
    public void testIsActivityStateBookmarkBarCompatible() {
        mActivityScenarioRule
                .getScenario()
                .onActivity(
                        activity -> {
                            // Case: Below "w412dp" threshold w/ feature disabled.
                            RuntimeEnvironment.setQualifiers("w411dp");
                            BookmarkBarUtils.setDeviceBookmarkBarCompatibleForTesting(false);
                            assertFalse(
                                    BookmarkBarUtils.isActivityStateBookmarkBarCompatible(
                                            activity));

                            // Case: Below "w412dp" threshold w/ feature enabled.
                            BookmarkBarUtils.setDeviceBookmarkBarCompatibleForTesting(true);
                            assertFalse(
                                    BookmarkBarUtils.isActivityStateBookmarkBarCompatible(
                                            activity));

                            // Case: At "w412dp" threshold w/ feature disabled.
                            RuntimeEnvironment.setQualifiers("w412dp");
                            BookmarkBarUtils.setDeviceBookmarkBarCompatibleForTesting(false);
                            assertFalse(
                                    BookmarkBarUtils.isActivityStateBookmarkBarCompatible(
                                            activity));

                            // Case: At "w412dp" threshold w/ feature enabled.
                            BookmarkBarUtils.setDeviceBookmarkBarCompatibleForTesting(true);
                            assertTrue(
                                    BookmarkBarUtils.isActivityStateBookmarkBarCompatible(
                                            activity));
                        });
    }

    @Test
    @SmallTest
    @Config(qualifiers = PHONE_QUALIFIER)
    @DisableFeatures(ChromeFeatureList.ANDROID_BOOKMARK_BAR)
    public void testIsFeatureEnabledWhenFlagIsDisabledOnPhone() {
        mActivityScenarioRule
                .getScenario()
                .onActivity(
                        activity ->
                                assertFalse(
                                        BookmarkBarUtils.isDeviceBookmarkBarCompatible(activity)));
    }

    @Test
    @SmallTest
    @Config(qualifiers = PHONE_QUALIFIER)
    public void testIsFeatureEnabledWhenFlagIsEnabledOnPhone() {
        mActivityScenarioRule
                .getScenario()
                .onActivity(
                        activity ->
                                assertFalse(
                                        BookmarkBarUtils.isDeviceBookmarkBarCompatible(activity)));
    }

    @Test
    @SmallTest
    @Config(qualifiers = TABLET_QUALIFIER)
    @DisableFeatures(ChromeFeatureList.ANDROID_BOOKMARK_BAR)
    public void testIsFeatureEnabledWhenFlagIsDisabledOnTablet() {
        mActivityScenarioRule
                .getScenario()
                .onActivity(
                        activity ->
                                assertFalse(
                                        BookmarkBarUtils.isDeviceBookmarkBarCompatible(activity)));
    }

    @Test
    @SmallTest
    @Config(qualifiers = TABLET_QUALIFIER)
    public void testIsFeatureEnabledWhenFlagIsEnabledOnTablet() {
        mActivityScenarioRule
                .getScenario()
                .onActivity(
                        activity ->
                                assertTrue(
                                        BookmarkBarUtils.isDeviceBookmarkBarCompatible(activity)));
    }

    @Test
    @SmallTest
    public void testIsBookmarkBarVisible_Desktop() {
        mOverrideContextRule.setIsDesktop(true);
        mActivityScenarioRule
                .getScenario()
                .onActivity(
                        activity -> {
                            // Case: feature disallowed and setting disabled.
                            BookmarkBarUtils.setActivityStateBookmarkBarCompatibleForTesting(false);
                            BookmarkBarUtils.setSettingEnabledForTesting(false);
                            assertFalse(BookmarkBarUtils.isBookmarkBarVisible(activity, mProfile));

                            // Case: feature disallowed and setting enabled.
                            BookmarkBarUtils.setSettingEnabledForTesting(true);
                            assertFalse(BookmarkBarUtils.isBookmarkBarVisible(activity, mProfile));

                            // Case: feature allowed and setting disabled.
                            BookmarkBarUtils.setActivityStateBookmarkBarCompatibleForTesting(true);
                            BookmarkBarUtils.setSettingEnabledForTesting(false);
                            assertFalse(BookmarkBarUtils.isBookmarkBarVisible(activity, mProfile));

                            // Case feature allowed and setting enabled.
                            BookmarkBarUtils.setSettingEnabledForTesting(true);
                            assertTrue(BookmarkBarUtils.isBookmarkBarVisible(activity, mProfile));
                        });
    }

    @Test
    @SmallTest
    public void testIsBookmarkBarVisible_Tablet() {
        mOverrideContextRule.setIsDesktop(false);
        mActivityScenarioRule
                .getScenario()
                .onActivity(
                        activity -> {
                            // Case: feature disallowed.
                            BookmarkBarUtils.setActivityStateBookmarkBarCompatibleForTesting(false);
                            assertFalse(BookmarkBarUtils.isBookmarkBarVisible(activity, mProfile));

                            // Case: feature allowed no device pref (FeatureParam = true).
                            BookmarkBarUtils.setActivityStateBookmarkBarCompatibleForTesting(true);
                            assertTrue(BookmarkBarUtils.isBookmarkBarVisible(activity, mProfile));

                            // Apply new FeatureParam override.
                            FeatureOverrides.Builder overrides = FeatureOverrides.newBuilder();
                            overrides =
                                    overrides.param(
                                            ChromeFeatureList.ANDROID_BOOKMARK_BAR,
                                            "show_bookmark_bar",
                                            false);
                            overrides.apply();

                            // Case: feature allowed no device pref (FeatureParam = false).
                            assertFalse(BookmarkBarUtils.isBookmarkBarVisible(activity, mProfile));

                            // Case: feature allowed explicit device pref
                            BookmarkBarUtils.setDevicePrefShowBookmarksBar(
                                    true, /* fromKeyboardShortcut= */ false);
                            assertTrue(BookmarkBarUtils.isBookmarkBarVisible(activity, mProfile));
                        });
    }

    // Test UserPrefs - only on Desktop

    @Test
    @SmallTest
    public void testIsUserPrefsShowBookmarksBarEnabled() {
        mOverrideContextRule.setIsDesktop(true);

        mSetting.set(false);
        assertFalse(BookmarkBarUtils.isUserPrefsShowBookmarksBarEnabled(mProfile));
        assertFalse(BookmarkBarUtils.isUserPrefsShowBookmarksBarEnabled(null));

        mSetting.set(true);
        assertTrue(BookmarkBarUtils.isUserPrefsShowBookmarksBarEnabled(mProfile));
        assertFalse(BookmarkBarUtils.isUserPrefsShowBookmarksBarEnabled(null));
    }

    @Test
    @SmallTest
    public void testSetUserPrefsShowBookmarksBar() {
        mOverrideContextRule.setIsDesktop(true);

        mSetting.set(false);
        assertFalse(BookmarkBarUtils.isUserPrefsShowBookmarksBarEnabled(mProfile));

        var histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectBooleanRecordTimes(
                                BookmarkBarUtils.TOGGLED_BY_KEYBOARD_SHORTCUT, true, 1)
                        .expectNoRecords(BookmarkBarUtils.TOGGLED_IN_SETTINGS)
                        .build();

        BookmarkBarUtils.setUserPrefsShowBookmarksBar(
                mProfile, true, /* fromKeyboardShortcut= */ true);
        assertTrue(BookmarkBarUtils.isUserPrefsShowBookmarksBarEnabled(mProfile));

        histogramWatcher.assertExpected();

        var histogramWatcher2 =
                HistogramWatcher.newBuilder()
                        .expectBooleanRecordTimes(BookmarkBarUtils.TOGGLED_IN_SETTINGS, false, 1)
                        .expectNoRecords(BookmarkBarUtils.TOGGLED_BY_KEYBOARD_SHORTCUT)
                        .build();

        BookmarkBarUtils.setUserPrefsShowBookmarksBar(
                mProfile, false, /* fromKeyboardShortcut= */ false);
        assertFalse(BookmarkBarUtils.isUserPrefsShowBookmarksBarEnabled(mProfile));

        histogramWatcher2.assertExpected();
    }

    @Test
    @SmallTest
    public void testToggleUserPrefsShowBookmarksBar() {
        mOverrideContextRule.setIsDesktop(true);

        mSetting.set(false);
        assertFalse(BookmarkBarUtils.isUserPrefsShowBookmarksBarEnabled(mProfile));

        BookmarkBarUtils.toggleUserPrefsShowBookmarksBar(mProfile, true);
        assertTrue(BookmarkBarUtils.isUserPrefsShowBookmarksBarEnabled(mProfile));

        BookmarkBarUtils.toggleUserPrefsShowBookmarksBar(mProfile, false);
        assertFalse(BookmarkBarUtils.isUserPrefsShowBookmarksBarEnabled(mProfile));
    }

    // Test device prefs - only on Tablet

    @Test
    @SmallTest
    public void testIsDevicePrefShowBookmarksBarEnabled() {
        mOverrideContextRule.setIsDesktop(false);

        // User should not have set any preference yet.
        assertFalse(BookmarkBarUtils.hasUserSetDevicePrefShowBookmarksBar());

        // Even though user has not set a device preference, the FeatureParam will make it true.
        assertTrue(BookmarkBarUtils.isDevicePrefShowBookmarksBarEnabled());

        // Apply new FeatureParam override.
        FeatureOverrides.Builder overrides = FeatureOverrides.newBuilder();
        overrides =
                overrides.param(ChromeFeatureList.ANDROID_BOOKMARK_BAR, "show_bookmark_bar", false);
        overrides.apply();

        assertFalse(BookmarkBarUtils.isDevicePrefShowBookmarksBarEnabled());
    }

    @Test
    @SmallTest
    public void testSetDevicePrefShowBookmarksBar() {
        mOverrideContextRule.setIsDesktop(false);
        // User should not have set any preference yet.
        assertFalse(BookmarkBarUtils.hasUserSetDevicePrefShowBookmarksBar());

        // Even though user has not set a device preference, the FeatureParam will make it true.
        assertTrue(BookmarkBarUtils.isDevicePrefShowBookmarksBarEnabled());

        var histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectBooleanRecordTimes(
                                BookmarkBarUtils.TOGGLED_BY_KEYBOARD_SHORTCUT, true, 1)
                        .expectNoRecords(BookmarkBarUtils.TOGGLED_IN_SETTINGS)
                        .build();

        BookmarkBarUtils.setDevicePrefShowBookmarksBar(true, /* fromKeyboardShortcut= */ true);
        assertTrue(BookmarkBarUtils.isDevicePrefShowBookmarksBarEnabled());
        assertTrue(BookmarkBarUtils.hasUserSetDevicePrefShowBookmarksBar());

        histogramWatcher.assertExpected();

        var histogramWatcher2 =
                HistogramWatcher.newBuilder()
                        .expectBooleanRecordTimes(BookmarkBarUtils.TOGGLED_IN_SETTINGS, false, 1)
                        .expectNoRecords(BookmarkBarUtils.TOGGLED_BY_KEYBOARD_SHORTCUT)
                        .build();

        BookmarkBarUtils.setDevicePrefShowBookmarksBar(false, /* fromKeyboardShortcut= */ false);
        assertFalse(BookmarkBarUtils.isDevicePrefShowBookmarksBarEnabled());
        assertTrue(BookmarkBarUtils.hasUserSetDevicePrefShowBookmarksBar());

        histogramWatcher2.assertExpected();
    }

    @Test
    @SmallTest
    public void testToggleDevicePrefShowBookmarksBar() {
        mOverrideContextRule.setIsDesktop(false);

        // User should not have set any preference yet.
        assertFalse(BookmarkBarUtils.hasUserSetDevicePrefShowBookmarksBar());

        // Even though user has not set a device preference, the FeatureParam will make it true.
        assertTrue(BookmarkBarUtils.isDevicePrefShowBookmarksBarEnabled());

        BookmarkBarUtils.toggleDevicePrefShowBookmarksBar(true);
        assertFalse(BookmarkBarUtils.isDevicePrefShowBookmarksBarEnabled());
        assertTrue(BookmarkBarUtils.hasUserSetDevicePrefShowBookmarksBar());

        BookmarkBarUtils.toggleDevicePrefShowBookmarksBar(false);
        assertTrue(BookmarkBarUtils.isDevicePrefShowBookmarksBarEnabled());
        assertTrue(BookmarkBarUtils.hasUserSetDevicePrefShowBookmarksBar());
    }

    @Test
    @SmallTest
    public void testCreateListItem() {
        testCreateListItem(/* isFolder= */ false);
    }

    @Test
    @SmallTest
    @Config(shadows = {ShadowDrawable.class})
    public void testCreateListItemForFolder() {
        testCreateListItem(/* isFolder= */ true);
    }

    private void testCreateListItem(boolean isFolder) {
        mActivityScenarioRule
                .getScenario()
                .onActivity(
                        activity -> {
                            // Set up mocks.
                            final var title = "Title";
                            when(mItem.getTitle()).thenReturn(title);
                            when(mItem.isFolder()).thenReturn(isFolder);
                            doAnswer(runCallbackAtIndexWithValue(1, mFavicon))
                                    .when(mImageFetcher)
                                    .fetchFaviconForBookmark(eq(mItem), any());

                            // Create list item.
                            final var listItem =
                                    BookmarkBarUtils.createListItemFor(
                                            mClickCallback, activity, mImageFetcher, mItem);

                            // Verify expected type.
                            assertEquals(BookmarkBarUtils.ViewType.ITEM, listItem.type);

                            // Bind list item to view.
                            final var view = inflateBookmarkBarButton(activity);
                            activity.setContentView(view);
                            Robolectric.flushForegroundThreadScheduler();
                            PropertyModelChangeProcessor.create(
                                    listItem.model, view, BookmarkBarButtonViewBinder::bind);

                            // Verify expected properties.
                            assertIcon(view, isFolder);
                            assertIconTintColorList(view, isFolder);
                            assertEquals(title, view.getTitleForTesting());

                            // Verify expected event propagation.
                            verify(mClickCallback, never()).accept(any(), any());
                            TouchCommon.singleClickView(view, KeyEvent.META_CTRL_ON);
                            Robolectric.flushForegroundThreadScheduler();
                            verify(mClickCallback).accept(mItem, KeyEvent.META_CTRL_ON);
                        });
    }

    private void assertIcon(@NonNull BookmarkBarButton view, boolean isFolder) {
        if (isFolder) {
            assertEquals(
                    R.drawable.ic_folder_outline_24dp,
                    Shadows.shadowOf(view.getIconForTesting()).getCreatedFromResId());
        } else {
            assertEquals(mFavicon, view.getIconForTesting());
        }
    }

    private void assertIconTintColorList(@NonNull BookmarkBarButton view, boolean isFolder) {
        ColorStateList expectedIconTintList = null;
        if (isFolder) {
            expectedIconTintList =
                    AppCompatResources.getColorStateList(
                            view.getContext(), R.color.default_icon_color_tint_list);
        }
        // NOTE: Reference equivalence may occasionally fail resulting in test flakiness. Instead,
        // compare string representations which should be sufficient to ensure equivalence.
        final ColorStateList actualIconTintList = view.getIconTintListForTesting();
        assertEquals(Objects.toString(expectedIconTintList), Objects.toString(actualIconTintList));
    }

    private @NonNull BookmarkBarButton inflateBookmarkBarButton(@NonNull Context context) {
        return (BookmarkBarButton)
                LayoutInflater.from(context).inflate(R.layout.bookmark_bar_button, null);
    }

    private @NonNull <T> Answer<Void> runCallbackWithValueAtIndex(
            @NonNull Callback<T> callback, int index) {
        return invocation -> {
            final T value = invocation.getArgument(index);
            callback.onResult(value);
            return null;
        };
    }

    private @NonNull <T> Answer<Void> runCallbackAtIndexWithValue(int index, @Nullable T value) {
        return invocation -> {
            final Callback<T> callback = invocation.getArgument(index);
            callback.onResult(value);
            return null;
        };
    }
}
