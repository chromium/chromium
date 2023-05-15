// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.paint_preview;

import android.content.Context;
import android.provider.Settings;
import android.text.TextUtils;

import androidx.test.core.app.ApplicationProvider;

import org.junit.runners.model.InitializationError;

import org.chromium.base.test.util.RestrictionSkipCheck;
import org.chromium.base.test.util.SkipCheck;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;

import java.util.List;

/**
 * Test runner class that handles a custom {@link Restriction} type.
 */
public class StartupPaintPreviewHelperTestRunner extends ChromeJUnit4ClassRunner {
    /**
     * The test is only valid if Settings.Global.ALWAYS_FINISH_ACTIVITIES on device settings is
     * disabled.
     */
    public static final String RESTRICTION_TYPE_KEEP_ACTIVITIES = "Keep_Activities";

    public StartupPaintPreviewHelperTestRunner(Class<?> klass) throws InitializationError {
        super(klass);
    }

    @Override
    protected List<SkipCheck> getSkipChecks() {
        return addToList(super.getSkipChecks(),
                new StartupPaintPreviewSkipCheck(ApplicationProvider.getApplicationContext()));
    }

    private static class StartupPaintPreviewSkipCheck extends RestrictionSkipCheck {
        public StartupPaintPreviewSkipCheck(Context targetContext) {
            super(targetContext);
        }

        @Override
        protected boolean restrictionApplies(String restriction) {
            if (TextUtils.equals(restriction, RESTRICTION_TYPE_KEEP_ACTIVITIES)) {
                int alwaysFinishActivities =
                        Settings.System.getInt(getTargetContext().getContentResolver(),
                                Settings.Global.ALWAYS_FINISH_ACTIVITIES, 0);
                return alwaysFinishActivities != 0;
            }
            return super.restrictionApplies(restriction);
        }
    }
}
