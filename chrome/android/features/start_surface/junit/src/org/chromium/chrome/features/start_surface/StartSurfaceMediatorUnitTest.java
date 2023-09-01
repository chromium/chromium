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
import static org.chromium.chrome.features.start_surface.StartSurfaceConfiguration.SURFACE_POLISH_USE_MAGIC_SPACE;
import static org.chromium.chrome.features.start_surface.StartSurfaceProperties.BOTTOM_BAR_HEIGHT;
import static org.chromium.chrome.features.start_surface.StartSurfaceProperties.EXPLORE_SURFACE_COORDINATOR;
import static org.chromium.chrome.features.start_surface.StartSurfaceProperties.IS_EXPLORE_SURFACE_VISIBLE;
import static org.chromium.chrome.features.start_surface.StartSurfaceProperties.IS_SECONDARY_SURFACE_VISIBLE;
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
import static org.chromium.chrome.features.tasks.TasksSurfaceProperties.IS_TAB_CAROUSEL_TITLE_VISIBLE;
import static org.chromium.chrome.features.tasks.TasksSurfaceProperties.IS_TAB_CAROUSEL_VISIBLE;
import static org.chromium.chrome.features.tasks.TasksSurfaceProperties.IS_VOICE_RECOGNITION_BUTTON_VISIBLE;
import static org.chromium.chrome.features.tasks.TasksSurfaceProperties.MV_TILES_CONTAINER_LEFT_RIGHT_MARGIN;
import static org.chromium.chrome.features.tasks.TasksSurfaceProperties.MV_TILES_CONTAINER_TOP_MARGIN;
import static org.chromium.chrome.features.tasks.TasksSurfaceProperties.MV_TILES_VISIBLE;
import static org.chromium.chrome.features.tasks.TasksSurfaceProperties.SINGLE_TAB_TOP_MARGIN;
import static org.chromium.chrome.features.tasks.TasksSurfaceProperties.TAB_SWITCHER_TITLE_TOP_MARGIN;
import static org.chromium.chrome.features.tasks.TasksSurfaceProperties.TASKS_SURFACE_BODY_TOP_MARGIN;
import static org.chromium.chrome.features.tasks.TasksSurfaceProperties.VOICE_SEARCH_BUTTON_CLICK_LISTENER;

import android.app.Activity;
import android.content.Context;
import android.content.res.Resources;
import android.view.View;
import android.view.View.OnClickListener;

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
import org.mockito.InOrder;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;

import org.chromium.base.ContextUtils;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.supplier.OneshotSupplier;
import org.chromium.base.supplier.Supplier;
import org.chromium.base.test.BaseRobolectricTestRunner;
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
import org.chromium.chrome.browser.tasks.tab_management.TabSwitcher.TabListDelegate;
import org.chromium.chrome.browser.tasks.tab_management.TabSwitcher.TabSwitcherViewObserver;
import org.chromium.chrome.features.start_surface.StartSurface.OnTabSelectingListener;
import org.chromium.chrome.features.start_surface.StartSurfaceMediator.SecondaryTasksSurfaceInitializer;
import org.chromium.chrome.features.tasks.TasksSurfaceProperties;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.chrome.test.util.browser.Features.DisableFeatures;
import org.chromium.chrome.test.util.browser.Features.EnableFeatures;
import org.chromium.components.browser_ui.styles.ChromeColors;
import org.chromium.components.prefs.PrefService;
import org.chromium.components.search_engines.TemplateUrlService;
import org.chromium.components.search_engines.TemplateUrlService.TemplateUrlServiceObserver;
import org.chromium.ui.base.TestActivity;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.url.JUnitTestGURLs;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;

