// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import android.content.Context;
import android.view.ContextThemeWrapper;
import android.view.LayoutInflater;
import android.view.View;

import androidx.test.core.app.ApplicationProvider;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.chrome.tab_ui.R;

/** Unit tests for {@link TabGroupCreationTextInputLayout}. */
@Batch(Batch.UNIT_TESTS)
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class TabGroupCreationTextInputLayoutTest {
    private Context mContext;
    private TabGroupCreationTextInputLayout mTabGroupCreationTextInputLayout;

    @Before
    public void setUp() {
        mContext =
                new ContextThemeWrapper(
                        ApplicationProvider.getApplicationContext(),
                        R.style.Theme_BrowserUI_DayNight);

        View customView =
                LayoutInflater.from(mContext).inflate(R.layout.tab_group_creation_dialog, null);
        mTabGroupCreationTextInputLayout = customView.findViewById(R.id.tab_group_title);
    }

    @Test
    public void testValidate() {
        mTabGroupCreationTextInputLayout.getEditText().setText("test");
        assertTrue(mTabGroupCreationTextInputLayout.validate());
    }

    @Test
    public void testValidate_empty() {
        assertFalse(mTabGroupCreationTextInputLayout.validate());
    }

    @Test
    public void testTrimmedText() {
        mTabGroupCreationTextInputLayout.getEditText().setText(" test ");
        assertEquals("test", mTabGroupCreationTextInputLayout.getTrimmedText());
    }
}
