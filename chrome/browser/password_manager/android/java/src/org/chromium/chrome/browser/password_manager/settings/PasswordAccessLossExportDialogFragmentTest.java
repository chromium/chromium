// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.password_manager.settings;

import static org.junit.Assert.assertEquals;

import android.app.Dialog;
import android.content.res.Resources;
import android.widget.Button;
import android.widget.TextView;

import androidx.fragment.app.FragmentActivity;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.Robolectric;
import org.robolectric.RuntimeEnvironment;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.chrome.browser.password_manager.R;
import org.chromium.components.browser_ui.test.BrowserUiDummyFragmentActivity;

/** Tests for {@link PasswordAccessLossExportDialogFragment} */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
@Batch(Batch.PER_CLASS)
public class PasswordAccessLossExportDialogFragmentTest {
    private PasswordAccessLossExportDialogFragment mFragment;
    private FragmentActivity mActivity;

    @Before
    public void setUp() {
        mActivity =
                Robolectric.buildActivity(BrowserUiDummyFragmentActivity.class)
                        .create()
                        .start()
                        .resume()
                        .get();
        mFragment = new PasswordAccessLossExportDialogFragment();
    }

    @Test
    public void testDialogStrings() {
        mFragment.show(mActivity.getSupportFragmentManager(), null);
        mActivity.getSupportFragmentManager().executePendingTransactions();

        Resources resources = RuntimeEnvironment.getApplication().getResources();
        Dialog dialog = mFragment.getDialog();
        assertEquals(
                resources.getString(R.string.access_loss_export_dialog_title),
                ((TextView) dialog.findViewById(R.id.title)).getText());
        assertEquals(
                resources.getString(R.string.access_loss_export_dialog_message),
                ((TextView) dialog.findViewById(R.id.message)).getText());
        assertEquals(
                resources.getString(R.string.access_loss_export_dialog_positive_button_text),
                ((Button) dialog.findViewById(R.id.positive_button)).getText());
        assertEquals(
                resources.getString(R.string.cancel),
                ((Button) dialog.findViewById(R.id.negative_button)).getText());
    }
}
