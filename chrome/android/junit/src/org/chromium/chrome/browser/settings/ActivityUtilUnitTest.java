// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.settings;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNull;

import android.app.Activity;
import android.content.Context;
import android.content.ContextWrapper;
import android.view.ContextThemeWrapper;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.Robolectric;
import org.robolectric.RuntimeEnvironment;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;

/** Unit tests for {@link ActivityUtil}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class ActivityUtilUnitTest {
    private Activity mActivity;

    @Before
    public void setUp() {
        mActivity = Robolectric.buildActivity(Activity.class).setup().get();
    }

    @Test
    public void testGetActivityFromContext_NullContext() {
        assertNull(ActivityUtil.getActivityFromContext(null));
    }

    @Test
    public void testGetActivityFromContext_ActivityContext() {
        assertEquals(mActivity, ActivityUtil.getActivityFromContext(mActivity));
    }

    @Test
    public void testGetActivityFromContext_ContextThemeWrapper() {
        Context themeWrapper = new ContextThemeWrapper(mActivity, 0);
        assertEquals(mActivity, ActivityUtil.getActivityFromContext(themeWrapper));
    }

    @Test
    public void testGetActivityFromContext_NestedContextWrapper() {
        Context themeWrapper = new ContextThemeWrapper(mActivity, 0);
        Context wrapper = new ContextWrapper(themeWrapper);
        assertEquals(mActivity, ActivityUtil.getActivityFromContext(wrapper));
    }

    @Test
    public void testGetActivityFromContext_NonActivityContext() {
        Context appContext = RuntimeEnvironment.getApplication();
        assertNull(ActivityUtil.getActivityFromContext(appContext));
    }

    @Test
    public void testGetActivityFromContext_WrapperWithNonActivityContext() {
        Context appContext = RuntimeEnvironment.getApplication();
        Context wrapper = new ContextWrapper(appContext);
        assertNull(ActivityUtil.getActivityFromContext(wrapper));
    }
}
