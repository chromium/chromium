// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.AdditionalMatchers.aryEq;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;
import static org.robolectric.Shadows.shadowOf;

import android.app.Activity;
import android.content.Intent;

import androidx.test.ext.junit.rules.ActivityScenarioRule;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Robolectric;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.UserActionTester;
import org.chromium.chrome.browser.IntentHandler;
import org.chromium.chrome.browser.IntentRecordingActivity;
import org.chromium.chrome.browser.app.bookmarks.BookmarkActivity;
import org.chromium.chrome.browser.app.bookmarks.BookmarkEditActivity;
import org.chromium.chrome.browser.app.bookmarks.BookmarkFolderPickerActivity;
import org.chromium.chrome.browser.app.tabmodel.AsyncTabParamsManagerSingleton;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileResolver;
import org.chromium.chrome.browser.profiles.ProfileResolverJni;
import org.chromium.chrome.browser.profiles.ProfileTokenUtils;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.components.bookmarks.BookmarkId;
import org.chromium.components.bookmarks.BookmarkType;
import org.chromium.ui.base.TestActivity;

/** Unit tests for {@link BookmarkManagerOpener}. */
@RunWith(BaseRobolectricTestRunner.class)
public class BookmarkManagerOpenerTest {
    private static final byte[] PROFILE_TOKEN = new byte[] {1, 2, 3};

    @Rule
    public ActivityScenarioRule<TestActivity> mActivityScenarioRule =
            new ActivityScenarioRule<>(TestActivity.class);

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private Tab mCurrentTab;
    @Mock private Profile mProfile;
    @Mock private ProfileResolver.Natives mProfileResolverNatives;

    private Activity mActivity;
    private final BookmarkManagerOpener mBookmarkManagerOpener = new BookmarkManagerOpenerImpl();

    @Before
    public void setUp() {
        ProfileResolverJni.setInstanceForTesting(mProfileResolverNatives);
        when(mProfileResolverNatives.tokenizeProfileToBytes(mProfile)).thenReturn(PROFILE_TOKEN);
        mActivityScenarioRule.getScenario().onActivity(this::onActivity);
    }

    private void onActivity(Activity activity) {
        mActivity = activity;
    }

    @After
    public void tearDown() {
        AsyncTabParamsManagerSingleton.getInstance().getAsyncTabParams().clear();
    }

    /** Asserts that {@code intent} carries the token for {@link #mProfile}. */
    private void assertIntentHasProfileToken(Intent intent) {
        assertTrue(ProfileTokenUtils.hasProfileToken(intent));
        ProfileTokenUtils.retrieveProfileFromIntent(intent, profile -> {});
        // A missing or malformed token short-circuits before reaching the resolver.
        verify(mProfileResolverNatives).resolveProfileFromBytes(aryEq(PROFILE_TOKEN), any());
    }

    @Test
    public void testReopeningBookmarkManagerRecordsMetric() {
        BookmarkUtils.setLastUsedUrl("https://test.com");
        UserActionTester userActionTester = new UserActionTester();

        mBookmarkManagerOpener.showBookmarkManager(
                mActivity, mCurrentTab, mProfile, /* folderId= */ null);
        assertEquals(
                1,
                userActionTester.getActionCount(
                        "MobileBookmarkManagerReopenBookmarksInSameSession"));
    }

    @Test
    public void testShowBookmarkManagerInTabPreservesParentTabAndFolder() {
        int parentTabId = 42;
        doReturn(parentTabId).when(mCurrentTab).getId();
        BookmarkUtils.setLastUsedUrl("chrome-native://bookmarks/folder/3");
        BookmarkManagerOpenerImpl opener = new BookmarkManagerOpenerImpl();

        opener.showBookmarkManager(mActivity, mCurrentTab, mProfile, /* folderId= */ null);
        Intent phoneIntent = shadowOf(mActivity).getNextStartedActivity();
        assertNotNull(phoneIntent);
        assertEquals(
                parentTabId,
                phoneIntent.getIntExtra(IntentHandler.EXTRA_PARENT_TAB_ID, Tab.INVALID_TAB_ID));

        doReturn(true).when(mProfile).isOffTheRecord();
        IntentRecordingActivity bookmarkActivity = buildIntentRecordingActivity();
        opener.showBookmarkManagerInTab(
                bookmarkActivity, mProfile, parentTabId, mActivity.getComponentName());
        Intent tabIntent = getActivityStartedIntent(bookmarkActivity);
        assertEquals("chrome-native://bookmarks/folder/3", tabIntent.getDataString());
        assertEquals(
                parentTabId,
                tabIntent.getIntExtra(IntentHandler.EXTRA_PARENT_TAB_ID, Tab.INVALID_TAB_ID));
        assertTrue(
                tabIntent.getBooleanExtra(
                        IntentHandler.EXTRA_OPEN_NEW_INCOGNITO_TAB, /* defaultValue= */ false));
        assertEquals(mActivity.getComponentName(), tabIntent.getComponent());
    }

