// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.glic;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.content.Context;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.actor.ActorKeyedService;
import org.chromium.chrome.browser.actor.ActorKeyedServiceFactory;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.ui.messages.snackbar.Snackbar;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager.SnackbarManageable;

/** Unit tests for {@link GlicHelper}. */
@RunWith(BaseRobolectricTestRunner.class)
public class GlicHelperUnitTest {
    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private SnackbarManageable mSnackbarManageableMock;
    @Mock private SnackbarManager mSnackbarManagerMock;
    @Mock private Profile mProfileMock;
    @Mock private Context mContextMock;
    @Mock private ActorKeyedService mActorServiceMock;

    @Before
    public void setUp() {
        when(mSnackbarManageableMock.getSnackbarManager()).thenReturn(mSnackbarManagerMock);
        ActorKeyedServiceFactory.setForTesting(mActorServiceMock);
    }

    @Test
    public void testMaybeShowSnackbar_WithActiveTasks() {
        when(mProfileMock.isOffTheRecord()).thenReturn(false);
        when(mActorServiceMock.getActiveTasksCount()).thenReturn(1);

        GlicHelper.maybeShowGlicTaskInProgressSnackbar(
                mSnackbarManageableMock, mProfileMock, mContextMock);

        verify(mSnackbarManagerMock).showSnackbar(any(Snackbar.class));
    }

    @Test
    public void testMaybeShowSnackbar_NoActiveTasks() {
        when(mProfileMock.isOffTheRecord()).thenReturn(false);
        when(mActorServiceMock.getActiveTasksCount()).thenReturn(0);

        GlicHelper.maybeShowGlicTaskInProgressSnackbar(
                mSnackbarManageableMock, mProfileMock, mContextMock);

        verify(mSnackbarManagerMock, never()).showSnackbar(any(Snackbar.class));
    }

    @Test
    public void testMaybeShowSnackbar_OffTheRecord() {
        when(mProfileMock.isOffTheRecord()).thenReturn(true);
        // Even if there are active tasks, we shouldn't show snackbar for OTR profiles.
        when(mActorServiceMock.getActiveTasksCount()).thenReturn(1);

        GlicHelper.maybeShowGlicTaskInProgressSnackbar(
                mSnackbarManageableMock, mProfileMock, mContextMock);

        verify(mSnackbarManagerMock, never()).showSnackbar(any(Snackbar.class));
    }
}
