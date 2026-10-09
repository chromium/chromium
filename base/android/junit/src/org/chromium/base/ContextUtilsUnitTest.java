// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertSame;
import static org.junit.Assert.assertTrue;

import android.app.Activity;
import android.content.Context;
import android.content.ContextWrapper;
import android.view.ContextThemeWrapper;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.Robolectric;

import org.chromium.base.test.BaseRobolectricTestRunner;

/** Unit tests for {@link ContextUtils}. */
@RunWith(BaseRobolectricTestRunner.class)
public class ContextUtilsUnitTest {

    @Test
    public void testGetBaseContext() {
        Context appContext = ContextUtils.getApplicationContext();
        Context baseContext = ContextUtils.getBaseContext(appContext);
        ContextWrapper wrappedOnce = new ContextWrapper(appContext);
        ContextWrapper wrappedTwice = new ContextWrapper(wrappedOnce);

        assertSame(baseContext, ContextUtils.getBaseContext(wrappedOnce));
        assertSame(baseContext, ContextUtils.getBaseContext(wrappedTwice));

        ContextWrapper unattached = new ContextWrapper(null);
        assertSame(unattached, ContextUtils.getBaseContext(unattached));
    }

    @Test
    public void testIsSameActivity_sameActivity() {
        Activity activity = Robolectric.buildActivity(Activity.class).create().get();
        ContextWrapper wrappedOnce = new ContextWrapper(activity);
        ContextWrapper wrappedTwice = new ContextWrapper(wrappedOnce);

        assertTrue(ContextUtils.isSameActivity(activity, wrappedOnce));
        assertTrue(ContextUtils.isSameActivity(wrappedOnce, wrappedTwice));
    }

    @Test
    public void testIsSameActivity_differentActivities() {
        Activity activity1 = Robolectric.buildActivity(Activity.class).create().get();
        Activity activity2 = Robolectric.buildActivity(Activity.class).create().get();

        assertFalse(ContextUtils.isSameActivity(activity1, activity2));
        assertFalse(
                ContextUtils.isSameActivity(
                        new ContextWrapper(activity1), new ContextWrapper(activity2)));
    }

    @Test
    public void testIsSameActivity_applicationAndActivity() {
        Activity activity = Robolectric.buildActivity(Activity.class).create().get();
        Context appContext = ContextUtils.getApplicationContext();

        assertFalse(ContextUtils.isSameActivity(appContext, activity));
        assertFalse(ContextUtils.isSameActivity(appContext, new ContextWrapper(appContext)));
    }

    @Test
    public void testIsSameActivity_nullContexts() {
        Context appContext = ContextUtils.getApplicationContext();

        assertFalse(ContextUtils.isSameActivity(null, null));
        assertFalse(ContextUtils.isSameActivity(null, appContext));
        assertFalse(ContextUtils.isSameActivity(appContext, null));
    }

    @Test
    public void testActivityFromContext() {
        assertNull(ContextUtils.activityFromContext(null));

        Activity activity = Robolectric.buildActivity(Activity.class).create().get();
        assertSame(activity, ContextUtils.activityFromContext(activity));

        ContextThemeWrapper themeWrapper = new ContextThemeWrapper(activity, 0);
        assertSame(activity, ContextUtils.activityFromContext(themeWrapper));

        ContextWrapper wrappedThemeWrapper = new ContextWrapper(themeWrapper);
        assertSame(activity, ContextUtils.activityFromContext(wrappedThemeWrapper));

        Context appContext = ContextUtils.getApplicationContext();
        assertNull(ContextUtils.activityFromContext(appContext));
        assertNull(ContextUtils.activityFromContext(new ContextWrapper(appContext)));
    }
}
