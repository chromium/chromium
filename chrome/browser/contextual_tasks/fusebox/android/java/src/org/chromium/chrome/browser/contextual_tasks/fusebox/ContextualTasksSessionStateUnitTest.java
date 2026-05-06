// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.contextual_tasks.fusebox;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import android.app.Activity;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Robolectric;
import org.robolectric.annotation.Config;

import org.chromium.base.supplier.ObservableSuppliers;
import org.chromium.base.supplier.SettableMonotonicObservableSupplier;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.content_public.browser.WebContents;

/** Unit tests for {@link ContextualTasksSessionState}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class ContextualTasksSessionStateUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private WebContents mWebContents;
    @Mock private Profile mProfile;
    private Activity mActivity;
    private ContextualTasksSessionState mSessionState;
    private final SettableMonotonicObservableSupplier<Profile> mProfileSupplier =
            ObservableSuppliers.createMonotonic();

    @Before
    public void setUp() {
        mActivity = Robolectric.buildActivity(Activity.class).setup().get();
        mSessionState = new ContextualTasksSessionState();
        mProfileSupplier.set(mProfile);
    }

    @Test
    public void testActivate() {
        assertFalse(mSessionState.isSessionActive());
        mSessionState.activate(mActivity, mWebContents, mProfileSupplier, null);
        assertTrue(mSessionState.isSessionActive());
    }

    @Test
    public void testDeactivateIsNoOp() {
        mSessionState.activate(mActivity, mWebContents, mProfileSupplier, null);
        assertTrue(mSessionState.isSessionActive());

        mSessionState.deactivate();
        // Should still be active because deactivate is a no-op.
        assertTrue(mSessionState.isSessionActive());
    }

    @Test
    public void testDestroyActuallyDeactivates() {
        mSessionState.activate(mActivity, mWebContents, mProfileSupplier, null);
        assertTrue(mSessionState.isSessionActive());

        mSessionState.destroy();
        assertFalse(mSessionState.isSessionActive());
    }

    @Test(expected = AssertionError.class)
    public void testActivateRequiresWebContents() {
        mSessionState.activate(mActivity, null, mProfileSupplier, null);
    }
}
