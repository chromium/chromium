// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks.bar;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.reset;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.app.Activity;
import android.view.View;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Robolectric;

import org.chromium.base.ContextUtils;
import org.chromium.base.supplier.ObservableSuppliers;
import org.chromium.base.supplier.SettableNonNullObservableSupplier;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.RobolectricUtil;
import org.chromium.base.test.util.Features;
import org.chromium.chrome.browser.bookmarks.BookmarkModel;
import org.chromium.chrome.browser.bookmarks.R;
import org.chromium.chrome.browser.bookmarks.bar.BookmarkBarUtils.BookmarkBarSettingChangeOrigin;
import org.chromium.chrome.browser.feature_engagement.TrackerFactory;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.ui.appmenu.AppMenuHandler;
import org.chromium.chrome.browser.user_education.IphCommand;
import org.chromium.chrome.browser.user_education.UserEducationHelper;
import org.chromium.components.bookmarks.BookmarkBarVisibilityState;
import org.chromium.components.bookmarks.BookmarkId;
import org.chromium.components.bookmarks.BookmarkItem;
import org.chromium.components.feature_engagement.FeatureConstants;
import org.chromium.components.feature_engagement.Tracker;
import org.chromium.components.prefs.PrefService;
import org.chromium.components.user_prefs.UserPrefs;
import org.chromium.components.user_prefs.UserPrefsJni;

import java.util.ArrayList;
import java.util.Collections;
import java.util.List;

