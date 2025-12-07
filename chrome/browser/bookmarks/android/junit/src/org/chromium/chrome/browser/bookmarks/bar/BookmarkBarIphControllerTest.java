// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks.bar;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.reset;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.content.Context;
import android.view.View;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.bookmarks.BookmarkModel;
import org.chromium.chrome.browser.feature_engagement.TrackerFactory;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.ui.appmenu.AppMenuHandler;
import org.chromium.chrome.browser.user_education.IphCommand;
import org.chromium.chrome.browser.user_education.UserEducationHelper;
import org.chromium.components.bookmarks.BookmarkId;
import org.chromium.components.bookmarks.BookmarkItem;
import org.chromium.components.feature_engagement.FeatureConstants;
import org.chromium.components.feature_engagement.Tracker;
import org.chromium.components.prefs.PrefService;
import org.chromium.components.user_prefs.UserPrefs;
import org.chromium.components.user_prefs.UserPrefsJni;

import java.util.ArrayList;
import java.util.List;

/** Unit tests for {@link BookmarkBarIphController}. */
@RunWith(BaseRobolectricTestRunner.class)
public class BookmarkBarIphControllerTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private Context mContext;
    @Mock private AppMenuHandler mAppMenuHandler;
    @Mock private View mToolbarMenuButton;
    @Mock private BookmarkModel mBookmarkModel;
    @Mock private Profile mProfile;
    @Mock private Tracker mTracker;
    @Mock private UserEducationHelper mUserEducationHelper;
    @Mock private BookmarkId mDesktopFolderId;
    @Mock private BookmarkId mChildBookmarkId;
    @Mock private BookmarkItem mDesktopFolderItem;
    @Mock private BookmarkItem mChildBookmarkItem;
    @Mock private UserPrefs.Natives mUserPrefsJni;
    @Mock private PrefService mPrefService;

    @Captor private ArgumentCaptor<IphCommand> mIphCommandCaptor;
    @Captor private ArgumentCaptor<Runnable> mRunnableCaptor;

    private BookmarkBarIphController mController;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);

        UserPrefsJni.setInstanceForTesting(mUserPrefsJni);
        when(mUserPrefsJni.get(mProfile)).thenReturn(mPrefService);
        when(mProfile.getOriginalProfile()).thenReturn(mProfile);

        // Set the default behavior for policy checks (no policy active).
        when(mPrefService.isManagedPreference(Pref.SHOW_BOOKMARK_BAR)).thenReturn(false);
        when(mPrefService.hasRecommendation(Pref.SHOW_BOOKMARK_BAR)).thenReturn(false);
        when(mPrefService.isRecommendedPreference(Pref.SHOW_BOOKMARK_BAR)).thenReturn(false);

        // Set the default behavior for reading the pref value.
        when(mPrefService.getBoolean(Pref.SHOW_BOOKMARK_BAR)).thenReturn(true);

        // Mock the .post() call to run immediately.
        when(mToolbarMenuButton.post(mRunnableCaptor.capture()))
                .thenAnswer(
                        invocation -> {
                            mRunnableCaptor.getValue().run();
                            return true;
                        });

        when(mToolbarMenuButton.getContext()).thenReturn(mContext);

        TrackerFactory.setTrackerForTests(mTracker);
        when(mTracker.wouldTriggerHelpUi(FeatureConstants.BOOKMARKS_BAR_FEATURE)).thenReturn(true);

        mController =
                new BookmarkBarIphController(
                        mProfile,
                        mAppMenuHandler,
                        mToolbarMenuButton,
                        mBookmarkModel,
                        mUserEducationHelper);
    }

    /**
     * Tests Trigger 1: Iph shows on startup if at least one bookmark already exists in the
     * bookmarks bar.
     */
    @Test
    public void testTrigger1_OnModelLoaded_WithBookmark() {
        BookmarkBarUtils.setActivityStateBookmarkBarCompatibleForTesting(true);

        // Create a fake bookmark and set up the model to return it.
        List<BookmarkId> children = new ArrayList<>();
        children.add(mChildBookmarkId);
        when(mBookmarkModel.getDesktopFolderId()).thenReturn(mDesktopFolderId);
        when(mBookmarkModel.getChildIds(mDesktopFolderId)).thenReturn(children);
        when(mBookmarkModel.getBookmarkById(mChildBookmarkId)).thenReturn(mChildBookmarkItem);
        when(mChildBookmarkItem.isFolder()).thenReturn(false);

        // Simulate the bookmark model finishing its load.
        mController.bookmarkModelLoaded();
        verifyIphCommand();
    }

    /** Tests Trigger 2: Iph shows when a new bookmark is added in the current device. */
    @Test
    public void testTrigger2_OnBookmarkNodeAdded() {
        BookmarkBarUtils.setActivityStateBookmarkBarCompatibleForTesting(true);

        when(mDesktopFolderItem.getId()).thenReturn(mDesktopFolderId);
        List<BookmarkId> children = new ArrayList<>();
        children.add(mChildBookmarkId);
        when(mBookmarkModel.getChildIds(any())).thenReturn(children);
        when(mBookmarkModel.getBookmarkById(mChildBookmarkId)).thenReturn(mChildBookmarkItem);
        when(mChildBookmarkItem.isFolder()).thenReturn(false);

        // Simulate a user adding a new bookmark.
        mController.bookmarkNodeAdded(mDesktopFolderItem, 0, /* addedByUser */ true);
        verifyIphCommand();
    }

    /**
     * Tests that trigger 2 is ignored if the bookmark addition came from sync (addedByUser is
     * false).
     */
    @Test
    public void testTrigger2_OnBookmarkNodeAdded_FromSync() {
        BookmarkBarUtils.setActivityStateBookmarkBarCompatibleForTesting(true);

        when(mDesktopFolderItem.getId()).thenReturn(mDesktopFolderId);
        List<BookmarkId> children = new ArrayList<>();
        children.add(mChildBookmarkId);
        when(mBookmarkModel.getChildIds(any())).thenReturn(children);
        when(mBookmarkModel.getBookmarkById(mChildBookmarkId)).thenReturn(mChildBookmarkItem);
        when(mChildBookmarkItem.isFolder()).thenReturn(false);

        // Simulate a bookmark being added by sync.
        mController.bookmarkNodeAdded(mDesktopFolderItem, 0, /* addedByUser */ false);

        // Verify the IPH was not shown.
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

        when(mTracker.wouldTriggerHelpUi(FeatureConstants.BOOKMARKS_BAR_FEATURE)).thenReturn(false);

        // Call the constructor again.
        BookmarkBarIphController newController =
                new BookmarkBarIphController(
                        mProfile,
                        mAppMenuHandler,
                        mToolbarMenuButton,
                        mBookmarkModel,
                        mUserEducationHelper);

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
                FeatureConstants.BOOKMARKS_BAR_FEATURE,
                command.featureName);

        Assert.assertEquals(
                "IphCommand stringId should match.",
                R.string.bookmarks_bar_iph_message,
                command.stringId);

        // Verify the callbacks inside the command are correct.
        command.onShowCallback.run();
        verify(mAppMenuHandler).setMenuHighlight(R.id.preferences_id);

        command.onDismissCallback.run();
        verify(mAppMenuHandler).clearMenuHighlight();
    }
}
