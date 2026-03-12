// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.glic;

import static org.mockito.Mockito.verify;

import android.content.Context;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.ContextUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.toolbar.optional_button.ButtonData;
import org.chromium.components.feature_engagement.Tracker;

/** Unit tests for {@link GlicToolbarButtonController}. */
@RunWith(BaseRobolectricTestRunner.class)
public class GlicToolbarButtonControllerTest {
    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private Tab mTab;
    @Mock private Runnable mToggleGlicCallback;
    @Mock private Tracker mTracker;

    private Context mContext;

    @Before
    public void setUp() {
        mContext = ContextUtils.getApplicationContext();
    }

    @Test
    public void testButtonData() {
        GlicToolbarButtonController controller =
                new GlicToolbarButtonController(
                        mContext, () -> mTab, mToggleGlicCallback, () -> mTracker);
        ButtonData buttonData = controller.get(mTab);

        Assert.assertTrue(buttonData.canShow());
        Assert.assertTrue(buttonData.isEnabled());
        Assert.assertNotNull(buttonData.getButtonSpec());
        Assert.assertEquals(
                mContext.getString(R.string.glic_button_entrypoint_ask_gemini_label),
                buttonData.getButtonSpec().getContentDescription());
    }

    @Test
    public void testOnClick() {
        GlicToolbarButtonController controller =
                new GlicToolbarButtonController(
                        mContext, () -> mTab, mToggleGlicCallback, () -> mTracker);

        controller.onClick(null);

        verify(mToggleGlicCallback).run();
    }
}
