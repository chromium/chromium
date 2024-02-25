// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.browserservices.ui.view;

import static org.junit.Assert.assertEquals;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import static org.chromium.chrome.browser.browserservices.ui.TrustedWebActivityModel.DISCLOSURE_EVENTS_CALLBACK;
import static org.chromium.chrome.browser.browserservices.ui.TrustedWebActivityModel.DISCLOSURE_STATE;
import static org.chromium.chrome.browser.browserservices.ui.TrustedWebActivityModel.DISCLOSURE_STATE_NOT_SHOWN;
import static org.chromium.chrome.browser.browserservices.ui.TrustedWebActivityModel.DISCLOSURE_STATE_SHOWN;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.RuntimeEnvironment;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.browserservices.ui.TrustedWebActivityModel;
import org.chromium.chrome.browser.browserservices.ui.trustedwebactivity.FilledLazy;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.ui.messages.snackbar.Snackbar;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;

/** Tests for {@link DisclosureInfobar}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class DisclosureInfobarTest {
    @Mock public ActivityLifecycleDispatcher mLifecycleDispatcher;
    @Mock public SnackbarManager mSnackbarManager;
    @Mock public TrustedWebActivityModel.DisclosureEventsCallback mCallback;

    private TrustedWebActivityModel mModel = new TrustedWebActivityModel();
    private DisclosureInfobar mInfobar;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);

        mModel.set(DISCLOSURE_EVENTS_CALLBACK, mCallback);
        mInfobar =
                new DisclosureInfobar(
                        RuntimeEnvironment.application.getResources(),
                        new FilledLazy<>(mSnackbarManager),
                        mModel,
                        mLifecycleDispatcher);
    }

    @Test
    public void registersForLifecycle() {
        verify(mLifecycleDispatcher).register(eq(mInfobar));
    }

    @Test
    public void displaysInfobar() {
        mModel.set(DISCLOSURE_STATE, DISCLOSURE_STATE_SHOWN);

        ArgumentCaptor<Snackbar> captor = ArgumentCaptor.forClass(Snackbar.class);
        verify(mSnackbarManager).showSnackbar(captor.capture());

        Snackbar snackbar = captor.getValue();

        assertEquals(Snackbar.UMA_TWA_PRIVACY_DISCLOSURE, snackbar.getIdentifierForTesting());

        verify(mCallback).onDisclosureShown();
    }

    @Test
    public void displaysInfobar_subsequentVisits() {
        mModel.set(DISCLOSURE_STATE, DISCLOSURE_STATE_SHOWN);
        mModel.set(DISCLOSURE_STATE, DISCLOSURE_STATE_NOT_SHOWN);
        mModel.set(DISCLOSURE_STATE, DISCLOSURE_STATE_SHOWN);

        verify(mSnackbarManager, times(2)).showSnackbar(any());
    }

    @Test
    public void dismissesInfobar() {
        mModel.set(DISCLOSURE_STATE, DISCLOSURE_STATE_SHOWN);
        ArgumentCaptor<Snackbar> snackbar = ArgumentCaptor.forClass(Snackbar.class);
        verify(mSnackbarManager).showSnackbar(snackbar.capture());

        SnackbarManager.SnackbarController controller = snackbar.getValue().getController();

        mModel.set(DISCLOSURE_STATE, DISCLOSURE_STATE_NOT_SHOWN);
        verify(mSnackbarManager).dismissSnackbars(eq(controller));
    }

    @Test
    public void snackbarAction() {
        mModel.set(DISCLOSURE_STATE, DISCLOSURE_STATE_SHOWN);
        ArgumentCaptor<Snackbar> snackbar = ArgumentCaptor.forClass(Snackbar.class);
        verify(mSnackbarManager).showSnackbar(snackbar.capture());

        SnackbarManager.SnackbarController controller = snackbar.getValue().getController();
        controller.onAction(null);

        verify(mCallback).onDisclosureAccepted();
    }
}
