// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.settings;

import static org.junit.Assert.assertFalse;

import android.content.Context;

import androidx.test.core.app.ApplicationProvider;
import androidx.test.filters.SmallTest;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseRobolectricTestRunner;

/** Unit tests for {@link AutofillProfileEditorPreference}. */
@RunWith(BaseRobolectricTestRunner.class)
public class AutofillProfileEditorPreferenceTest {
    private Context mContext;

    @Before
    public void setUp() {
        mContext = ApplicationProvider.getApplicationContext();
    }

    @Test
    @SmallTest
    public void testIconSpaceReservedDefaultFalse() {
        AutofillProfileEditorPreference preference = new AutofillProfileEditorPreference(mContext);
        assertFalse(preference.isIconSpaceReserved());
    }
}
