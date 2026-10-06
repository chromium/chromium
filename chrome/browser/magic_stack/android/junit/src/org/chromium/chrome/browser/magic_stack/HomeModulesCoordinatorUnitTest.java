// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.magic_stack;

import static android.view.ViewGroup.LayoutParams.MATCH_PARENT;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.reset;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;
import static org.robolectric.Shadows.shadowOf;

import static org.chromium.chrome.browser.magic_stack.CirclePagerIndicatorDecoration.getItemPerScreen;

import android.app.Activity;
import android.graphics.Color;
import android.view.View;
import android.view.View.OnCreateContextMenuListener;
import android.view.ViewGroup;
import android.view.ViewGroup.MarginLayoutParams;
import android.widget.FrameLayout;

import androidx.recyclerview.widget.RecyclerView;

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
import org.robolectric.Robolectric;
import org.robolectric.annotation.Config;

import org.chromium.base.Callback;
import org.chromium.base.CallbackUtils;
import org.chromium.base.FeatureOverrides;
import org.chromium.base.supplier.ObservableSuppliers;
import org.chromium.base.supplier.SettableMonotonicObservableSupplier;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.magic_stack.ModuleDelegate.ModuleType;
import org.chromium.chrome.browser.ntp.NewTabPageUtils.PaddingStyle;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileManager;
import org.chromium.chrome.browser.segmentation_platform.client_util.HomeModulesRankingHelper;
import org.chromium.chrome.browser.segmentation_platform.client_util.HomeModulesRankingHelperJni;
import org.chromium.components.browser_ui.styles.SemanticColorUtils;
import org.chromium.components.browser_ui.widget.displaystyle.DisplayStyleObserver;
import org.chromium.components.browser_ui.widget.displaystyle.HorizontalDisplayStyle;
import org.chromium.components.browser_ui.widget.displaystyle.UiConfig;
import org.chromium.components.browser_ui.widget.displaystyle.UiConfig.DisplayStyle;
import org.chromium.components.browser_ui.widget.displaystyle.VerticalDisplayStyle;
import org.chromium.components.segmentation_platform.ClassificationResult;
import org.chromium.components.segmentation_platform.prediction_status.PredictionStatus;
import org.chromium.ui.base.DeviceFormFactor;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.test.util.MockitoHelper;

import java.util.HashSet;
import java.util.Set;

@RunWith(BaseRobolectricTestRunner.class)
public class HomeModulesCoordinatorUnitTest {
    /** An adapter with a fixed number of plain items of the given width. */
    private static class FixedItemsAdapter extends RecyclerView.Adapter<RecyclerView.ViewHolder> {
        private final int mItemCount;
        private final int mItemWidth;

        FixedItemsAdapter(int itemCount, int itemWidth) {
            mItemCount = itemCount;
            mItemWidth = itemWidth;
        }

        @Override
        public int getItemCount() {
            return mItemCount;
        }

        @Override
        public RecyclerView.ViewHolder onCreateViewHolder(ViewGroup parent, int viewType) {
            View view = new View(parent.getContext());
            view.setLayoutParams(new RecyclerView.LayoutParams(mItemWidth, MATCH_PARENT));
            return new RecyclerView.ViewHolder(view) {};
        }

        @Override
        public void onBindViewHolder(RecyclerView.ViewHolder holder, int position) {}
    }

    // Sentinel value that differs from any real top margin.
    private static final int INITIAL_TOP_MARGIN = 1234;

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private ModuleDelegateHost mModuleDelegateHost;
    @Mock private UiConfig mUiConfig;
    @Mock private HomeModulesConfigManager mHomeModulesConfigManager;
    @Mock private Profile mProfile;
    @Mock private ModuleRegistry mModuleRegistry;
    @Mock private HomeModulesMediator mMediator;
    @Mock private ModelList mModel;
    @Mock private ModuleProvider mModuleProvider;
    @Mock private HomeModulesRankingHelper.Natives mHomeModulesRankingHelperJniMock;

    @Captor private ArgumentCaptor<DisplayStyleObserver> mDisplayStyleObserver;
    @Captor private ArgumentCaptor<Callback<ClassificationResult>> mClassificationResultCaptor;

