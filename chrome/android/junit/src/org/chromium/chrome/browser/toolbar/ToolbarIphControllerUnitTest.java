// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.verify;

import static org.chromium.chrome.browser.toolbar.ToolbarIphController.GLIC_IPH_AUTO_DISMISS_TIMEOUT_MS;

import android.content.Context;
import android.view.View;

import androidx.test.core.app.ApplicationProvider;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.user_education.IphCommand;
import org.chromium.chrome.browser.user_education.UserEducationHelper;
import org.chromium.components.feature_engagement.FeatureConstants;

/** Unit tests for {@link ToolbarIphController}. */
@RunWith(BaseRobolectricTestRunner.class)
@Batch(Batch.UNIT_TESTS)
@Config(manifest = Config.NONE)
public class ToolbarIphControllerUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private UserEducationHelper mUserEducationHelper;
    @Mock private View mAnchorView;
    @Captor private ArgumentCaptor<IphCommand> mIphCommandCaptor;

    private Context mContext;
    private ToolbarIphController mController;

    @Before
    public void setUp() {
        mContext = ApplicationProvider.getApplicationContext();
        mController = new ToolbarIphController(mContext, mUserEducationHelper);
    }

    @Test
    public void testShowGlicIph() {
        mController.showGlicIph(mAnchorView);

        verify(mUserEducationHelper).requestShowIph(mIphCommandCaptor.capture());
        IphCommand cmd = mIphCommandCaptor.getValue();
        assertNotNull(cmd);
        assertEquals(FeatureConstants.GLIC_PROMO_ANDROID_FEATURE, cmd.featureName);
        assertEquals(mAnchorView, cmd.anchorView);
        assertEquals(R.string.iph_glic_promo_text, cmd.stringId);
        assertEquals(R.string.iph_glic_promo_accessibility_text, cmd.accessibilityStringId);
        assertTrue(cmd.dismissOnTouch);
        assertEquals(GLIC_IPH_AUTO_DISMISS_TIMEOUT_MS, cmd.autoDismissTimeout);
    }

    @Test
    public void testShowBottomToolbarIph() {
        mController.showBottomToolbarIph(mAnchorView);

        verify(mUserEducationHelper).requestShowIph(mIphCommandCaptor.capture());
        IphCommand cmd = mIphCommandCaptor.getValue();
        assertNotNull(cmd);
        assertEquals(FeatureConstants.BOTTOM_TOOLBAR_FEATURE, cmd.featureName);
        assertEquals(mAnchorView, cmd.anchorView);
    }
}
