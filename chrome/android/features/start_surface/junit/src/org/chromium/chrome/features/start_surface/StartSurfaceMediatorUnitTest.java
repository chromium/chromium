// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.features.start_surface;

import static org.hamcrest.CoreMatchers.equalTo;
import static org.hamcrest.MatcherAssert.assertThat;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.anyLong;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.description;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.doNothing;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.inOrder;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import static org.chromium.chrome.browser.flags.ChromeFeatureList.INSTANT_START;
import static org.chromium.chrome.features.start_surface.StartSurfaceProperties.BOTTOM_BAR_HEIGHT;
import static org.chromium.chrome.features.start_surface.StartSurfaceProperties.EXPLORE_SURFACE_COORDINATOR;
import static org.chromium.chrome.features.start_surface.StartSurfaceProperties.IS_EXPLORE_SURFACE_VISIBLE;
import static org.chromium.chrome.features.start_surface.StartSurfaceProperties.IS_SECONDARY_SURFACE_VISIBLE;
import static org.chromium.chrome.features.start_surface.StartSurfaceProperties.IS_SHOWING_OVERVIEW;
import static org.chromium.chrome.features.start_surface.StartSurfaceProperties.TOP_MARGIN;
import static org.chromium.chrome.features.tasks.TasksSurfaceProperties.IS_FAKE_SEARCH_BOX_VISIBLE;
import static org.chromium.chrome.features.tasks.TasksSurfaceProperties.IS_INCOGNITO;
import static org.chromium.chrome.features.tasks.TasksSurfaceProperties.IS_INCOGNITO_DESCRIPTION_INITIALIZED;
import static org.chromium.chrome.features.tasks.TasksSurfaceProperties.IS_INCOGNITO_DESCRIPTION_VISIBLE;
import static org.chromium.chrome.features.tasks.TasksSurfaceProperties.IS_TAB_CAROUSEL_TITLE_VISIBLE;
import static org.chromium.chrome.features.tasks.TasksSurfaceProperties.IS_TAB_CAROUSEL_VISIBLE;
import static org.chromium.chrome.features.tasks.TasksSurfaceProperties.IS_VOICE_RECOGNITION_BUTTON_VISIBLE;
import static org.chromium.chrome.features.tasks.TasksSurfaceProperties.MV_TILES_CONTAINER_LEFT_RIGHT_MARGIN;
import static org.chromium.chrome.features.tasks.TasksSurfaceProperties.MV_TILES_CONTAINER_TOP_MARGIN;
import static org.chromium.chrome.features.tasks.TasksSurfaceProperties.MV_TILES_VISIBLE;
import static org.chromium.chrome.features.tasks.TasksSurfaceProperties.SINGLE_TAB_TOP_MARGIN;
import static org.chromium.chrome.features.tasks.TasksSurfaceProperties.TAB_SWITCHER_TITLE_TOP_MARGIN;
import static org.chromium.chrome.features.tasks.TasksSurfaceProperties.TASKS_SURFACE_BODY_TOP_MARGIN;

import android.content.Context;
import android.content.res.Resources;
import android.view.View;

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
import org.mockito.InOrder;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;

import org.chromium.base.ContextUtils;
import org.chromium.base.jank_tracker.DummyJankTracker;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.supplier.OneshotSupplier;
import org.chromium.base.supplier.Supplier;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.back_press.BackPressManager;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider;
import org.chromium.chrome.browser.feed.FeedReliabilityLogger;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.lifecycle.PauseResumeWithNativeObserver;
import org.chromium.chrome.browser.logo.LogoBridge;
import org.chromium.chrome.browser.logo.LogoBridgeJni;
import org.chromium.chrome.browser.logo.LogoView;
import org.chromium.chrome.browser.night_mode.NightModeStateProvider;
import org.chromium.chrome.browser.ntp.NewTabPageLaunchOrigin;
import org.chromium.chrome.browser.omnibox.OmniboxStub;
import org.chromium.chrome.browser.omnibox.UrlFocusChangeListener;
import org.chromium.chrome.browser.omnibox.voice.VoiceRecognitionHandler;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.SharedPreferencesManager;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.search_engines.TemplateUrlServiceFactory;
import org.chromium.chrome.browser.tab.MockTab;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tab.TabSelectionType;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelFilter;
import org.chromium.chrome.browser.tabmodel.TabModelFilterProvider;
import org.chromium.chrome.browser.tabmodel.TabModelObserver;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorObserver;
import org.chromium.chrome.browser.tasks.ReturnToChromeUtil;
import org.chromium.chrome.browser.tasks.tab_management.TabManagementDelegate.TabSwitcherType;
import org.chromium.chrome.browser.tasks.tab_management.TabSwitcher;
import org.chromium.chrome.browser.tasks.tab_management.TabSwitcher.TabSwitcherViewObserver;
import org.chromium.chrome.features.start_surface.StartSurfaceMediator.SecondaryTasksSurfaceInitializer;
import org.chromium.chrome.features.tasks.TasksSurfaceProperties;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.chrome.test.util.browser.Features.DisableFeatures;
import org.chromium.chrome.test.util.browser.Features.EnableFeatures;
import org.chromium.components.prefs.PrefService;
import org.chromium.components.search_engines.TemplateUrlService;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.url.JUnitTestGURLs;
import org.chromium.url.ShadowGURL;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;

