// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import android.app.Activity;
import android.view.ViewGroup;
import android.widget.FrameLayout;

import androidx.test.ext.junit.rules.ActivityScenarioRule;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.supplier.OneshotSupplierImpl;
import org.chromium.base.supplier.Supplier;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.bookmarks.TabBookmarker;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider;
import org.chromium.chrome.browser.data_sharing.DataSharingTabManager;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.layouts.LayoutStateProvider;
import org.chromium.chrome.browser.share.ShareDelegate;
import org.chromium.chrome.browser.tab_ui.TabContentManager;
import org.chromium.chrome.browser.tabmodel.TabCreatorManager;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.theme.ThemeColorProvider;
import org.chromium.chrome.browser.undo_tab_close_snackbar.UndoBarThrottle;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.widget.scrim.ScrimManager;
import org.chromium.ui.base.TestActivity;
import org.chromium.ui.modaldialog.ModalDialogManager;

/** Unit tests for {@link TabGroupUiCoordinator}. */
@RunWith(BaseRobolectricTestRunner.class)
@EnableFeatures(ChromeFeatureList.DATA_SHARING)
public class TabGroupUiCoordinatorUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Rule
    public ActivityScenarioRule<TestActivity> mActivityScenarioRule =
            new ActivityScenarioRule<TestActivity>(TestActivity.class);

    @Mock private BrowserControlsStateProvider mBrowserControlsStateProvider;
    @Mock private ScrimManager mScrimManager;
    @Mock private BottomSheetController mBottomSheetController;
    @Mock private DataSharingTabManager mDataSharingTabManager;
    @Mock private TabModelSelector mTabModelSelector;
    @Mock private TabContentManager mTabContentManager;
    @Mock private TabCreatorManager mTabCreatorManager;
    @Mock private ModalDialogManager mModalDialogManager;
    @Mock private ThemeColorProvider mThemeColorProvider;
    @Mock private UndoBarThrottle mUndoBarThrottle;
    @Mock private ObservableSupplier<TabBookmarker> mTabBookmarkerSupplier;
    @Mock private Supplier<ShareDelegate> mShareDelegateSupplier;

    private Activity mActivity;

    private final ObservableSupplierImpl<Boolean> mOmniboxFocusStateSupplier =
            new ObservableSupplierImpl<>(false);
    private final OneshotSupplierImpl<LayoutStateProvider> mLayoutStateProviderSupplier =
            new OneshotSupplierImpl<>();

    @Before
    public void setUp() {
        mActivityScenarioRule.getScenario().onActivity(activity -> mActivity = activity);
    }

    @Test
    public void testDestroy() {
        ViewGroup parentView = new FrameLayout(mActivity);
        TabGroupUiCoordinator coordinator =
                new TabGroupUiCoordinator(
                        mActivity,
                        parentView,
                        mBrowserControlsStateProvider,
                        mScrimManager,
                        mOmniboxFocusStateSupplier,
                        mBottomSheetController,
                        mDataSharingTabManager,
                        mTabModelSelector,
                        mTabContentManager,
                        mTabCreatorManager,
                        mLayoutStateProviderSupplier,
                        mModalDialogManager,
                        mThemeColorProvider,
                        mUndoBarThrottle,
                        mTabBookmarkerSupplier,
                        mShareDelegateSupplier);
        coordinator.destroy();
        // When called before #initializeWithNative, it should not result in a crash.
    }
}