    @Captor
    private ArgumentCaptor<HomeModulesConfigManager.HomeModulesStateListener>
            mHomeModulesStateListener;

    private final SettableMonotonicObservableSupplier<Profile> mProfileSupplier =
            ObservableSuppliers.createMonotonic();
    private Activity mActivity;
    private FrameLayout mView;
    private HomeModulesRecyclerView mRecyclerView;
    private HomeModulesCoordinator mCoordinator;

    @Before
    public void setUp() {
        SemanticColorUtils.setDefaultIconColorSecondaryForTesting(Color.LTGRAY);
        when(mModuleDelegateHost.getUiConfig()).thenReturn(mUiConfig);
        mActivity = Robolectric.buildActivity(Activity.class).setup().get();
        mActivity.setTheme(R.style.Theme_BrowserUI_DayNight);
        mView = new FrameLayout(mActivity);
        mActivity.getLayoutInflater().inflate(R.layout.home_modules_recycler_view_layout, mView);
        mRecyclerView = mView.findViewById(R.id.home_modules_recycler_view);
        Set<Integer> enabledModules = Set.of(ModuleType.PRICE_CHANGE, ModuleType.SINGLE_TAB);
        when(mModuleRegistry.getEnabledModuleSet()).thenReturn(new HashSet<>(enabledModules));

        // Register mock builders for enabled modules to prevent NPE in mediator.
        for (int type : enabledModules) {
            ModuleProviderBuilder builder = mock(ModuleProviderBuilder.class);
            when(builder.getManualRank()).thenReturn(null);
            when(mModuleRegistry.getModuleProviderBuilder(type)).thenReturn(builder);
        }

        ProfileManager.setLastUsedProfileForTesting(mProfile);
        HomeModulesRankingHelperJni.setInstanceForTesting(mHomeModulesRankingHelperJniMock);

        FeatureOverrides.newBuilder()
                .enable(ChromeFeatureList.SEGMENTATION_PLATFORM_EPHEMERAL_CARD_RANKER)
                .enable(ChromeFeatureList.SEGMENTATION_PLATFORM_ANDROID_HOME_MODULE_RANKER)
                .enable(ChromeFeatureList.SEGMENTATION_PLATFORM_ANDROID_HOME_MODULE_RANKER_V2)
                .apply();
    }

    @After
    public void tearDown() {
        mCoordinator.destroy();
    }

    @Test
    public void testCreate_phones() {
        assertFalse(DeviceFormFactor.isNonMultiDisplayContextOnTablet(mActivity));
        mCoordinator = createCoordinator(/* skipInitProfile= */ false);

        // Verifies that there isn't an observer of UiConfig registered.
        assertTrue(mCoordinator.getIsSnapHelperAttachedForTesting());
        verify(mUiConfig, never()).addObserver(mDisplayStyleObserver.capture());
    }

    @Test
    @Config(qualifiers = "sw600dp")
    public void testCreate_tablets() {
        assertTrue(DeviceFormFactor.isNonMultiDisplayContextOnTablet(mActivity));

        DisplayStyle displayStyle =
                new DisplayStyle(HorizontalDisplayStyle.WIDE, VerticalDisplayStyle.REGULAR);
        when(mUiConfig.getCurrentDisplayStyle()).thenReturn(displayStyle);

        mCoordinator = createCoordinator(/* skipInitProfile= */ false);
        // Verifies that an observer is registered to the mUiConfig on tablets.
        verify(mUiConfig).addObserver(mDisplayStyleObserver.capture());

        // Verifies that the snap scroll helper isn't attached to the recyclerview if there are more
        // than one item shown per screen.
        assertEquals(2, getItemPerScreen(displayStyle));
        assertFalse(mCoordinator.getIsSnapHelperAttachedForTesting());

        // Verifies that the snap scroll helper is attached to the recyclerview if there is only one
        // item shown per screen.
        DisplayStyle newDisplayStyle =
                new DisplayStyle(HorizontalDisplayStyle.REGULAR, VerticalDisplayStyle.REGULAR);
        mDisplayStyleObserver.getValue().onDisplayStyleChanged(newDisplayStyle);

        assertEquals(1, getItemPerScreen(newDisplayStyle));
        assertTrue(mCoordinator.getIsSnapHelperAttachedForTesting());
    }