/** Tests for {@link StartSurfaceMediator}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE, shadows = ShadowGURL.class)
public class StartSurfaceMediatorUnitTest {
    private static final String START_SURFACE_TIME_SPENT = "StartSurface.TimeSpent";
    private PropertyModel mPropertyModel;
    private PropertyModel mSecondaryTasksSurfacePropertyModel;

    @Rule
    public JniMocker mJniMocker = new JniMocker();
    @Rule
    public TestRule mProcessor = new Features.JUnitProcessor();
    @Mock
    private TabSwitcher.Controller mMainTabGridController;
    @Mock
    private TabModelSelector mTabModelSelector;
    @Mock
    private Tab mTab;
    @Mock
    private TabModel mNormalTabModel;
    @Mock
    private TabModel mIncognitoTabModel;
    @Mock
    private List<TabModel> mTabModels;
    @Mock
    private TabModelFilterProvider mTabModelFilterProvider;
    @Mock
    private TabModelFilter mTabModelFilter;
    @Mock
    private OmniboxStub mOmniboxStub;
    @Mock
    private ExploreSurfaceCoordinator mExploreSurfaceCoordinator;
    @Mock
    private ExploreSurfaceCoordinatorFactory mExploreSurfaceCoordinatorFactory;
    @Mock
    private NightModeStateProvider mNightModeStateProvider;
    @Mock
    private BrowserControlsStateProvider mBrowserControlsStateProvider;
    @Mock
    private StartSurfaceMediator.ActivityStateChecker mActivityStateChecker;
    @Mock
    private VoiceRecognitionHandler mVoiceRecognitionHandler;
    @Mock
    private SecondaryTasksSurfaceInitializer mSecondaryTasksSurfaceInitializer;
    @Mock
    private TabSwitcher.Controller mSecondaryTasksSurfaceController;
    @Mock
    private PrefService mPrefService;
    @Mock
    private OneshotSupplier<StartSurface> mStartSurfaceSupplier;
    @Mock
    private Runnable mInitializeMVTilesRunnable;
    @Mock
    private FeedReliabilityLogger mFeedReliabilityLogger;
    @Mock
    private BackPressManager mBackPressManager;
    @Mock
    private Supplier<Tab> mParentTabSupplier;
    @Mock
    private View mLogoContainerView;
    @Mock
    private LogoView mLogoView;
    @Mock
    LogoBridge.Natives mLogoBridge;
    @Mock
    private Profile mProfile;
    @Mock
    private TemplateUrlService mTemplateUrlService;
    @Mock
    private ActivityLifecycleDispatcher mActivityLifecycleDispatcher;
    @Captor
    private ArgumentCaptor<TabModelSelectorObserver> mTabModelSelectorObserverCaptor;
    @Captor
    private ArgumentCaptor<TabModelObserver> mTabModelObserverCaptor;
    @Captor
    private ArgumentCaptor<TabSwitcherViewObserver> mTabSwitcherVisibilityObserverCaptor;
    @Captor
    private ArgumentCaptor<UrlFocusChangeListener> mUrlFocusChangeListenerCaptor;
    @Captor
    private ArgumentCaptor<BrowserControlsStateProvider.Observer>
            mBrowserControlsStateProviderCaptor;
    @Captor
    private ArgumentCaptor<PauseResumeWithNativeObserver>
            mPauseResumeWithNativeObserverArgumentCaptor;

    private ObservableSupplierImpl<Boolean> mControllerBackPressStateSupplier =
            new ObservableSupplierImpl<>();
    private ObservableSupplierImpl<Boolean> mControllerDialogVisibleSupplier =
            new ObservableSupplierImpl<>();
    private ObservableSupplierImpl<Boolean> mSecondaryControllerBackPressStateSupplier =
            new ObservableSupplierImpl<>();
    private ObservableSupplierImpl<Boolean> mSecondaryControllerDialogVisibleSupplier =
            new ObservableSupplierImpl<>();

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);

        Profile.setLastUsedProfileForTesting(mProfile);
        TemplateUrlServiceFactory.setInstanceForTesting(mTemplateUrlService);

        ArrayList<PropertyKey> allProperties =
                new ArrayList<>(Arrays.asList(TasksSurfaceProperties.ALL_KEYS));
        allProperties.addAll(Arrays.asList(StartSurfaceProperties.ALL_KEYS));
        mPropertyModel = new PropertyModel(allProperties);
        mSecondaryTasksSurfacePropertyModel = new PropertyModel(allProperties);

        mTabModels = new ArrayList<>(2);
        mTabModels.add(mNormalTabModel);
        mTabModels.add(mIncognitoTabModel);
        when(mTabModelSelector.getModels()).thenReturn(mTabModels);
        doReturn(mNormalTabModel).when(mTabModelSelector).getModel(false);
        doReturn(mIncognitoTabModel).when(mTabModelSelector).getModel(true);
        doReturn(false).when(mNormalTabModel).isIncognito();
        doReturn(true).when(mIncognitoTabModel).isIncognito();
        doReturn(TabSwitcherType.CAROUSEL).when(mMainTabGridController).getTabSwitcherType();
        doReturn(mControllerBackPressStateSupplier)
                .when(mMainTabGridController)
                .getHandleBackPressChangedSupplier();
        doReturn(mControllerDialogVisibleSupplier)
                .when(mMainTabGridController)
                .isDialogVisibleSupplier();
        doReturn(mSecondaryTasksSurfaceController)
                .when(mSecondaryTasksSurfaceInitializer)
                .initialize();
        doReturn(mSecondaryControllerBackPressStateSupplier)
                .when(mSecondaryTasksSurfaceController)
                .getHandleBackPressChangedSupplier();
        doReturn(mSecondaryControllerDialogVisibleSupplier)
                .when(mSecondaryTasksSurfaceController)
                .isDialogVisibleSupplier();
        doReturn(false).when(mActivityStateChecker).isFinishingOrDestroyed();
        doReturn(mTab).when(mTabModelSelector).getCurrentTab();
        doReturn(mExploreSurfaceCoordinator)
                .when(mExploreSurfaceCoordinatorFactory)
                .create(anyBoolean(), anyBoolean(), anyInt());
        doReturn(mFeedReliabilityLogger)
                .when(mExploreSurfaceCoordinator)
                .getFeedReliabilityLogger();
        mJniMocker.mock(LogoBridgeJni.TEST_HOOKS, mLogoBridge);
        doReturn(mLogoView).when(mLogoContainerView).findViewById(R.id.search_provider_logo);
    }

    @After
    public void tearDown() {
        mPropertyModel = null;
    }

    @Test
    public void showAndHideNoStartSurface() {
        doReturn(false).when(mTabModelSelector).isIncognitoSelected();

        StartSurfaceMediator mediator =
                createStartSurfaceMediator(/* isStartSurfaceEnabled= */ false);
        verify(mTabModelSelector, never()).addObserver(mTabModelSelectorObserverCaptor.capture());
        verify(mMainTabGridController)
                .addTabSwitcherViewObserver(mTabSwitcherVisibilityObserverCaptor.capture());

        mediator.showOverview(false);
        verify(mMainTabGridController).showTabSwitcherView(eq(false));

        mTabSwitcherVisibilityObserverCaptor.getValue().startedShowing();
        mTabSwitcherVisibilityObserverCaptor.getValue().finishedShowing();

        mediator.hideTabSwitcherView(true);
        verify(mMainTabGridController).hideTabSwitcherView(eq(true));

        mTabSwitcherVisibilityObserverCaptor.getValue().startedHiding();
        mTabSwitcherVisibilityObserverCaptor.getValue().finishedHiding();

        // TODO(crbug.com/1020223): Test the other SurfaceMode.NO_START_SURFACE operations.
    }

    @Test
    public void showAndHideSingleSurface() {
        doReturn(false).when(mTabModelSelector).isIncognitoSelected();
        doReturn(mVoiceRecognitionHandler).when(mOmniboxStub).getVoiceRecognitionHandler();
        doReturn(true).when(mVoiceRecognitionHandler).isVoiceSearchEnabled();

        StartSurfaceMediator mediator =
                createStartSurfaceMediator(/* isStartSurfaceEnabled= */ true);
        verify(mMainTabGridController)
                .addTabSwitcherViewObserver(mTabSwitcherVisibilityObserverCaptor.capture());

        assertThat(mediator.getStartSurfaceState(), equalTo(StartSurfaceState.NOT_SHOWN));
        // Sets the current StartSurfaceState to SHOWING_START before calling the
        // {@link StartSurfaceMediator#showOverview()}. This is because if the current
        // StartSurfaceState is NOT_SHOWN, the state will be set default to SHOWING_TABSWITCHER in
        // {@link StartSurfaceMediator#showOverview()}.
        mediator.setStartSurfaceState(StartSurfaceState.SHOWING_START);

        mediator.showOverview(false);
        verify(mMainTabGridController).showTabSwitcherView(eq(false));
        verify(mOmniboxStub, times(2))
                .addUrlFocusChangeListener(mUrlFocusChangeListenerCaptor.capture());
        assertThat(mediator.getStartSurfaceState(), equalTo(StartSurfaceState.SHOWN_HOMEPAGE));
        assertThat(mPropertyModel.get(IS_INCOGNITO), equalTo(false));
        assertThat(mPropertyModel.get(IS_VOICE_RECOGNITION_BUTTON_VISIBLE), equalTo(true));
        assertThat(mPropertyModel.get(IS_EXPLORE_SURFACE_VISIBLE), equalTo(true));
        assertThat(mPropertyModel.get(MV_TILES_VISIBLE), equalTo(true));
        assertThat(mPropertyModel.get(IS_SHOWING_OVERVIEW), equalTo(true));

        mTabSwitcherVisibilityObserverCaptor.getValue().startedShowing();
        mTabSwitcherVisibilityObserverCaptor.getValue().finishedShowing();

        UrlFocusChangeListener urlFocusChangeListener =
                mUrlFocusChangeListenerCaptor.getAllValues().get(1);

        urlFocusChangeListener.onUrlFocusChange(true);
        assertThat(mPropertyModel.get(IS_FAKE_SEARCH_BOX_VISIBLE), equalTo(false));
        urlFocusChangeListener.onUrlFocusChange(false);
        assertThat(mPropertyModel.get(IS_FAKE_SEARCH_BOX_VISIBLE), equalTo(true));
        assertThat(mPropertyModel.get(IS_VOICE_RECOGNITION_BUTTON_VISIBLE), equalTo(true));

        mediator.hideTabSwitcherView(true);
        verify(mMainTabGridController).hideTabSwitcherView(eq(true));

        mTabSwitcherVisibilityObserverCaptor.getValue().startedHiding();
        assertThat(mPropertyModel.get(IS_SHOWING_OVERVIEW), equalTo(false));
        verify(mOmniboxStub).removeUrlFocusChangeListener(mUrlFocusChangeListenerCaptor.getValue());

        mTabSwitcherVisibilityObserverCaptor.getValue().finishedHiding();

        // TODO(crbug.com/1020223): Test the other SurfaceMode.SINGLE_PANE operations.
    }

    // TODO(crbug.com/1020223): Test SurfaceMode.SINGLE_PANE and SurfaceMode.TWO_PANES modes.
    @Test
    public void hideTabCarouselWithNoTabs() {
        doReturn(false).when(mTabModelSelector).isIncognitoSelected();
        doReturn(mVoiceRecognitionHandler).when(mOmniboxStub).getVoiceRecognitionHandler();
        doReturn(true).when(mVoiceRecognitionHandler).isVoiceSearchEnabled();

        StartSurfaceMediator mediator =
                createStartSurfaceMediator(/* isStartSurfaceEnabled= */ true);

        doReturn(0).when(mNormalTabModel).getCount();
        mediator.setStartSurfaceState(StartSurfaceState.SHOWN_HOMEPAGE);
        mediator.showOverview(false);
        mediator.setStartSurfaceState(StartSurfaceState.SHOWN_HOMEPAGE);
        verify(mNormalTabModel).addObserver(mTabModelObserverCaptor.capture());
        assertThat(mPropertyModel.get(IS_SHOWING_OVERVIEW), equalTo(true));
        assertThat(mPropertyModel.get(IS_TAB_CAROUSEL_VISIBLE), equalTo(false));
        assertThat(mPropertyModel.get(IS_TAB_CAROUSEL_TITLE_VISIBLE), equalTo(false));
    }

    @Test
    public void hideTabCarouselWhenClosingLastTab() {
        doReturn(false).when(mTabModelSelector).isIncognitoSelected();
        doReturn(mVoiceRecognitionHandler).when(mOmniboxStub).getVoiceRecognitionHandler();
        doReturn(true).when(mVoiceRecognitionHandler).isVoiceSearchEnabled();

        StartSurfaceMediator mediator =
                createStartSurfaceMediator(/* isStartSurfaceEnabled= */ true);

        doReturn(2).when(mNormalTabModel).getCount();
        doReturn(true).when(mTabModelSelector).isTabStateInitialized();
        mediator.setStartSurfaceState(StartSurfaceState.SHOWING_HOMEPAGE);
        mediator.showOverview(false);
        mediator.setStartSurfaceState(StartSurfaceState.SHOWN_HOMEPAGE);
        verify(mNormalTabModel).addObserver(mTabModelObserverCaptor.capture());
        assertThat(mPropertyModel.get(IS_SHOWING_OVERVIEW), equalTo(true));
        assertThat(mPropertyModel.get(IS_TAB_CAROUSEL_VISIBLE), equalTo(true));
        assertThat(mPropertyModel.get(IS_TAB_CAROUSEL_TITLE_VISIBLE), equalTo(true));

        mTabModelObserverCaptor.getValue().willCloseTab(mock(Tab.class), false, true);
        assertThat(mPropertyModel.get(IS_SHOWING_OVERVIEW), equalTo(true));
        assertThat(mPropertyModel.get(IS_TAB_CAROUSEL_VISIBLE), equalTo(true));
        assertThat(mPropertyModel.get(IS_TAB_CAROUSEL_TITLE_VISIBLE), equalTo(true));

        doReturn(1).when(mNormalTabModel).getCount();
        mTabModelObserverCaptor.getValue().willCloseTab(mock(Tab.class), false, true);
        assertThat(mPropertyModel.get(IS_SHOWING_OVERVIEW), equalTo(true));
        assertThat(mPropertyModel.get(IS_TAB_CAROUSEL_VISIBLE), equalTo(false));
        assertThat(mPropertyModel.get(IS_TAB_CAROUSEL_TITLE_VISIBLE), equalTo(false));
    }

    @Test
    public void hideTabCarouselWhenClosingAndSelectingNTPTab() {
        doReturn(false).when(mTabModelSelector).isIncognitoSelected();
        doReturn(mVoiceRecognitionHandler).when(mOmniboxStub).getVoiceRecognitionHandler();
        doReturn(true).when(mVoiceRecognitionHandler).isVoiceSearchEnabled();

        StartSurfaceMediator mediator =
                createStartSurfaceMediator(/* isStartSurfaceEnabled= */ true);

        doReturn(2).when(mNormalTabModel).getCount();
        doReturn(true).when(mTabModelSelector).isTabStateInitialized();
        mediator.setStartSurfaceState(StartSurfaceState.SHOWING_HOMEPAGE);
        mediator.showOverview(false);
        mediator.setStartSurfaceState(StartSurfaceState.SHOWN_HOMEPAGE);
        verify(mNormalTabModel).addObserver(mTabModelObserverCaptor.capture());
        assertThat(mPropertyModel.get(IS_SHOWING_OVERVIEW), equalTo(true));
        assertThat(mPropertyModel.get(IS_TAB_CAROUSEL_VISIBLE), equalTo(true));
        assertThat(mPropertyModel.get(IS_TAB_CAROUSEL_TITLE_VISIBLE), equalTo(true));

        Tab tab1 = mock(Tab.class);
        doReturn(JUnitTestGURLs.getGURL(JUnitTestGURLs.URL_1)).when(tab1).getUrl();
        mTabModelObserverCaptor.getValue().didSelectTab(tab1, TabSelectionType.FROM_CLOSE, 1);
        assertThat(mPropertyModel.get(IS_SHOWING_OVERVIEW), equalTo(true));
        assertThat(mPropertyModel.get(IS_TAB_CAROUSEL_VISIBLE), equalTo(true));
        assertThat(mPropertyModel.get(IS_TAB_CAROUSEL_TITLE_VISIBLE), equalTo(true));

        Tab NTPTab = mock(Tab.class);
        doReturn(JUnitTestGURLs.getGURL(JUnitTestGURLs.NTP_URL)).when(NTPTab).getUrl();
        mTabModelObserverCaptor.getValue().didSelectTab(NTPTab, TabSelectionType.FROM_CLOSE, 2);
        assertThat(mPropertyModel.get(IS_SHOWING_OVERVIEW), equalTo(true));
        assertThat(mPropertyModel.get(IS_TAB_CAROUSEL_VISIBLE), equalTo(false));
        assertThat(mPropertyModel.get(IS_TAB_CAROUSEL_TITLE_VISIBLE), equalTo(false));
    }

    @Test
    public void reshowTabCarouselWhenTabClosureUndone() {
        doReturn(false).when(mTabModelSelector).isIncognitoSelected();
        doReturn(mVoiceRecognitionHandler).when(mOmniboxStub).getVoiceRecognitionHandler();
        doReturn(true).when(mVoiceRecognitionHandler).isVoiceSearchEnabled();

        StartSurfaceMediator mediator =
                createStartSurfaceMediator(/* isStartSurfaceEnabled= */ true);

        doReturn(1).when(mNormalTabModel).getCount();

        mediator.setSecondaryTasksSurfacePropertyModel(mSecondaryTasksSurfacePropertyModel);
        mediator.setStartSurfaceState(StartSurfaceState.SHOWING_HOMEPAGE);
        mediator.showOverview(false);

        verify(mNormalTabModel).addObserver(mTabModelObserverCaptor.capture());

        mediator.setStartSurfaceState(StartSurfaceState.SHOWN_HOMEPAGE);
        mTabModelObserverCaptor.getValue().willCloseTab(mock(Tab.class), false, true);
        assertThat(mPropertyModel.get(IS_SHOWING_OVERVIEW), equalTo(true));
        assertThat(mPropertyModel.get(IS_TAB_CAROUSEL_VISIBLE), equalTo(false));
        assertThat(mPropertyModel.get(IS_TAB_CAROUSEL_TITLE_VISIBLE), equalTo(false));

        mTabModelObserverCaptor.getValue().tabClosureUndone(mock(Tab.class));
        assertThat(mPropertyModel.get(IS_SHOWING_OVERVIEW), equalTo(true));
        assertThat(mPropertyModel.get(IS_TAB_CAROUSEL_VISIBLE), equalTo(true));
        assertThat(mPropertyModel.get(IS_TAB_CAROUSEL_TITLE_VISIBLE), equalTo(true));

        doReturn(2).when(mNormalTabModel).getCount();
        mediator.setStartSurfaceState(StartSurfaceState.SHOWN_TABSWITCHER);
        mTabModelObserverCaptor.getValue().willCloseTab(mock(Tab.class), false, true);
        mTabModelObserverCaptor.getValue().tabClosureUndone(mock(Tab.class));
        doReturn(0).when(mNormalTabModel).getCount();
        mTabModelObserverCaptor.getValue().willCloseTab(mock(Tab.class), false, true);
        assertThat(mPropertyModel.get(IS_TAB_CAROUSEL_VISIBLE), equalTo(false));
        assertThat(mPropertyModel.get(IS_TAB_CAROUSEL_TITLE_VISIBLE), equalTo(false));
        assertThat(mPropertyModel.get(IS_SECONDARY_SURFACE_VISIBLE), equalTo(true));
    }

    @Test
    public void pendingTabModelObserverWithBothShowOverviewAndHideBeforeTabModelInitialization() {
        doReturn(false).when(mTabModelSelector).isIncognitoSelected();
        doReturn(mVoiceRecognitionHandler).when(mOmniboxStub).getVoiceRecognitionHandler();
        doReturn(true).when(mVoiceRecognitionHandler).isVoiceSearchEnabled();

        mTabModels.clear();
        StartSurfaceMediator mediator =
                createStartSurfaceMediator(/* isStartSurfaceEnabled= */ true);
        verify(mTabModelSelector).addObserver(mTabModelSelectorObserverCaptor.capture());

        mediator.setStartSurfaceState(StartSurfaceState.SHOWN_HOMEPAGE);
        mediator.showOverview(false);
        verify(mNormalTabModel, never()).addObserver(mTabModelObserverCaptor.capture());
        mediator.startedHiding();
        verify(mNormalTabModel, never()).addObserver(mTabModelObserverCaptor.capture());
        verify(mNormalTabModel, never()).removeObserver(mTabModelObserverCaptor.capture());
    }

    @Test
    public void pendingTabModelObserverWithShowOverviewBeforeAndHideAfterTabModelInitialization() {
        doReturn(false).when(mTabModelSelector).isIncognitoSelected();
        doReturn(mVoiceRecognitionHandler).when(mOmniboxStub).getVoiceRecognitionHandler();
        doReturn(true).when(mVoiceRecognitionHandler).isVoiceSearchEnabled();

        mTabModels.clear();
        StartSurfaceMediator mediator =
                createStartSurfaceMediator(/* isStartSurfaceEnabled= */ true);
        verify(mTabModelSelector).addObserver(mTabModelSelectorObserverCaptor.capture());

        mediator.setStartSurfaceState(StartSurfaceState.SHOWN_HOMEPAGE);
        mediator.showOverview(false);
        verify(mNormalTabModel, never()).addObserver(mTabModelObserverCaptor.capture());

        mTabModels.add(mNormalTabModel);
        mTabModels.add(mIncognitoTabModel);
        when(mTabModelSelector.getTabModelFilterProvider()).thenReturn(mTabModelFilterProvider);
        when(mTabModelFilterProvider.getTabModelFilter(false)).thenReturn(mTabModelFilter);
        when(mTabModelFilterProvider.getTabModelFilter(true)).thenReturn(mTabModelFilter);
        mTabModelSelectorObserverCaptor.getValue().onChange();
        verify(mTabModelSelector).removeObserver(mTabModelSelectorObserverCaptor.capture());
        verify(mNormalTabModel).addObserver(mTabModelObserverCaptor.capture());

        mediator.startedHiding();
        verify(mNormalTabModel).removeObserver(mTabModelObserverCaptor.capture());
    }

    @Test
    public void pendingTabModelObserverWithBothShowAndHideOverviewAfterTabModelInitialization() {
        doReturn(false).when(mTabModelSelector).isIncognitoSelected();
        doReturn(mVoiceRecognitionHandler).when(mOmniboxStub).getVoiceRecognitionHandler();
        doReturn(true).when(mVoiceRecognitionHandler).isVoiceSearchEnabled();

        mTabModels.clear();
        StartSurfaceMediator mediator =
                createStartSurfaceMediator(/* isStartSurfaceEnabled= */ true);
        verify(mTabModelSelector).addObserver(mTabModelSelectorObserverCaptor.capture());

        mTabModels.add(mNormalTabModel);
        mTabModels.add(mIncognitoTabModel);
        when(mTabModelSelector.getTabModelFilterProvider()).thenReturn(mTabModelFilterProvider);
        when(mTabModelFilterProvider.getTabModelFilter(false)).thenReturn(mTabModelFilter);
        when(mTabModelFilterProvider.getTabModelFilter(true)).thenReturn(mTabModelFilter);
        mTabModelSelectorObserverCaptor.getValue().onChange();
        verify(mTabModelSelector).removeObserver(mTabModelSelectorObserverCaptor.capture());
        verify(mNormalTabModel, never()).addObserver(mTabModelObserverCaptor.capture());

        mediator.setStartSurfaceState(StartSurfaceState.SHOWN_HOMEPAGE);
        mediator.showOverview(false);
        verify(mNormalTabModel).addObserver(mTabModelObserverCaptor.capture());

        mediator.startedHiding();
        verify(mNormalTabModel).removeObserver(mTabModelObserverCaptor.capture());
    }

    @Test
    public void addAndRemoveTabModelSelectorObserverWithOverviewAfterTabModelInitialization() {
        doReturn(false).when(mTabModelSelector).isIncognitoSelected();
        doReturn(mVoiceRecognitionHandler).when(mOmniboxStub).getVoiceRecognitionHandler();
        doReturn(true).when(mVoiceRecognitionHandler).isVoiceSearchEnabled();

        StartSurfaceMediator mediator =
                createStartSurfaceMediator(/* isStartSurfaceEnabled= */ true);
        verify(mNormalTabModel, never()).addObserver(mTabModelObserverCaptor.capture());

        mediator.setStartSurfaceState(StartSurfaceState.SHOWING_HOMEPAGE);
        mediator.showOverview(false);
        verify(mNormalTabModel).addObserver(mTabModelObserverCaptor.capture());

        mediator.startedHiding();
        verify(mNormalTabModel).removeObserver(mTabModelObserverCaptor.capture());
    }

    @Test
    public void addAndRemoveTabModelSelectorObserverWithOverview() {
        doReturn(false).when(mTabModelSelector).isIncognitoSelected();
        doReturn(mVoiceRecognitionHandler).when(mOmniboxStub).getVoiceRecognitionHandler();
        doReturn(true).when(mVoiceRecognitionHandler).isVoiceSearchEnabled();

        StartSurfaceMediator mediator =
                createStartSurfaceMediator(/* isStartSurfaceEnabled= */ true);

        verify(mTabModelSelector, never()).addObserver(mTabModelSelectorObserverCaptor.capture());

        mediator.setStartSurfaceState(StartSurfaceState.SHOWN_HOMEPAGE);
        mediator.showOverview(false);
        verify(mTabModelSelector).addObserver(mTabModelSelectorObserverCaptor.capture());

        mediator.startedHiding();
        verify(mTabModelSelector).removeObserver(mTabModelSelectorObserverCaptor.capture());
    }

    @Test
    public void overviewModeStatesNormalModeSinglePane() {
        doReturn(false).when(mTabModelSelector).isIncognitoSelected();
        doReturn(mVoiceRecognitionHandler).when(mOmniboxStub).getVoiceRecognitionHandler();
        doReturn(true).when(mVoiceRecognitionHandler).isVoiceSearchEnabled();

        StartSurfaceMediator mediator =
                createStartSurfaceMediator(/* isStartSurfaceEnabled= */ true);
        assertThat(mediator.getStartSurfaceState(), equalTo(StartSurfaceState.NOT_SHOWN));

        doReturn(2).when(mNormalTabModel).getCount();
        doReturn(true).when(mTabModelSelector).isTabStateInitialized();
        mediator.setStartSurfaceState(StartSurfaceState.SHOWING_HOMEPAGE);
        mediator.showOverview(false);
        assertThat(mediator.getStartSurfaceState(), equalTo(StartSurfaceState.SHOWN_HOMEPAGE));
        assertThat(mPropertyModel.get(IS_SHOWING_OVERVIEW), equalTo(true));
        assertThat(mPropertyModel.get(IS_INCOGNITO), equalTo(false));
        assertThat(mPropertyModel.get(IS_EXPLORE_SURFACE_VISIBLE), equalTo(true));
        assertThat(mPropertyModel.get(MV_TILES_VISIBLE), equalTo(true));
        assertThat(mPropertyModel.get(IS_TAB_CAROUSEL_VISIBLE), equalTo(true));
        assertThat(mPropertyModel.get(IS_TAB_CAROUSEL_TITLE_VISIBLE), equalTo(true));
        assertThat(mPropertyModel.get(IS_SECONDARY_SURFACE_VISIBLE), equalTo(false));

        mediator.setSecondaryTasksSurfacePropertyModel(mSecondaryTasksSurfacePropertyModel);
        mediator.onClick(mock(View.class));
        assertThat(mediator.getStartSurfaceState(), equalTo(StartSurfaceState.SHOWN_TABSWITCHER));
        assertThat(mPropertyModel.get(IS_SHOWING_OVERVIEW), equalTo(true));
        assertThat(mPropertyModel.get(IS_INCOGNITO), equalTo(false));
        assertThat(mPropertyModel.get(IS_EXPLORE_SURFACE_VISIBLE), equalTo(false));
        assertThat(mPropertyModel.get(MV_TILES_VISIBLE), equalTo(false));
        assertThat(mPropertyModel.get(IS_TAB_CAROUSEL_VISIBLE), equalTo(false));
        assertThat(mPropertyModel.get(IS_TAB_CAROUSEL_TITLE_VISIBLE), equalTo(false));
        assertThat(mPropertyModel.get(IS_SECONDARY_SURFACE_VISIBLE), equalTo(true));
        assertThat(mSecondaryTasksSurfacePropertyModel.get(IS_FAKE_SEARCH_BOX_VISIBLE),
                equalTo(false));
        assertThat(mSecondaryTasksSurfacePropertyModel.get(IS_INCOGNITO), equalTo(false));

        mediator.onBackPressed();
        assertThat(mediator.getStartSurfaceState(), equalTo(StartSurfaceState.SHOWN_HOMEPAGE));
        assertThat(mPropertyModel.get(IS_SHOWING_OVERVIEW), equalTo(true));
        assertThat(mPropertyModel.get(IS_INCOGNITO), equalTo(false));
        assertThat(mPropertyModel.get(IS_EXPLORE_SURFACE_VISIBLE), equalTo(true));
        assertThat(mPropertyModel.get(MV_TILES_VISIBLE), equalTo(true));
        assertThat(mPropertyModel.get(IS_TAB_CAROUSEL_VISIBLE), equalTo(true));
        assertThat(mPropertyModel.get(IS_TAB_CAROUSEL_TITLE_VISIBLE), equalTo(true));
        assertThat(mPropertyModel.get(IS_SECONDARY_SURFACE_VISIBLE), equalTo(false));

        mediator.startedHiding();
        assertThat(mediator.getStartSurfaceState(), equalTo(StartSurfaceState.NOT_SHOWN));
        assertThat(mPropertyModel.get(IS_SECONDARY_SURFACE_VISIBLE), equalTo(false));
    }

    @Test
    public void overviewModeIncognitoModeSinglePane() {
        doReturn(true).when(mTabModelSelector).isIncognitoSelected();
        doReturn(mVoiceRecognitionHandler).when(mOmniboxStub).getVoiceRecognitionHandler();
        doReturn(true).when(mVoiceRecognitionHandler).isVoiceSearchEnabled();

        StartSurfaceMediator mediator =
                createStartSurfaceMediator(/* isStartSurfaceEnabled= */ true);
        mediator.setSecondaryTasksSurfacePropertyModel(mSecondaryTasksSurfacePropertyModel);

        assertThat(mediator.getStartSurfaceState(), equalTo(StartSurfaceState.NOT_SHOWN));

        doReturn(2).when(mNormalTabModel).getCount();
        mediator.setStartSurfaceState(StartSurfaceState.SHOWN_TABSWITCHER);
        mediator.showOverview(false);
        assertThat(mediator.getStartSurfaceState(), equalTo(StartSurfaceState.SHOWN_TABSWITCHER));
        assertThat(mPropertyModel.get(IS_SHOWING_OVERVIEW), equalTo(true));
        assertThat(mPropertyModel.get(IS_INCOGNITO), equalTo(true));
        assertThat(mPropertyModel.get(IS_EXPLORE_SURFACE_VISIBLE), equalTo(false));
        assertThat(mPropertyModel.get(MV_TILES_VISIBLE), equalTo(false));
        assertThat(mPropertyModel.get(IS_TAB_CAROUSEL_VISIBLE), equalTo(false));
        assertThat(mPropertyModel.get(IS_TAB_CAROUSEL_TITLE_VISIBLE), equalTo(false));
        assertThat(mPropertyModel.get(IS_SECONDARY_SURFACE_VISIBLE), equalTo(true));
        assertThat(mSecondaryTasksSurfacePropertyModel.get(IS_FAKE_SEARCH_BOX_VISIBLE),
                equalTo(false));
        assertThat(mSecondaryTasksSurfacePropertyModel.get(IS_INCOGNITO), equalTo(true));

        mediator.hideTabSwitcherView(false);
        mediator.startedHiding();
        assertThat(mediator.getStartSurfaceState(), equalTo(StartSurfaceState.NOT_SHOWN));
        assertThat(mPropertyModel.get(IS_SECONDARY_SURFACE_VISIBLE), equalTo(false));
    }

    @Test
    public void overviewModeSwitchToIncognitoModeAndBackSinglePane() {
        doReturn(false).when(mTabModelSelector).isIncognitoSelected();
        doReturn(mVoiceRecognitionHandler).when(mOmniboxStub).getVoiceRecognitionHandler();
        doReturn(true).when(mVoiceRecognitionHandler).isVoiceSearchEnabled();

        StartSurfaceMediator mediator =
                createStartSurfaceMediator(/* isStartSurfaceEnabled= */ true);
        mediator.setSecondaryTasksSurfacePropertyModel(mSecondaryTasksSurfacePropertyModel);
        assertThat(mediator.getStartSurfaceState(), equalTo(StartSurfaceState.NOT_SHOWN));

        doReturn(2).when(mNormalTabModel).getCount();
        doReturn(true).when(mTabModelSelector).isTabStateInitialized();
        mediator.setStartSurfaceState(StartSurfaceState.SHOWING_HOMEPAGE);
        mediator.showOverview(false);
        verify(mTabModelSelector).addObserver(mTabModelSelectorObserverCaptor.capture());
        assertThat(mediator.getStartSurfaceState(), equalTo(StartSurfaceState.SHOWN_HOMEPAGE));
        assertThat(mPropertyModel.get(IS_SHOWING_OVERVIEW), equalTo(true));
        assertThat(mPropertyModel.get(IS_INCOGNITO), equalTo(false));
        assertThat(mPropertyModel.get(IS_EXPLORE_SURFACE_VISIBLE), equalTo(true));
        assertThat(mPropertyModel.get(MV_TILES_VISIBLE), equalTo(true));
        assertThat(mPropertyModel.get(IS_TAB_CAROUSEL_VISIBLE), equalTo(true));
        assertThat(mPropertyModel.get(IS_TAB_CAROUSEL_TITLE_VISIBLE), equalTo(true));
        assertThat(mPropertyModel.get(IS_SECONDARY_SURFACE_VISIBLE), equalTo(false));

        doReturn(true).when(mTabModelSelector).isIncognitoSelected();
        mTabModelSelector.selectModel(true);
        mTabModelSelectorObserverCaptor.getValue().onTabModelSelected(
                mIncognitoTabModel, mNormalTabModel);

        doReturn(false).when(mTabModelSelector).isIncognitoSelected();
        mTabModelSelector.selectModel(false);
        mTabModelSelectorObserverCaptor.getValue().onTabModelSelected(
                mNormalTabModel, mIncognitoTabModel);
        assertThat(mediator.getStartSurfaceState(), equalTo(StartSurfaceState.SHOWN_HOMEPAGE));
        assertThat(mPropertyModel.get(IS_SHOWING_OVERVIEW), equalTo(true));
        assertThat(mPropertyModel.get(IS_INCOGNITO), equalTo(false));
        assertThat(mPropertyModel.get(IS_EXPLORE_SURFACE_VISIBLE), equalTo(true));
        assertThat(mPropertyModel.get(MV_TILES_VISIBLE), equalTo(true));
        assertThat(mPropertyModel.get(IS_TAB_CAROUSEL_VISIBLE), equalTo(true));
        assertThat(mPropertyModel.get(IS_TAB_CAROUSEL_TITLE_VISIBLE), equalTo(true));
        assertThat(mPropertyModel.get(IS_SECONDARY_SURFACE_VISIBLE), equalTo(false));
    }

    @Test
    public void activityIsFinishingOrDestroyedSinglePane() {
        doReturn(false).when(mTabModelSelector).isIncognitoSelected();
        doReturn(mVoiceRecognitionHandler).when(mOmniboxStub).getVoiceRecognitionHandler();
        doReturn(true).when(mVoiceRecognitionHandler).isVoiceSearchEnabled();

        StartSurfaceMediator mediator =
                createStartSurfaceMediator(/* isStartSurfaceEnabled= */ true);
        assertThat(mediator.getStartSurfaceState(), equalTo(StartSurfaceState.NOT_SHOWN));

        doReturn(2).when(mNormalTabModel).getCount();
        doReturn(true).when(mActivityStateChecker).isFinishingOrDestroyed();
        doReturn(true).when(mTabModelSelector).isTabStateInitialized();
        mediator.setStartSurfaceState(StartSurfaceState.SHOWING_HOMEPAGE);
        mediator.showOverview(false);
        assertThat(mediator.getStartSurfaceState(), equalTo(StartSurfaceState.SHOWN_HOMEPAGE));
        assertThat(mPropertyModel.get(IS_SHOWING_OVERVIEW), equalTo(true));
        assertThat(mPropertyModel.get(IS_INCOGNITO), equalTo(false));
        assertThat(mPropertyModel.get(IS_EXPLORE_SURFACE_VISIBLE), equalTo(true));
        assertThat(mPropertyModel.get(MV_TILES_VISIBLE), equalTo(true));
        assertThat(mPropertyModel.get(IS_TAB_CAROUSEL_VISIBLE), equalTo(true));
        assertThat(mPropertyModel.get(IS_TAB_CAROUSEL_TITLE_VISIBLE), equalTo(true));
        assertThat(mPropertyModel.get(IS_SECONDARY_SURFACE_VISIBLE), equalTo(false));
        assertThat(mPropertyModel.get(EXPLORE_SURFACE_COORDINATOR), equalTo(null));

        mediator.setSecondaryTasksSurfacePropertyModel(mSecondaryTasksSurfacePropertyModel);
        mediator.onClick(mock(View.class));
        assertThat(mediator.getStartSurfaceState(), equalTo(StartSurfaceState.SHOWN_TABSWITCHER));
        assertThat(mPropertyModel.get(IS_SHOWING_OVERVIEW), equalTo(true));
        assertThat(mPropertyModel.get(IS_INCOGNITO), equalTo(false));
        assertThat(mPropertyModel.get(IS_EXPLORE_SURFACE_VISIBLE), equalTo(false));
        assertThat(mPropertyModel.get(MV_TILES_VISIBLE), equalTo(false));
        assertThat(mPropertyModel.get(IS_TAB_CAROUSEL_VISIBLE), equalTo(false));
        assertThat(mPropertyModel.get(IS_TAB_CAROUSEL_TITLE_VISIBLE), equalTo(false));
        assertThat(mPropertyModel.get(IS_SECONDARY_SURFACE_VISIBLE), equalTo(true));
        assertThat(mSecondaryTasksSurfacePropertyModel.get(IS_FAKE_SEARCH_BOX_VISIBLE),
                equalTo(false));
        assertThat(mSecondaryTasksSurfacePropertyModel.get(IS_INCOGNITO), equalTo(false));
        assertThat(mPropertyModel.get(EXPLORE_SURFACE_COORDINATOR), equalTo(null));

        mediator.onBackPressed();
        assertThat(mediator.getStartSurfaceState(), equalTo(StartSurfaceState.SHOWN_HOMEPAGE));
        assertThat(mPropertyModel.get(IS_SHOWING_OVERVIEW), equalTo(true));
        assertThat(mPropertyModel.get(IS_INCOGNITO), equalTo(false));
        assertThat(mPropertyModel.get(IS_EXPLORE_SURFACE_VISIBLE), equalTo(true));
        assertThat(mPropertyModel.get(MV_TILES_VISIBLE), equalTo(true));
        assertThat(mPropertyModel.get(IS_TAB_CAROUSEL_VISIBLE), equalTo(true));
        assertThat(mPropertyModel.get(IS_TAB_CAROUSEL_TITLE_VISIBLE), equalTo(true));
        assertThat(mPropertyModel.get(IS_SECONDARY_SURFACE_VISIBLE), equalTo(false));
        assertThat(mPropertyModel.get(EXPLORE_SURFACE_COORDINATOR), equalTo(null));

        mediator.startedHiding();
        assertThat(mediator.getStartSurfaceState(), equalTo(StartSurfaceState.NOT_SHOWN));
        assertThat(mPropertyModel.get(IS_SECONDARY_SURFACE_VISIBLE), equalTo(false));
    }

    @Test
    public void overviewModeIncognitoTabswitcher() {
        doReturn(false).when(mTabModelSelector).isIncognitoSelected();
        doReturn(mVoiceRecognitionHandler).when(mOmniboxStub).getVoiceRecognitionHandler();
        doReturn(true).when(mVoiceRecognitionHandler).isVoiceSearchEnabled();

        StartSurfaceMediator mediator =
                createStartSurfaceMediator(/* isStartSurfaceEnabled= */ true);
        mediator.setSecondaryTasksSurfacePropertyModel(mSecondaryTasksSurfacePropertyModel);

        assertThat(mediator.getStartSurfaceState(), equalTo(StartSurfaceState.NOT_SHOWN));

        doReturn(2).when(mNormalTabModel).getCount();
        mediator.setStartSurfaceState(StartSurfaceState.SHOWING_HOMEPAGE);
        mediator.showOverview(false);
        verify(mTabModelSelector).addObserver(mTabModelSelectorObserverCaptor.capture());
        assertThat(mediator.getStartSurfaceState(), equalTo(StartSurfaceState.SHOWN_HOMEPAGE));

        mediator.setStartSurfaceState(StartSurfaceState.SHOWN_TABSWITCHER);
        assertThat(mSecondaryTasksSurfacePropertyModel.get(IS_INCOGNITO), equalTo(false));

        doReturn(true).when(mTabModelSelector).isIncognitoSelected();
        mTabModelSelector.selectModel(true);
        mTabModelSelectorObserverCaptor.getValue().onTabModelSelected(
                mIncognitoTabModel, mNormalTabModel);
        assertThat(mSecondaryTasksSurfacePropertyModel.get(IS_INCOGNITO), equalTo(true));

        mediator.setStartSurfaceState(StartSurfaceState.SHOWN_HOMEPAGE);
        assertThat(mSecondaryTasksSurfacePropertyModel.get(IS_INCOGNITO), equalTo(true));
        assertThat(mSecondaryTasksSurfacePropertyModel.get(IS_FAKE_SEARCH_BOX_VISIBLE),
                equalTo(false));
        assertThat(mSecondaryTasksSurfacePropertyModel.get(IS_VOICE_RECOGNITION_BUTTON_VISIBLE),
                equalTo(false));
        assertThat(mSecondaryTasksSurfacePropertyModel.get(IS_INCOGNITO), equalTo(true));
    }

    @Test
    public void paddingForBottomBarSinglePane() {
        doReturn(false).when(mTabModelSelector).isIncognitoSelected();
        doReturn(mVoiceRecognitionHandler).when(mOmniboxStub).getVoiceRecognitionHandler();
        doReturn(true).when(mVoiceRecognitionHandler).isVoiceSearchEnabled();

        StartSurfaceMediator mediator =
                createStartSurfaceMediator(/* isStartSurfaceEnabled= */ true);
        verify(mMainTabGridController)
                .addTabSwitcherViewObserver(mTabSwitcherVisibilityObserverCaptor.capture());
        mediator.setSecondaryTasksSurfacePropertyModel(mSecondaryTasksSurfacePropertyModel);
        assertThat(mediator.getStartSurfaceState(), equalTo(StartSurfaceState.NOT_SHOWN));

        doReturn(30).when(mBrowserControlsStateProvider).getBottomControlsHeight();
        doReturn(2).when(mNormalTabModel).getCount();
        mediator.setStartSurfaceState(StartSurfaceState.SHOWING_HOMEPAGE);
        mediator.showOverview(false);
        verify(mBrowserControlsStateProvider)
                .addObserver(mBrowserControlsStateProviderCaptor.capture());
        assertThat(mPropertyModel.get(BOTTOM_BAR_HEIGHT), equalTo(30));
        assertThat(mSecondaryTasksSurfacePropertyModel.get(BOTTOM_BAR_HEIGHT), equalTo(0));

        mTabSwitcherVisibilityObserverCaptor.getValue().startedShowing();
        mTabSwitcherVisibilityObserverCaptor.getValue().finishedShowing();

        mBrowserControlsStateProviderCaptor.getValue().onBottomControlsHeightChanged(0, 0);
        assertThat(mPropertyModel.get(BOTTOM_BAR_HEIGHT), equalTo(0));
        assertThat(mSecondaryTasksSurfacePropertyModel.get(BOTTOM_BAR_HEIGHT), equalTo(0));

        mBrowserControlsStateProviderCaptor.getValue().onBottomControlsHeightChanged(10, 10);
        assertThat(mPropertyModel.get(BOTTOM_BAR_HEIGHT), equalTo(10));
        assertThat(mSecondaryTasksSurfacePropertyModel.get(BOTTOM_BAR_HEIGHT), equalTo(0));

        mediator.hideTabSwitcherView(false);
        mTabSwitcherVisibilityObserverCaptor.getValue().startedHiding();
        verify(mBrowserControlsStateProvider)
                .removeObserver(mBrowserControlsStateProviderCaptor.getValue());
    }

    @Test
    public void setIncognitoDescriptionShowSinglePane() {
        doReturn(false).when(mTabModelSelector).isIncognitoSelected();
        doReturn(mVoiceRecognitionHandler).when(mOmniboxStub).getVoiceRecognitionHandler();
        doReturn(true).when(mVoiceRecognitionHandler).isVoiceSearchEnabled();

        StartSurfaceMediator mediator =
                createStartSurfaceMediator(/* isStartSurfaceEnabled= */ true);
        mediator.setStartSurfaceState(StartSurfaceState.SHOWING_HOMEPAGE);
        mediator.showOverview(false);
        verify(mTabModelSelector).addObserver(mTabModelSelectorObserverCaptor.capture());

        assertThat(mPropertyModel.get(IS_INCOGNITO_DESCRIPTION_INITIALIZED), equalTo(false));
        assertThat(mPropertyModel.get(IS_INCOGNITO_DESCRIPTION_VISIBLE), equalTo(false));
        assertThat(mSecondaryTasksSurfacePropertyModel.get(IS_INCOGNITO_DESCRIPTION_INITIALIZED),
                equalTo(false));
        assertThat(mSecondaryTasksSurfacePropertyModel.get(IS_INCOGNITO_DESCRIPTION_VISIBLE),
                equalTo(false));

        mediator.setSecondaryTasksSurfacePropertyModel(mSecondaryTasksSurfacePropertyModel);
        doReturn(0).when(mIncognitoTabModel).getCount();
        mTabModelSelectorObserverCaptor.getValue().onTabModelSelected(
                mIncognitoTabModel, mNormalTabModel);
        assertThat(mSecondaryTasksSurfacePropertyModel.get(IS_INCOGNITO_DESCRIPTION_INITIALIZED),
                equalTo(true));
        assertThat(mSecondaryTasksSurfacePropertyModel.get(IS_INCOGNITO_DESCRIPTION_VISIBLE),
                equalTo(true));

        mTabModelSelectorObserverCaptor.getValue().onTabModelSelected(
                mNormalTabModel, mIncognitoTabModel);
        assertThat(mSecondaryTasksSurfacePropertyModel.get(IS_INCOGNITO_DESCRIPTION_INITIALIZED),
                equalTo(true));
        assertThat(mSecondaryTasksSurfacePropertyModel.get(IS_INCOGNITO_DESCRIPTION_VISIBLE),
                equalTo(false));

        mediator.hideTabSwitcherView(true);
    }

    @Test
    public void setIncognitoDescriptionHideSinglePane() {
        doReturn(false).when(mTabModelSelector).isIncognitoSelected();
        doReturn(mVoiceRecognitionHandler).when(mOmniboxStub).getVoiceRecognitionHandler();
        doReturn(true).when(mVoiceRecognitionHandler).isVoiceSearchEnabled();

        StartSurfaceMediator mediator =
                createStartSurfaceMediator(/* isStartSurfaceEnabled= */ true);
        mediator.setStartSurfaceState(StartSurfaceState.SHOWN_HOMEPAGE);
        mediator.showOverview(false);
        verify(mTabModelSelector).addObserver(mTabModelSelectorObserverCaptor.capture());

        assertThat(mPropertyModel.get(IS_INCOGNITO_DESCRIPTION_INITIALIZED), equalTo(false));
        assertThat(mPropertyModel.get(IS_INCOGNITO_DESCRIPTION_VISIBLE), equalTo(false));
        assertThat(mSecondaryTasksSurfacePropertyModel.get(IS_INCOGNITO_DESCRIPTION_INITIALIZED),
                equalTo(false));
        assertThat(mSecondaryTasksSurfacePropertyModel.get(IS_INCOGNITO_DESCRIPTION_VISIBLE),
                equalTo(false));

        mediator.setSecondaryTasksSurfacePropertyModel(mSecondaryTasksSurfacePropertyModel);
        doReturn(1).when(mIncognitoTabModel).getCount();
        mTabModelSelectorObserverCaptor.getValue().onTabModelSelected(
                mIncognitoTabModel, mNormalTabModel);
        assertThat(mSecondaryTasksSurfacePropertyModel.get(IS_INCOGNITO_DESCRIPTION_INITIALIZED),
                equalTo(false));
        assertThat(mSecondaryTasksSurfacePropertyModel.get(IS_INCOGNITO_DESCRIPTION_VISIBLE),
                equalTo(false));

        mTabModelSelectorObserverCaptor.getValue().onTabModelSelected(
                mNormalTabModel, mIncognitoTabModel);
        assertThat(mSecondaryTasksSurfacePropertyModel.get(IS_INCOGNITO_DESCRIPTION_INITIALIZED),
                equalTo(false));
        assertThat(mSecondaryTasksSurfacePropertyModel.get(IS_INCOGNITO_DESCRIPTION_VISIBLE),
                equalTo(false));

        mediator.hideTabSwitcherView(true);
    }

    @Test
    public void showAndHideTabSwitcherToolbarHomePage() {
        doReturn(false).when(mTabModelSelector).isIncognitoSelected();
        doReturn(mVoiceRecognitionHandler).when(mOmniboxStub).getVoiceRecognitionHandler();
        doReturn(true).when(mVoiceRecognitionHandler).isVoiceSearchEnabled();

        StartSurfaceMediator mediator =
                createStartSurfaceMediator(/* isStartSurfaceEnabled= */ true);
        verify(mMainTabGridController)
                .addTabSwitcherViewObserver(mTabSwitcherVisibilityObserverCaptor.capture());

        assertThat(mediator.getStartSurfaceState(), equalTo(StartSurfaceState.NOT_SHOWN));

        mediator.setSecondaryTasksSurfacePropertyModel(mSecondaryTasksSurfacePropertyModel);
        mediator.setStartSurfaceState(StartSurfaceState.SHOWN_HOMEPAGE);
        mediator.showOverview(false);
        verify(mTabModelSelector).addObserver(mTabModelSelectorObserverCaptor.capture());
        verify(mMainTabGridController).showTabSwitcherView(eq(false));
        verify(mOmniboxStub, times(2))
                .addUrlFocusChangeListener(mUrlFocusChangeListenerCaptor.capture());
        assertThat(mediator.getStartSurfaceState(), equalTo(StartSurfaceState.SHOWN_HOMEPAGE));
        assertThat(mPropertyModel.get(IS_FAKE_SEARCH_BOX_VISIBLE), equalTo(true));
        assertThat(mediator.shouldShowTabSwitcherToolbar(), equalTo(true));

        UrlFocusChangeListener urlFocusChangeListener =
                mUrlFocusChangeListenerCaptor.getAllValues().get(1);

        urlFocusChangeListener.onUrlFocusChange(true);
        assertThat(mPropertyModel.get(IS_FAKE_SEARCH_BOX_VISIBLE), equalTo(false));
        assertThat(mediator.shouldShowTabSwitcherToolbar(), equalTo(false));

        urlFocusChangeListener.onUrlFocusChange(false);
        assertThat(mPropertyModel.get(IS_FAKE_SEARCH_BOX_VISIBLE), equalTo(true));
        assertThat(mPropertyModel.get(IS_VOICE_RECOGNITION_BUTTON_VISIBLE), equalTo(true));
        assertThat(mediator.shouldShowTabSwitcherToolbar(), equalTo(true));

        doReturn(true).when(mTabModelSelector).isIncognitoSelected();
        mTabModelSelectorObserverCaptor.getValue().onTabModelSelected(
                mIncognitoTabModel, mNormalTabModel);
        assertThat(mSecondaryTasksSurfacePropertyModel.get(IS_FAKE_SEARCH_BOX_VISIBLE),
                equalTo(false));
        assertThat(mediator.shouldShowTabSwitcherToolbar(), equalTo(true));
    }

    @Test
    public void setOverviewState_webFeed_resetsFeedInstanceState() {
        doReturn(false).when(mTabModelSelector).isIncognitoSelected();
        doReturn(mVoiceRecognitionHandler).when(mOmniboxStub).getVoiceRecognitionHandler();
        doReturn(true).when(mVoiceRecognitionHandler).isVoiceSearchEnabled();
        StartSurfaceMediator mediator =
                createStartSurfaceMediator(/* isStartSurfaceEnabled= */ true);

        String instanceState = "state";
        StartSurfaceUserData.getInstance().saveFeedInstanceState(instanceState);

        assertEquals(StartSurfaceUserData.getInstance().restoreFeedInstanceState(), instanceState);

        mediator.setLaunchOrigin(NewTabPageLaunchOrigin.WEB_FEED);
        assertNull(StartSurfaceUserData.getInstance().restoreFeedInstanceState());
    }

    @Test
    public void setOverviewState_nonWebFeed_doesNotResetFeedInstanceState() {
        doReturn(false).when(mTabModelSelector).isIncognitoSelected();
        doReturn(mVoiceRecognitionHandler).when(mOmniboxStub).getVoiceRecognitionHandler();
        doReturn(true).when(mVoiceRecognitionHandler).isVoiceSearchEnabled();
        StartSurfaceMediator mediator =
                createStartSurfaceMediator(/* isStartSurfaceEnabled= */ true);

        String instanceState = "state";
        StartSurfaceUserData.getInstance().saveFeedInstanceState(instanceState);

        assertEquals(StartSurfaceUserData.getInstance().restoreFeedInstanceState(), instanceState);

        mediator.setLaunchOrigin(NewTabPageLaunchOrigin.UNKNOWN);
        assertNotNull(StartSurfaceUserData.getInstance().restoreFeedInstanceState());
    }

    @Test
    public void defaultStateSinglePane() {
        doReturn(false).when(mTabModelSelector).isIncognitoSelected();
        doReturn(mVoiceRecognitionHandler).when(mOmniboxStub).getVoiceRecognitionHandler();
        doReturn(true).when(mVoiceRecognitionHandler).isVoiceSearchEnabled();

        StartSurfaceMediator mediator =
                createStartSurfaceMediator(/* isStartSurfaceEnabled= */ true);
        verify(mMainTabGridController)
                .addTabSwitcherViewObserver(mTabSwitcherVisibilityObserverCaptor.capture());

        assertThat(mediator.getStartSurfaceState(), equalTo(StartSurfaceState.NOT_SHOWN));
        mediator.showOverview(false);
        assertThat(mediator.getStartSurfaceState(), equalTo(StartSurfaceState.SHOWN_TABSWITCHER));
    }

    @Test
    public void showAndHideTabSwitcherToolbarTabswitcher() {
        doReturn(false).when(mTabModelSelector).isIncognitoSelected();
        doReturn(mVoiceRecognitionHandler).when(mOmniboxStub).getVoiceRecognitionHandler();
        doReturn(true).when(mVoiceRecognitionHandler).isVoiceSearchEnabled();

        StartSurfaceMediator mediator =
                createStartSurfaceMediator(/* isStartSurfaceEnabled= */ true);
        verify(mMainTabGridController)
                .addTabSwitcherViewObserver(mTabSwitcherVisibilityObserverCaptor.capture());

        assertThat(mediator.getStartSurfaceState(), equalTo(StartSurfaceState.NOT_SHOWN));

        mediator.setSecondaryTasksSurfacePropertyModel(mSecondaryTasksSurfacePropertyModel);
        mediator.setStartSurfaceState(StartSurfaceState.SHOWING_HOMEPAGE);
        mediator.showOverview(false);
        verify(mMainTabGridController).showTabSwitcherView(eq(false));
        verify(mOmniboxStub, times(2))
                .addUrlFocusChangeListener(mUrlFocusChangeListenerCaptor.capture());
        assertThat(mediator.getStartSurfaceState(), equalTo(StartSurfaceState.SHOWN_HOMEPAGE));
        assertThat(mPropertyModel.get(IS_FAKE_SEARCH_BOX_VISIBLE), equalTo(true));
        assertThat(mediator.shouldShowTabSwitcherToolbar(), equalTo(true));

        UrlFocusChangeListener urlFocusChangeListener =
                mUrlFocusChangeListenerCaptor.getAllValues().get(1);

        urlFocusChangeListener.onUrlFocusChange(true);
        assertThat(mPropertyModel.get(IS_FAKE_SEARCH_BOX_VISIBLE), equalTo(false));
        assertThat(mediator.shouldShowTabSwitcherToolbar(), equalTo(false));

        urlFocusChangeListener.onUrlFocusChange(false);
        assertThat(mPropertyModel.get(IS_FAKE_SEARCH_BOX_VISIBLE), equalTo(true));
        assertThat(mPropertyModel.get(IS_VOICE_RECOGNITION_BUTTON_VISIBLE), equalTo(true));
        assertThat(mediator.shouldShowTabSwitcherToolbar(), equalTo(true));

        mediator.setStartSurfaceState(StartSurfaceState.SHOWN_TABSWITCHER);
        assertThat(mSecondaryTasksSurfacePropertyModel.get(IS_FAKE_SEARCH_BOX_VISIBLE),
                equalTo(false));
        assertThat(mediator.shouldShowTabSwitcherToolbar(), equalTo(true));
    }

    @Test
    public void singleShowingPrevious() {
        doReturn(false).when(mTabModelSelector).isIncognitoSelected();
        doReturn(mVoiceRecognitionHandler).when(mOmniboxStub).getVoiceRecognitionHandler();
        doReturn(true).when(mVoiceRecognitionHandler).isVoiceSearchEnabled();

        StartSurfaceMediator mediator =
                createStartSurfaceMediator(/* isStartSurfaceEnabled= */ true);
        InOrder mainTabGridController = inOrder(mMainTabGridController);
        mainTabGridController.verify(mMainTabGridController)
                .addTabSwitcherViewObserver(mTabSwitcherVisibilityObserverCaptor.capture());

        assertThat(mediator.getStartSurfaceState(), equalTo(StartSurfaceState.NOT_SHOWN));

        mediator.setSecondaryTasksSurfacePropertyModel(mSecondaryTasksSurfacePropertyModel);
        mediator.setStartSurfaceState(StartSurfaceState.SHOWING_PREVIOUS);
        mediator.showOverview(false);
        mainTabGridController.verify(mMainTabGridController).showTabSwitcherView(eq(false));
        InOrder omniboxStub = inOrder(mOmniboxStub);
        omniboxStub.verify(mOmniboxStub, times(2))
                .addUrlFocusChangeListener(mUrlFocusChangeListenerCaptor.capture());
        assertThat(mediator.getStartSurfaceState(), equalTo(StartSurfaceState.SHOWN_HOMEPAGE));
        assertThat(mPropertyModel.get(IS_FAKE_SEARCH_BOX_VISIBLE), equalTo(true));
        assertThat(mediator.shouldShowTabSwitcherToolbar(), equalTo(true));

        mediator.hideTabSwitcherView(true);
        mTabSwitcherVisibilityObserverCaptor.getValue().startedHiding();
        mTabSwitcherVisibilityObserverCaptor.getValue().finishedHiding();

        mediator.setStartSurfaceState(StartSurfaceState.SHOWING_PREVIOUS);
        mediator.showOverview(false);
        mainTabGridController.verify(mMainTabGridController).showTabSwitcherView(eq(false));
        omniboxStub.verify(mOmniboxStub, times(2))
                .addUrlFocusChangeListener(mUrlFocusChangeListenerCaptor.capture());
        assertThat(mediator.getStartSurfaceState(), equalTo(StartSurfaceState.SHOWN_HOMEPAGE));
        assertThat(mPropertyModel.get(IS_FAKE_SEARCH_BOX_VISIBLE), equalTo(true));
        assertThat(mediator.shouldShowTabSwitcherToolbar(), equalTo(true));

        mediator.setStartSurfaceState(StartSurfaceState.SHOWN_TABSWITCHER);
        assertThat(mSecondaryTasksSurfacePropertyModel.get(IS_FAKE_SEARCH_BOX_VISIBLE),
                equalTo(false));
        assertThat(mediator.shouldShowTabSwitcherToolbar(), equalTo(true));
        assertThat(mediator.getStartSurfaceState(), equalTo(StartSurfaceState.SHOWN_TABSWITCHER));

        mediator.hideTabSwitcherView(true);
        mTabSwitcherVisibilityObserverCaptor.getValue().startedHiding();
        mTabSwitcherVisibilityObserverCaptor.getValue().finishedHiding();

        mediator.setStartSurfaceState(StartSurfaceState.SHOWING_PREVIOUS);
        mediator.showOverview(false);
        assertThat(mSecondaryTasksSurfacePropertyModel.get(IS_FAKE_SEARCH_BOX_VISIBLE),
                equalTo(false));
        assertThat(mediator.shouldShowTabSwitcherToolbar(), equalTo(true));
        assertThat(mediator.getStartSurfaceState(), equalTo(StartSurfaceState.SHOWN_TABSWITCHER));
    }

    @Test
    public void singleShowingPreviousFromATabOfFeeds() {
        doReturn(false).when(mTabModelSelector).isIncognitoSelected();
        doReturn(mVoiceRecognitionHandler).when(mOmniboxStub).getVoiceRecognitionHandler();
        doReturn(true).when(mVoiceRecognitionHandler).isVoiceSearchEnabled();

        StartSurfaceMediator mediator =
                createStartSurfaceMediator(/* isStartSurfaceEnabled= */ true);
        InOrder mainTabGridController = inOrder(mMainTabGridController);
        mainTabGridController.verify(mMainTabGridController)
                .addTabSwitcherViewObserver(mTabSwitcherVisibilityObserverCaptor.capture());
        assertThat(mediator.getStartSurfaceState(), equalTo(StartSurfaceState.NOT_SHOWN));

        mediator.setStartSurfaceState(StartSurfaceState.SHOWING_HOMEPAGE);
        mPropertyModel.set(IS_EXPLORE_SURFACE_VISIBLE, true);
        when(mExploreSurfaceCoordinatorFactory.create(anyBoolean(), anyBoolean(), anyInt()))
                .thenReturn(mExploreSurfaceCoordinator);
        mediator.showOverview(false);
        mainTabGridController.verify(mMainTabGridController).showTabSwitcherView(eq(false));
        assertThat(mediator.getStartSurfaceState(), equalTo(StartSurfaceState.SHOWN_HOMEPAGE));
        assertThat(mPropertyModel.get(EXPLORE_SURFACE_COORDINATOR),
                equalTo(mExploreSurfaceCoordinator));

        doReturn(TabLaunchType.FROM_START_SURFACE).when(mTab).getLaunchType();
        mediator.hideTabSwitcherView(true);
        mTabSwitcherVisibilityObserverCaptor.getValue().startedHiding();
        mTabSwitcherVisibilityObserverCaptor.getValue().finishedHiding();
        assertNull(mPropertyModel.get(EXPLORE_SURFACE_COORDINATOR));

        mediator.setStartSurfaceState(StartSurfaceState.SHOWING_PREVIOUS);
        mediator.showOverview(false);
        assertThat(mediator.getStartSurfaceState(), equalTo(StartSurfaceState.SHOWN_HOMEPAGE));
        assertThat(mPropertyModel.get(EXPLORE_SURFACE_COORDINATOR),
                equalTo(mExploreSurfaceCoordinator));

        doReturn(TabLaunchType.FROM_LINK).when(mTab).getLaunchType();
        mediator.hideTabSwitcherView(true);
        mTabSwitcherVisibilityObserverCaptor.getValue().startedHiding();
        mTabSwitcherVisibilityObserverCaptor.getValue().finishedHiding();
        assertNull(mPropertyModel.get(EXPLORE_SURFACE_COORDINATOR));
    }

    @Test
    // When the refactoring is enabled, the StartSurfaceMediator is no longer responsible for
    // showing the Grid tab switcher.
    @DisableFeatures({ChromeFeatureList.START_SURFACE_REFACTOR})
    public void changeTopContentOffset() {
        doReturn(false).when(mTabModelSelector).isIncognitoSelected();
        doReturn(mVoiceRecognitionHandler).when(mOmniboxStub).getVoiceRecognitionHandler();
        doReturn(true).when(mVoiceRecognitionHandler).isVoiceSearchEnabled();

        doNothing()
                .when(mBrowserControlsStateProvider)
                .addObserver(mBrowserControlsStateProviderCaptor.capture());
        StartSurfaceMediator mediator =
                createStartSurfaceMediator(/* isStartSurfaceEnabled= */ true);

        // The top margin of homepage should be consistent with top controls min height/offset
        // (indicator height).
        doReturn(10).when(mBrowserControlsStateProvider).getTopControlsMinHeight();
        // Sets the current StartSurfaceState to SHOWING_START before calling the
        // {@link StartSurfaceMediator#showOverview()}. This is because if the current
        // StartSurfaceState is NOT_SHOWN, the state will be set default to SHOWING_TABSWITCHER in
        // {@link StartSurfaceMediator#showOverview()}.
        mediator.setStartSurfaceState(StartSurfaceState.SHOWING_START);
        mediator.showOverview(false);

        verify(mBrowserControlsStateProvider).addObserver(ArgumentMatchers.any());
        assertEquals("Wrong top content offset on homepage.", 10, mPropertyModel.get(TOP_MARGIN));

        onControlsOffsetChanged(/*topOffset=*/100, /*topControlsMinHeightOffset=*/20);
        assertEquals("Wrong top content offset on homepage.", 20, mPropertyModel.get(TOP_MARGIN));

        onControlsOffsetChanged(/*topOffset=*/130, /*topControlsMinHeightOffset=*/50);
        assertEquals("Wrong top content offset on homepage.", 50, mPropertyModel.get(TOP_MARGIN));

        // The top margin of tab switcher surface should be consistent with top controls
        // height/offset.
        doReturn(15).when(mBrowserControlsStateProvider).getTopControlsHeight();
        mediator.setStartSurfaceState(StartSurfaceState.SHOWING_TABSWITCHER);
        mediator.showOverview(false);

        assertEquals("Wrong top content offset on tab switcher surface.", 15,
                mPropertyModel.get(TOP_MARGIN));

        onControlsOffsetChanged(/*topOffset=*/100, /*topControlsMinHeightOffset=*/20);
        assertEquals("Wrong top content offset on tab switcher surface.", 100,
                mPropertyModel.get(TOP_MARGIN));

        onControlsOffsetChanged(/*topOffset=*/130, /*topControlsMinHeightOffset=*/50);
        assertEquals("Wrong top content offset on tab switcher surface.", 130,
                mPropertyModel.get(TOP_MARGIN));
    }

    @Test
    public void exploreSurfaceInitializedAfterNativeInSinglePane() {
        doReturn(false).when(mTabModelSelector).isIncognitoSelected();
        doReturn(mVoiceRecognitionHandler).when(mOmniboxStub).getVoiceRecognitionHandler();
        doReturn(true).when(mVoiceRecognitionHandler).isVoiceSearchEnabled();

        StartSurfaceMediator mediator = createStartSurfaceMediatorWithoutInit(
                /* isStartSurfaceEnabled= */ true,
                /* hadWarmStart= */ false);
        verify(mMainTabGridController)
                .addTabSwitcherViewObserver(mTabSwitcherVisibilityObserverCaptor.capture());

        assertThat(mediator.getStartSurfaceState(), equalTo(StartSurfaceState.NOT_SHOWN));
        // Sets the current StartSurfaceState to SHOWING_START before calling the
        // {@link StartSurfaceMediator#showOverview()}. This is because if the current
        // StartSurfaceState is NOT_SHOWN, the state will be set default to SHOWING_TABSWITCHER in
        // {@link StartSurfaceMediator#showOverview()}.
        mediator.setStartSurfaceState(StartSurfaceState.SHOWING_START);
        mediator.showOverview(false);
        assertThat(mediator.getStartSurfaceState(), equalTo(StartSurfaceState.SHOWN_HOMEPAGE));
        assertThat(mPropertyModel.get(IS_EXPLORE_SURFACE_VISIBLE), equalTo(false));
        verify(mMainTabGridController).showTabSwitcherView(eq(false));

        when(mMainTabGridController.overviewVisible()).thenReturn(true);
        mediator.initWithNative(
                mOmniboxStub, mExploreSurfaceCoordinatorFactory, mPrefService, null);
        when(mMainTabGridController.overviewVisible()).thenReturn(true);
        mediator.initWithNative(
                mOmniboxStub, mExploreSurfaceCoordinatorFactory, mPrefService, null);
        assertThat(mPropertyModel.get(IS_EXPLORE_SURFACE_VISIBLE), equalTo(true));
    }

    @Test
    public void initializeStartSurfaceTopMargins() {
        Resources resources = ContextUtils.getApplicationContext().getResources();
        int tasksSurfaceBodyTopMargin =
                resources.getDimensionPixelSize(R.dimen.tasks_surface_body_top_margin);
        int mvTilesContainerTopMargin =
                resources.getDimensionPixelSize(R.dimen.mv_tiles_container_top_margin);
        int tabSwitcherTitleTopMargin =
                resources.getDimensionPixelSize(R.dimen.tab_switcher_title_top_margin);

        createStartSurfaceMediatorWithoutInit(/* isStartSurfaceEnabled= */ true,
                /* hadWarmStart= */ false);
        assertThat(mPropertyModel.get(TASKS_SURFACE_BODY_TOP_MARGIN),
                equalTo(tasksSurfaceBodyTopMargin));
        assertThat(mPropertyModel.get(MV_TILES_CONTAINER_TOP_MARGIN),
                equalTo(mvTilesContainerTopMargin));
        assertThat(mPropertyModel.get(TAB_SWITCHER_TITLE_TOP_MARGIN),
                equalTo(tabSwitcherTitleTopMargin));

        assertThat(mPropertyModel.get(MV_TILES_CONTAINER_LEFT_RIGHT_MARGIN), equalTo(0));
        assertThat(mPropertyModel.get(SINGLE_TAB_TOP_MARGIN), equalTo(0));
    }

    @Test
    @EnableFeatures(ChromeFeatureList.START_SURFACE_DISABLED_FEED_IMPROVEMENT)
    public void testStartSurfaceTopMarginsWhenFeedGoneImprovementEnabled() {
        SharedPreferencesManager.getInstance().writeBoolean(
                ChromePreferenceKeys.FEED_ARTICLES_LIST_VISIBLE, false);
        Context context = ContextUtils.getApplicationContext();
        Assert.assertTrue(ReturnToChromeUtil.shouldImproveStartWhenFeedIsDisabled(context));
        doReturn(TabSwitcherType.SINGLE).when(mMainTabGridController).getTabSwitcherType();

        Resources resources = context.getResources();
        int tasksSurfaceBodyTopMarginWithTab =
                resources.getDimensionPixelSize(R.dimen.tasks_surface_body_top_margin);
        int tasksSurfaceBodyTopMarginWithoutTab =
                resources.getDimensionPixelSize(R.dimen.tile_grid_layout_bottom_margin);
        int mvTilesContainerTopMargin =
                resources.getDimensionPixelOffset(R.dimen.tile_grid_layout_top_margin)
                + resources.getDimensionPixelOffset(R.dimen.ntp_search_box_bottom_margin);
        int tabSwitcherTitleTopMargin =
                resources.getDimensionPixelSize(R.dimen.tab_switcher_title_top_margin);
        int singleTopMargin = resources.getDimensionPixelSize(
                R.dimen.single_tab_view_top_margin_for_feed_improvement);

        StartSurfaceMediator mediator =
                createStartSurfaceMediatorWithoutInit(/* isStartSurfaceEnabled= */ true,
                        /* hadWarmStart= */ false);
        assertThat(mPropertyModel.get(TASKS_SURFACE_BODY_TOP_MARGIN),
                equalTo(tasksSurfaceBodyTopMarginWithTab));
        assertThat(mPropertyModel.get(MV_TILES_CONTAINER_TOP_MARGIN),
                equalTo(mvTilesContainerTopMargin));
        assertThat(mPropertyModel.get(TAB_SWITCHER_TITLE_TOP_MARGIN),
                equalTo(tabSwitcherTitleTopMargin));
        assertThat(mPropertyModel.get(SINGLE_TAB_TOP_MARGIN), equalTo(singleTopMargin));

        // Tasks surface body top margin should be updated when tab carousel/single card visibility
        // is changed.
        doReturn(0).when(mNormalTabModel).getCount();
        doReturn(true).when(mTabModelSelector).isTabStateInitialized();
        mediator.showOverview(false);
        mediator.setStartSurfaceState(StartSurfaceState.SHOWN_HOMEPAGE);
        assertThat(mPropertyModel.get(IS_TAB_CAROUSEL_VISIBLE), equalTo(false));
        assertThat(mPropertyModel.get(TASKS_SURFACE_BODY_TOP_MARGIN),
                equalTo(tasksSurfaceBodyTopMarginWithoutTab));

        doReturn(2).when(mNormalTabModel).getCount();
        mediator.setStartSurfaceState(StartSurfaceState.SHOWING_HOMEPAGE);
        mediator.showOverview(false);
        mediator.setStartSurfaceState(StartSurfaceState.SHOWN_HOMEPAGE);
        assertThat(mPropertyModel.get(IS_TAB_CAROUSEL_VISIBLE), equalTo(true));
        assertThat(mPropertyModel.get(TASKS_SURFACE_BODY_TOP_MARGIN),
                equalTo(tasksSurfaceBodyTopMarginWithTab));
    }

    @Test
    @Features.EnableFeatures(INSTANT_START)
    public void feedPlaceholderFromWarmStart() {
        doReturn(false).when(mTabModelSelector).isIncognitoSelected();
        doReturn(mVoiceRecognitionHandler).when(mOmniboxStub).getVoiceRecognitionHandler();
        doReturn(true).when(mVoiceRecognitionHandler).isVoiceSearchEnabled();

        StartSurfaceMediator mediator = createStartSurfaceMediator(
                /* isStartSurfaceEnabled= */ true,
                /* hadWarmStart= */ true);
        assertFalse(mediator.shouldShowFeedPlaceholder());

        mediator.setStartSurfaceState(StartSurfaceState.SHOWING_HOMEPAGE);
        mPropertyModel.set(IS_EXPLORE_SURFACE_VISIBLE, true);
        when(mExploreSurfaceCoordinatorFactory.create(anyBoolean(), anyBoolean(), anyInt()))
                .thenReturn(mExploreSurfaceCoordinator);
        mediator.showOverview(false);
        assertThat(mediator.getStartSurfaceState(), equalTo(StartSurfaceState.SHOWN_HOMEPAGE));

        assertThat(mPropertyModel.get(EXPLORE_SURFACE_COORDINATOR),
                equalTo(mExploreSurfaceCoordinator));
    }

    @Test
    public void setSecondaryTasksSurfaceVisibilityWhenShowingTabSwitcher() {
        doReturn(false).when(mTabModelSelector).isIncognitoSelected();
        doReturn(mVoiceRecognitionHandler).when(mOmniboxStub).getVoiceRecognitionHandler();
        doReturn(true).when(mVoiceRecognitionHandler).isVoiceSearchEnabled();

        StartSurfaceMediator mediator =
                createStartSurfaceMediator(/* isStartSurfaceEnabled= */ true);
        mediator.setSecondaryTasksSurfacePropertyModel(mSecondaryTasksSurfacePropertyModel);
        mediator.setSecondaryTasksSurfaceController(mSecondaryTasksSurfaceController);
        assertThat(mediator.getStartSurfaceState(), equalTo(StartSurfaceState.NOT_SHOWN));

        mediator.setStartSurfaceState(StartSurfaceState.SHOWING_TABSWITCHER);
        assertFalse(mSecondaryTasksSurfacePropertyModel.get(IS_FAKE_SEARCH_BOX_VISIBLE));
        assertTrue(mPropertyModel.get(IS_SECONDARY_SURFACE_VISIBLE));
        verify(mSecondaryTasksSurfaceController, times(0)).showTabSwitcherView(true);

        mediator.showOverview(false);
        assertThat(mediator.getStartSurfaceState(), equalTo(StartSurfaceState.SHOWN_TABSWITCHER));
        verify(mSecondaryTasksSurfaceController, times(1)).showTabSwitcherView(true);
    }

    @Test
    public void singeTabSwitcherHideTabSwitcherTitle() {
        doReturn(false).when(mTabModelSelector).isIncognitoSelected();
        doReturn(mVoiceRecognitionHandler).when(mOmniboxStub).getVoiceRecognitionHandler();
        doReturn(true).when(mVoiceRecognitionHandler).isVoiceSearchEnabled();
        doReturn(TabSwitcherType.SINGLE).when(mMainTabGridController).getTabSwitcherType();

        StartSurfaceMediator mediator =
                createStartSurfaceMediator(/* isStartSurfaceEnabled= */ true);

        doReturn(2).when(mNormalTabModel).getCount();
        doReturn(true).when(mTabModelSelector).isTabStateInitialized();
        mediator.setStartSurfaceState(StartSurfaceState.SHOWING_HOMEPAGE);
        mediator.showOverview(false);
        mediator.setStartSurfaceState(StartSurfaceState.SHOWN_HOMEPAGE);
        verify(mNormalTabModel).addObserver(mTabModelObserverCaptor.capture());
        assertThat(mPropertyModel.get(IS_SHOWING_OVERVIEW), equalTo(true));
        assertThat(mPropertyModel.get(IS_TAB_CAROUSEL_VISIBLE), equalTo(true));
        assertThat(mPropertyModel.get(IS_TAB_CAROUSEL_TITLE_VISIBLE), equalTo(false));
    }

    @Test
    public void hideSingleTabSwitcherWhenCurrentSelectedTabIsNTP() {
        doReturn(false).when(mTabModelSelector).isIncognitoSelected();
        doReturn(mVoiceRecognitionHandler).when(mOmniboxStub).getVoiceRecognitionHandler();
        doReturn(true).when(mVoiceRecognitionHandler).isVoiceSearchEnabled();
        doReturn(2).when(mNormalTabModel).getCount();
        doReturn(true).when(mTabModelSelector).isTabStateInitialized();

        doReturn(TabSwitcherType.SINGLE).when(mMainTabGridController).getTabSwitcherType();
        MockTab regularTab = new MockTab(1, false);
        regularTab.setGurlOverrideForTesting(JUnitTestGURLs.getGURL(JUnitTestGURLs.NTP_URL));
        when(mTabModelSelector.getCurrentTab()).thenReturn(regularTab);

        StartSurfaceMediator mediator =
                createStartSurfaceMediator(/* isStartSurfaceEnabled= */ true);

        mediator.setStartSurfaceState(StartSurfaceState.SHOWING_HOMEPAGE);
        mediator.showOverview(false);
        mediator.setStartSurfaceState(StartSurfaceState.SHOWN_HOMEPAGE);
        verify(mNormalTabModel).addObserver(mTabModelObserverCaptor.capture());
        assertThat(mPropertyModel.get(IS_SHOWING_OVERVIEW), equalTo(true));
        assertThat(mPropertyModel.get(IS_TAB_CAROUSEL_VISIBLE), equalTo(false));
        assertThat(mPropertyModel.get(IS_TAB_CAROUSEL_TITLE_VISIBLE), equalTo(false));
    }

    @Test
    public void testInitializeMVTilesWhenShownHomepage() {
        doReturn(false).when(mTabModelSelector).isIncognitoSelected();
        doReturn(mVoiceRecognitionHandler).when(mOmniboxStub).getVoiceRecognitionHandler();
        doReturn(true).when(mVoiceRecognitionHandler).isVoiceSearchEnabled();
        doReturn(2).when(mNormalTabModel).getCount();
        doReturn(true).when(mTabModelSelector).isTabStateInitialized();

        StartSurfaceMediator mediator =
                createStartSurfaceMediator(/* isStartSurfaceEnabled= */ true);
        mediator.setStartSurfaceState(StartSurfaceState.SHOWN_HOMEPAGE);
        verify(mInitializeMVTilesRunnable).run();
    }

    @Test
    @EnableFeatures(ChromeFeatureList.START_SURFACE_DISABLED_FEED_IMPROVEMENT)
    public void testInitializeLogoWhenShownHomepageWithFeedDisabled() throws Exception {
        doReturn(mVoiceRecognitionHandler).when(mOmniboxStub).getVoiceRecognitionHandler();
        SharedPreferencesManager.getInstance().writeBoolean(
                ChromePreferenceKeys.FEED_ARTICLES_LIST_VISIBLE, false);
        when(mTemplateUrlService.doesDefaultSearchEngineHaveLogo()).thenReturn(true);

        Assert.assertTrue(ReturnToChromeUtil.shouldImproveStartWhenFeedIsDisabled(
                ContextUtils.getApplicationContext()));

        StartSurfaceMediator mediator =
                createStartSurfaceMediator(/*isStartSurfaceEnabled=*/true, false);
        mediator.setStartSurfaceState(StartSurfaceState.SHOWN_HOMEPAGE);
        mediator.showOverview(true);

        verify(mLogoContainerView).setVisibility(View.VISIBLE);
        verify(mLogoBridge).getCurrentLogo(anyLong(), any(), any());
        Assert.assertTrue(mediator.isLogoVisible());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.START_SURFACE_DISABLED_FEED_IMPROVEMENT)
    public void testNotInitializeLogoWhenShownHomepageWithFeedEnabled() throws Exception {
        doReturn(mVoiceRecognitionHandler).when(mOmniboxStub).getVoiceRecognitionHandler();
        SharedPreferencesManager.getInstance().writeBoolean(
                ChromePreferenceKeys.FEED_ARTICLES_LIST_VISIBLE, true);
        when(mTemplateUrlService.doesDefaultSearchEngineHaveLogo()).thenReturn(true);

        Assert.assertFalse(ReturnToChromeUtil.shouldImproveStartWhenFeedIsDisabled(
                ContextUtils.getApplicationContext()));

        StartSurfaceMediator mediator =
                createStartSurfaceMediator(/*isStartSurfaceEnabled=*/true, false);
        mediator.setStartSurfaceState(StartSurfaceState.SHOWN_HOMEPAGE);
        mediator.showOverview(true);

        verify(mLogoContainerView, times(0)).setVisibility(View.VISIBLE);
        Assert.assertFalse(mediator.isLogoVisible());
    }

    @Test
    public void testFeedReliabilityLoggerPageLoadStarted() {
        doReturn(mVoiceRecognitionHandler).when(mOmniboxStub).getVoiceRecognitionHandler();

        StartSurfaceMediator mediator = createStartSurfaceMediator(/*isStartSurfaceEnabled=*/true);
        mediator.setStartSurfaceState(StartSurfaceState.SHOWN_HOMEPAGE);
        mediator.showOverview(true);

        verify(mNormalTabModel).addObserver(mTabModelObserverCaptor.capture());
        assertThat(mPropertyModel.get(IS_EXPLORE_SURFACE_VISIBLE), equalTo(true));
        mTabModelObserverCaptor.getValue().willAddTab(/*tab=*/null, TabLaunchType.FROM_LINK);
        verify(mFeedReliabilityLogger, times(1)).onPageLoadStarted();
    }

    @Test
    public void testFeedReliabilityLoggerObservesUrlFocus() {
        doReturn(mVoiceRecognitionHandler).when(mOmniboxStub).getVoiceRecognitionHandler();

        StartSurfaceMediator mediator = createStartSurfaceMediator(/*isStartSurfaceEnabled=*/true);
        verify(mMainTabGridController)
                .addTabSwitcherViewObserver(mTabSwitcherVisibilityObserverCaptor.capture());
        mediator.setStartSurfaceState(StartSurfaceState.SHOWN_HOMEPAGE);
        mediator.showOverview(true);

        assertThat(mPropertyModel.get(IS_EXPLORE_SURFACE_VISIBLE), equalTo(true));
        verify(mOmniboxStub, times(2))
                .addUrlFocusChangeListener(mUrlFocusChangeListenerCaptor.capture());
        UrlFocusChangeListener listener = mUrlFocusChangeListenerCaptor.getAllValues().get(0);
        assertThat(listener, equalTo(mFeedReliabilityLogger));

        mTabSwitcherVisibilityObserverCaptor.getValue().startedShowing();
        mTabSwitcherVisibilityObserverCaptor.getValue().finishedShowing();
        mTabSwitcherVisibilityObserverCaptor.getValue().startedHiding();

        mediator.hideTabSwitcherView(true);
        verify(mOmniboxStub).removeUrlFocusChangeListener(listener);

        mTabSwitcherVisibilityObserverCaptor.getValue().finishedHiding();
    }

    @Test
    public void testFeedReliabilityLoggerBackPressed() {
        doReturn(mVoiceRecognitionHandler).when(mOmniboxStub).getVoiceRecognitionHandler();
        StartSurfaceMediator mediator = createStartSurfaceMediator(/*isStartSurfaceEnabled=*/true);
        mediator.setStartSurfaceState(StartSurfaceState.SHOWN_HOMEPAGE);
        mediator.showOverview(true);
        mediator.onBackPressed();
        verify(mFeedReliabilityLogger).onNavigateBack();
    }

    @Test
    @EnableFeatures({ChromeFeatureList.BACK_GESTURE_REFACTOR})
    public void testBackPressHandler() {
        doReturn(false).when(mTabModelSelector).isIncognitoSelected();
        doReturn(mVoiceRecognitionHandler).when(mOmniboxStub).getVoiceRecognitionHandler();
        doReturn(true).when(mVoiceRecognitionHandler).isVoiceSearchEnabled();
        doAnswer((inv) -> mControllerDialogVisibleSupplier.get())
                .when(mMainTabGridController)
                .isDialogVisible();
        doAnswer((inv) -> mSecondaryControllerDialogVisibleSupplier.get())
                .when(mSecondaryTasksSurfaceController)
                .isDialogVisible();

        StartSurfaceMediator mediator = createStartSurfaceMediator(/*isStartSurfaceEnabled=*/true);
        mediator.setSecondaryTasksSurfacePropertyModel(mSecondaryTasksSurfacePropertyModel);
        mediator.setSecondaryTasksSurfaceController(mSecondaryTasksSurfaceController);

        mediator.setStartSurfaceState(StartSurfaceState.SHOWN_TABSWITCHER);
        Assert.assertFalse(
                "Should not intercept back press by default", mediator.shouldInterceptBackPress());
        mControllerDialogVisibleSupplier.set(true);
        Assert.assertTrue(mediator.shouldInterceptBackPress());
        doReturn(true).when(mMainTabGridController).onBackPressed(false);
        mediator.onBackPressed();
        verify(mMainTabGridController).onBackPressed(false);

        mControllerDialogVisibleSupplier.set(false);
        Assert.assertFalse(mediator.shouldInterceptBackPress());

        mControllerDialogVisibleSupplier.set(true);
        mSecondaryControllerDialogVisibleSupplier.set(true);
        Assert.assertTrue(mediator.shouldInterceptBackPress());
        doReturn(true).when(mSecondaryTasksSurfaceController).onBackPressed(false);
        mediator.onBackPressed();
        verify(mMainTabGridController).onBackPressed(false);
        verify(mSecondaryTasksSurfaceController,
                description("Secondary task surface has a higher priority of handling back press"))
                .onBackPressed(false);

        mSecondaryControllerDialogVisibleSupplier.set(false);
        mControllerDialogVisibleSupplier.set(false);
        verify(mSecondaryTasksSurfaceController,
                description(
                        "Secondary task surface consumes back press when no dialog is visible."))
                .onBackPressed(false);

        mediator.setStartSurfaceState(StartSurfaceState.SHOWN_HOMEPAGE);
        mediator.setStartSurfaceState(StartSurfaceState.SHOWN_TABSWITCHER);
        Assert.assertTrue(mediator.onBackPressed());
        Assert.assertEquals("Should return to home page on back press.",
                StartSurfaceState.SHOWN_HOMEPAGE, mediator.getStartSurfaceState());
    }

    /**
     * Tests the logic of StartSurfaceMediator#onBackPressedInternal() when the Start surface is
     * showing but Tab switcher hasn't been created yet.
     */
    @Test
    // TODO(1347089): Fix the back operation behaviours when the refactoring is enabled.
    @DisableFeatures(ChromeFeatureList.START_SURFACE_REFACTOR)
    public void testBackPressHandlerOnStartSurfaceWithoutTabSwitcherCreated() {
        doReturn(false).when(mTabModelSelector).isIncognitoSelected();
        doReturn(false).when(mTabModelSelector).isIncognitoSelected();
        doReturn(mVoiceRecognitionHandler).when(mOmniboxStub).getVoiceRecognitionHandler();
        doReturn(true).when(mVoiceRecognitionHandler).isVoiceSearchEnabled();
        StartSurfaceMediator mediator = createStartSurfaceMediator(/*isStartSurfaceEnabled=*/true);

        mediator.setStartSurfaceState(StartSurfaceState.SHOWN_HOMEPAGE);
        Assert.assertEquals(StartSurfaceState.SHOWN_HOMEPAGE, mediator.getStartSurfaceState());

        doReturn(true).when(mMainTabGridController).isDialogVisible();
        mediator.onBackPressed();
        verify(mMainTabGridController).onBackPressed(true);

        doReturn(false).when(mMainTabGridController).isDialogVisible();
        mediator.onBackPressed();
        verify(mMainTabGridController, times(2)).onBackPressed(true);
    }

    /**
     * Tests the logic of StartSurfaceMediator#onBackPressedInternal() when the Start surface is
     * showing and the Tab switcher has been created.
     */
    @Test
    // TODO(1347089): Removes this test after the refactoring is enabled by default. This is because
    // the SecondaryTasksSurface will go away.
    @DisableFeatures({ChromeFeatureList.START_SURFACE_REFACTOR})
    public void testBackPressHandlerOnStartSurfaceWithTabSwitcherCreated() {
        doReturn(false).when(mTabModelSelector).isIncognitoSelected();
        doReturn(false).when(mTabModelSelector).isIncognitoSelected();
        doReturn(mVoiceRecognitionHandler).when(mOmniboxStub).getVoiceRecognitionHandler();
        doReturn(true).when(mVoiceRecognitionHandler).isVoiceSearchEnabled();

        StartSurfaceMediator mediator = createStartSurfaceMediator(/*isStartSurfaceEnabled=*/true);
        mediator.setSecondaryTasksSurfacePropertyModel(mSecondaryTasksSurfacePropertyModel);
        mediator.setSecondaryTasksSurfaceController(mSecondaryTasksSurfaceController);

        mediator.setStartSurfaceState(StartSurfaceState.SHOWN_HOMEPAGE);
        Assert.assertEquals(StartSurfaceState.SHOWN_HOMEPAGE, mediator.getStartSurfaceState());

        doReturn(true).when(mMainTabGridController).isDialogVisible();
        doReturn(true).when(mSecondaryTasksSurfaceController).isDialogVisible();
        mediator.onBackPressed();
        verify(mMainTabGridController, never()).onBackPressed(true);
        verify(mSecondaryTasksSurfaceController,
                description("Secondary task surface has a higher priority of handling back press"))
                .onBackPressed(true);

        doReturn(true).when(mMainTabGridController).isDialogVisible();
        doReturn(false).when(mSecondaryTasksSurfaceController).isDialogVisible();
        mediator.onBackPressed();
        verify(mMainTabGridController).onBackPressed(true);

        doReturn(false).when(mMainTabGridController).isDialogVisible();
        doReturn(false).when(mSecondaryTasksSurfaceController).isDialogVisible();
        mediator.onBackPressed();
        verify(mMainTabGridController, times(2)).onBackPressed(true);
    }

    /**
     * Tests the logic of StartSurfaceMediator#onBackPressedInternal() when the Tab switcher is
     * showing.
     */
    @Test
    public void testBackPressHandlerOnTabSwitcher() {
        doReturn(false).when(mTabModelSelector).isIncognitoSelected();
        doReturn(false).when(mTabModelSelector).isIncognitoSelected();
        doReturn(mVoiceRecognitionHandler).when(mOmniboxStub).getVoiceRecognitionHandler();
        doReturn(true).when(mVoiceRecognitionHandler).isVoiceSearchEnabled();

        StartSurfaceMediator mediator = createStartSurfaceMediator(/*isStartSurfaceEnabled=*/true);
        mediator.setSecondaryTasksSurfacePropertyModel(mSecondaryTasksSurfacePropertyModel);
        mediator.setSecondaryTasksSurfaceController(mSecondaryTasksSurfaceController);

        mediator.setStartSurfaceState(StartSurfaceState.SHOWN_TABSWITCHER);
        Assert.assertEquals(StartSurfaceState.SHOWN_TABSWITCHER, mediator.getStartSurfaceState());
        // The primary task surface is invisible when showing the Tab Switcher.
        doReturn(false).when(mMainTabGridController).isDialogVisible();

        doReturn(true).when(mSecondaryTasksSurfaceController).isDialogVisible();
        mediator.onBackPressed();
        verify(mMainTabGridController, never()).onBackPressed(false);
        verify(mSecondaryTasksSurfaceController).onBackPressed(false);

        doReturn(false).when(mSecondaryTasksSurfaceController).isDialogVisible();
        verify(mSecondaryTasksSurfaceController).onBackPressed(false);

        mediator.setStartSurfaceState(StartSurfaceState.SHOWN_HOMEPAGE);
        mediator.setStartSurfaceState(StartSurfaceState.SHOWN_TABSWITCHER);
        Assert.assertEquals(StartSurfaceState.SHOWN_TABSWITCHER, mediator.getStartSurfaceState());
        mediator.onBackPressed();
        Assert.assertEquals("Should return to home page on back press.",
                StartSurfaceState.SHOWN_HOMEPAGE, mediator.getStartSurfaceState());
    }

    /**
     * Tests the logic of recording time spend in start surface.
     */
    @Test
    public void testRecordTimeSpendInStart() {
        doReturn(false).when(mTabModelSelector).isIncognitoSelected();
        doReturn(mVoiceRecognitionHandler).when(mOmniboxStub).getVoiceRecognitionHandler();
        doReturn(true).when(mVoiceRecognitionHandler).isVoiceSearchEnabled();
        StartSurfaceMediator mediator =
                createStartSurfaceMediator(/* isStartSurfaceEnabled= */ true);
        verify(mActivityLifecycleDispatcher)
                .register(mPauseResumeWithNativeObserverArgumentCaptor.capture());
        // Verifies that the histograms are logged in the following transitions:
        // Start Surface -> Grid Tab Switcher -> Start Surface -> onPauseWithNative ->
        // onResumeWithNative -> destroy.
        mediator.setStartSurfaceState(StartSurfaceState.SHOWING_START);
        mediator.showOverview(false);
        assertThat(mediator.getStartSurfaceState(), equalTo(StartSurfaceState.SHOWN_HOMEPAGE));
        mediator.setStartSurfaceState(StartSurfaceState.SHOWN_TABSWITCHER);
        Assert.assertEquals(
                1, RecordHistogram.getHistogramTotalCountForTesting(START_SURFACE_TIME_SPENT));
        mediator.setStartSurfaceState(StartSurfaceState.SHOWN_HOMEPAGE);
        mPauseResumeWithNativeObserverArgumentCaptor.getValue().onPauseWithNative();
        Assert.assertEquals(
                2, RecordHistogram.getHistogramTotalCountForTesting(START_SURFACE_TIME_SPENT));
        mPauseResumeWithNativeObserverArgumentCaptor.getValue().onResumeWithNative();
        mediator.destroy();
        Assert.assertEquals(
                3, RecordHistogram.getHistogramTotalCountForTesting(START_SURFACE_TIME_SPENT));
    }

    private StartSurfaceMediator createStartSurfaceMediator(boolean isStartSurfaceEnabled) {
        return createStartSurfaceMediator(isStartSurfaceEnabled, /* hadWarmStart= */ false);
    }

    private StartSurfaceMediator createStartSurfaceMediator(
            boolean isStartSurfaceEnabled, boolean hadWarmStart) {
        StartSurfaceMediator mediator =
                createStartSurfaceMediatorWithoutInit(isStartSurfaceEnabled, hadWarmStart);
        mediator.initWithNative(mOmniboxStub,
                isStartSurfaceEnabled ? mExploreSurfaceCoordinatorFactory : null, mPrefService,
                null);
        mediator.initWithNative(mOmniboxStub,
                isStartSurfaceEnabled ? mExploreSurfaceCoordinatorFactory : null, mPrefService,
                null);
        return mediator;
    }

    private StartSurfaceMediator createStartSurfaceMediatorWithoutInit(
            boolean isStartSurfaceEnabled, boolean hadWarmStart) {
        return new StartSurfaceMediator(mMainTabGridController, null /* tabSwitcherContainer */,
                mTabModelSelector, !isStartSurfaceEnabled ? null : mPropertyModel,
                isStartSurfaceEnabled ? mSecondaryTasksSurfaceInitializer : null,
                isStartSurfaceEnabled, ContextUtils.getApplicationContext(),
                mBrowserControlsStateProvider, mActivityStateChecker, true /* excludeQueryTiles */,
                mStartSurfaceSupplier, hadWarmStart, new DummyJankTracker(),
                mInitializeMVTilesRunnable, mParentTabSupplier, mLogoContainerView,
                mBackPressManager, null /* feedPlaceholderParentView */,
                mActivityLifecycleDispatcher);
    }

    private void onControlsOffsetChanged(int topOffset, int topControlsMinHeightOffset) {
        doReturn(topOffset).when(mBrowserControlsStateProvider).getContentOffset();
        doReturn(topControlsMinHeightOffset)
                .when(mBrowserControlsStateProvider)
                .getTopControlsMinHeightOffset();
        mBrowserControlsStateProviderCaptor.getValue().onControlsOffsetChanged(
                topOffset, topControlsMinHeightOffset, 0, 0, false);
    }
}
