// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.partnerbookmarks;

import android.content.Context;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.partnercustomizations.PartnerBrowserCustomizations;
import org.chromium.chrome.browser.profiles.Profile;

/** Unit tests for PartnerBookmarksReader. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class PartnerBookmarksReaderTest {

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock Context mContextMock;

    @Mock PartnerBookmarksReader.Natives mJniMock;

    @Mock PartnerBrowserCustomizations mBrowserCustomizations;

    @Mock Profile mProfile;

    @Captor ArgumentCaptor<Runnable> mBrowserCustomizationsInitCallback;

    @Before
    public void setUp() {
        PartnerBookmarksReaderJni.setInstanceForTesting(mJniMock);
        Mockito.doNothing()
                .when(mBrowserCustomizations)
                .setOnInitializeAsyncFinished(mBrowserCustomizationsInitCallback.capture());
    }

    @Test
    public void partnerBrowserCustomizations_BookmarkEditingDisabled_AlreadyInitialized() {
        Mockito.when(mBrowserCustomizations.isInitialized()).thenReturn(true);

        @SuppressWarnings("unused")
        PartnerBookmarksReader reader =
                new PartnerBookmarksReader(mContextMock, mProfile, mBrowserCustomizations);

        Mockito.verify(mBrowserCustomizations, Mockito.never()).initializeAsync(mContextMock);

        Mockito.when(mBrowserCustomizations.isBookmarksEditingDisabled()).thenReturn(true);
        mBrowserCustomizationsInitCallback.getValue().run();
        Mockito.verify(mJniMock).disablePartnerBookmarksEditing();
    }

    @Test
    public void partnerBrowserCustomizations_BookmarkEditingDisabled_NotAlreadyInitialized() {
        Mockito.when(mBrowserCustomizations.isInitialized()).thenReturn(false);

        @SuppressWarnings("unused")
        PartnerBookmarksReader reader =
                new PartnerBookmarksReader(mContextMock, mProfile, mBrowserCustomizations);

        Mockito.verify(mBrowserCustomizations).initializeAsync(mContextMock);

        Mockito.when(mBrowserCustomizations.isBookmarksEditingDisabled()).thenReturn(true);
        mBrowserCustomizationsInitCallback.getValue().run();
        Mockito.verify(mJniMock).disablePartnerBookmarksEditing();
    }

    @Test
    public void partnerBrowserCustomizations_BookmarkEditingAllowed_AlreadyInitialized() {
        Mockito.when(mBrowserCustomizations.isInitialized()).thenReturn(true);

        @SuppressWarnings("unused")
        PartnerBookmarksReader reader =
                new PartnerBookmarksReader(mContextMock, mProfile, mBrowserCustomizations);

        Mockito.verify(mBrowserCustomizations, Mockito.never()).initializeAsync(mContextMock);

        Mockito.when(mBrowserCustomizations.isBookmarksEditingDisabled()).thenReturn(false);
        mBrowserCustomizationsInitCallback.getValue().run();
        Mockito.verify(mJniMock, Mockito.never()).disablePartnerBookmarksEditing();
    }

    @Test
    public void partnerBrowserCustomizations_BookmarkEditingAllowed_NotAlreadyInitialized() {
        Mockito.when(mBrowserCustomizations.isInitialized()).thenReturn(false);

        @SuppressWarnings("unused")
        PartnerBookmarksReader reader =
                new PartnerBookmarksReader(mContextMock, mProfile, mBrowserCustomizations);

        Mockito.verify(mBrowserCustomizations).initializeAsync(mContextMock);

        Mockito.when(mBrowserCustomizations.isBookmarksEditingDisabled()).thenReturn(false);
        mBrowserCustomizationsInitCallback.getValue().run();
        Mockito.verify(mJniMock, Mockito.never()).disablePartnerBookmarksEditing();
    }

    @Test
    public void partnerBookmarksCreationComplete_NotCalledWithoutBrowserCustomizations() {
        Mockito.when(mBrowserCustomizations.isInitialized()).thenReturn(true);

        @SuppressWarnings("unused")
        PartnerBookmarksReader reader =
                new PartnerBookmarksReader(mContextMock, mProfile, mBrowserCustomizations);
        reader.onBookmarksRead();

        Mockito.verify(mJniMock, Mockito.never())
                .partnerBookmarksCreationComplete(Mockito.anyLong(), Mockito.any());
    }

    @Test
    public void partnerBookmarksCreationComplete_NotCalledWithoutBookmarksRead() {
        Mockito.when(mBrowserCustomizations.isInitialized()).thenReturn(true);

        @SuppressWarnings("unused")
        PartnerBookmarksReader reader =
                new PartnerBookmarksReader(mContextMock, mProfile, mBrowserCustomizations);
        Mockito.when(mBrowserCustomizations.isBookmarksEditingDisabled()).thenReturn(false);
        mBrowserCustomizationsInitCallback.getValue().run();

        Mockito.verify(mJniMock, Mockito.never())
                .partnerBookmarksCreationComplete(Mockito.anyLong(), Mockito.any());
    }

    @Test
    public void partnerBookmarksCreationComplete_Called_WithCustomizationsFirstThenBookmarks() {
        Mockito.when(mBrowserCustomizations.isInitialized()).thenReturn(true);

        @SuppressWarnings("unused")
        PartnerBookmarksReader reader =
                new PartnerBookmarksReader(mContextMock, mProfile, mBrowserCustomizations);
        Mockito.when(mBrowserCustomizations.isBookmarksEditingDisabled()).thenReturn(false);
        mBrowserCustomizationsInitCallback.getValue().run();
        reader.onBookmarksRead();

        Mockito.verify(mJniMock)
                .partnerBookmarksCreationComplete(Mockito.anyLong(), Mockito.eq(reader));
    }

    @Test
    public void partnerBookmarksCreationComplete_Called_WithBookmarksFirstThenCustomizations() {
        Mockito.when(mBrowserCustomizations.isInitialized()).thenReturn(true);

        @SuppressWarnings("unused")
        PartnerBookmarksReader reader =
                new PartnerBookmarksReader(mContextMock, mProfile, mBrowserCustomizations);
        Mockito.when(mBrowserCustomizations.isBookmarksEditingDisabled()).thenReturn(false);
        reader.onBookmarksRead();
        mBrowserCustomizationsInitCallback.getValue().run();

        Mockito.verify(mJniMock)
                .partnerBookmarksCreationComplete(Mockito.anyLong(), Mockito.eq(reader));
    }
}