    @Test
    @Config(qualifiers = "sw600dp")
    public void testShowBookmarkManagerOnTablet_withCurrentTab_startsActivityFromActivity() {
        doReturn(7).when(mCurrentTab).getId();
        IntentRecordingActivity activity = buildIntentRecordingActivity();

        mBookmarkManagerOpener.showBookmarkManager(
                activity, mCurrentTab, mProfile, /* folderId= */ null);

        Intent intent = getActivityStartedIntent(activity);
        assertEquals(activity.getComponentName(), intent.getComponent());
        assertEquals(7, intent.getIntExtra(IntentHandler.EXTRA_PARENT_TAB_ID, Tab.INVALID_TAB_ID));
    }

    @Test
    @Config(qualifiers = "sw600dp")
    public void testShowBookmarkManagerOnTablet_withNullTab_startsActivityFromActivity() {
        IntentRecordingActivity activity = buildIntentRecordingActivity();

        mBookmarkManagerOpener.showBookmarkManager(
                activity, /* tab= */ null, mProfile, /* folderId= */ null);

        Intent intent = getActivityStartedIntent(activity);
        assertEquals(activity.getComponentName(), intent.getComponent());
        assertEquals(Tab.INVALID_TAB_ID, intent.getIntExtra(IntentHandler.EXTRA_PARENT_TAB_ID, 0));
    }

    @Test
    public void testShowBookmarkManager_addsProfileTokenToIntent() {
        mBookmarkManagerOpener.showBookmarkManager(
                mActivity, mCurrentTab, mProfile, /* folderId= */ null);

        Intent intent = shadowOf(mActivity).getNextStartedActivity();
        assertNotNull(intent);
        assertNotNull(intent.getComponent());
        assertEquals(BookmarkActivity.class.getName(), intent.getComponent().getClassName());
        assertIntentHasProfileToken(intent);
    }

    @Test
    public void testStartEditActivity_addsProfileTokenToIntent() {
        BookmarkId bookmarkId = new BookmarkId(/* id= */ 1, BookmarkType.NORMAL);
        mBookmarkManagerOpener.startEditActivity(mActivity, mProfile, bookmarkId);

        Intent intent = shadowOf(mActivity).getNextStartedActivity();
        assertNotNull(intent);
        assertNotNull(intent.getComponent());
        assertEquals(BookmarkEditActivity.class.getName(), intent.getComponent().getClassName());
        assertIntentHasProfileToken(intent);
    }

    @Test
    public void testStartFolderPickerActivity_addsProfileTokenToIntent() {
        BookmarkId bookmarkId = new BookmarkId(/* id= */ 1, BookmarkType.NORMAL);
        mBookmarkManagerOpener.startFolderPickerActivity(mActivity, mProfile, bookmarkId);

        Intent intent = shadowOf(mActivity).getNextStartedActivity();
        assertNotNull(intent);
        assertNotNull(intent.getComponent());
        assertEquals(
                BookmarkFolderPickerActivity.class.getName(), intent.getComponent().getClassName());
        assertIntentHasProfileToken(intent);
    }

    private static IntentRecordingActivity buildIntentRecordingActivity() {
        return Robolectric.buildActivity(IntentRecordingActivity.class).setup().get();
    }

    private static Intent getActivityStartedIntent(IntentRecordingActivity activity) {
        assertEquals(1, activity.getStartedIntents().size());
        return activity.getStartedIntents().get(0);
    }
}
