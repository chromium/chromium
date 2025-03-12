// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks;

import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoInteractions;

import android.app.Activity;

import androidx.test.ext.junit.rules.ActivityScenarioRule;
import androidx.test.filters.SmallTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.ActivityState;
import org.chromium.base.ApplicationStatus;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.chrome.browser.app.bookmarks.BookmarkFolderPickerActivity;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileResolver;
import org.chromium.chrome.browser.profiles.ProfileResolverJni;
import org.chromium.components.bookmarks.BookmarkId;
import org.chromium.components.bookmarks.BookmarkType;
import org.chromium.ui.base.TestActivity;

import java.util.concurrent.TimeoutException;

/** Unit tests for {@link BookmarkManagerOpener}. */
@Batch(Batch.UNIT_TESTS)
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class BookmarkManagerOpenerTest {
    @Rule
    public ActivityScenarioRule<TestActivity> mActivityScenarioRule =
            new ActivityScenarioRule<>(TestActivity.class);

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private Profile mProfile;
    @Mock private ProfileResolver.Natives mProfileResolverNatives;
    @Mock private BookmarkFolderPickerActivity mBookmarkFolderPickerActivity;
    @Mock private BookmarkFolderPickerActivity mBookmarkFolderPickerActivity2;
    @Mock private Runnable mRunnable;

    private Activity mActivity;
    private BookmarkManagerOpener mBookmarkManagerOpener = new BookmarkManagerOpenerImpl();

    @Before
    public void setUp() {
        ProfileResolverJni.setInstanceForTesting(mProfileResolverNatives);
        mActivityScenarioRule.getScenario().onActivity(this::onActivity);
    }

    private void onActivity(Activity activity) {
        mActivity = activity;
    }

    @Test
    @SmallTest
    public void testStateChangeForSameActivityTriggersCallback() throws TimeoutException {
        CallbackHelper callbackHelper = new CallbackHelper();
        Runnable runnable =
                () -> {
                    callbackHelper.notifyCalled();
                };
        mBookmarkManagerOpener.startFolderPickerActivity(
                mActivity, mProfile, runnable, new BookmarkId(0, BookmarkType.NORMAL));
        ApplicationStatus.onStateChangeForTesting(
                mBookmarkFolderPickerActivity, ActivityState.CREATED);
        ApplicationStatus.onStateChangeForTesting(
                mBookmarkFolderPickerActivity, ActivityState.DESTROYED);
        callbackHelper.waitForNext();
    }

    @Test
    @SmallTest
    public void testStateChangeForDifferentActivityDoesNotTrigger() {
        mBookmarkManagerOpener.startFolderPickerActivity(
                mActivity, mProfile, mRunnable, new BookmarkId(0, BookmarkType.NORMAL));
        ApplicationStatus.onStateChangeForTesting(
                mBookmarkFolderPickerActivity, ActivityState.CREATED);
        ApplicationStatus.onStateChangeForTesting(
                mBookmarkFolderPickerActivity2, ActivityState.CREATED);
        ApplicationStatus.onStateChangeForTesting(
                mBookmarkFolderPickerActivity2, ActivityState.DESTROYED);
        verifyNoInteractions(mRunnable);
        ApplicationStatus.onStateChangeForTesting(
                mBookmarkFolderPickerActivity, ActivityState.DESTROYED);
        verify(mRunnable).run();
    }
}
