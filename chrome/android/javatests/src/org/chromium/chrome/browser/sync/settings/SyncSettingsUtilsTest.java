// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.sync.settings;

import android.content.Context;

import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Batch;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.sync.SyncServiceFactory;
import org.chromium.chrome.browser.sync.SyncSettingsUtils;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.components.browser_ui.settings.SettingsCustomTabLauncher;
import org.chromium.components.sync.BookmarksLimitExceededHelpClickedSource;
import org.chromium.components.sync.SyncService;
import org.chromium.components.sync.UserActionableError;
import org.chromium.content_public.browser.test.NativeLibraryTestUtils;

/** Unit tests for {@link SyncSettingsUtils}. */
@RunWith(ChromeJUnit4ClassRunner.class)
@Batch(Batch.PER_CLASS)
public class SyncSettingsUtilsTest {

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private Profile mProfile;

    @Mock private SyncService mSyncService;

    @Mock private SettingsCustomTabLauncher mCustomTabLauncher;

    @Before
    public void setUp() {
        NativeLibraryTestUtils.loadNativeLibraryAndInitBrowserProcess();
    }

    @Test
    @SmallTest
    public void testGetSyncError_NullSyncService() {
        SyncServiceFactory.setInstanceForTesting(null);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    Assert.assertEquals(
                            UserActionableError.NONE, SyncSettingsUtils.getSyncError(mProfile));
                });
    }

    @Test
    @SmallTest
    public void testGetSyncError_NonNullSyncService() {
        SyncServiceFactory.setInstanceForTesting(mSyncService);
        Mockito.when(mSyncService.getUserActionableError())
                .thenReturn(UserActionableError.SIGN_IN_NEEDS_UPDATE);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    Assert.assertEquals(
                            UserActionableError.SIGN_IN_NEEDS_UPDATE,
                            SyncSettingsUtils.getSyncError(mProfile));
                });
    }

    @Test
    @SmallTest
    public void testOpenBookmarkLimitHelpPage() {
        Context context = Mockito.mock(Context.class);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    SyncSettingsUtils.openBookmarkLimitHelpPage(
                            context,
                            mSyncService,
                            BookmarksLimitExceededHelpClickedSource.SETTINGS,
                            mCustomTabLauncher);
                });

        Mockito.verify(mSyncService)
                .acknowledgeBookmarksLimitExceededError(
                        BookmarksLimitExceededHelpClickedSource.SETTINGS);
        Mockito.verify(mCustomTabLauncher)
                .openUrlInCct(context, SyncSettingsUtils.BOOKMARKS_LIMIT_EXCEEDED_HELP_CENTER_URL);
    }
}
