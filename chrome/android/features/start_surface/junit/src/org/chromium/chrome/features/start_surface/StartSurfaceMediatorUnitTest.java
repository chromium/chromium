// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.features.start_surface;

import static org.hamcrest.CoreMatchers.equalTo;
import static org.hamcrest.MatcherAssert.assertThat;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.anyLong;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doNothing;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import static org.chromium.chrome.features.start_surface.StartSurfaceProperties.EXPLORE_SURFACE_COORDINATOR;
import static org.chromium.chrome.features.start_surface.StartSurfaceProperties.IS_EXPLORE_SURFACE_VISIBLE;
import static org.chromium.chrome.features.start_surface.StartSurfaceProperties.IS_SHOWING_OVERVIEW;
import static org.chromium.chrome.features.start_surface.StartSurfaceProperties.TOP_MARGIN;
import static org.chromium.chrome.features.tasks.TasksSurfaceProperties.BACKGROUND_COLOR;
import static org.chromium.chrome.features.tasks.TasksSurfaceProperties.FAKE_SEARCH_BOX_CLICK_LISTENER;
import static org.chromium.chrome.features.tasks.TasksSurfaceProperties.FAKE_SEARCH_BOX_TEXT_WATCHER;
import static org.chromium.chrome.features.tasks.TasksSurfaceProperties.IS_FAKE_SEARCH_BOX_VISIBLE;
import static org.chromium.chrome.features.tasks.TasksSurfaceProperties.IS_INCOGNITO;
import static org.chromium.chrome.features.tasks.TasksSurfaceProperties.IS_INCOGNITO_DESCRIPTION_INITIALIZED;
import static org.chromium.chrome.features.tasks.TasksSurfaceProperties.IS_INCOGNITO_DESCRIPTION_VISIBLE;
import static org.chromium.chrome.features.tasks.TasksSurfaceProperties.IS_LENS_BUTTON_VISIBLE;
import static org.chromium.chrome.features.tasks.TasksSurfaceProperties.IS_SURFACE_BODY_VISIBLE;
import static org.chromium.chrome.features.tasks.TasksSurfaceProperties.IS_VOICE_RECOGNITION_BUTTON_VISIBLE;
import static org.chromium.chrome.features.tasks.TasksSurfaceProperties.MV_TILES_VISIBLE;
import static org.chromium.chrome.features.tasks.TasksSurfaceProperties.VOICE_SEARCH_BUTTON_CLICK_LISTENER;

import android.app.Activity;
import android.view.View;

import androidx.annotation.ColorInt;
import androidx.test.ext.junit.rules.ActivityScenarioRule;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.ArgumentMatchers;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;

import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.supplier.Supplier;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.back_press.BackPressManager;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider;
import org.chromium.chrome.browser.feed.FeedReliabilityLogger;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.lens.LensEntryPoint;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.lifecycle.PauseResumeWithNativeObserver;
import org.chromium.chrome.browser.logo.LogoBridge;
import org.chromium.chrome.browser.logo.LogoBridgeJni;
import org.chromium.chrome.browser.logo.LogoView;
import org.chromium.chrome.browser.magic_stack.HomeModulesCoordinator;
import org.chromium.chrome.browser.ntp.NewTabPageLaunchOrigin;
import org.chromium.chrome.browser.omnibox.OmniboxStub;
import org.chromium.chrome.browser.omnibox.UrlFocusChangeListener;
import org.chromium.chrome.browser.omnibox.voice.VoiceRecognitionHandler;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileManager;
import org.chromium.chrome.browser.search_engines.TemplateUrlServiceFactory;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabSelectionType;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelFilter;
import org.chromium.chrome.browser.tabmodel.TabModelFilterProvider;
import org.chromium.chrome.browser.tabmodel.TabModelObserver;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorObserver;
import org.chromium.chrome.features.start_surface.StartSurface.OnTabSelectingListener;
import org.chromium.chrome.features.tasks.TasksSurfaceProperties;
import org.chromium.components.browser_ui.styles.ChromeColors;
import org.chromium.components.prefs.PrefService;
import org.chromium.components.search_engines.TemplateUrlService;
import org.chromium.components.search_engines.TemplateUrlService.TemplateUrlServiceObserver;
import org.chromium.ui.base.TestActivity;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;

