// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.browserservices.ui.view;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.verify;

import static org.chromium.chrome.browser.browserservices.ui.TrustedWebActivityModel.DISCLOSURE_EVENTS_CALLBACK;
import static org.chromium.chrome.browser.browserservices.ui.TrustedWebActivityModel.DISCLOSURE_STATE;
import static org.chromium.chrome.browser.browserservices.ui.TrustedWebActivityModel.DISCLOSURE_STATE_NOT_SHOWN;
import static org.chromium.chrome.browser.browserservices.ui.TrustedWebActivityModel.DISCLOSURE_STATE_SHOWN;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.RuntimeEnvironment;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.browserservices.ui.TrustedWebActivityModel;
import org.chromium.chrome.browser.browserservices.ui.trustedwebactivity.FilledLazy;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;

/**
 * Tests for {@link DisclosureSnackbar}. Most of the behaviour for that class will have been tested
 * in the {@link DisclosureInfobarTest}.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class DisclosureSnackbarTest {
    @Mock public ActivityLifecycleDispatcher mLifecycleDispatcher;
    @Mock public SnackbarManager mSnackbarManager;
    @Mock public TrustedWebActivityModel.DisclosureEventsCallback mCallback;

    private TrustedWebActivityModel mModel = new TrustedWebActivityModel();

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);

        mModel.set(DISCLOSURE_EVENTS_CALLBACK, mCallback);
        new DisclosureSnackbar(
                RuntimeEnvironment.application.getResources(),
                new FilledLazy<>(mSnackbarManager),
                mModel,
                mLifecycleDispatcher);
    }

    @Test
    public void displaysSnackbar_onlyOnce() {
        mModel.set(DISCLOSURE_STATE, DISCLOSURE_STATE_SHOWN);
        mModel.set(DISCLOSURE_STATE, DISCLOSURE_STATE_NOT_SHOWN);
        mModel.set(DISCLOSURE_STATE, DISCLOSURE_STATE_SHOWN);

        verify(mSnackbarManager).showSnackbar(any());
    }
}
