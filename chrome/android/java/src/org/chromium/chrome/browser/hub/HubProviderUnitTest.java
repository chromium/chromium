// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.hub;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.app.Activity;

import androidx.test.ext.junit.rules.ActivityScenarioRule;
import androidx.test.filters.SmallTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.shadows.ShadowLooper;

import org.chromium.base.Callback;
import org.chromium.base.supplier.LazyOneshotSupplier;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.supplier.OneshotSupplierImpl;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.chrome.browser.back_press.BackPressManager;
import org.chromium.chrome.browser.profiles.ProfileProvider;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.toolbar.menu_button.MenuButtonCoordinator;
import org.chromium.chrome.browser.ui.edge_to_edge.EdgeToEdgeController;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.chrome.browser.ui.searchactivityutils.SearchActivityClient;
import org.chromium.components.browser_ui.widget.MenuOrKeyboardActionController;
import org.chromium.ui.base.TestActivity;

/** Unit tests for {@link HubProvider}. */
@RunWith(BaseRobolectricTestRunner.class)
public class HubProviderUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Rule
    public ActivityScenarioRule<TestActivity> mActivityScenarioRule =
            new ActivityScenarioRule<>(TestActivity.class);

    private final ObservableSupplierImpl<Integer> mTabCountSupplier =
            new ObservableSupplierImpl<>();
    private final ObservableSupplierImpl<Tab> mTabSupplierMock = new ObservableSupplierImpl<>();
    private final ObservableSupplierImpl<TabModel> mTabModelSupplier =
            new ObservableSupplierImpl<>();
    private final ObservableSupplierImpl<DisplayButtonData> mReferenceButtonDataSupplier =
            new ObservableSupplierImpl<>();
    private final OneshotSupplierImpl<ProfileProvider> mProfileProviderSupplier =
            new OneshotSupplierImpl<>();
    private final ObservableSupplierImpl<EdgeToEdgeController> mEdgeToEdgeSupplier =
            new ObservableSupplierImpl<>();

    @Mock private Callback<HubManager> mHubManagerCallback;
    @Mock private DisplayButtonData mReferenceButtonData;
    @Mock private TabModel mTabModel;
    @Mock private TabModelSelector mTabModelSelector;
    @Mock private Pane mMockTabSwitcherPane;
    @Mock private Pane mMockIncognitoTabSwitcherPane;
    @Mock private Pane mMockBookmarksPane;
    @Mock private BackPressManager mBackPressManagerMock;
    @Mock private MenuOrKeyboardActionController mMenuOrKeyboardActionController;
    @Mock private SnackbarManager mSnackbarManager;
    @Mock private MenuButtonCoordinator mMenuButtonCoordinator;
    @Mock private SearchActivityClient mSearchActivityClient;

    private Activity mActivity;
    private HubProvider mHubProvider;

    @Before
    public void setUp() {
        when(mTabModelSelector.getModel(false)).thenReturn(mTabModel);
        when(mTabModelSelector.getModel(true)).thenReturn(mTabModel);
        when(mTabModelSelector.getCurrentTabModelSupplier()).thenReturn(mTabModelSupplier);
        mTabModelSupplier.set(mTabModel);

        mReferenceButtonDataSupplier.set(mReferenceButtonData);
        when(mMockTabSwitcherPane.getPaneId()).thenReturn(PaneId.TAB_SWITCHER);
        when(mMockTabSwitcherPane.getReferenceButtonDataSupplier())
                .thenReturn(mReferenceButtonDataSupplier);
        when(mMockIncognitoTabSwitcherPane.getPaneId()).thenReturn(PaneId.INCOGNITO_TAB_SWITCHER);
        when(mMockIncognitoTabSwitcherPane.getReferenceButtonDataSupplier())
                .thenReturn(mReferenceButtonDataSupplier);
        when(mMockBookmarksPane.getPaneId()).thenReturn(PaneId.BOOKMARKS);
        when(mMockBookmarksPane.getReferenceButtonDataSupplier())
                .thenReturn(mReferenceButtonDataSupplier);

        when(mTabModelSelector.getCurrentTabSupplier()).thenReturn(mTabSupplierMock);
        when(mTabModelSelector.getCurrentModelTabCountSupplier()).thenReturn(mTabCountSupplier);
        mTabCountSupplier.set(0);
        mActivityScenarioRule.getScenario().onActivity(this::onActivity);
    }

    private void onActivity(Activity activity) {
        mActivity = activity;

        mHubProvider =
                new HubProvider(
                        mActivity,
                        mProfileProviderSupplier,
                        new DefaultPaneOrderController(),
                        mBackPressManagerMock,
                        mMenuOrKeyboardActionController,
                        () -> mSnackbarManager,
                        () -> mTabModelSelector,
                        () -> mMenuButtonCoordinator,
                        mEdgeToEdgeSupplier,
                        mSearchActivityClient);
    }

    @Test
    @SmallTest
    public void testHubProvider() {
        PaneListBuilder builder = mHubProvider.getPaneListBuilder();

        var hubManagerSupplier = mHubProvider.getHubManagerSupplier();
        assertNotNull(hubManagerSupplier);
        assertFalse(hubManagerSupplier.hasValue());
        hubManagerSupplier.onAvailable(mHubManagerCallback);

        builder.registerPane(
                PaneId.TAB_SWITCHER, LazyOneshotSupplier.fromValue(mMockTabSwitcherPane));
        builder.registerPane(
                PaneId.INCOGNITO_TAB_SWITCHER,
                LazyOneshotSupplier.fromValue(mMockIncognitoTabSwitcherPane));
        builder.registerPane(PaneId.BOOKMARKS, LazyOneshotSupplier.fromValue(mMockBookmarksPane));
        assertFalse(builder.isBuilt());

        HubManager hubManager = hubManagerSupplier.get();
        assertNotNull(hubManager);
        assertTrue(hubManagerSupplier.hasValue());
        assertTrue(builder.isBuilt());

        PaneManager paneManager = hubManager.getPaneManager();
        assertNotNull(paneManager);

        ShadowLooper.runUiThreadTasks();
        when(mTabModelSelector.isIncognitoSelected()).thenReturn(false);

        paneManager.focusPane(PaneId.TAB_SWITCHER);
        assertEquals(mMockTabSwitcherPane, paneManager.getFocusedPaneSupplier().get());
        verify(mTabModelSelector, never()).commitAllTabClosures();
        verify(mTabModelSelector, never()).selectModel(anyBoolean());

        paneManager.focusPane(PaneId.BOOKMARKS);
        assertEquals(mMockBookmarksPane, paneManager.getFocusedPaneSupplier().get());
        verify(mTabModelSelector, never()).commitAllTabClosures();
        verify(mTabModelSelector, never()).selectModel(anyBoolean());

        HistogramWatcher watcher =
                HistogramWatcher.newSingleRecordWatcher(
                        "Android.TabSwitcher.IncognitoClickedIsEmpty", true);
        paneManager.focusPane(PaneId.INCOGNITO_TAB_SWITCHER);
        assertEquals(mMockIncognitoTabSwitcherPane, paneManager.getFocusedPaneSupplier().get());
        verify(mTabModelSelector).commitAllTabClosures();
        verify(mTabModelSelector).selectModel(true);
        watcher.assertExpected();

        when(mTabModelSelector.isIncognitoSelected()).thenReturn(true);

        paneManager.focusPane(PaneId.TAB_SWITCHER);
        assertEquals(mMockTabSwitcherPane, paneManager.getFocusedPaneSupplier().get());
        verify(mTabModelSelector, times(2)).commitAllTabClosures();
        verify(mTabModelSelector).selectModel(false);

        verify(mHubManagerCallback).bind(any());

        mHubProvider.destroy();
    }

    @Test
    @SmallTest
    public void testHubProviderDestroyBeforeOnAvailable() {
        PaneListBuilder builder = mHubProvider.getPaneListBuilder();

        var hubManagerSupplier = mHubProvider.getHubManagerSupplier();
        assertNotNull(hubManagerSupplier);
        assertFalse(hubManagerSupplier.hasValue());

        builder.registerPane(
                PaneId.TAB_SWITCHER, LazyOneshotSupplier.fromValue(mMockTabSwitcherPane));
        builder.registerPane(
                PaneId.INCOGNITO_TAB_SWITCHER,
                LazyOneshotSupplier.fromValue(mMockIncognitoTabSwitcherPane));
        builder.registerPane(PaneId.BOOKMARKS, LazyOneshotSupplier.fromValue(mMockBookmarksPane));
        assertFalse(builder.isBuilt());

        HubManager hubManager = hubManagerSupplier.get();
        assertNotNull(hubManager);
        assertTrue(hubManagerSupplier.hasValue());
        assertTrue(builder.isBuilt());

        PaneManager paneManager = hubManager.getPaneManager();
        assertNotNull(paneManager);

        // This shouldn't crash.
        mHubProvider.destroy();

        ShadowLooper.runUiThreadTasks();
    }

    @Test
    @SmallTest
    public void testHubProviderDestroyBeforeCreation() {
        PaneListBuilder builder = mHubProvider.getPaneListBuilder();

        var hubManagerSupplier = mHubProvider.getHubManagerSupplier();
        assertNotNull(hubManagerSupplier);
        assertFalse(hubManagerSupplier.hasValue());
        hubManagerSupplier.onAvailable(mHubManagerCallback);

        builder.registerPane(
                PaneId.TAB_SWITCHER, LazyOneshotSupplier.fromValue(mMockTabSwitcherPane));
        builder.registerPane(
                PaneId.INCOGNITO_TAB_SWITCHER,
                LazyOneshotSupplier.fromValue(mMockIncognitoTabSwitcherPane));
        builder.registerPane(PaneId.BOOKMARKS, LazyOneshotSupplier.fromValue(mMockBookmarksPane));
        assertFalse(builder.isBuilt());

        assertFalse(hubManagerSupplier.hasValue());
        mHubProvider.destroy();
        ShadowLooper.runUiThreadTasks();

        assertFalse(hubManagerSupplier.hasValue());
        verify(mHubManagerCallback, never()).onResult(any());
        verify(mHubManagerCallback, never()).bind(any());
    }
}