    @Test
    public void testHide() {
        mCoordinator = createCoordinator(/* skipInitProfile= */ false);
        assertNotNull(mRecyclerView.getAdapter());

        mCoordinator.hide();
        assertNull(mRecyclerView.getAdapter());

        showWithSegmentation((isVisible) -> {});
        assertNotNull(mRecyclerView.getAdapter());
    }

    @Test
    @Config(qualifiers = "sw600dp")
    public void testDestroy() {
        assertTrue(DeviceFormFactor.isNonMultiDisplayContextOnTablet(mActivity));

        DisplayStyle displayStyle =
                new DisplayStyle(HorizontalDisplayStyle.REGULAR, VerticalDisplayStyle.REGULAR);
        when(mUiConfig.getCurrentDisplayStyle()).thenReturn(displayStyle);

        mCoordinator = createCoordinator(/* skipInitProfile= */ false);
        // Verifies that an observer is registered to the mUiConfig on tablets.
        verify(mUiConfig).addObserver(mDisplayStyleObserver.capture());
        assertTrue(mCoordinator.getIsSnapHelperAttachedForTesting());

        mCoordinator.destroy();
        verify(mUiConfig).removeObserver(mDisplayStyleObserver.capture());
        assertNull(mCoordinator.getHomeModulesContextMenuManagerForTesting());
        assertFalse(mCoordinator.getIsSnapHelperAttachedForTesting());
    }

    @Test
    public void testOnModuleConfigChanged() {
        assertFalse(DeviceFormFactor.isNonMultiDisplayContextOnTablet(mActivity));
        when(mModuleDelegateHost.isHomeSurface()).thenReturn(true);
        mCoordinator = createCoordinator(/* skipInitProfile= */ false);

        verify(mHomeModulesConfigManager).addListener(mHomeModulesStateListener.capture());
        Set<Integer> expectedModuleListBeforeHidingModule =
                Set.of(ModuleType.PRICE_CHANGE, ModuleType.SINGLE_TAB);
        assertEquals(
                expectedModuleListBeforeHidingModule,
                mCoordinator.getFilteredEnabledModuleSetForTesting());

        mHomeModulesStateListener.getValue().onModuleConfigChanged(ModuleType.PRICE_CHANGE, false);
        Set<Integer> expectedModuleListAfterHidingModule = Set.of(ModuleType.SINGLE_TAB);
        assertEquals(
                expectedModuleListAfterHidingModule,
                mCoordinator.getFilteredEnabledModuleSetForTesting());

        mHomeModulesStateListener.getValue().onModuleConfigChanged(ModuleType.PRICE_CHANGE, true);
        assertEquals(
                expectedModuleListBeforeHidingModule,
                mCoordinator.getFilteredEnabledModuleSetForTesting());

        mCoordinator.destroy();
        verify(mHomeModulesConfigManager).removeListener(mHomeModulesStateListener.capture());
    }