/** Tests for {@link StartSurfaceMediator}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class StartSurfaceMediatorUnitTest {
    private static final String START_SURFACE_TIME_SPENT = "StartSurface.TimeSpent";
    private PropertyModel mPropertyModel;
    private PropertyModel mSecondaryTasksSurfacePropertyModel;

    @Rule
    public JniMocker mJniMocker = new JniMocker();
    @Rule
    public TestRule mProcessor = new Features.JUnitProcessor();
    @Mock
    private TabSwitcher.Controller mCarouselOrSingleTabSwitcherModuleController;
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
    private TabModelFilter mNormalTabModelFilter;
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
    @Mock
    private OnClickListener mTabSwitcherClickHandler;
    @Mock
    private TabSwitcher mTabSwitcherModule;
    @Mock
    private TabListDelegate mTabListDelegate;
    @Mock
    private TabSwitcher.Controller mSingleTabSwitcherModuleController;
    @Captor
    private ArgumentCaptor<TabModelSelectorObserver> mTabModelSelectorObserverCaptor;
    @Captor
    private ArgumentCaptor<TabModelObserver> mTabModelObserverCaptor;
    @Captor
    private ArgumentCaptor<TabSwitcherViewObserver>
            mCarouselTabSwitcherModuleVisibilityObserverCaptor;
    @Captor
    private ArgumentCaptor<UrlFocusChangeListener> mUrlFocusChangeListenerCaptor;
    @Captor
    private ArgumentCaptor<BrowserControlsStateProvider.Observer>
            mBrowserControlsStateProviderCaptor;
    @Captor
    private ArgumentCaptor<PauseResumeWithNativeObserver>
            mPauseResumeWithNativeObserverArgumentCaptor;
    @Captor
    private ArgumentCaptor<TemplateUrlServiceObserver> mTemplateUrlServiceObserverCaptor;

    private ObservableSupplierImpl<Boolean>
            mCarouselTabSwitcherModuleControllerBackPressStateSupplier =
                    new ObservableSupplierImpl<>();
    private ObservableSupplierImpl<Boolean>
            mCarouselTabSwitcherModuleControllerDialogVisibleSupplier =
                    new ObservableSupplierImpl<>();
    private ObservableSupplierImpl<Boolean> mSecondaryControllerBackPressStateSupplier =
            new ObservableSupplierImpl<>();
    private ObservableSupplierImpl<Boolean> mSecondaryControllerDialogVisibleSupplier =
            new ObservableSupplierImpl<>();
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

        when(mTabModelSelector.getTabModelFilterProvider()).thenReturn(mTabModelFilterProvider);
        when(mTabModelFilterProvider.getTabModelFilter(false)).thenReturn(mNormalTabModelFilter);
        when(mTabModelFilterProvider.getTabModelFilter(true)).thenReturn(mNormalTabModelFilter);

        doReturn(TabSwitcherType.CAROUSEL)
                .when(mCarouselOrSingleTabSwitcherModuleController)
                .getTabSwitcherType();
        doReturn(mCarouselTabSwitcherModuleControllerBackPressStateSupplier)
                .when(mCarouselOrSingleTabSwitcherModuleController)
                .getHandleBackPressChangedSupplier();
        doReturn(mCarouselTabSwitcherModuleControllerDialogVisibleSupplier)
                .when(mCarouselOrSingleTabSwitcherModuleController)
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
        verify(mCarouselOrSingleTabSwitcherModuleController)
                .addTabSwitcherViewObserver(
                        mCarouselTabSwitcherModuleVisibilityObserverCaptor.capture());

        mediator.showOverview(false);
        verify(mCarouselOrSingleTabSwitcherModuleController).showTabSwitcherView(eq(false));

        mCarouselTabSwitcherModuleVisibilityObserverCaptor.getValue().startedShowing();
        mCarouselTabSwitcherModuleVisibilityObserverCaptor.getValue().finishedShowing();

        mediator.hideTabSwitcherView(true);
        verify(mCarouselOrSingleTabSwitcherModuleController).hideTabSwitcherView(eq(true));

        mCarouselTabSwitcherModuleVisibilityObserverCaptor.getValue().startedHiding();
        mCarouselTabSwitcherModuleVisibilityObserverCaptor.getValue().finishedHiding();

        // TODO(crbug.com/1020223): Test the other SurfaceMode.NO_START_SURFACE operations.
    }

    @Test
    public void showAndHideSingleSurface() {
        doReturn(false).when(mTabModelSelector).isIncognitoSelected();
        doReturn(mVoiceRecognitionHandler).when(mOmniboxStub).getVoiceRecognitionHandler();
        doReturn(true).when(mVoiceRecognitionHandler).isVoiceSearchEnabled();

        StartSurfaceMediator mediator =
                createStartSurfaceMediator(/* isStartSurfaceEnabled= */ true);
        verify(mCarouselOrSingleTabSwitcherModuleController)
                .addTabSwitcherViewObserver(
                        mCarouselTabSwitcherModuleVisibilityObserverCaptor.capture());

        assertThat(mediator.getStartSurfaceState(), equalTo(StartSurfaceState.NOT_SHOWN));
        // Sets the current StartSurfaceState to SHOWING_START before calling the
        // {@link StartSurfaceMediator#showOverview()}. This is because if the current
        // StartSurfaceState is NOT_SHOWN, the state will be set default to SHOWING_TABSWITCHER in
        // {@link StartSurfaceMediator#showOverview()}.
        showHomepageAndVerify(mediator, StartSurfaceState.SHOWING_START);

        verify(mCarouselOrSingleTabSwitcherModuleController).showTabSwitcherView(eq(false));
        verify(mOmniboxStub, times(2))
                .addUrlFocusChangeListener(mUrlFocusChangeListenerCaptor.capture());
        assertThat(mPropertyModel.get(IS_INCOGNITO), equalTo(false));
        assertThat(mPropertyModel.get(IS_VOICE_RECOGNITION_BUTTON_VISIBLE), equalTo(true));
        assertThat(mPropertyModel.get(IS_EXPLORE_SURFACE_VISIBLE), equalTo(true));
        assertThat(mPropertyModel.get(MV_TILES_VISIBLE), equalTo(true));
        assertThat(mPropertyModel.get(IS_SHOWING_OVERVIEW), equalTo(true));

        mCarouselTabSwitcherModuleVisibilityObserverCaptor.getValue().startedShowing();
        mCarouselTabSwitcherModuleVisibilityObserverCaptor.getValue().finishedShowing();

        UrlFocusChangeListener urlFocusChangeListener =
                mUrlFocusChangeListenerCaptor.getAllValues().get(1);

        urlFocusChangeListener.onUrlFocusChange(true);
        assertThat(mPropertyModel.get(IS_FAKE_SEARCH_BOX_VISIBLE), equalTo(false));
        urlFocusChangeListener.onUrlFocusChange(false);
        assertThat(mPropertyModel.get(IS_FAKE_SEARCH_BOX_VISIBLE), equalTo(true));
        assertThat(mPropertyModel.get(IS_VOICE_RECOGNITION_BUTTON_VISIBLE), equalTo(true));

        mediator.hideTabSwitcherView(true);
        verify(mCarouselOrSingleTabSwitcherModuleController).hideTabSwitcherView(eq(true));

        mCarouselTabSwitcherModuleVisibilityObserverCaptor.getValue().startedHiding();
        assertThat(mPropertyModel.get(IS_SHOWING_OVERVIEW), equalTo(false));
        verify(mOmniboxStub).removeUrlFocusChangeListener(mUrlFocusChangeListenerCaptor.getValue());

        mCarouselTabSwitcherModuleVisibilityObserverCaptor.getValue().finishedHiding();

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
        showHomepageAndVerify(mediator, StartSurfaceState.SHOWN_HOMEPAGE);
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
        showHomepageAndVerify(mediator, StartSurfaceState.SHOWING_HOMEPAGE);
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
        showHomepageAndVerify(mediator, StartSurfaceState.SHOWING_HOMEPAGE);
        verify(mNormalTabModel).addObserver(mTabModelObserverCaptor.capture());
        assertThat(mPropertyModel.get(IS_SHOWING_OVERVIEW), equalTo(true));
        assertThat(mPropertyModel.get(IS_TAB_CAROUSEL_VISIBLE), equalTo(true));
        assertThat(mPropertyModel.get(IS_TAB_CAROUSEL_TITLE_VISIBLE), equalTo(true));

        Tab tab1 = mock(Tab.class);
        doReturn(JUnitTestGURLs.URL_1).when(tab1).getUrl();
        mTabModelObserverCaptor.getValue().didSelectTab(tab1, TabSelectionType.FROM_CLOSE, 1);
        assertThat(mPropertyModel.get(IS_SHOWING_OVERVIEW), equalTo(true));
        assertThat(mPropertyModel.get(IS_TAB_CAROUSEL_VISIBLE), equalTo(true));
        assertThat(mPropertyModel.get(IS_TAB_CAROUSEL_TITLE_VISIBLE), equalTo(true));

        Tab NTPTab = mock(Tab.class);
        doReturn(JUnitTestGURLs.NTP_URL).when(NTPTab).getUrl();
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
        showHomepageAndVerify(mediator, StartSurfaceState.SHOWING_HOMEPAGE);

        verify(mNormalTabModel).addObserver(mTabModelObserverCaptor.capture());

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

        showHomepageAndVerify(mediator, StartSurfaceState.SHOWN_HOMEPAGE);
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

        showHomepageAndVerify(mediator, StartSurfaceState.SHOWN_HOMEPAGE);
        verify(mNormalTabModel, never()).addObserver(mTabModelObserverCaptor.capture());

        mTabModels.add(mNormalTabModel);
        mTabModels.add(mIncognitoTabModel);
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
        mTabModelSelectorObserverCaptor.getValue().onChange();
        verify(mTabModelSelector).removeObserver(mTabModelSelectorObserverCaptor.capture());
        verify(mNormalTabModel, never()).addObserver(mTabModelObserverCaptor.capture());

        showHomepageAndVerify(mediator, StartSurfaceState.SHOWN_HOMEPAGE);
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

        showHomepageAndVerify(mediator, StartSurfaceState.SHOWING_HOMEPAGE);
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

        showHomepageAndVerify(mediator, StartSurfaceState.SHOWN_HOMEPAGE);
        verify(mTabModelSelector).addObserver(mTabModelSelectorObserverCaptor.capture());

        mediator.startedHiding();
        verify(mTabModelSelector).removeObserver(mTabModelSelectorObserverCaptor.capture());
    }

    @Test
    // TODO(crbug.com/1315676): removes this test once the Start surface refactoring is enabled.
    // This is because the StartSurfaceMediator is no longer responsible for the transition between
    // the Start surface and the Tab switcher.
    @DisableFeatures(ChromeFeatureList.START_SURFACE_REFACTOR)
    public void overviewModeStatesNormalModeSinglePane() {
        doReturn(false).when(mTabModelSelector).isIncognitoSelected();
        doReturn(mVoiceRecognitionHandler).when(mOmniboxStub).getVoiceRecognitionHandler();
        doReturn(true).when(mVoiceRecognitionHandler).isVoiceSearchEnabled();

        StartSurfaceMediator mediator =
                createStartSurfaceMediator(/* isStartSurfaceEnabled= */ true);
        assertThat(mediator.getStartSurfaceState(), equalTo(StartSurfaceState.NOT_SHOWN));

        doReturn(2).when(mNormalTabModel).getCount();
        doReturn(true).when(mTabModelSelector).isTabStateInitialized();
        showHomepageAndVerify(mediator, StartSurfaceState.SHOWING_HOMEPAGE);
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
        showHomepageAndVerify(mediator, StartSurfaceState.SHOWING_HOMEPAGE);
        verify(mTabModelSelector).addObserver(mTabModelSelectorObserverCaptor.capture());
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
        assertTrue(mediator.isHomepageShown());
        assertThat(mPropertyModel.get(IS_SHOWING_OVERVIEW), equalTo(true));
        assertThat(mPropertyModel.get(IS_INCOGNITO), equalTo(false));
        assertThat(mPropertyModel.get(IS_EXPLORE_SURFACE_VISIBLE), equalTo(true));
        assertThat(mPropertyModel.get(MV_TILES_VISIBLE), equalTo(true));
        assertThat(mPropertyModel.get(IS_TAB_CAROUSEL_VISIBLE), equalTo(true));
        assertThat(mPropertyModel.get(IS_TAB_CAROUSEL_TITLE_VISIBLE), equalTo(true));
        assertThat(mPropertyModel.get(IS_SECONDARY_SURFACE_VISIBLE), equalTo(false));
    }

    @Test
    // TODO(crbug.com/1315676): removes this test once the Start surface refactoring is enabled.
    // This is because the StartSurfaceMediator is no longer responsible for the transition between
    // the Start surface and the Tab switcher.
    @DisableFeatures(ChromeFeatureList.START_SURFACE_REFACTOR)
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
        showHomepageAndVerify(mediator, StartSurfaceState.SHOWING_HOMEPAGE);
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
    @EnableFeatures(ChromeFeatureList.START_SURFACE_REFACTOR)
    public void activityIsFinishingOrDestroyedSinglePaneWithRefactorEnabled() {
        doReturn(false).when(mTabModelSelector).isIncognitoSelected();
        doReturn(mVoiceRecognitionHandler).when(mOmniboxStub).getVoiceRecognitionHandler();
        doReturn(true).when(mVoiceRecognitionHandler).isVoiceSearchEnabled();

        StartSurfaceMediator mediator =
                createStartSurfaceMediator(/* isStartSurfaceEnabled= */ true);
        assertFalse(mediator.isHomepageShown());

        doReturn(2).when(mNormalTabModel).getCount();
        doReturn(true).when(mActivityStateChecker).isFinishingOrDestroyed();
        doReturn(true).when(mTabModelSelector).isTabStateInitialized();
        showHomepageAndVerify(mediator, null);
        assertThat(mPropertyModel.get(IS_SHOWING_OVERVIEW), equalTo(true));
        assertThat(mPropertyModel.get(IS_INCOGNITO), equalTo(false));
        assertThat(mPropertyModel.get(IS_EXPLORE_SURFACE_VISIBLE), equalTo(true));
        assertThat(mPropertyModel.get(MV_TILES_VISIBLE), equalTo(true));
        assertThat(mPropertyModel.get(IS_TAB_CAROUSEL_VISIBLE), equalTo(true));
        assertThat(mPropertyModel.get(IS_TAB_CAROUSEL_TITLE_VISIBLE), equalTo(true));
        assertThat(mPropertyModel.get(IS_SECONDARY_SURFACE_VISIBLE), equalTo(false));
        assertThat(mPropertyModel.get(EXPLORE_SURFACE_COORDINATOR), equalTo(null));

        mediator.startedHiding();
        assertFalse(mediator.isHomepageShown());
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
        showHomepageAndVerify(mediator, StartSurfaceState.SHOWING_HOMEPAGE);
        verify(mTabModelSelector).addObserver(mTabModelSelectorObserverCaptor.capture());

        mediator.setStartSurfaceState(StartSurfaceState.SHOWN_TABSWITCHER);
        assertThat(mSecondaryTasksSurfacePropertyModel.get(IS_INCOGNITO), equalTo(false));

        doReturn(true).when(mTabModelSelector).isIncognitoSelected();
        mTabModelSelector.selectModel(true);
        mTabModelSelectorObserverCaptor.getValue().onTabModelSelected(
                mIncognitoTabModel, mNormalTabModel);
        assertThat(mSecondaryTasksSurfacePropertyModel.get(IS_INCOGNITO), equalTo(true));

        showHomepageAndVerify(mediator, StartSurfaceState.SHOWN_HOMEPAGE);
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
        verify(mCarouselOrSingleTabSwitcherModuleController)
                .addTabSwitcherViewObserver(
                        mCarouselTabSwitcherModuleVisibilityObserverCaptor.capture());
        mediator.setSecondaryTasksSurfacePropertyModel(mSecondaryTasksSurfacePropertyModel);
        assertThat(mediator.getStartSurfaceState(), equalTo(StartSurfaceState.NOT_SHOWN));

        doReturn(30).when(mBrowserControlsStateProvider).getBottomControlsHeight();
        doReturn(2).when(mNormalTabModel).getCount();
        showHomepageAndVerify(mediator, StartSurfaceState.SHOWING_HOMEPAGE);
        verify(mBrowserControlsStateProvider)
                .addObserver(mBrowserControlsStateProviderCaptor.capture());
        assertThat(mPropertyModel.get(BOTTOM_BAR_HEIGHT), equalTo(30));
        assertThat(mSecondaryTasksSurfacePropertyModel.get(BOTTOM_BAR_HEIGHT), equalTo(0));

        mCarouselTabSwitcherModuleVisibilityObserverCaptor.getValue().startedShowing();
        mCarouselTabSwitcherModuleVisibilityObserverCaptor.getValue().finishedShowing();

        mBrowserControlsStateProviderCaptor.getValue().onBottomControlsHeightChanged(0, 0);
        assertThat(mPropertyModel.get(BOTTOM_BAR_HEIGHT), equalTo(0));
        assertThat(mSecondaryTasksSurfacePropertyModel.get(BOTTOM_BAR_HEIGHT), equalTo(0));

        mBrowserControlsStateProviderCaptor.getValue().onBottomControlsHeightChanged(10, 10);
        assertThat(mPropertyModel.get(BOTTOM_BAR_HEIGHT), equalTo(10));
        assertThat(mSecondaryTasksSurfacePropertyModel.get(BOTTOM_BAR_HEIGHT), equalTo(0));

        mediator.hideTabSwitcherView(false);
        mCarouselTabSwitcherModuleVisibilityObserverCaptor.getValue().startedHiding();
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
        showHomepageAndVerify(mediator, StartSurfaceState.SHOWING_HOMEPAGE);
        verify(mTabModelSelector).addObserver(mTabModelSelectorObserverCaptor.capture());

        assertThat(mPropertyModel.get(IS_INCOGNITO_DESCRIPTION_INITIALIZED), equalTo(false));
        assertThat(mPropertyModel.get(IS_INCOGNITO_DESCRIPTION_VISIBLE), equalTo(false));
        if (ChromeFeatureList.sStartSurfaceRefactor.isEnabled()) {
            // Early returns since the SecondaryTasksSurface will go away when the refactoring is
            // enabled.
            return;
        }

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
        showHomepageAndVerify(mediator, StartSurfaceState.SHOWN_HOMEPAGE);
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
        verify(mCarouselOrSingleTabSwitcherModuleController)
                .addTabSwitcherViewObserver(
                        mCarouselTabSwitcherModuleVisibilityObserverCaptor.capture());

        if (!ChromeFeatureList.sStartSurfaceRefactor.isEnabled()) {
            assertThat(mediator.getStartSurfaceState(), equalTo(StartSurfaceState.NOT_SHOWN));
            mediator.setSecondaryTasksSurfacePropertyModel(mSecondaryTasksSurfacePropertyModel);
        }
        showHomepageAndVerify(mediator, StartSurfaceState.SHOWN_HOMEPAGE);
        verify(mTabModelSelector).addObserver(mTabModelSelectorObserverCaptor.capture());
        verify(mCarouselOrSingleTabSwitcherModuleController).showTabSwitcherView(eq(false));
        verify(mOmniboxStub, times(2))
                .addUrlFocusChangeListener(mUrlFocusChangeListenerCaptor.capture());
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
    public void defaultStateSinglePane() {
        doReturn(false).when(mTabModelSelector).isIncognitoSelected();
        doReturn(mVoiceRecognitionHandler).when(mOmniboxStub).getVoiceRecognitionHandler();
        doReturn(true).when(mVoiceRecognitionHandler).isVoiceSearchEnabled();

        StartSurfaceMediator mediator =
                createStartSurfaceMediator(/* isStartSurfaceEnabled= */ true);
        verify(mCarouselOrSingleTabSwitcherModuleController)
                .addTabSwitcherViewObserver(
                        mCarouselTabSwitcherModuleVisibilityObserverCaptor.capture());

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
        verify(mCarouselOrSingleTabSwitcherModuleController)
                .addTabSwitcherViewObserver(
                        mCarouselTabSwitcherModuleVisibilityObserverCaptor.capture());

        assertThat(mediator.getStartSurfaceState(), equalTo(StartSurfaceState.NOT_SHOWN));

        mediator.setSecondaryTasksSurfacePropertyModel(mSecondaryTasksSurfacePropertyModel);
        showHomepageAndVerify(mediator, StartSurfaceState.SHOWING_HOMEPAGE);
        verify(mCarouselOrSingleTabSwitcherModuleController).showTabSwitcherView(eq(false));
        verify(mOmniboxStub, times(2))
                .addUrlFocusChangeListener(mUrlFocusChangeListenerCaptor.capture());
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
    // TODO(crbug.com/1315676): removes this test once the Start surface refactoring is enabled.
    // This is because the StartSurfaceMediator is no longer responsible for the transition between
    // the Start surface and the Tab switcher.
    @DisableFeatures(ChromeFeatureList.START_SURFACE_REFACTOR)
    public void singleShowingPrevious() {
        doReturn(false).when(mTabModelSelector).isIncognitoSelected();
        doReturn(mVoiceRecognitionHandler).when(mOmniboxStub).getVoiceRecognitionHandler();
        doReturn(true).when(mVoiceRecognitionHandler).isVoiceSearchEnabled();

        StartSurfaceMediator mediator =
                createStartSurfaceMediator(/* isStartSurfaceEnabled= */ true);
        InOrder carouselTabSwitcherModuleController =
                inOrder(mCarouselOrSingleTabSwitcherModuleController);
        carouselTabSwitcherModuleController.verify(mCarouselOrSingleTabSwitcherModuleController)
                .addTabSwitcherViewObserver(
                        mCarouselTabSwitcherModuleVisibilityObserverCaptor.capture());

        assertThat(mediator.getStartSurfaceState(), equalTo(StartSurfaceState.NOT_SHOWN));

        mediator.setSecondaryTasksSurfacePropertyModel(mSecondaryTasksSurfacePropertyModel);
        showHomepageAndVerify(mediator, StartSurfaceState.SHOWING_PREVIOUS);
        carouselTabSwitcherModuleController.verify(mCarouselOrSingleTabSwitcherModuleController)
                .showTabSwitcherView(eq(false));
        InOrder omniboxStub = inOrder(mOmniboxStub);
        omniboxStub.verify(mOmniboxStub, times(2))
                .addUrlFocusChangeListener(mUrlFocusChangeListenerCaptor.capture());
        assertThat(mPropertyModel.get(IS_FAKE_SEARCH_BOX_VISIBLE), equalTo(true));
        assertThat(mediator.shouldShowTabSwitcherToolbar(), equalTo(true));

        mediator.hideTabSwitcherView(true);
        mCarouselTabSwitcherModuleVisibilityObserverCaptor.getValue().startedHiding();
        mCarouselTabSwitcherModuleVisibilityObserverCaptor.getValue().finishedHiding();

        showHomepageAndVerify(mediator, StartSurfaceState.SHOWING_PREVIOUS);
        carouselTabSwitcherModuleController.verify(mCarouselOrSingleTabSwitcherModuleController)
                .showTabSwitcherView(eq(false));
        omniboxStub.verify(mOmniboxStub, times(2))
                .addUrlFocusChangeListener(mUrlFocusChangeListenerCaptor.capture());
        assertThat(mPropertyModel.get(IS_FAKE_SEARCH_BOX_VISIBLE), equalTo(true));
        assertThat(mediator.shouldShowTabSwitcherToolbar(), equalTo(true));

        mediator.setStartSurfaceState(StartSurfaceState.SHOWN_TABSWITCHER);
        assertThat(mSecondaryTasksSurfacePropertyModel.get(IS_FAKE_SEARCH_BOX_VISIBLE),
                equalTo(false));
        assertThat(mediator.shouldShowTabSwitcherToolbar(), equalTo(true));
        assertThat(mediator.getStartSurfaceState(), equalTo(StartSurfaceState.SHOWN_TABSWITCHER));

        mediator.hideTabSwitcherView(true);
        mCarouselTabSwitcherModuleVisibilityObserverCaptor.getValue().startedHiding();
        mCarouselTabSwitcherModuleVisibilityObserverCaptor.getValue().finishedHiding();

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
        InOrder carouselTabSwitcherModuleController =
                inOrder(mCarouselOrSingleTabSwitcherModuleController);
        carouselTabSwitcherModuleController.verify(mCarouselOrSingleTabSwitcherModuleController)
                .addTabSwitcherViewObserver(
                        mCarouselTabSwitcherModuleVisibilityObserverCaptor.capture());
        assertThat(mediator.getStartSurfaceState(), equalTo(StartSurfaceState.NOT_SHOWN));

        mPropertyModel.set(IS_EXPLORE_SURFACE_VISIBLE, true);
        when(mExploreSurfaceCoordinatorFactory.create(anyBoolean(), anyBoolean(), anyInt()))
                .thenReturn(mExploreSurfaceCoordinator);
        showHomepageAndVerify(mediator, StartSurfaceState.SHOWING_HOMEPAGE);
        carouselTabSwitcherModuleController.verify(mCarouselOrSingleTabSwitcherModuleController)
                .showTabSwitcherView(eq(false));
        assertThat(mPropertyModel.get(EXPLORE_SURFACE_COORDINATOR),
                equalTo(mExploreSurfaceCoordinator));

        doReturn(TabLaunchType.FROM_START_SURFACE).when(mTab).getLaunchType();
        mediator.hideTabSwitcherView(true);
        mCarouselTabSwitcherModuleVisibilityObserverCaptor.getValue().startedHiding();
        mCarouselTabSwitcherModuleVisibilityObserverCaptor.getValue().finishedHiding();
        assertNull(mPropertyModel.get(EXPLORE_SURFACE_COORDINATOR));

        showHomepageAndVerify(mediator, StartSurfaceState.SHOWING_PREVIOUS);
        assertThat(mPropertyModel.get(EXPLORE_SURFACE_COORDINATOR),
                equalTo(mExploreSurfaceCoordinator));

        doReturn(TabLaunchType.FROM_LINK).when(mTab).getLaunchType();
        mediator.hideTabSwitcherView(true);
        mCarouselTabSwitcherModuleVisibilityObserverCaptor.getValue().startedHiding();
        mCarouselTabSwitcherModuleVisibilityObserverCaptor.getValue().finishedHiding();
        assertNull(mPropertyModel.get(EXPLORE_SURFACE_COORDINATOR));
    }

    @Test
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
        showHomepageAndVerify(mediator, StartSurfaceState.SHOWING_START);

        verify(mBrowserControlsStateProvider).addObserver(ArgumentMatchers.any());
        assertEquals("Wrong top content offset on homepage.", 10, mPropertyModel.get(TOP_MARGIN));

        onControlsOffsetChanged(/*topOffset=*/100, /*topControlsMinHeightOffset=*/20);
        assertEquals("Wrong top content offset on homepage.", 20, mPropertyModel.get(TOP_MARGIN));

        onControlsOffsetChanged(/*topOffset=*/130, /*topControlsMinHeightOffset=*/50);
        assertEquals("Wrong top content offset on homepage.", 50, mPropertyModel.get(TOP_MARGIN));

        if (ChromeFeatureList.sStartSurfaceRefactor.isEnabled()) {
            // When the refactoring is enabled, the StartSurfaceMediator is no longer responsible
            // for showing the Grid tab switcher.
            return;
        }

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
                /* isRefactorEnabled */ false, /* hadWarmStart= */ false,
                /* useMagicSpace*/ false);
        verify(mCarouselOrSingleTabSwitcherModuleController)
                .addTabSwitcherViewObserver(
                        mCarouselTabSwitcherModuleVisibilityObserverCaptor.capture());

        assertThat(mediator.getStartSurfaceState(), equalTo(StartSurfaceState.NOT_SHOWN));
        // Sets the current StartSurfaceState to SHOWING_START before calling the
        // {@link StartSurfaceMediator#showOverview()}. This is because if the current
        // StartSurfaceState is NOT_SHOWN, the state will be set default to SHOWING_TABSWITCHER in
        // {@link StartSurfaceMediator#showOverview()}.
        showHomepageAndVerify(mediator, StartSurfaceState.SHOWING_START);
        assertThat(mPropertyModel.get(IS_EXPLORE_SURFACE_VISIBLE), equalTo(false));
        verify(mCarouselOrSingleTabSwitcherModuleController).showTabSwitcherView(eq(false));

        when(mCarouselOrSingleTabSwitcherModuleController.overviewVisible()).thenReturn(true);
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
                /* isRefactorEnabled */ false, /* hadWarmStart= */ false,
                /* useMagicSpace*/ false);
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
    @EnableFeatures(ChromeFeatureList.SURFACE_POLISH)
    public void initializeStartSurfaceTopMargins_SurfacePolish() {
        Resources resources = ContextUtils.getApplicationContext().getResources();
        int tasksSurfaceBodyTopMarginPolished = 0;
        int tabSwitcherTitleTopMargin =
                resources.getDimensionPixelSize(R.dimen.tab_switcher_title_top_margin);

        createStartSurfaceMediatorWithoutInit(/* isStartSurfaceEnabled= */ true,
                /* isRefactorEnabled */ false, /* hadWarmStart= */ false,
                /* useMagicSpace*/ false);
        assertThat(mPropertyModel.get(TASKS_SURFACE_BODY_TOP_MARGIN),
                equalTo(tasksSurfaceBodyTopMarginPolished));
        assertThat(mPropertyModel.get(MV_TILES_CONTAINER_TOP_MARGIN), equalTo(0));
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
        doReturn(TabSwitcherType.SINGLE)
                .when(mCarouselOrSingleTabSwitcherModuleController)
                .getTabSwitcherType();

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
                        /* isRefactorEnabled */ false, /* hadWarmStart= */ false,
                        /* useMagicSpace*/ false);
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
        showHomepageAndVerify(mediator, StartSurfaceState.SHOWING_HOMEPAGE);
        assertThat(mPropertyModel.get(IS_TAB_CAROUSEL_VISIBLE), equalTo(false));
        assertThat(mPropertyModel.get(TASKS_SURFACE_BODY_TOP_MARGIN),
                equalTo(tasksSurfaceBodyTopMarginWithoutTab));

        doReturn(2).when(mNormalTabModel).getCount();
        showHomepageAndVerify(mediator, StartSurfaceState.SHOWING_HOMEPAGE);
        assertThat(mPropertyModel.get(IS_TAB_CAROUSEL_VISIBLE), equalTo(true));
        assertThat(mPropertyModel.get(TASKS_SURFACE_BODY_TOP_MARGIN),
                equalTo(tasksSurfaceBodyTopMarginWithTab));
    }

    @Test
    @EnableFeatures(INSTANT_START)
    public void feedPlaceholderFromWarmStart() {
        doReturn(false).when(mTabModelSelector).isIncognitoSelected();
        doReturn(mVoiceRecognitionHandler).when(mOmniboxStub).getVoiceRecognitionHandler();
        doReturn(true).when(mVoiceRecognitionHandler).isVoiceSearchEnabled();

        StartSurfaceMediator mediator = createStartSurfaceMediator(
                /* isStartSurfaceEnabled= */ true,
                /* isRefactorEnabled */ false, /* hadWarmStart= */ true,
                /* useMagicSpace*/ false);
        assertFalse(mediator.shouldShowFeedPlaceholder());

        mPropertyModel.set(IS_EXPLORE_SURFACE_VISIBLE, true);
        when(mExploreSurfaceCoordinatorFactory.create(anyBoolean(), anyBoolean(), anyInt()))
                .thenReturn(mExploreSurfaceCoordinator);
        showHomepageAndVerify(mediator, StartSurfaceState.SHOWING_HOMEPAGE);

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
        doReturn(TabSwitcherType.SINGLE)
                .when(mCarouselOrSingleTabSwitcherModuleController)
                .getTabSwitcherType();

        StartSurfaceMediator mediator =
                createStartSurfaceMediator(/* isStartSurfaceEnabled= */ true);

        doReturn(2).when(mNormalTabModel).getCount();
        doReturn(true).when(mTabModelSelector).isTabStateInitialized();
        showHomepageAndVerify(mediator, StartSurfaceState.SHOWING_HOMEPAGE);
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

        doReturn(TabSwitcherType.SINGLE)
                .when(mCarouselOrSingleTabSwitcherModuleController)
                .getTabSwitcherType();
        MockTab regularTab = new MockTab(1, false);
        regularTab.setGurlOverrideForTesting(JUnitTestGURLs.NTP_URL);
        when(mTabModelSelector.getCurrentTab()).thenReturn(regularTab);

        StartSurfaceMediator mediator =
                createStartSurfaceMediator(/* isStartSurfaceEnabled= */ true);

        showHomepageAndVerify(mediator, StartSurfaceState.SHOWING_HOMEPAGE);
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
        showHomepageAndVerify(mediator, StartSurfaceState.SHOWN_HOMEPAGE);
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

        StartSurfaceMediator mediator = createStartSurfaceMediator(/*isStartSurfaceEnabled=*/true,
                /* isRefactorEnabled */ false, /* hadWarmStart= */ false,
                /* useMagicSpace*/ false);
        showHomepageAndVerify(mediator, StartSurfaceState.SHOWN_HOMEPAGE);

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

        StartSurfaceMediator mediator = createStartSurfaceMediator(/*isStartSurfaceEnabled=*/true,
                /* isRefactorEnabled */ false, /* hadWarmStart= */ false,
                /* useMagicSpace*/ false);
        showHomepageAndVerify(mediator, StartSurfaceState.SHOWN_HOMEPAGE);

        verify(mLogoContainerView, times(0)).setVisibility(View.VISIBLE);
        Assert.assertFalse(mediator.isLogoVisible());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.SURFACE_POLISH)
    public void testInitializeLogoWhenSurfacePolishedMoveDownLogoEnabled() {
        doReturn(mVoiceRecognitionHandler).when(mOmniboxStub).getVoiceRecognitionHandler();
        when(mTemplateUrlService.doesDefaultSearchEngineHaveLogo()).thenReturn(true);

        StartSurfaceConfiguration.SURFACE_POLISH_MOVE_DOWN_LOGO.setForTesting(true);
        Assert.assertTrue(ReturnToChromeUtil.moveDownLogo());

        StartSurfaceMediator mediator = createStartSurfaceMediator(/*isStartSurfaceEnabled=*/true,
                /* isRefactorEnabled */ false, /* hadWarmStart= */ false,
                /* useMagicSpace*/ false);
        showHomepageAndVerify(mediator, StartSurfaceState.SHOWN_HOMEPAGE);

        verify(mLogoContainerView).setVisibility(View.VISIBLE);
        verify(mLogoBridge).getCurrentLogo(anyLong(), any(), any());
        Assert.assertTrue(mediator.isLogoVisible());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.SURFACE_POLISH)
    public void testNotInitializeLogoWhenSurfacePolishedMoveDownLogoDisabled() {
        doReturn(mVoiceRecognitionHandler).when(mOmniboxStub).getVoiceRecognitionHandler();
        when(mTemplateUrlService.doesDefaultSearchEngineHaveLogo()).thenReturn(true);

        Assert.assertFalse(ReturnToChromeUtil.moveDownLogo());

        StartSurfaceMediator mediator = createStartSurfaceMediator(/*isStartSurfaceEnabled=*/true,
                /* isRefactorEnabled */ false, /* hadWarmStart= */ false,
                /* useMagicSpace*/ false);
        showHomepageAndVerify(mediator, StartSurfaceState.SHOWN_HOMEPAGE);

        verify(mLogoContainerView, times(0)).setVisibility(View.VISIBLE);
        Assert.assertFalse(mediator.isLogoVisible());
    }

    @Test
    public void testFeedReliabilityLoggerPageLoadStarted() {
        doReturn(mVoiceRecognitionHandler).when(mOmniboxStub).getVoiceRecognitionHandler();

        StartSurfaceMediator mediator = createStartSurfaceMediator(/*isStartSurfaceEnabled=*/true);
        showHomepageAndVerify(mediator, StartSurfaceState.SHOWN_HOMEPAGE);

        verify(mNormalTabModel).addObserver(mTabModelObserverCaptor.capture());
        assertThat(mPropertyModel.get(IS_EXPLORE_SURFACE_VISIBLE), equalTo(true));
        mTabModelObserverCaptor.getValue().willAddTab(/*tab=*/null, TabLaunchType.FROM_LINK);
        verify(mFeedReliabilityLogger, times(1)).onPageLoadStarted();
    }

    @Test
    public void testFeedReliabilityLoggerObservesUrlFocus() {
        doReturn(mVoiceRecognitionHandler).when(mOmniboxStub).getVoiceRecognitionHandler();

        StartSurfaceMediator mediator = createStartSurfaceMediator(/*isStartSurfaceEnabled=*/true);
        verify(mCarouselOrSingleTabSwitcherModuleController)
                .addTabSwitcherViewObserver(
                        mCarouselTabSwitcherModuleVisibilityObserverCaptor.capture());
        showHomepageAndVerify(mediator, StartSurfaceState.SHOWN_HOMEPAGE);

        assertThat(mPropertyModel.get(IS_EXPLORE_SURFACE_VISIBLE), equalTo(true));
        verify(mOmniboxStub, times(2))
                .addUrlFocusChangeListener(mUrlFocusChangeListenerCaptor.capture());
        UrlFocusChangeListener listener = mUrlFocusChangeListenerCaptor.getAllValues().get(0);
        assertThat(listener, equalTo(mFeedReliabilityLogger));

        mCarouselTabSwitcherModuleVisibilityObserverCaptor.getValue().startedShowing();
        mCarouselTabSwitcherModuleVisibilityObserverCaptor.getValue().finishedShowing();
        mCarouselTabSwitcherModuleVisibilityObserverCaptor.getValue().startedHiding();

        mediator.hideTabSwitcherView(true);
        verify(mOmniboxStub).removeUrlFocusChangeListener(listener);

        mCarouselTabSwitcherModuleVisibilityObserverCaptor.getValue().finishedHiding();
    }

    @Test
    public void testFeedReliabilityLoggerBackPressed() {
        doReturn(mVoiceRecognitionHandler).when(mOmniboxStub).getVoiceRecognitionHandler();
        StartSurfaceMediator mediator = createStartSurfaceMediator(/*isStartSurfaceEnabled=*/true);
        showHomepageAndVerify(mediator, StartSurfaceState.SHOWN_HOMEPAGE);
        mediator.onBackPressed();
        verify(mFeedReliabilityLogger).onNavigateBack();
    }

    @Test
    @EnableFeatures(ChromeFeatureList.BACK_GESTURE_REFACTOR)
    // TODO(crbug.com/1315676): removes this test once the Start surface refactoring is enabled.
    // This is because the StartSurfaceMediator is no longer responsible for the transition between
    // the Start surface and the Tab switcher.
    @DisableFeatures(ChromeFeatureList.START_SURFACE_REFACTOR)
    public void testBackPressHandler() {
        doReturn(false).when(mTabModelSelector).isIncognitoSelected();
        doReturn(mVoiceRecognitionHandler).when(mOmniboxStub).getVoiceRecognitionHandler();
        doReturn(true).when(mVoiceRecognitionHandler).isVoiceSearchEnabled();
        doAnswer((inv) -> mCarouselTabSwitcherModuleControllerDialogVisibleSupplier.get())
                .when(mCarouselOrSingleTabSwitcherModuleController)
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
        mCarouselTabSwitcherModuleControllerDialogVisibleSupplier.set(true);
        Assert.assertTrue(mediator.shouldInterceptBackPress());
        doReturn(true).when(mCarouselOrSingleTabSwitcherModuleController).onBackPressed();
        mediator.onBackPressed();
        verify(mCarouselOrSingleTabSwitcherModuleController).onBackPressed();

        mCarouselTabSwitcherModuleControllerDialogVisibleSupplier.set(false);
        Assert.assertFalse(mediator.shouldInterceptBackPress());

        mCarouselTabSwitcherModuleControllerDialogVisibleSupplier.set(true);
        mSecondaryControllerDialogVisibleSupplier.set(true);
        Assert.assertTrue(mediator.shouldInterceptBackPress());
        doReturn(true).when(mSecondaryTasksSurfaceController).onBackPressed();
        mediator.onBackPressed();
        verify(mCarouselOrSingleTabSwitcherModuleController).onBackPressed();
        verify(mSecondaryTasksSurfaceController,
                description("Secondary task surface has a higher priority of handling back press"))
                .onBackPressed();

        mSecondaryControllerDialogVisibleSupplier.set(false);
        mCarouselTabSwitcherModuleControllerDialogVisibleSupplier.set(false);
        verify(mSecondaryTasksSurfaceController,
                description(
                        "Secondary task surface consumes back press when no dialog is visible."))
                .onBackPressed();

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
    @DisableFeatures(ChromeFeatureList.START_SURFACE_REFACTOR)
    public void testBackPressHandlerOnStartSurfaceWithoutTabSwitcherCreated() {
        backPressHandlerOnStartSurfaceWithoutTabSwitcherCreatedImpl(
                StartSurfaceState.SHOWN_HOMEPAGE);
    }

    @Test
    @EnableFeatures(ChromeFeatureList.START_SURFACE_REFACTOR)
    public void testBackPressHandlerOnStartSurfaceWithoutTabSwitcherCreated_RefactorEnabled() {
        backPressHandlerOnStartSurfaceWithoutTabSwitcherCreatedImpl(null);
    }

    private void backPressHandlerOnStartSurfaceWithoutTabSwitcherCreatedImpl(
            @StartSurfaceState Integer state) {
        doReturn(false).when(mTabModelSelector).isIncognitoSelected();
        doReturn(false).when(mTabModelSelector).isIncognitoSelected();
        doReturn(mVoiceRecognitionHandler).when(mOmniboxStub).getVoiceRecognitionHandler();
        doReturn(true).when(mVoiceRecognitionHandler).isVoiceSearchEnabled();
        StartSurfaceMediator mediator = createStartSurfaceMediator(/*isStartSurfaceEnabled=*/true);

        showHomepageAndVerify(mediator, state);

        doReturn(true).when(mCarouselOrSingleTabSwitcherModuleController).isDialogVisible();
        mediator.onBackPressed();
        verify(mCarouselOrSingleTabSwitcherModuleController).onBackPressed();

        doReturn(false).when(mCarouselOrSingleTabSwitcherModuleController).isDialogVisible();
        mediator.onBackPressed();
        verify(mCarouselOrSingleTabSwitcherModuleController, times(2)).onBackPressed();
    }

    /**
     * Tests the logic of StartSurfaceMediator#onBackPressedInternal() when the Start surface is
     * showing and the Tab switcher has been created.
     */
    @Test
    // TODO(crbug.com/1315676): Removes this test after the refactoring is enabled by default. This
    // is because the SecondaryTasksSurface will go away.
    @DisableFeatures(ChromeFeatureList.START_SURFACE_REFACTOR)
    public void testBackPressHandlerOnStartSurfaceWithTabSwitcherCreated() {
        doReturn(false).when(mTabModelSelector).isIncognitoSelected();
        doReturn(false).when(mTabModelSelector).isIncognitoSelected();
        doReturn(mVoiceRecognitionHandler).when(mOmniboxStub).getVoiceRecognitionHandler();
        doReturn(true).when(mVoiceRecognitionHandler).isVoiceSearchEnabled();

        StartSurfaceMediator mediator = createStartSurfaceMediator(/*isStartSurfaceEnabled=*/true);
        mediator.setSecondaryTasksSurfacePropertyModel(mSecondaryTasksSurfacePropertyModel);
        mediator.setSecondaryTasksSurfaceController(mSecondaryTasksSurfaceController);

        showHomepageAndVerify(mediator, StartSurfaceState.SHOWN_HOMEPAGE);

        doReturn(true).when(mCarouselOrSingleTabSwitcherModuleController).isDialogVisible();
        doReturn(false).when(mSecondaryTasksSurfaceController).isDialogVisible();
        mediator.onBackPressed();
        verify(mCarouselOrSingleTabSwitcherModuleController).onBackPressed();
        verify(mSecondaryTasksSurfaceController, never()).onBackPressed();

        doReturn(false).when(mCarouselOrSingleTabSwitcherModuleController).isDialogVisible();
        doReturn(false).when(mSecondaryTasksSurfaceController).isDialogVisible();
        mediator.onBackPressed();
        verify(mCarouselOrSingleTabSwitcherModuleController, times(2)).onBackPressed();
        // Reach the end of #onBackPressed.
        verify(mSecondaryTasksSurfaceController).onBackPressed();
    }

    @Test
    @EnableFeatures(ChromeFeatureList.START_SURFACE_REFACTOR)
    public void testBackPressHandlerOnStartSurfaceWithTabSwitcherCreatedAndRefactorEnabled() {
        doReturn(false).when(mTabModelSelector).isIncognitoSelected();
        doReturn(false).when(mTabModelSelector).isIncognitoSelected();
        doReturn(mVoiceRecognitionHandler).when(mOmniboxStub).getVoiceRecognitionHandler();
        doReturn(true).when(mVoiceRecognitionHandler).isVoiceSearchEnabled();

        StartSurfaceMediator mediator = createStartSurfaceMediator(/*isStartSurfaceEnabled=*/true);

        showHomepageAndVerify(mediator, null);

        doReturn(true).when(mCarouselOrSingleTabSwitcherModuleController).isDialogVisible();
        mediator.onBackPressed();
        verify(mCarouselOrSingleTabSwitcherModuleController, times(1)).onBackPressed();

        doReturn(false).when(mCarouselOrSingleTabSwitcherModuleController).isDialogVisible();
        mediator.onBackPressed();
        verify(mCarouselOrSingleTabSwitcherModuleController, times(2)).onBackPressed();
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
        doReturn(false).when(mCarouselOrSingleTabSwitcherModuleController).isDialogVisible();

        doReturn(true).when(mSecondaryTasksSurfaceController).isDialogVisible();
        doReturn(true).when(mSecondaryTasksSurfaceController).onBackPressed();
        var histogramWatcher = HistogramWatcher.newSingleRecordWatcher(
                "Android.BackPress.Intercept", 4); // START_SURFACE enum value
        Assert.assertTrue(mediator.onBackPressed());
        verify(mCarouselOrSingleTabSwitcherModuleController, never()).onBackPressed();
        verify(mSecondaryTasksSurfaceController).onBackPressed();
        histogramWatcher.assertExpected();

        doReturn(false).when(mSecondaryTasksSurfaceController).isDialogVisible();
        doReturn(false).when(mSecondaryTasksSurfaceController).onBackPressed();
        histogramWatcher = HistogramWatcher.newBuilder()
                                   .expectNoRecords("Android.BackPress.Intercept")
                                   .build();
        Assert.assertFalse(verify(mSecondaryTasksSurfaceController).onBackPressed());
        histogramWatcher.assertExpected();

        histogramWatcher = HistogramWatcher.newSingleRecordWatcher(
                "Android.BackPress.Intercept", 4); // START_SURFACE enum value
        mediator.setStartSurfaceState(StartSurfaceState.SHOWN_HOMEPAGE);
        mediator.setStartSurfaceState(StartSurfaceState.SHOWN_TABSWITCHER);
        Assert.assertEquals(StartSurfaceState.SHOWN_TABSWITCHER, mediator.getStartSurfaceState());
        Assert.assertTrue(mediator.onBackPressed());
        Assert.assertEquals("Should return to home page on back press.",
                StartSurfaceState.SHOWN_HOMEPAGE, mediator.getStartSurfaceState());
        histogramWatcher.assertExpected();
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
        showHomepageAndVerify(mediator, StartSurfaceState.SHOWING_START);
        if (!ChromeFeatureList.sStartSurfaceRefactor.isEnabled()) {
            var histogramWatcher =
                    HistogramWatcher.newSingleRecordWatcher(START_SURFACE_TIME_SPENT);
            // Verifies that the histograms are logged in the transitions of Start Surface -> Grid
            // Tab Switcher. Only testing in the case when the refactoring is disabled, since
            // StartSurfaceState isn't used if the refactoring is enabled.
            mediator.setStartSurfaceState(StartSurfaceState.SHOWN_TABSWITCHER);
            histogramWatcher.assertExpected();
            showHomepageAndVerify(mediator, StartSurfaceState.SHOWING_HOMEPAGE);
        }

        var histogramWatcher = HistogramWatcher.newSingleRecordWatcher(START_SURFACE_TIME_SPENT);
        mPauseResumeWithNativeObserverArgumentCaptor.getValue().onPauseWithNative();
        histogramWatcher.assertExpected();

        histogramWatcher = HistogramWatcher.newSingleRecordWatcher(START_SURFACE_TIME_SPENT);
        mPauseResumeWithNativeObserverArgumentCaptor.getValue().onResumeWithNative();
        mediator.destroy();
        histogramWatcher.assertExpected();
    }

    @Test
    @EnableFeatures(ChromeFeatureList.START_SURFACE_REFACTOR)
    public void testShowAndOnHide() {
        doReturn(false).when(mTabModelSelector).isIncognitoSelected();
        doReturn(mVoiceRecognitionHandler).when(mOmniboxStub).getVoiceRecognitionHandler();
        doReturn(true).when(mVoiceRecognitionHandler).isVoiceSearchEnabled();
        doReturn(mSingleTabSwitcherModuleController).when(mTabSwitcherModule).getController();
        doReturn(TabSwitcherType.SINGLE)
                .when(mSingleTabSwitcherModuleController)
                .getTabSwitcherType();
        doReturn(true).when(mOmniboxStub).isLensEnabled(LensEntryPoint.TASKS_SURFACE);

        StartSurfaceMediator mediator = createStartSurfaceMediator(/*isStartSurfaceEnabled=*/true,
                /* isRefactorEnabled */ true);
        assertEquals(mInitializeMVTilesRunnable, mediator.getInitializeMVTilesRunnableForTesting());
        assertEquals(mTabSwitcherModule, mediator.getTabSwitcherModuleForTesting());

        showHomepageAndVerify(mediator, null);
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

        doReturn(mTabListDelegate).when(mTabSwitcherModule).getTabListDelegate();
        mediator.onHide();
        verify(mTabListDelegate).postHiding();
    }

    @Test
    public void testDefaultSearchEngineChanged() {
        doReturn(mVoiceRecognitionHandler).when(mOmniboxStub).getVoiceRecognitionHandler();

        mProfileSupplier = new ObservableSupplierImpl<>();
        StartSurfaceMediator mediator = createStartSurfaceMediator(/*isStartSurfaceEnabled=*/true,
                /* isRefactorEnabled */ false, /* hadWarmStart= */ false,
                /* useMagicSpace*/ false);
        showHomepageAndVerify(mediator, StartSurfaceState.SHOWN_HOMEPAGE);

        mProfileSupplier.set(mProfile);
        verify(mTemplateUrlService).addObserver(mTemplateUrlServiceObserverCaptor.capture());
        doReturn(true).when(mOmniboxStub).isLensEnabled(LensEntryPoint.TASKS_SURFACE);
        mTemplateUrlServiceObserverCaptor.getValue().onTemplateURLServiceChanged();
        assertTrue(mPropertyModel.get(IS_LENS_BUTTON_VISIBLE));

        doReturn(false).when(mOmniboxStub).isLensEnabled(LensEntryPoint.TASKS_SURFACE);
        mTemplateUrlServiceObserverCaptor.getValue().onTemplateURLServiceChanged();
        assertFalse(mPropertyModel.get(IS_LENS_BUTTON_VISIBLE));

        mediator.destroy();
        verify(mTemplateUrlService).removeObserver(mTemplateUrlServiceObserverCaptor.capture());
    }

    @Test
    @EnableFeatures({ChromeFeatureList.START_SURFACE_REFACTOR, ChromeFeatureList.SURFACE_POLISH})
    public void testObserverWithSurfacePolish() {
        SURFACE_POLISH_USE_MAGIC_SPACE.setForTesting(true);
        Assert.assertTrue(ChromeFeatureList.sSurfacePolish.isEnabled());
        Assert.assertTrue(SURFACE_POLISH_USE_MAGIC_SPACE.getValue());

        doReturn(false).when(mTabModelSelector).isIncognitoSelected();
        doReturn(mVoiceRecognitionHandler).when(mOmniboxStub).getVoiceRecognitionHandler();
        doReturn(true).when(mVoiceRecognitionHandler).isVoiceSearchEnabled();
        doReturn(mTabModelFilterProvider).when(mTabModelSelector).getTabModelFilterProvider();
        doReturn(true).when(mOmniboxStub).isLensEnabled(LensEntryPoint.TASKS_SURFACE);

        StartSurfaceMediator mediator = createStartSurfaceMediator(/*isStartSurfaceEnabled=*/true,
                /* isRefactorEnabled */ true, /* hadWarmStart= */ false,
                /* useMagicSpace */ true);
        assertEquals(mInitializeMVTilesRunnable, mediator.getInitializeMVTilesRunnableForTesting());
        assertNull(mediator.getTabSwitcherModuleForTesting());
        assertNull(mediator.getControllerForTesting());

        showHomepageAndVerify(mediator, null);
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
        mTabModelSelectorObserverCaptor.getValue().onTabModelSelected(
                mIncognitoTabModel, mNormalTabModel);
        assertTrue(mPropertyModel.get(IS_INCOGNITO));

        doReturn(false).when(mTabModelSelector).isIncognitoSelected();
        mTabModelSelector.selectModel(false);
        mTabModelSelectorObserverCaptor.getValue().onTabModelSelected(
                mNormalTabModel, mIncognitoTabModel);
        assertFalse(mPropertyModel.get(IS_INCOGNITO));

        int lastId = 1;
        int currentId = 2;
        doReturn(false).when(mTab).isIncognito();
        doReturn(currentId).when(mTabModelSelector).getCurrentTabId();
        OnTabSelectingListener onTabSelectingListener = Mockito.mock(OnTabSelectingListener.class);
        mediator.setOnTabSelectingListener(onTabSelectingListener);
        mTabModelObserverCaptor.getValue().didSelectTab(mTab, TabSelectionType.FROM_USER, lastId);
        verify(onTabSelectingListener).onTabSelecting(anyLong(), eq(currentId));

        doReturn(true).when(mTab).isIncognito();
        mTabModelObserverCaptor.getValue().didSelectTab(mTab, TabSelectionType.FROM_USER, lastId);
        verify(onTabSelectingListener, times(2)).onTabSelecting(anyLong(), eq(currentId));

        mTabModelObserverCaptor.getValue().didSelectTab(mTab, TabSelectionType.FROM_CLOSE, lastId);
        verify(onTabSelectingListener, times(2)).onTabSelecting(anyLong(), eq(currentId));
    }

    @Test
    @EnableFeatures({ChromeFeatureList.START_SURFACE_REFACTOR, ChromeFeatureList.SURFACE_POLISH})
    public void testShowAndOnHideWithSurfacePolish() {
        SURFACE_POLISH_USE_MAGIC_SPACE.setForTesting(true);
        Assert.assertTrue(ChromeFeatureList.sSurfacePolish.isEnabled());
        Assert.assertTrue(SURFACE_POLISH_USE_MAGIC_SPACE.getValue());

        doReturn(false).when(mTabModelSelector).isIncognitoSelected();
        doReturn(mVoiceRecognitionHandler).when(mOmniboxStub).getVoiceRecognitionHandler();
        doReturn(true).when(mVoiceRecognitionHandler).isVoiceSearchEnabled();
        doReturn(mTabModelFilterProvider).when(mTabModelSelector).getTabModelFilterProvider();
        doReturn(true).when(mOmniboxStub).isLensEnabled(LensEntryPoint.TASKS_SURFACE);

        StartSurfaceMediator mediator = createStartSurfaceMediator(/*isStartSurfaceEnabled=*/true,
                /* isRefactorEnabled */ true, /* hadWarmStart= */ false,
                /* useMagicSpace */ true);
        assertEquals(mInitializeMVTilesRunnable, mediator.getInitializeMVTilesRunnableForTesting());
        assertNull(mediator.getTabSwitcherModuleForTesting());
        assertNull(mediator.getControllerForTesting());

        showHomepageAndVerify(mediator, null);
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
    @EnableFeatures(ChromeFeatureList.SURFACE_POLISH)
    public void testUpdateStartSurfaceBackgroundColor() {
        Assert.assertTrue(ChromeFeatureList.sSurfacePolish.isEnabled());
        doReturn(false).when(mTabModelSelector).isIncognitoSelected();
        doReturn(mVoiceRecognitionHandler).when(mOmniboxStub).getVoiceRecognitionHandler();
        // Make sure the background color is not set.
        assertEquals(0, mPropertyModel.get(BACKGROUND_COLOR));

        StartSurfaceMediator mediator = createStartSurfaceMediator(/*isStartSurfaceEnabled=*/true,
                /* isRefactorEnabled= */ false, /* hadWarmStart= */ false, false);
        @ColorInt
        int backgroundColor = ChromeColors.getSurfaceColor(
                mActivity, R.dimen.home_surface_background_color_elevation);
        assertNotEquals(backgroundColor, 0);
        assertEquals(backgroundColor, mPropertyModel.get(BACKGROUND_COLOR));

        mediator.setIsIncognitoForTesting(true);
        mediator.updateBackgroundColor(mPropertyModel);
        @ColorInt
        int newBackgroundColor = ChromeColors.getPrimaryBackgroundColor(mActivity, true);
        assertNotEquals(newBackgroundColor, backgroundColor);
        assertEquals(newBackgroundColor, mPropertyModel.get(BACKGROUND_COLOR));
    }

    private StartSurfaceMediator createStartSurfaceMediator(boolean isStartSurfaceEnabled) {
        return createStartSurfaceMediator(isStartSurfaceEnabled, /* isRefactorEnabled */ false,
                /* hadWarmStart= */ false, /* useMagicSpace= */ false);
    }

    private StartSurfaceMediator createStartSurfaceMediator(
            boolean isStartSurfaceEnabled, boolean isRefactorEnabled) {
        return createStartSurfaceMediator(isStartSurfaceEnabled, isRefactorEnabled,
                /* hadWarmStart= */ false, /* useMagicSpace= */ false);
    }

    private StartSurfaceMediator createStartSurfaceMediator(boolean isStartSurfaceEnabled,
            boolean isRefactorEnabled, boolean hadWarmStart, boolean useMagicSpace) {
        StartSurfaceMediator mediator = createStartSurfaceMediatorWithoutInit(
                isStartSurfaceEnabled, isRefactorEnabled, hadWarmStart, useMagicSpace);
        mediator.initWithNative(mOmniboxStub,
                isStartSurfaceEnabled ? mExploreSurfaceCoordinatorFactory : null, mPrefService,
                null);
        return mediator;
    }

    private StartSurfaceMediator createStartSurfaceMediatorWithoutInit(
            boolean isStartSurfaceEnabled, boolean isRefactorEnabled, boolean hadWarmStart,
            boolean useMagicSpace) {
        boolean hasTasksView = isStartSurfaceEnabled && !isRefactorEnabled;
        boolean hasTabSwitcherModule = isStartSurfaceEnabled && isRefactorEnabled && !useMagicSpace;
        return new StartSurfaceMediator(
                useMagicSpace ? null : mCarouselOrSingleTabSwitcherModuleController,
                null /* tabSwitcherContainer */, hasTabSwitcherModule ? mTabSwitcherModule : null,
                mTabModelSelector, !isStartSurfaceEnabled ? null : mPropertyModel,
                hasTasksView ? mSecondaryTasksSurfaceInitializer : null, isStartSurfaceEnabled,
                mActivity, mBrowserControlsStateProvider, mActivityStateChecker,
                null /* TabCreatorManager */, true /* excludeQueryTiles */, mStartSurfaceSupplier,
                hadWarmStart, isStartSurfaceEnabled ? mInitializeMVTilesRunnable : null,
                mParentTabSupplier, mLogoContainerView, mBackPressManager,
                null /* feedPlaceholderParentView */, mActivityLifecycleDispatcher,
                mTabSwitcherClickHandler, mProfileSupplier);
    }

    private void onControlsOffsetChanged(int topOffset, int topControlsMinHeightOffset) {
        doReturn(topOffset).when(mBrowserControlsStateProvider).getContentOffset();
        doReturn(topControlsMinHeightOffset)
                .when(mBrowserControlsStateProvider)
                .getTopControlsMinHeightOffset();
        mBrowserControlsStateProviderCaptor.getValue().onControlsOffsetChanged(
                topOffset, topControlsMinHeightOffset, 0, 0, false);
    }

    private void showHomepageAndVerify(
            StartSurfaceMediator mediator, @StartSurfaceState Integer state) {
        if (ChromeFeatureList.sStartSurfaceRefactor.isEnabled()) {
            mediator.show(false);
            assertTrue(mediator.isHomepageShown());
        } else {
            mediator.setStartSurfaceState(state);
            mediator.showOverview(false);
            assertThat(mediator.getStartSurfaceState(), equalTo(StartSurfaceState.SHOWN_HOMEPAGE));
        }
    }
}