/** Unit tests for {@link BookmarkBarIphController}. */
@RunWith(BaseRobolectricTestRunner.class)
@Features.DisableFeatures(ChromeFeatureList.BOOKMARKS_BAR_NTP)
public class BookmarkBarIphControllerTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private AppMenuHandler mAppMenuHandler;
    @Mock private BookmarkModel mBookmarkModel;
    @Mock private Profile mProfile;
    @Mock private Tracker mTracker;
    @Mock private UserEducationHelper mUserEducationHelper;
    @Mock private BookmarkId mDesktopFolderId;
    @Mock private BookmarkId mAccountDesktopFolderId;
    @Mock private BookmarkItem mDesktopFolderItem;
    @Mock private BookmarkId mChildBookmarkId;
    @Mock private BookmarkItem mChildBookmarkItem;
    @Mock private UserPrefs.Natives mUserPrefsJni;
    @Mock private PrefService mPrefService;

    @Captor private ArgumentCaptor<IphCommand> mIphCommandCaptor;

    private View mToolbarMenuButton;
    private BookmarkBarIphController mController;
    private final SettableNonNullObservableSupplier<Boolean> mXrSpaceModeSupplier =
            ObservableSuppliers.createNonNull(false);

    @Before
    public void setUp() {
        ContextUtils.getAppSharedPreferences().edit().clear().apply();
        BookmarkBarUtils.setActivityStateBookmarkBarCompatibleForTesting(true);

        UserPrefsJni.setInstanceForTesting(mUserPrefsJni);
        when(mUserPrefsJni.get(mProfile)).thenReturn(mPrefService);
        when(mProfile.getOriginalProfile()).thenReturn(mProfile);

        // Set the default behavior for policy checks (no policy active).
        when(mPrefService.isManagedPreference(Pref.SHOW_BOOKMARK_BAR)).thenReturn(false);
        when(mPrefService.hasRecommendation(Pref.SHOW_BOOKMARK_BAR)).thenReturn(false);
        when(mPrefService.isRecommendedPreference(Pref.SHOW_BOOKMARK_BAR)).thenReturn(false);
        when(mPrefService.isManagedPreference(Pref.BOOKMARK_BAR_VISIBILITY_STATE))
                .thenReturn(false);
        when(mPrefService.hasRecommendation(Pref.BOOKMARK_BAR_VISIBILITY_STATE)).thenReturn(false);

        // Set the default behavior for reading the pref value.
        when(mPrefService.getBoolean(Pref.SHOW_BOOKMARK_BAR)).thenReturn(true);
        when(mPrefService.getInteger(Pref.BOOKMARK_BAR_VISIBILITY_STATE))
                .thenReturn(BookmarkBarVisibilityState.ALWAYS_HIDE);

        // Attach the button to a window so that View#post() runs on the main looper.
        Activity activity = Robolectric.buildActivity(Activity.class).setup().get();
        mToolbarMenuButton = new View(activity);
        activity.setContentView(mToolbarMenuButton);

        TrackerFactory.setTrackerForTests(mTracker);
        when(mTracker.wouldTriggerHelpUi(FeatureConstants.BOOKMARK_BAR_VISIBILITY_FEATURE))
                .thenReturn(true);

        mController =
                new BookmarkBarIphController(
                        mProfile,
                        mAppMenuHandler,
                        mToolbarMenuButton,
                        mBookmarkModel,
                        mUserEducationHelper,
                        mXrSpaceModeSupplier);
    }

    @Test
    public void testShowIph() {
        mController.showIph();
        RobolectricUtil.runAllBackgroundAndUi();
        verifyIphCommand();
    }

    @Test
    @Features.EnableFeatures(ChromeFeatureList.BOOKMARKS_BAR_NTP)
    public void testShowIph_TriState() {
        mController.showIph();
        RobolectricUtil.runAllBackgroundAndUi();
        verifyIphCommand();
    }

    /**
     * Tests Trigger 1: IPH shows on startup if at least one bookmark already exists in the local
     * bookmarks bar.
     */
    @Test
    public void testTrigger1_OnModelLoaded_WithLocalBookmark() {
        List<BookmarkId> children = new ArrayList<>();
        children.add(mChildBookmarkId);
        when(mBookmarkModel.getDesktopFolderId()).thenReturn(mDesktopFolderId);
        when(mBookmarkModel.getChildIds(mDesktopFolderId)).thenReturn(children);
        when(mBookmarkModel.getBookmarkById(mChildBookmarkId)).thenReturn(mChildBookmarkItem);
        when(mChildBookmarkItem.isFolder()).thenReturn(false);

        mController.bookmarkModelLoaded();
        RobolectricUtil.runAllBackgroundAndUi();
        verifyIphCommand();
    }

    /**
     * Tests Trigger 1: IPH shows on startup if at least one bookmark already exists in the account
     * bookmarks bar.
     */
    @Test
    public void testTrigger1_OnModelLoaded_WithAccountBookmark() {
        List<BookmarkId> children = new ArrayList<>();
        children.add(mChildBookmarkId);
        when(mBookmarkModel.getDesktopFolderId()).thenReturn(mDesktopFolderId);
        when(mBookmarkModel.getChildIds(mDesktopFolderId)).thenReturn(Collections.emptyList());
        when(mBookmarkModel.getAccountDesktopFolderId()).thenReturn(mAccountDesktopFolderId);
        when(mBookmarkModel.getChildIds(mAccountDesktopFolderId)).thenReturn(children);
        when(mBookmarkModel.getBookmarkById(mChildBookmarkId)).thenReturn(mChildBookmarkItem);
        when(mChildBookmarkItem.isFolder()).thenReturn(false);

        mController.bookmarkModelLoaded();
        RobolectricUtil.runAllBackgroundAndUi();
        verifyIphCommand();
    }

    /**
     * Tests Trigger 1: IPH does not show on startup if the bookmarks bar only contains folders (no
     * non-folder bookmarks).
     */
    @Test
    public void testTrigger1_OnModelLoaded_OnlyFolders() {
        List<BookmarkId> children = new ArrayList<>();
        children.add(mChildBookmarkId);
        when(mBookmarkModel.getDesktopFolderId()).thenReturn(mDesktopFolderId);
        when(mBookmarkModel.getChildIds(mDesktopFolderId)).thenReturn(children);
        when(mBookmarkModel.getBookmarkById(mChildBookmarkId)).thenReturn(mChildBookmarkItem);
        when(mChildBookmarkItem.isFolder()).thenReturn(true);

        mController.bookmarkModelLoaded();
        RobolectricUtil.runAllBackgroundAndUi();
        verify(mUserEducationHelper, never()).requestShowIph(any());
    }

    /** Tests Trigger 2: IPH shows when a new bookmark is added on the current device. */
    @Test
    public void testTrigger2_OnBookmarkNodeAdded() {
        when(mDesktopFolderItem.getId()).thenReturn(mDesktopFolderId);
        List<BookmarkId> children = new ArrayList<>();
        children.add(mChildBookmarkId);
        when(mBookmarkModel.getChildIds(any())).thenReturn(children);
        when(mBookmarkModel.getBookmarkById(mChildBookmarkId)).thenReturn(mChildBookmarkItem);
        when(mChildBookmarkItem.isFolder()).thenReturn(false);

        mController.bookmarkNodeAdded(mDesktopFolderItem, 0, /* addedByUser= */ true);
        RobolectricUtil.runAllBackgroundAndUi();
        verifyIphCommand();
    }

    /**
     * Tests that Trigger 2 is ignored if the bookmark addition came from sync (addedByUser is
     * false).
     */
    @Test
    public void testTrigger2_OnBookmarkNodeAdded_FromSync() {
        when(mDesktopFolderItem.getId()).thenReturn(mDesktopFolderId);
        List<BookmarkId> children = new ArrayList<>();
        children.add(mChildBookmarkId);
        when(mBookmarkModel.getChildIds(any())).thenReturn(children);
        when(mBookmarkModel.getBookmarkById(mChildBookmarkId)).thenReturn(mChildBookmarkItem);
        when(mChildBookmarkItem.isFolder()).thenReturn(false);

        mController.bookmarkNodeAdded(mDesktopFolderItem, 0, /* addedByUser= */ false);
        RobolectricUtil.runAllBackgroundAndUi();
        verify(mUserEducationHelper, never()).requestShowIph(any());
    }

    /** Tests that Trigger 2 is ignored when a folder (rather than a bookmark) is added. */
    @Test
    public void testTrigger2_OnBookmarkNodeAdded_FolderIgnored() {
        when(mDesktopFolderItem.getId()).thenReturn(mDesktopFolderId);
        List<BookmarkId> children = new ArrayList<>();
        children.add(mChildBookmarkId);
        when(mBookmarkModel.getChildIds(any())).thenReturn(children);
        when(mBookmarkModel.getBookmarkById(mChildBookmarkId)).thenReturn(mChildBookmarkItem);
        when(mChildBookmarkItem.isFolder()).thenReturn(true);

        mController.bookmarkNodeAdded(mDesktopFolderItem, 0, /* addedByUser= */ true);
        RobolectricUtil.runAllBackgroundAndUi();
        verify(mUserEducationHelper, never()).requestShowIph(any());
    }

    @Test
    @Features.EnableFeatures(ChromeFeatureList.BOOKMARKS_BAR_NTP)
    public void testDoesNotShowIphIfDevicePrefSet_TriState() {
        BookmarkBarUtils.setDevicePrefBookmarkBarVisibilityState(
                BookmarkBarVisibilityState.ALWAYS_HIDE,
                BookmarkBarSettingChangeOrigin.APPEARANCE_SETTINGS);

        mController.showIph();
        RobolectricUtil.runAllBackgroundAndUi();
        verify(mUserEducationHelper, never()).requestShowIph(any());
    }

    @Test
    @Features.EnableFeatures(ChromeFeatureList.BOOKMARKS_BAR_NTP)
    public void testDoesNotShowIphIfNotAlwaysHide_TriState() {
        when(mPrefService.isManagedPreference(Pref.BOOKMARK_BAR_VISIBILITY_STATE)).thenReturn(true);
        when(mPrefService.getInteger(Pref.BOOKMARK_BAR_VISIBILITY_STATE))
                .thenReturn(BookmarkBarVisibilityState.ONLY_SHOW_ON_NTP);

        mController.showIph();
        RobolectricUtil.runAllBackgroundAndUi();
        verify(mUserEducationHelper, never()).requestShowIph(any());
    }

    /**
     * Tests that the controller does not call finishLoadingBookmarkModel if wouldTriggerHelpUi is
     * false.
     */
    @Test
    public void testDoesNotLoadModelIfIphWillNotTrigger() {
        // Reset the mocks that were already used in the @Before setup.
        reset(mTracker);
        reset(mBookmarkModel);

        when(mTracker.wouldTriggerHelpUi(FeatureConstants.BOOKMARK_BAR_VISIBILITY_FEATURE))
                .thenReturn(false);

        // Call the constructor again.
        new BookmarkBarIphController(
                mProfile,
                mAppMenuHandler,
                mToolbarMenuButton,
                mBookmarkModel,
                mUserEducationHelper,
                mXrSpaceModeSupplier);

        // Verify that #finishLoadingBookmarkModel was never called.
        verify(mBookmarkModel, never()).finishLoadingBookmarkModel(any());
    }

    @Test
    public void testDoesNotLoadModelIfXrModeIsOn() {
        // Reset the mocks that were already used in the @Before setup.
        reset(mTracker);
        reset(mBookmarkModel);

        mXrSpaceModeSupplier.set(true);

        // Call the constructor again.
        new BookmarkBarIphController(
                mProfile,
                mAppMenuHandler,
                mToolbarMenuButton,
                mBookmarkModel,
                mUserEducationHelper,
                mXrSpaceModeSupplier);

        // Verify that #finishLoadingBookmarkModel was never called.
        verify(mBookmarkModel, never()).finishLoadingBookmarkModel(any());
    }

    /**
     * Verifies that the correct IPH command was requested and that its callbacks function as
     * expected.
     */
    private void verifyIphCommand() {
        // Verify that #requestShowIph was called and capture the Iph command.
        verify(mUserEducationHelper).requestShowIph(mIphCommandCaptor.capture());

        IphCommand command = mIphCommandCaptor.getValue();

        // Verify that it's the correct command.
        Assert.assertEquals(
                "IphCommand feature should match.",
                FeatureConstants.BOOKMARK_BAR_VISIBILITY_FEATURE,
                command.featureName);

        Assert.assertEquals(
                "IphCommand stringId should match.",
                R.string.bookmark_bar_iph_message,
                command.stringId);

        // Verify the callbacks inside the command are correct.
        command.onShowCallback.run();
        verify(mAppMenuHandler).setMenuHighlight(R.id.preferences_id);

        command.onDismissCallback.run();
        verify(mAppMenuHandler).clearMenuHighlight();
    }
}