    @Test
    @EnableFeatures({ChromeFeatureList.SEGMENTATION_PLATFORM_EPHEMERAL_CARD_RANKER})
    public void testOnModuleConfigChangedForEducationalTipModules() {
        assertFalse(DeviceFormFactor.isNonMultiDisplayContextOnTablet(mActivity));
        when(mModuleDelegateHost.isHomeSurface()).thenReturn(true);

        Set<Integer> expectedModuleListBeforeHidingModule =
                Set.of(
                        ModuleType.PRICE_CHANGE,
                        ModuleType.SINGLE_TAB,
                        ModuleType.DEFAULT_BROWSER_PROMO,
                        ModuleType.TAB_GROUP_PROMO,
                        ModuleType.TAB_GROUP_SYNC_PROMO,
                        ModuleType.QUICK_DELETE_PROMO,
                        ModuleType.HISTORY_SYNC_PROMO,
                        ModuleType.ENHANCED_SAFE_BROWSING_PROMO,
                        ModuleType.ADDRESS_BAR_PLACEMENT_PROMO,
                        ModuleType.SETUP_LIST_TWO_CELL_CONTAINER,
                        ModuleType.SETUP_LIST_CELEBRATORY_PROMO,
                        ModuleType.NTP_THEME_PROMO);
        when(mModuleRegistry.getEnabledModuleSet())
                .thenReturn(new HashSet<>(expectedModuleListBeforeHidingModule));

        // Register mock builders for all modules in this test.
        for (int type : expectedModuleListBeforeHidingModule) {
            ModuleProviderBuilder builder = mock(ModuleProviderBuilder.class);
            when(builder.getManualRank()).thenReturn(null);
            when(mModuleRegistry.getModuleProviderBuilder(type)).thenReturn(builder);
        }

        mCoordinator = createCoordinator(/* skipInitProfile= */ false);

        verify(mHomeModulesConfigManager).addListener(mHomeModulesStateListener.capture());
        assertEquals(
                expectedModuleListBeforeHidingModule,
                mCoordinator.getFilteredEnabledModuleSetForTesting());

        mHomeModulesStateListener
                .getValue()
                .onModuleConfigChanged(ModuleType.DEFAULT_BROWSER_PROMO, false);
        Set<Integer> expectedModuleListAfterHidingModule =
                Set.of(ModuleType.PRICE_CHANGE, ModuleType.SINGLE_TAB);
        assertEquals(
                expectedModuleListAfterHidingModule,
                mCoordinator.getFilteredEnabledModuleSetForTesting());

        mHomeModulesStateListener
                .getValue()
                .onModuleConfigChanged(ModuleType.DEFAULT_BROWSER_PROMO, true);
        assertEquals(
                expectedModuleListBeforeHidingModule,
                mCoordinator.getFilteredEnabledModuleSetForTesting());

        mHomeModulesStateListener
                .getValue()
                .onModuleConfigChanged(ModuleType.TAB_GROUP_SYNC_PROMO, false);
        assertEquals(
                expectedModuleListAfterHidingModule,
                mCoordinator.getFilteredEnabledModuleSetForTesting());

        mHomeModulesStateListener
                .getValue()
                .onModuleConfigChanged(ModuleType.ENHANCED_SAFE_BROWSING_PROMO, true);
        assertEquals(
                expectedModuleListBeforeHidingModule,
                mCoordinator.getFilteredEnabledModuleSetForTesting());

        mHomeModulesStateListener
                .getValue()
                .onModuleConfigChanged(ModuleType.ADDRESS_BAR_PLACEMENT_PROMO, true);
        assertEquals(
                expectedModuleListBeforeHidingModule,
                mCoordinator.getFilteredEnabledModuleSetForTesting());

        mHomeModulesStateListener
                .getValue()
                .onModuleConfigChanged(ModuleType.SETUP_LIST_TWO_CELL_CONTAINER, true);
        assertEquals(
                expectedModuleListBeforeHidingModule,
                mCoordinator.getFilteredEnabledModuleSetForTesting());

        mHomeModulesStateListener
                .getValue()
                .onModuleConfigChanged(ModuleType.SETUP_LIST_TWO_CELL_CONTAINER, false);
        assertEquals(
                expectedModuleListAfterHidingModule,
                mCoordinator.getFilteredEnabledModuleSetForTesting());

        mHomeModulesStateListener
                .getValue()
                .onModuleConfigChanged(ModuleType.SETUP_LIST_TWO_CELL_CONTAINER, true);
        assertEquals(
                expectedModuleListBeforeHidingModule,
                mCoordinator.getFilteredEnabledModuleSetForTesting());

        mCoordinator.destroy();
        verify(mHomeModulesConfigManager).removeListener(mHomeModulesStateListener.capture());
    }

    @Test
    public void testRemoveModuleAndDisable() {
        assertFalse(DeviceFormFactor.isNonMultiDisplayContextOnTablet(mActivity));
        mCoordinator = createCoordinator(/* skipInitProfile= */ false);

        mCoordinator.removeModuleAndDisable(ModuleType.PRICE_CHANGE);
        verify(mHomeModulesConfigManager)
                .setPrefModuleTypeEnabled(eq(ModuleType.PRICE_CHANGE), eq(false));
    }

    @Test
    public void testProfileNotReady() {
        mCoordinator = createCoordinator(/* skipInitProfile= */ true);
        Callback<Boolean> callback = MockitoHelper.mockCallback();
        mCoordinator.show(callback);

        assertTrue(mProfileSupplier.hasObservers());
        mProfileSupplier.set(mProfile);
        assertFalse(mProfileSupplier.hasObservers());
    }