/** Tests for {@link StartSurfaceMediator}. */
@DisableFeatures({ChromeFeatureList.SHOW_NTP_AT_STARTUP_ANDROID})
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class StartSurfaceMediatorUnitTest {
    private static final String START_SURFACE_TIME_SPENT = "StartSurface.TimeSpent";
    private PropertyModel mPropertyModel;

    @Rule public JniMocker mJniMocker = new JniMocker();
    @Rule public TestRule mProcessor = new Features.JUnitProcessor();
    @Mock private TabModelSelector mTabModelSelector;
    @Mock private Tab mTab;
    @Mock private TabModel mNormalTabModel;
    @Mock private TabModel mIncognitoTabModel;
    @Mock private List<TabModel> mTabModels;
    @Mock private TabModelFilterProvider mTabModelFilterProvider;
    @Mock private TabModelFilter mNormalTabModelFilter;
    @Mock private OmniboxStub mOmniboxStub;
    @Mock private ExploreSurfaceCoordinator mExploreSurfaceCoordinator;
    @Mock private ExploreSurfaceCoordinatorFactory mExploreSurfaceCoordinatorFactory;
    @Mock private BrowserControlsStateProvider mBrowserControlsStateProvider;
    @Mock private StartSurfaceMediator.ActivityStateChecker mActivityStateChecker;
    @Mock private VoiceRecognitionHandler mVoiceRecognitionHandler;
    @Mock private PrefService mPrefService;
    @Mock private Runnable mInitializeMVTilesRunnable;
    @Mock private FeedReliabilityLogger mFeedReliabilityLogger;
    @Mock private BackPressManager mBackPressManager;
    @Mock private Supplier<Tab> mParentTabSupplier;
    @Mock private View mLogoContainerView;
    @Mock private LogoView mLogoView;
    @Mock private LogoBridge.Natives mLogoBridge;
    @Mock private Profile mProfile;
    @Mock private TemplateUrlService mTemplateUrlService;
    @Mock private ActivityLifecycleDispatcher mActivityLifecycleDispatcher;
    @Mock private HomeModulesCoordinator mHomeModulesCoordinator;
    @Captor private ArgumentCaptor<TabModelSelectorObserver> mTabModelSelectorObserverCaptor;
    @Captor private ArgumentCaptor<TabModelObserver> mTabModelObserverCaptor;
    @Captor private ArgumentCaptor<UrlFocusChangeListener> mUrlFocusChangeListenerCaptor;

    @Captor
    private ArgumentCaptor<BrowserControlsStateProvider.Observer>
            mBrowserControlsStateProviderCaptor;

    @Captor
    private ArgumentCaptor<PauseResumeWithNativeObserver>
            mPauseResumeWithNativeObserverArgumentCaptor;

    @Captor private ArgumentCaptor<TemplateUrlServiceObserver> mTemplateUrlServiceObserverCaptor;
    private ObservableSupplierImpl<Profile> mProfileSupplier = new ObservableSupplierImpl<>();

    private Activity mActivity;

    @Rule
    public ActivityScenarioRule<TestActivity> mActivityScenarioRule =
            new ActivityScenarioRule<>(TestActivity.class);

    @Before
    public void setUp() {
        mActivityScenarioRule.getScenario().onActivity((activity) -> mActivity = activity);
        MockitoAnnotations.initMocks(this);

        doReturn(false).when(mProfile).isOffTheRecord();
        mProfileSupplier.set(mProfile);
        ProfileManager.setLastUsedProfileForTesting(mProfile);
        TemplateUrlServiceFactory.setInstanceForTesting(mTemplateUrlService);

        ArrayList<PropertyKey> allProperties =
                new ArrayList<>(Arrays.asList(TasksSurfaceProperties.ALL_KEYS));
        allProperties.addAll(Arrays.asList(StartSurfaceProperties.ALL_KEYS));
        mPropertyModel = new PropertyModel(allProperties);

        mTabModels = new ArrayList<>(2);
        mTabModels.add(mNormalTabModel);
        mTabModels.add(mIncognitoTabModel);
        when(mTabModelSelector.getModels()).thenReturn(mTabModels);
        doReturn(mNormalTabModel).when(mTabModelSelector).getModel(false);
        doReturn(mIncognitoTabModel).when(mTabModelSelector).getModel(true);
        doReturn(false).when(mNormalTabModel).isIncognito();
        doReturn(true).when(mIncognitoTabModel).isIncognito();

        when(mTabModelSelector.getTabModelFilterProvider()).thenReturn(mTabModelFilterProvider);
        when(mTabModelFilterProvider.getTabModelFilter(false)).thenReturn(mNormalTabModelFilter);
        when(mTabModelFilterProvider.getTabModelFilter(true)).thenReturn(mNormalTabModelFilter);

        doReturn(false).when(mActivityStateChecker).isFinishingOrDestroyed();
        doReturn(mTab).when(mTabModelSelector).getCurrentTab();
        doReturn(mExploreSurfaceCoordinator)
                .when(mExploreSurfaceCoordinatorFactory)
                .create(anyBoolean(), anyInt());
        doReturn(mFeedReliabilityLogger)
                .when(mExploreSurfaceCoordinator)
                .getFeedReliabilityLogger();
        mJniMocker.mock(LogoBridgeJni.TEST_HOOKS, mLogoBridge);
        doReturn(mLogoView).when(mLogoContainerView).findViewById(R.id.search_provider_logo);

        when(mOmniboxStub.getVoiceRecognitionHandler()).thenReturn(mVoiceRecognitionHandler);
    }

    @After
    public void tearDown() {
        mPropertyModel = null;
    }

    @Test
    public void overviewModeSwitchToIncognitoModeAndBackSinglePane() {
        doReturn(false).when(mTabModelSelector).isIncognitoSelected();
        doReturn(true).when(mVoiceRecognitionHandler).isVoiceSearchEnabled();

        StartSurfaceMediator mediator = createStartSurfaceMediator();
        assertFalse(mediator.isHomepageShown());

        doReturn(2).when(mNormalTabModel).getCount();
        doReturn(true).when(mTabModelSelector).isTabStateInitialized();
        showHomepageAndVerify(mediator);
        verify(mTabModelSelector).addObserver(mTabModelSelectorObserverCaptor.capture());
        assertThat(mPropertyModel.get(IS_SHOWING_OVERVIEW), equalTo(true));
        assertThat(mPropertyModel.get(IS_INCOGNITO), equalTo(false));
        assertThat(mPropertyModel.get(IS_EXPLORE_SURFACE_VISIBLE), equalTo(true));
        assertThat(mPropertyModel.get(MV_TILES_VISIBLE), equalTo(true));

        doReturn(true).when(mTabModelSelector).isIncognitoSelected();
        mTabModelSelector.selectModel(true);
        mTabModelSelectorObserverCaptor
                .getValue()
                .onTabModelSelected(mIncognitoTabModel, mNormalTabModel);

        doReturn(false).when(mTabModelSelector).isIncognitoSelected();
        mTabModelSelector.selectModel(false);
        mTabModelSelectorObserverCaptor
                .getValue()
                .onTabModelSelected(mNormalTabModel, mIncognitoTabModel);
        assertTrue(mediator.isHomepageShown());
        assertThat(mPropertyModel.get(IS_SHOWING_OVERVIEW), equalTo(true));
        assertThat(mPropertyModel.get(IS_INCOGNITO), equalTo(false));
        assertThat(mPropertyModel.get(IS_EXPLORE_SURFACE_VISIBLE), equalTo(true));
        assertThat(mPropertyModel.get(MV_TILES_VISIBLE), equalTo(true));
    }

    @Test
    public void activityIsFinishingOrDestroyedSinglePaneWithRefactorEnabled() {
        doReturn(false).when(mTabModelSelector).isIncognitoSelected();
        doReturn(true).when(mVoiceRecognitionHandler).isVoiceSearchEnabled();

        StartSurfaceMediator mediator = createStartSurfaceMediator();
        assertFalse(mediator.isHomepageShown());

        doReturn(2).when(mNormalTabModel).getCount();
        doReturn(true).when(mActivityStateChecker).isFinishingOrDestroyed();
        doReturn(true).when(mTabModelSelector).isTabStateInitialized();
        showHomepageAndVerify(mediator);
        assertThat(mPropertyModel.get(IS_SHOWING_OVERVIEW), equalTo(true));
        assertThat(mPropertyModel.get(IS_INCOGNITO), equalTo(false));
        assertThat(mPropertyModel.get(IS_EXPLORE_SURFACE_VISIBLE), equalTo(true));
        assertThat(mPropertyModel.get(MV_TILES_VISIBLE), equalTo(true));
        assertThat(mPropertyModel.get(EXPLORE_SURFACE_COORDINATOR), equalTo(null));

        mediator.startedHiding();
        assertFalse(mediator.isHomepageShown());
    }

    @Test
    public void setIncognitoDescriptionShowSinglePane() {
        doReturn(false).when(mTabModelSelector).isIncognitoSelected();
        doReturn(true).when(mVoiceRecognitionHandler).isVoiceSearchEnabled();

        StartSurfaceMediator mediator = createStartSurfaceMediator();
        showHomepageAndVerify(mediator);
        verify(mTabModelSelector).addObserver(mTabModelSelectorObserverCaptor.capture());

        assertThat(mPropertyModel.get(IS_INCOGNITO_DESCRIPTION_INITIALIZED), equalTo(false));
        assertThat(mPropertyModel.get(IS_INCOGNITO_DESCRIPTION_VISIBLE), equalTo(false));
    }

    @Test
    public void setIncognitoDescriptionHideSinglePane() {
        doReturn(false).when(mTabModelSelector).isIncognitoSelected();
        doReturn(true).when(mVoiceRecognitionHandler).isVoiceSearchEnabled();

        StartSurfaceMediator mediator = createStartSurfaceMediator();
        showHomepageAndVerify(mediator);
        verify(mTabModelSelector).addObserver(mTabModelSelectorObserverCaptor.capture());

        assertThat(mPropertyModel.get(IS_INCOGNITO_DESCRIPTION_INITIALIZED), equalTo(false));
        assertThat(mPropertyModel.get(IS_INCOGNITO_DESCRIPTION_VISIBLE), equalTo(false));
        mediator.hideTabSwitcherView(true);
    }

    @Test
    public void setOverviewState_webFeed_resetsFeedInstanceState() {
        doReturn(false).when(mTabModelSelector).isIncognitoSelected();
        doReturn(true).when(mVoiceRecognitionHandler).isVoiceSearchEnabled();
        StartSurfaceMediator mediator = createStartSurfaceMediator();

        String instanceState = "state";
        StartSurfaceUserData.getInstance().saveFeedInstanceState(instanceState);

        assertEquals(StartSurfaceUserData.getInstance().restoreFeedInstanceState(), instanceState);

        mediator.setLaunchOrigin(NewTabPageLaunchOrigin.WEB_FEED);
        assertNull(StartSurfaceUserData.getInstance().restoreFeedInstanceState());
    }

    @Test
    public void setOverviewState_nonWebFeed_doesNotResetFeedInstanceState() {
        doReturn(false).when(mTabModelSelector).isIncognitoSelected();
        doReturn(true).when(mVoiceRecognitionHandler).isVoiceSearchEnabled();
        StartSurfaceMediator mediator = createStartSurfaceMediator();

        String instanceState = "state";
        StartSurfaceUserData.getInstance().saveFeedInstanceState(instanceState);

        assertEquals(StartSurfaceUserData.getInstance().restoreFeedInstanceState(), instanceState);

        mediator.setLaunchOrigin(NewTabPageLaunchOrigin.UNKNOWN);
        assertNotNull(StartSurfaceUserData.getInstance().restoreFeedInstanceState());
    }

    @Test
    public void testResetFeedInstanceState() {
        String instanceState = "state";
        StartSurfaceUserData.getInstance().saveFeedInstanceState(instanceState);
        assertTrue(StartSurfaceUserData.hasInstanceForTesting());
        assertEquals(StartSurfaceUserData.getInstance().restoreFeedInstanceState(), instanceState);

        StartSurfaceUserData.reset();
        assertTrue(StartSurfaceUserData.hasInstanceForTesting());
        assertNull(StartSurfaceUserData.getInstance().restoreFeedInstanceState());
    }

    @Test
    public void changeTopContentOffset() {
        doReturn(false).when(mTabModelSelector).isIncognitoSelected();
        doReturn(true).when(mVoiceRecognitionHandler).isVoiceSearchEnabled();

        doNothing()
                .when(mBrowserControlsStateProvider)
                .addObserver(mBrowserControlsStateProviderCaptor.capture());
        StartSurfaceMediator mediator = createStartSurfaceMediator();

        // The top margin of homepage should be consistent with top controls min height/offset
        // (indicator height).
        doReturn(10).when(mBrowserControlsStateProvider).getTopControlsMinHeight();
        // Sets the current StartSurfaceState to SHOWING_START before calling the
        // {@link StartSurfaceMediator#showOverview()}. This is because if the current
        // StartSurfaceState is NOT_SHOWN, the state will be set default to SHOWING_TABSWITCHER in
        // {@link StartSurfaceMediator#showOverview()}.
        showHomepageAndVerify(mediator);

        verify(mBrowserControlsStateProvider).addObserver(ArgumentMatchers.any());
        assertEquals("Wrong top content offset on homepage.", 10, mPropertyModel.get(TOP_MARGIN));

        onControlsOffsetChanged(/* topOffset= */ 100, /* topControlsMinHeightOffset= */ 20);
        assertEquals("Wrong top content offset on homepage.", 20, mPropertyModel.get(TOP_MARGIN));

        onControlsOffsetChanged(/* topOffset= */ 130, /* topControlsMinHeightOffset= */ 50);
        assertEquals("Wrong top content offset on homepage.", 50, mPropertyModel.get(TOP_MARGIN));
    }

    @Test
    public void testInitializeMVTilesWhenShownHomepage() {
        doReturn(false).when(mTabModelSelector).isIncognitoSelected();
        doReturn(true).when(mVoiceRecognitionHandler).isVoiceSearchEnabled();
        doReturn(2).when(mNormalTabModel).getCount();
        doReturn(true).when(mTabModelSelector).isTabStateInitialized();

        StartSurfaceMediator mediator = createStartSurfaceMediator();
        showHomepageAndVerify(mediator);
        verify(mInitializeMVTilesRunnable).run();
    }

    @Test
    public void testInitializeLogo() {
        when(mTemplateUrlService.doesDefaultSearchEngineHaveLogo()).thenReturn(true);

        StartSurfaceMediator mediator = createStartSurfaceMediator(/* hadWarmStart= */ false);
        showHomepageAndVerify(mediator);

        verify(mLogoContainerView).setVisibility(View.VISIBLE);
        verify(mLogoBridge).getCurrentLogo(anyLong(), any(), any());
        Assert.assertTrue(mediator.isLogoVisible());
    }

    @Test
    @DisableFeatures({ChromeFeatureList.BACK_GESTURE_REFACTOR})
    public void testFeedReliabilityLoggerBackPressed() {
        StartSurfaceMediator mediator = createStartSurfaceMediator();
        showHomepageAndVerify(mediator);
        mediator.onBackPressed();
        verify(mFeedReliabilityLogger).onNavigateBack();
    }

    /** Tests the logic of recording time spend in start surface. */
    @Test
    public void testRecordTimeSpendInStart() {
        doReturn(false).when(mTabModelSelector).isIncognitoSelected();
        doReturn(true).when(mVoiceRecognitionHandler).isVoiceSearchEnabled();
        StartSurfaceMediator mediator = createStartSurfaceMediator();
        verify(mActivityLifecycleDispatcher)
                .register(mPauseResumeWithNativeObserverArgumentCaptor.capture());

        // Verifies that the histograms are logged in the following transitions:
        // Start Surface -> Grid Tab Switcher -> Start Surface -> onPauseWithNative ->
        // onResumeWithNative -> destroy.
        showHomepageAndVerify(mediator);

        var histogramWatcher = HistogramWatcher.newSingleRecordWatcher(START_SURFACE_TIME_SPENT);
        mPauseResumeWithNativeObserverArgumentCaptor.getValue().onPauseWithNative();
        histogramWatcher.assertExpected();

        histogramWatcher = HistogramWatcher.newSingleRecordWatcher(START_SURFACE_TIME_SPENT);
        mPauseResumeWithNativeObserverArgumentCaptor.getValue().onResumeWithNative();
        mediator.destroy();
        histogramWatcher.assertExpected();
    }

    @Test
    public void testDefaultSearchEngineChanged() {
        mProfileSupplier = new ObservableSupplierImpl<>();
        StartSurfaceMediator mediator = createStartSurfaceMediator(/* hadWarmStart= */ false);
        showHomepageAndVerify(mediator);

        mProfileSupplier.set(mProfile);
        verify(mTemplateUrlService, times(2))
                .addObserver(mTemplateUrlServiceObserverCaptor.capture());
        doReturn(true).when(mOmniboxStub).isLensEnabled(LensEntryPoint.TASKS_SURFACE);
        mTemplateUrlServiceObserverCaptor.getValue().onTemplateURLServiceChanged();
        assertTrue(mPropertyModel.get(IS_LENS_BUTTON_VISIBLE));

        doReturn(false).when(mOmniboxStub).isLensEnabled(LensEntryPoint.TASKS_SURFACE);
        mTemplateUrlServiceObserverCaptor.getValue().onTemplateURLServiceChanged();
        assertFalse(mPropertyModel.get(IS_LENS_BUTTON_VISIBLE));

        mediator.destroy();
        verify(mTemplateUrlService, times(2))
                .removeObserver(mTemplateUrlServiceObserverCaptor.capture());
    }

    @Test
    @EnableFeatures({ChromeFeatureList.MAGIC_STACK_ANDROID})
    public void testObserver() {
        Assert.assertTrue(ChromeFeatureList.sMagicStackAndroid.isEnabled());

        doReturn(false).when(mTabModelSelector).isIncognitoSelected();
        doReturn(true).when(mVoiceRecognitionHandler).isVoiceSearchEnabled();
        doReturn(mTabModelFilterProvider).when(mTabModelSelector).getTabModelFilterProvider();
        doReturn(true).when(mOmniboxStub).isLensEnabled(LensEntryPoint.TASKS_SURFACE);

        StartSurfaceMediator mediator = createStartSurfaceMediator(/* hadWarmStart= */ false);
        assertEquals(mInitializeMVTilesRunnable, mediator.getInitializeMVTilesRunnableForTesting());
        assertNull(mediator.getTabSwitcherModuleForTesting());
        assertNull(mediator.getControllerForTesting());

        showHomepageAndVerify(mediator);
        verify(mTabModelSelector).addObserver(mTabModelSelectorObserverCaptor.capture());
        verify(mNormalTabModel, never()).addObserver(mTabModelObserverCaptor.capture());
        verify(mTabModelFilterProvider)
                .addTabModelFilterObserver(mTabModelObserverCaptor.capture());
        verify(mOmniboxStub, times(2))
                .addUrlFocusChangeListener(mUrlFocusChangeListenerCaptor.capture());
        assertFalse(mPropertyModel.get(IS_INCOGNITO));
        assertTrue(mPropertyModel.get(IS_VOICE_RECOGNITION_BUTTON_VISIBLE));
        assertTrue(mPropertyModel.get(IS_EXPLORE_SURFACE_VISIBLE));
        assertTrue(mPropertyModel.get(MV_TILES_VISIBLE));
        assertTrue(mPropertyModel.get(IS_SHOWING_OVERVIEW));
        assertTrue(mPropertyModel.get(IS_SURFACE_BODY_VISIBLE));
        assertTrue(mPropertyModel.get(IS_FAKE_SEARCH_BOX_VISIBLE));
        assertTrue(mPropertyModel.get(IS_LENS_BUTTON_VISIBLE));
        assertNotNull(mPropertyModel.get(FAKE_SEARCH_BOX_CLICK_LISTENER));
        assertNotNull(mPropertyModel.get(FAKE_SEARCH_BOX_TEXT_WATCHER));
        assertNotNull(mPropertyModel.get(VOICE_SEARCH_BUTTON_CLICK_LISTENER));

        doReturn(true).when(mTabModelSelector).isIncognitoSelected();
        mTabModelSelector.selectModel(true);
        mTabModelSelectorObserverCaptor
                .getValue()
                .onTabModelSelected(mIncognitoTabModel, mNormalTabModel);
        assertTrue(mPropertyModel.get(IS_INCOGNITO));

        doReturn(false).when(mTabModelSelector).isIncognitoSelected();
        mTabModelSelector.selectModel(false);
        mTabModelSelectorObserverCaptor
                .getValue()
                .onTabModelSelected(mNormalTabModel, mIncognitoTabModel);
        assertFalse(mPropertyModel.get(IS_INCOGNITO));

        int lastId = 1;
        int currentId = 2;
        doReturn(false).when(mTab).isIncognito();
        doReturn(currentId).when(mTabModelSelector).getCurrentTabId();
        OnTabSelectingListener onTabSelectingListener = Mockito.mock(OnTabSelectingListener.class);
        mediator.setOnTabSelectingListener(onTabSelectingListener);
        mTabModelObserverCaptor.getValue().didSelectTab(mTab, TabSelectionType.FROM_USER, lastId);
        verify(onTabSelectingListener).onTabSelecting(eq(currentId));

        doReturn(true).when(mTab).isIncognito();
        mTabModelObserverCaptor.getValue().didSelectTab(mTab, TabSelectionType.FROM_USER, lastId);
        verify(onTabSelectingListener, times(2)).onTabSelecting(eq(currentId));

        mTabModelObserverCaptor.getValue().didSelectTab(mTab, TabSelectionType.FROM_CLOSE, lastId);
        verify(onTabSelectingListener, times(2)).onTabSelecting(eq(currentId));
    }

    @Test
    @EnableFeatures({ChromeFeatureList.MAGIC_STACK_ANDROID})
    public void testShowAndOnHideForMagicStack() {
        Assert.assertTrue(ChromeFeatureList.sMagicStackAndroid.isEnabled());

        doReturn(false).when(mTabModelSelector).isIncognitoSelected();
        doReturn(true).when(mVoiceRecognitionHandler).isVoiceSearchEnabled();
        doReturn(mTabModelFilterProvider).when(mTabModelSelector).getTabModelFilterProvider();
        doReturn(true).when(mOmniboxStub).isLensEnabled(LensEntryPoint.TASKS_SURFACE);

        StartSurfaceMediator mediator = createStartSurfaceMediator(/* hadWarmStart= */ false);
        assertEquals(mInitializeMVTilesRunnable, mediator.getInitializeMVTilesRunnableForTesting());
        assertNull(mediator.getTabSwitcherModuleForTesting());
        assertNull(mediator.getControllerForTesting());

        showHomepageAndVerify(mediator);
        verify(mTabModelSelector).addObserver(mTabModelSelectorObserverCaptor.capture());
        verify(mNormalTabModel, never()).addObserver(mTabModelObserverCaptor.capture());
        verify(mTabModelFilterProvider)
                .addTabModelFilterObserver(mTabModelObserverCaptor.capture());
        verify(mOmniboxStub, times(2))
                .addUrlFocusChangeListener(mUrlFocusChangeListenerCaptor.capture());
        assertFalse(mPropertyModel.get(IS_INCOGNITO));
        assertTrue(mPropertyModel.get(IS_VOICE_RECOGNITION_BUTTON_VISIBLE));
        assertTrue(mPropertyModel.get(IS_EXPLORE_SURFACE_VISIBLE));
        assertTrue(mPropertyModel.get(MV_TILES_VISIBLE));
        assertTrue(mPropertyModel.get(IS_SHOWING_OVERVIEW));
        assertTrue(mPropertyModel.get(IS_SURFACE_BODY_VISIBLE));
        assertTrue(mPropertyModel.get(IS_FAKE_SEARCH_BOX_VISIBLE));
        assertTrue(mPropertyModel.get(IS_LENS_BUTTON_VISIBLE));
        assertNotNull(mPropertyModel.get(FAKE_SEARCH_BOX_CLICK_LISTENER));
        assertNotNull(mPropertyModel.get(FAKE_SEARCH_BOX_TEXT_WATCHER));
        assertNotNull(mPropertyModel.get(VOICE_SEARCH_BUTTON_CLICK_LISTENER));

        mediator.hideTabSwitcherView(false);
        assertFalse(mPropertyModel.get(IS_SHOWING_OVERVIEW));
        verify(mOmniboxStub, times(2))
                .removeUrlFocusChangeListener(mUrlFocusChangeListenerCaptor.capture());
        verify(mTabModelFilterProvider)
                .removeTabModelFilterObserver(mTabModelObserverCaptor.capture());
        verify(mTabModelSelector).removeObserver(mTabModelSelectorObserverCaptor.capture());
        assertFalse(mediator.isHomepageShown());
    }

    @Test
    public void testUpdateStartSurfaceBackgroundColor() {
        doReturn(false).when(mTabModelSelector).isIncognitoSelected();
        // Make sure the background color is not set.
        assertEquals(0, mPropertyModel.get(BACKGROUND_COLOR));

        StartSurfaceMediator mediator = createStartSurfaceMediator(/* hadWarmStart= */ false);
        @ColorInt
        int backgroundColor =
                ChromeColors.getSurfaceColor(
                        mActivity, R.dimen.home_surface_background_color_elevation);
        assertNotEquals(backgroundColor, 0);
        assertEquals(backgroundColor, mPropertyModel.get(BACKGROUND_COLOR));

        mediator.setIsIncognitoForTesting(true);
        mediator.updateBackgroundColor(mPropertyModel);
        @ColorInt int newBackgroundColor = ChromeColors.getPrimaryBackgroundColor(mActivity, true);
        assertNotEquals(newBackgroundColor, backgroundColor);
        assertEquals(newBackgroundColor, mPropertyModel.get(BACKGROUND_COLOR));
    }

    @Test
    @EnableFeatures({ChromeFeatureList.MAGIC_STACK_ANDROID})
    public void testSetMagicStackVisibility() {
        assertTrue(StartSurfaceConfiguration.useMagicStack());
        StartSurfaceMediator mediator = createStartSurfaceMediator(/* hadWarmStart= */ false);
        assertNull(mediator.getHomeModulesCoordinatorForTesting());

        mediator.setMagicStackVisibility(true);
        // Verifies that a HomeModulesCoordinator is created and shown.
        assertEquals(mHomeModulesCoordinator, mediator.getHomeModulesCoordinatorForTesting());
        verify(mHomeModulesCoordinator).show(any());

        mediator.setMagicStackVisibility(false);
        verify(mHomeModulesCoordinator).show(any());
    }

    private StartSurfaceMediator createStartSurfaceMediator() {
        return createStartSurfaceMediator(/* hadWarmStart= */ false);
    }

    private StartSurfaceMediator createStartSurfaceMediator(boolean hadWarmStart) {
        StartSurfaceMediator mediator = createStartSurfaceMediatorWithoutInit(hadWarmStart);
        mediator.initWithNative(mOmniboxStub, mExploreSurfaceCoordinatorFactory, mPrefService);
        return mediator;
    }

    private StartSurfaceMediator createStartSurfaceMediatorWithoutInit(boolean hadWarmStart) {
        return new StartSurfaceMediator(
                /* tabSwitcherContainer= */ null,
                mTabModelSelector,
                mPropertyModel,
                /* isStartSurfaceEnabled= */ true,
                mActivity,
                mBrowserControlsStateProvider,
                mActivityStateChecker,
                /* tabCreatorManager= */ null,
                hadWarmStart,
                mInitializeMVTilesRunnable,
                (homeSurfaceHost) -> mHomeModulesCoordinator,
                mParentTabSupplier,
                mLogoContainerView,
                mBackPressManager,
                mActivityLifecycleDispatcher,
                mProfileSupplier);
    }

    private void onControlsOffsetChanged(int topOffset, int topControlsMinHeightOffset) {
        doReturn(topOffset).when(mBrowserControlsStateProvider).getContentOffset();
        doReturn(topControlsMinHeightOffset)
                .when(mBrowserControlsStateProvider)
                .getTopControlsMinHeightOffset();
        mBrowserControlsStateProviderCaptor
                .getValue()
                .onControlsOffsetChanged(topOffset, topControlsMinHeightOffset, 0, 0, false);
    }

    private void showHomepageAndVerify(StartSurfaceMediator mediator) {
        mediator.show(false);
        assertTrue(mediator.isHomepageShown());
    }
}
