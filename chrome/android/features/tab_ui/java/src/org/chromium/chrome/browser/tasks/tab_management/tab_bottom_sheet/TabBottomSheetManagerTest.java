// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management.tab_bottom_sheet;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.widget.FrameLayout;

import androidx.test.core.app.ApplicationProvider;

import org.junit.After;
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
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tabmodel.TabModelObserver;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.ui.base.WindowAndroid;

/** Unit tests for {@link TabBottomSheetManager}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
@EnableFeatures({ChromeFeatureList.TAB_BOTTOM_SHEET})
public class TabBottomSheetManagerTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();
    @Mock private BottomSheetController mMockBottomSheetController;
    @Mock private Profile mMockProfile;
    @Mock private WindowAndroid mMockWindowAndroid;
    @Mock private TabBottomSheetToolbar mMockToolbar;

    @Captor private ArgumentCaptor<TabModelObserver> mTabModelObserverCaptor;

    private TabBottomSheetManager mManager;

    @Before
    public void setUp() {
        // Setup generic toolbar
        when(mMockToolbar.getToolbarView())
                .thenReturn(new FrameLayout(ApplicationProvider.getApplicationContext()));
    }

    @After
    public void tearDown() {
        if (mManager != null) {
            mManager.destroy();
        }
    }

    /** Helper to create the manager with a specific feature flag state. */
    private void createManager() {
        mManager =
                new TabBottomSheetManager(
                        ApplicationProvider.getApplicationContext(),
                        mMockProfile,
                        mMockWindowAndroid,
                        mMockBottomSheetController);
    }

    @Test
    @DisableFeatures({ChromeFeatureList.TAB_BOTTOM_SHEET})
    public void testTryToShowBottomSheet_FeatureDisabled_NoBottomSheet() {
        createManager();
        mManager.tryToShowBottomSheet(mMockToolbar);
        verify(mMockBottomSheetController, never()).requestShowContent(any(), anyBoolean());
    }
}