    @Test
    public void testRecordMagicStackScroll_Scrolled() {
        mCoordinator = createCoordinator(/* skipInitProfile= */ true);
        mCoordinator.setMediatorForTesting(mMediator);

        mCoordinator.prepareBuildAndShow();

        // Populate the RecyclerView with items wider than itself so that it can actually scroll.
        mRecyclerView.setVisibility(View.VISIBLE);
        mRecyclerView.setAdapter(new FixedItemsAdapter(/* itemCount= */ 2, /* itemWidth= */ 200));
        layoutRecyclerView(/* width= */ 100);
        mRecyclerView.scrollBy(1, 0);

        verify(mMediator).recordMagicStackScroll(/* hasHomeModulesBeenScrolled= */ true);
    }

    @Test
    public void testRecordMagicStackScroll_NotScrolled() {
        when(mModuleDelegateHost.isHomeSurface()).thenReturn(true);
        mCoordinator = createCoordinator(/* skipInitProfile= */ false);
        mCoordinator.setMediatorForTesting(mMediator);
        mCoordinator.show(CallbackUtils.emptyCallback());

        mCoordinator.destroy();

        verify(mMediator).recordMagicStackScroll(/* hasHomeModulesBeenScrolled= */ false);
    }

    @Test
    public void testOnModuleChangedCallback() {
        when(mModuleDelegateHost.isHomeSurface()).thenReturn(true);
        mCoordinator = createCoordinator(/* skipInitProfile= */ true);
        Callback<Boolean> onHomeModulesShownCallback = MockitoHelper.mockCallback();
        mCoordinator.setMediatorForTesting(mMediator);
        mCoordinator.setModelForTesting(mModel);

        Runnable onHomeModulesChangedCallback =
                mCoordinator.createOnModuleChangedCallback(onHomeModulesShownCallback);
        layoutRecyclerView(/* width= */ 100);
        assertFalse(mRecyclerView.isLayoutRequested());

        when(mModel.size()).thenReturn(2);
        onHomeModulesChangedCallback.run();
        // invalidateItemDecorations() requests a layout.
        assertTrue(mRecyclerView.isLayoutRequested());

        when(mModel.size()).thenReturn(1);
        onHomeModulesChangedCallback.run();
        verify(onHomeModulesShownCallback).onResult(true);

        when(mModel.size()).thenReturn(0);
        onHomeModulesChangedCallback.run();
        verify(onHomeModulesShownCallback).onResult(false);
    }

    @Test
    public void testOnViewCreated() {
        mCoordinator = createCoordinator(/* skipInitProfile= */ true);
        mCoordinator.setMediatorForTesting(mMediator);
        when(mMediator.getModuleProvider(ModuleType.SINGLE_TAB)).thenReturn(mModuleProvider);
        FrameLayout moduleView = new FrameLayout(mActivity);
        assertFalse(moduleView.isFocusable());

        mCoordinator.onViewCreated(ModuleType.SINGLE_TAB, moduleView);
        verify(mModuleProvider).onViewCreated();
        verify(mMediator).onModuleViewCreated(eq(ModuleType.SINGLE_TAB));
        assertTrue(moduleView.isFocusable());
    }

    @Test
    public void testOnModuleClicked() {
        mCoordinator = createCoordinator(/* skipInitProfile= */ true);
        mCoordinator.setMediatorForTesting(mMediator);

        when(mMediator.getModuleRank(eq(ModuleType.SINGLE_TAB))).thenReturn(0);
        mCoordinator.onModuleClicked(ModuleType.SINGLE_TAB);
        verify(mMediator).onModuleClicked(eq(ModuleType.SINGLE_TAB));
    }

