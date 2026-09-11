// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download.settings;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;

import android.app.Activity;
import android.content.Context;
import android.util.TypedValue;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.Robolectric;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.download.R;

/** Unit tests for {@link DownloadLocationPreferenceDialog}. */
@RunWith(BaseRobolectricTestRunner.class)
public class DownloadLocationPreferenceDialogUnitTest {
    @Test
    public void testDialogUsesSettingsAlertDialogTheme() {
        // Typed as Context so this resolves to Fragment#onAttach(Context) rather than the
        // deprecated Fragment#onAttach(Activity) overload.
        Context activity = Robolectric.buildActivity(Activity.class).setup().get();
        DownloadLocationPreferenceDialog dialog = new DownloadLocationPreferenceDialog();
        dialog.onAttach(activity);

        // The dialog must use the Chrome alert dialog theme, which provides rounded corners, even
        // when it is hosted by an activity that does not use the settings theme.
        Context context = dialog.getContext();
        assertNotNull(context);
        TypedValue value = new TypedValue();
        assertTrue(context.getTheme().resolveAttribute(R.attr.alertDialogTheme, value, true));
        assertEquals(R.style.ThemeOverlay_BrowserUI_AlertDialog, value.resourceId);
    }

    @Test
    public void testThemedContextClearedOnDetach() {
        // Attach the dialog, which creates the themed context.
        Context activity = Robolectric.buildActivity(Activity.class).setup().get();
        DownloadLocationPreferenceDialog dialog = new DownloadLocationPreferenceDialog();
        dialog.onAttach(activity);
        assertNotNull(dialog.getContext());

        // Detaching drops the host.
        dialog.onDetach();

        // The dialog must not hold on to a context for the detached activity.
        assertNull(dialog.getContext());
    }
}
