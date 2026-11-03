// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.partnerbookmarks;

import android.content.Context;

import org.jni_zero.JniUniquePtr;
import org.junit.After;
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

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.partnercustomizations.PartnerBrowserCustomizations;

/** Unit tests for PartnerBookmarksReader. */
@RunWith(BaseRobolectricTestRunner.class)
public class PartnerBookmarksReaderTest {

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private Context mContextMock;
    @Mock private PartnerBookmarksReader.Natives mJniMock;
    @Mock private PartnerBrowserCustomizations mBrowserCustomizations;
    @Captor private ArgumentCaptor<Runnable> mBrowserCustomizationsInitCallback;

    private JniUniquePtr<PartnerBookmarksReader.NativePartnerBookmarksReader> mNativeReader;

    @Before
    public void setUp() {
        PartnerBookmarksReaderJni.setInstanceForTesting(mJniMock);
        Mockito.doNothing()
                .when(mBrowserCustomizations)
                .setOnInitializeAsyncFinished(mBrowserCustomizationsInitCallback.capture());
        mNativeReader = JniUniquePtr.createForTesting(1L);
    }

    @After
    public void tearDown() {
        mNativeReader.destroy();
    }

    /**
     * Utility to decrease code duplication across the different tests in this suite.
     *
     * @param browserCustomizationsInitialized Whether the partner customizations is initialized.
     *     Used to test the potential async initialization of the class.
     * @param nativePartnerBookmarksReader The native partner bookmarks reader is null. Used to test
     *     potential failure paths when the profile is null.
     */
    PartnerBookmarksReader createPartnerBookmarksReader(
            boolean browserCustomizationsInitialized, boolean hasNullNativePointer) {
        Mockito.when(mBrowserCustomizations.isInitialized())
                .thenReturn(browserCustomizationsInitialized);
        return new PartnerBookmarksReader(
                mContextMock, mBrowserCustomizations, hasNullNativePointer ? null : mNativeReader);
    }

    @Test
    public void partnerBrowserCustomizations_BookmarkEditingDisabled_AlreadyInitialized() {
        createPartnerBookmarksReader(
                /* browserCustomizationsInitialized= */ true, /* hasNullNativePointer= */ false);

        Mockito.verify(mBrowserCustomizations, Mockito.never()).initializeAsync(mContextMock);

        Mockito.when(mBrowserCustomizations.isBookmarksEditingDisabled()).thenReturn(true);
        mBrowserCustomizationsInitCallback.getValue().run();
        Mockito.verify(mJniMock).disablePartnerBookmarksEditing();
    }

    @Test
    public void partnerBrowserCustomizations_BookmarkEditingDisabled_NotAlreadyInitialized() {
        createPartnerBookmarksReader(
                /* browserCustomizationsInitialized= */ false, /* hasNullNativePointer= */ false);

        Mockito.verify(mBrowserCustomizations).initializeAsync(mContextMock);

        Mockito.when(mBrowserCustomizations.isBookmarksEditingDisabled()).thenReturn(true);
        mBrowserCustomizationsInitCallback.getValue().run();
        Mockito.verify(mJniMock).disablePartnerBookmarksEditing();
    }

    @Test
    public void partnerBrowserCustomizations_BookmarkEditingAllowed_AlreadyInitialized() {
        createPartnerBookmarksReader(
                /* browserCustomizationsInitialized= */ true, /* hasNullNativePointer= */ false);

        Mockito.verify(mBrowserCustomizations, Mockito.never()).initializeAsync(mContextMock);

        Mockito.when(mBrowserCustomizations.isBookmarksEditingDisabled()).thenReturn(false);
        mBrowserCustomizationsInitCallback.getValue().run();
        Mockito.verify(mJniMock, Mockito.never()).disablePartnerBookmarksEditing();
    }

    @Test
    public void partnerBrowserCustomizations_BookmarkEditingAllowed_NotAlreadyInitialized() {
        createPartnerBookmarksReader(
                /* browserCustomizationsInitialized= */ false, /* hasNullNativePointer= */ false);

        Mockito.verify(mBrowserCustomizations).initializeAsync(mContextMock);

        Mockito.when(mBrowserCustomizations.isBookmarksEditingDisabled()).thenReturn(false);
        mBrowserCustomizationsInitCallback.getValue().run();
        Mockito.verify(mJniMock, Mockito.never()).disablePartnerBookmarksEditing();
    }

    @Test
    public void partnerBookmarksCreationComplete_NotCalledWithoutBrowserCustomizations() {
        PartnerBookmarksReader reader =
                createPartnerBookmarksReader(
                        /* browserCustomizationsInitialized= */ true,
                        /* hasNullNativePointer= */ false);
        reader.onBookmarksRead();

        Mockito.verify(mJniMock, Mockito.never()).partnerBookmarksCreationComplete(mNativeReader);
    }

    @Test
    public void partnerBookmarksCreationComplete_NotCalledWithoutBookmarksRead() {
        createPartnerBookmarksReader(
                /* browserCustomizationsInitialized= */ true, /* hasNullNativePointer= */ false);
        Mockito.when(mBrowserCustomizations.isBookmarksEditingDisabled()).thenReturn(false);
        mBrowserCustomizationsInitCallback.getValue().run();

        Mockito.verify(mJniMock, Mockito.never()).partnerBookmarksCreationComplete(mNativeReader);
    }

    @Test
    public void partnerBookmarksCreationComplete_Called_WithCustomizationsFirstThenBookmarks() {
        PartnerBookmarksReader reader =
                createPartnerBookmarksReader(
                        /* browserCustomizationsInitialized= */ true,
                        /* hasNullNativePointer= */ false);
        Mockito.when(mBrowserCustomizations.isBookmarksEditingDisabled()).thenReturn(false);
        mBrowserCustomizationsInitCallback.getValue().run();
        reader.onBookmarksRead();

        Mockito.verify(mJniMock).partnerBookmarksCreationComplete(mNativeReader);
    }

    @Test
    public void partnerBookmarksCreationComplete_Called_WithBookmarksFirstThenCustomizations() {
        PartnerBookmarksReader reader =
                createPartnerBookmarksReader(
                        /* browserCustomizationsInitialized= */ true,
                        /* hasNullNativePointer= */ false);
        Mockito.when(mBrowserCustomizations.isBookmarksEditingDisabled()).thenReturn(false);
        reader.onBookmarksRead();
        mBrowserCustomizationsInitCallback.getValue().run();

        Mockito.verify(mJniMock).partnerBookmarksCreationComplete(mNativeReader);
    }
}
