// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.chrome_item_picker;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;

import android.content.Context;
import android.content.Intent;
import android.graphics.drawable.ColorDrawable;
import android.graphics.drawable.Drawable;

import androidx.test.filters.MediumTest;

import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ContextUtils;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.BaseActivityTestRule;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.DoNotBatch;
import org.chromium.chrome.browser.IntentHandler;
import org.chromium.chrome.browser.omnibox.fusebox.FuseboxMediator;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.components.browser_ui.styles.ChromeColors;

/** Instrumentation Test for ChromeItemPickerActivity. */
@RunWith(ChromeJUnit4ClassRunner.class)
@DoNotBatch(reason = "Testing ChromeItemPickerActivity launch behavior.")
public class ChromeItemPickerActivityTest {
    @Rule
    public BaseActivityTestRule<ChromeItemPickerActivity> mActivityRule =
            new BaseActivityTestRule<>(ChromeItemPickerActivity.class);

    private static final int TEST_WINDOW_ID = 1;

    private Intent createPickerIntent(boolean isIncognito) {
        Context context = ContextUtils.getApplicationContext();
        Intent intent = new Intent(context, ChromeItemPickerActivity.class);
        intent.putExtra(FuseboxMediator.EXTRA_IS_INCOGNITO_BRANDED, isIncognito);
        intent.putExtra(IntentHandler.EXTRA_WINDOW_ID, TEST_WINDOW_ID);
        intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);

        return intent;
    }

    private void doTestActivityThemeColor(boolean isIncognito) {
        // EnableFeature(ChromeFeatureList.ANDROID_OPEN_INCOGNITO_AS_WINDOW)
        final ChromeItemPickerActivity activity =
                mActivityRule.launchActivity(createPickerIntent(isIncognito));

        final int expectedColor = ChromeColors.getDefaultThemeColor(activity, isIncognito);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    // Get the decor view background, which should reflect the theme color set
                    // in initializeSystemBarColors.
                    Drawable background = activity.getWindow().getDecorView().getBackground();

                    assertNotNull("Activity DecorView background must not be null.", background);
                    assertTrue(
                            "Background must be a ColorDrawable to check color directly.",
                            background instanceof ColorDrawable);

                    final int actualColor = ((ColorDrawable) background).getColor();

                    assertEquals(expectedColor, actualColor);
                });
    }

    @Test
    @MediumTest
    public void testActivityThemeColorIsIncognito() {
        doTestActivityThemeColor(true);
    }

    @Test
    @MediumTest
    @DisabledTest(message = "https://crbug.com/463427787")
    public void testActivityThemeColorIsDefault() {
        doTestActivityThemeColor(false);
    }
}