    @Test
    public void testOnLongClick() {
        HomeModulesContextMenuManager homeModulesContextMenuManager = mock();
        mCoordinator = createCoordinator(/* skipInitProfile= */ true);
        mCoordinator.setMediatorForTesting(mMediator);
        mCoordinator.setHomeModulesContextMenuManagerForTesting(homeModulesContextMenuManager);
        when(mMediator.getModuleProvider(ModuleType.SINGLE_TAB)).thenReturn(mModuleProvider);
        FrameLayout moduleView = new FrameLayout(mActivity);

        mCoordinator.onViewCreated(ModuleType.SINGLE_TAB, moduleView);
        assertTrue(moduleView.performLongClick());
        verify(homeModulesContextMenuManager).displayMenu(eq(moduleView), eq(mModuleProvider));

        reset(homeModulesContextMenuManager);
        OnCreateContextMenuListener contextMenuListener =
                shadowOf(moduleView).getOnCreateContextMenuListener();
        assertNotNull(contextMenuListener);
        contextMenuListener.onCreateContextMenu(/* menu= */ null, moduleView, /* menuInfo= */ null);
        verify(homeModulesContextMenuManager).displayMenu(eq(moduleView), eq(mModuleProvider));
    }

    @Test
    public void testAllCardsConfigChanged() {
        assertFalse(DeviceFormFactor.isNonMultiDisplayContextOnTablet(mActivity));
        when(mModuleDelegateHost.isHomeSurface()).thenReturn(true);
        mCoordinator = createCoordinator(/* skipInitProfile= */ false);

        verify(mHomeModulesConfigManager).addListener(mHomeModulesStateListener.capture());

        mRecyclerView.setVisibility(View.VISIBLE);
        mHomeModulesStateListener.getValue().allCardsConfigChanged(false);
        assertEquals(View.GONE, mRecyclerView.getVisibility());

        mHomeModulesStateListener.getValue().allCardsConfigChanged(true);
        assertEquals(View.VISIBLE, mRecyclerView.getVisibility());
    }

    @Test
    public void testAuroraPaddingStyle_Default() {
        testAuroraPaddingStyleImpl(PaddingStyle.DEFAULT, INITIAL_TOP_MARGIN);
    }

    @Test
    public void testAuroraPaddingStyle_NonDefault() {
        testAuroraPaddingStyleImpl(
                PaddingStyle.SMALL,
                mActivity
                        .getResources()
                        .getDimensionPixelSize(R.dimen.ntp_section_top_margin_small));
    }

    private void testAuroraPaddingStyleImpl(int paddingStyle, int expectedTopMargin) {
        MarginLayoutParams marginLayoutParams =
                (MarginLayoutParams) mRecyclerView.getLayoutParams();
        marginLayoutParams.topMargin = INITIAL_TOP_MARGIN;

        FeatureOverrides.overrideParam(ChromeFeatureList.NTP_AURORA, "padding_style", paddingStyle);

        mCoordinator = createCoordinator(/* skipInitProfile= */ false);

        assertEquals(
                expectedTopMargin,
                ((MarginLayoutParams) mRecyclerView.getLayoutParams()).topMargin);
    }

    private void layoutRecyclerView(int width) {
        mRecyclerView.measure(
                View.MeasureSpec.makeMeasureSpec(width, View.MeasureSpec.EXACTLY),
                View.MeasureSpec.makeMeasureSpec(100, View.MeasureSpec.EXACTLY));
        mRecyclerView.layout(0, 0, width, 100);
    }

    private HomeModulesCoordinator createCoordinator(boolean skipInitProfile) {
        if (!skipInitProfile) {
            mProfileSupplier.set(mProfile);
        }
        HomeModulesCoordinator homeModulesCoordinator =
                new HomeModulesCoordinator(
                        mActivity,
                        mModuleDelegateHost,
                        mView,
                        mHomeModulesConfigManager,
                        mProfileSupplier,
                        mModuleRegistry);
        return homeModulesCoordinator;
    }

    private void showWithSegmentation(Callback<Boolean> callback) {
        mCoordinator.show(callback);
        // TODO(ssid): Move this to a utility of Helper instead of tests mocking the jni.
        verify(mHomeModulesRankingHelperJniMock)
                .getClassificationResult(
                        any(), any(), any(), mClassificationResultCaptor.capture());
        String[] orderedLabels = {"SingleTab", "PriceChange"};
        ClassificationResult result =
                new ClassificationResult(
                        PredictionStatus.SUCCEEDED, orderedLabels, /* requestId= */ 0);
        mClassificationResultCaptor.getValue().onResult(result);
    }
}
