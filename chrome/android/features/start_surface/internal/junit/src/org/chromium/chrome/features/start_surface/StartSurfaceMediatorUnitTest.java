// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.features.start_surface;

import static org.hamcrest.CoreMatchers.equalTo;
import static org.hamcrest.MatcherAssert.assertThat;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotEquals;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doNothing;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.inOrder;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import static org.chromium.chrome.browser.flags.ChromeFeatureList.INSTANT_START;
import static org.chromium.chrome.browser.tasks.TasksSurfaceProperties.IS_FAKE_SEARCH_BOX_VISIBLE;
import static org.chromium.chrome.browser.tasks.TasksSurfaceProperties.IS_INCOGNITO;
import static org.chromium.chrome.browser.tasks.TasksSurfaceProperties.IS_INCOGNITO_DESCRIPTION_INITIALIZED;
import static org.chromium.chrome.browser.tasks.TasksSurfaceProperties.IS_INCOGNITO_DESCRIPTION_VISIBLE;
import static org.chromium.chrome.browser.tasks.TasksSurfaceProperties.IS_SURFACE_BODY_VISIBLE;
import static org.chromium.chrome.browser.tasks.TasksSurfaceProperties.IS_TAB_CAROUSEL_VISIBLE;
import static org.chromium.chrome.browser.tasks.TasksSurfaceProperties.IS_VOICE_RECOGNITION_BUTTON_VISIBLE;
import static org.chromium.chrome.browser.tasks.TasksSurfaceProperties.MV_TILES_CONTAINER_TOP_MARGIN;
import static org.chromium.chrome.browser.tasks.TasksSurfaceProperties.MV_TILES_VISIBLE;
import static org.chromium.chrome.browser.tasks.TasksSurfaceProperties.TAB_SWITCHER_TITLE_TOP_MARGIN;
import static org.chromium.chrome.browser.tasks.TasksSurfaceProperties.TASKS_SURFACE_BODY_TOP_MARGIN;
import static org.chromium.chrome.browser.tasks.TasksSurfaceProperties.TRENDY_TERMS_VISIBLE;
import static org.chromium.chrome.features.start_surface.StartSurfaceProperties.BOTTOM_BAR_CLICKLISTENER;
import static org.chromium.chrome.features.start_surface.StartSurfaceProperties.BOTTOM_BAR_HEIGHT;
import static org.chromium.chrome.features.start_surface.StartSurfaceProperties.BOTTOM_BAR_SELECTED_TAB_POSITION;
import static org.chromium.chrome.features.start_surface.StartSurfaceProperties.FEED_SURFACE_COORDINATOR;
import static org.chromium.chrome.features.start_surface.StartSurfaceProperties.IS_BOTTOM_BAR_VISIBLE;
import static org.chromium.chrome.features.start_surface.StartSurfaceProperties.IS_EXPLORE_SURFACE_VISIBLE;
import static org.chromium.chrome.features.start_surface.StartSurfaceProperties.IS_SECONDARY_SURFACE_VISIBLE;
import static org.chromium.chrome.features.start_surface.StartSurfaceProperties.IS_SHOWING_OVERVIEW;
import static org.chromium.chrome.features.start_surface.StartSurfaceProperties.TOP_MARGIN;

import android.content.res.Resources;
import android.view.View;

import org.junit.After;
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
import org.chromium.base.supplier.OneshotSupplier;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider;
import org.chromium.chrome.browser.feed.FeedSurfaceCoordinator;
import org.chromium.chrome.browser.night_mode.NightModeStateProvider;
import org.chromium.chrome.browser.omnibox.OmniboxStub;
import org.chromium.chrome.browser.omnibox.UrlFocusChangeListener;
import org.chromium.chrome.browser.omnibox.voice.VoiceRecognitionHandler;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelFilter;
import org.chromium.chrome.browser.tabmodel.TabModelFilterProvider;
import org.chromium.chrome.browser.tabmodel.TabModelObserver;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorObserver;
import org.chromium.chrome.browser.tasks.TasksSurfaceProperties;
import org.chromium.chrome.browser.tasks.tab_management.TabSwitcher;
import org.chromium.chrome.browser.tasks.tab_management.TabSwitcher.OverviewModeObserver;
import org.chromium.chrome.features.start_surface.StartSurfaceMediator.SecondaryTasksSurfaceInitializer;
import org.chromium.chrome.features.start_surface.StartSurfaceMediator.SurfaceMode;
import org.chromium.chrome.start_surface.R;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.components.prefs.PrefService;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;

/** Tests for {@link StartSurfaceMediator}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class StartSurfaceMediatorUnitTest {
    private PropertyModel mPropertyModel;
    private PropertyModel mSecondaryTasksSurfacePropertyModel;

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
    private ExploreSurfaceCoordinator.FeedSurfaceCreator mFeedSurfaceCreator;
    @Mock
    private FeedSurfaceCoordinator mFeedSurfaceCoordinator;
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
    @Captor
    private ArgumentCaptor<TabModelSelectorObserver> mTabModelSelectorObserverCaptor;
    @Captor
    private ArgumentCaptor<TabModelObserver> mTabModelObserverCaptor;
    @Captor
    private ArgumentCaptor<OverviewModeObserver> mOverviewModeObserverCaptor;
    @Captor
    private ArgumentCaptor<UrlFocusChangeListener> mUrlFocusChangeListenerCaptor;
    @Captor
    private ArgumentCaptor<BrowserControlsStateProvider.Observer>
            mBrowserControlsStateProviderCaptor;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);

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
        doReturn(mSecondaryTasksSurfaceController)
                .when(mSecondaryTasksSurfaceInitializer)
                .initialize();
        doReturn(false).when(mActivityStateChecker).isFinishingOrDestroyed();
        doReturn(mTab).when(mTabModelSelector).getCurrentTab();
    }

    @After
    public void tearDown() {
        mPropertyModel = null;
    }

    @Test
    public void showAndHideNoStartSurface() {
        doReturn(false).when(mTabModelSelector).isIncognitoSelected();

        StartSurfaceMediator mediator =
                createStartSurfaceMediator(SurfaceMode.NO_START_SURFACE, true);
        verify(mTabModelSelector, never()).addObserver(mTabModelSelectorObserverCaptor.capture());
        verify(mMainTabGridController)
                .addOverviewModeObserver(mOverviewModeObserverCaptor.capture());

        mediator.showOverview(false);
        verify(mMainTabGridController).showOverview(eq(false));

        mOverviewModeObserverCaptor.getValue().startedShowing();
        mOverviewModeObserverCaptor.getValue().finishedShowing();

        mediator.hideOverview(true);
        verify(mMainTabGridController).hideOverview(eq(true));

        mOverviewModeObserverCaptor.getValue().startedHiding();
        mOverviewModeObserverCaptor.getValue().finishedHiding();

        // TODO(crbug.com/1020223): Test the other SurfaceMode.NO_START_SURFACE operations.
    }

    @Test
    public void showAndHideTasksOnlySurface() {
        doReturn(false).when(mTabModelSelector).isIncognitoSelected();
        doReturn(mVoiceRecognitionHandler).when(mOmniboxStub).getVoiceRecognitionHandler();
        doReturn(true).when(mVoiceRecognitionHandler).isVoiceSearchEnabled();

        StartSurfaceMediator mediator = createStartSurfaceMediator(SurfaceMode.TASKS_ONLY, false);
        verify(mMainTabGridController)
                .addOverviewModeObserver(mOverviewModeObserverCaptor.capture());

        assertThat(mediator.getStartSurfaceState(), equalTo(StartSurfaceState.NOT_SHOWN));

        mediator.showOverview(false);
        verify(mMainTabGridController).showOverview(eq(false));
        verify(mOmniboxStub).addUrlFocusChangeListener(mUrlFocusChangeListenerCaptor.capture());
        assertThat(mediator.getStartSurfaceState(),
                equalTo(StartSurfaceState.SHOWN_TABSWITCHER_TASKS_ONLY));
        assertThat(mPropertyModel.get(IS_INCOGNITO), equalTo(false));
        assertThat(mPropertyModel.get(IS_VOICE_RECOGNITION_BUTTON_VISIBLE), equalTo(true));
        assertThat(mPropertyModel.get(IS_EXPLORE_SURFACE_VISIBLE), equalTo(false));
        assertThat(mPropertyModel.get(MV_TILES_VISIBLE), equalTo(true));
        assertThat(mPropertyModel.get(IS_SHOWING_OVERVIEW), equalTo(true));
        assertThat(mPropertyModel.get(TRENDY_TERMS_VISIBLE), equalTo(false));

        mOverviewModeObserverCaptor.getValue().startedShowing();
        mOverviewModeObserverCaptor.getValue().finishedShowing();

        mUrlFocusChangeListenerCaptor.getValue().onUrlFocusChange(true);
        assertThat(mPropertyModel.get(IS_FAKE_SEARCH_BOX_VISIBLE), equalTo(false));
        mUrlFocusChangeListenerCaptor.getValue().onUrlFocusChange(false);
        assertThat(mPropertyModel.get(IS_FAKE_SEARCH_BOX_VISIBLE), equalTo(true));
        assertThat(mPropertyModel.get(IS_VOICE_RECOGNITION_BUTTON_VISIBLE), equalTo(true));

        mediator.hideOverview(true);
        verify(mMainTabGridController).hideOverview(eq(true));

        mOverviewModeObserverCaptor.getValue().startedHiding();
        assertThat(mPropertyModel.get(IS_SHOWING_OVERVIEW), equalTo(false));
        verify(mOmniboxStub).removeUrlFocusChangeListener(mUrlFocusChangeListenerCaptor.getValue());

        mOverviewModeObserverCaptor.getValue().finishedHiding();

        // TODO(crbug.com/1020223): Test the other SurfaceMode.TASKS_ONLY operations.
    }

    @Test
    public void showAndHideOmniboxOnlySurface() {
        doReturn(false).when(mTabModelSelector).isIncognitoSelected();
        doReturn(mVoiceRecognitionHandler).when(mOmniboxStub).getVoiceRecognitionHandler();
        doReturn(true).when(mVoiceRecognitionHandler).isVoiceSearchEnabled();

        StartSurfaceMediator mediator = createStartSurfaceMediator(SurfaceMode.OMNIBOX_ONLY, true);
        verify(mMainTabGridController)
                .addOverviewModeObserver(mOverviewModeObserverCaptor.capture());

        assertThat(mediator.getStartSurfaceState(), equalTo(StartSurfaceState.NOT_SHOWN));

        mediator.showOverview(false);
        verify(mMainTabGridController).showOverview(eq(false));
        verify(mOmniboxStub).addUrlFocusChangeListener(mUrlFocusChangeListenerCaptor.capture());
        assertThat(mediator.getStartSurfaceState(),
                equalTo(StartSurfaceState.SHOWN_TABSWITCHER_OMNIBOX_ONLY));
        assertThat(mPropertyModel.get(IS_INCOGNITO), equalTo(false));
        assertThat(mPropertyModel.get(IS_VOICE_RECOGNITION_BUTTON_VISIBLE), equalTo(true));
        assertThat(mPropertyModel.get(IS_EXPLORE_SURFACE_VISIBLE), equalTo(false));
        assertThat(mPropertyModel.get(MV_TILES_VISIBLE), equalTo(false));
        assertThat(mPropertyModel.get(IS_SHOWING_OVERVIEW), equalTo(true));
        assertThat(mPropertyModel.get(TRENDY_TERMS_VISIBLE), equalTo(false));

        mOverviewModeObserverCaptor.getValue().startedShowing();
        mOverviewModeObserverCaptor.getValue().finishedShowing();

        mUrlFocusChangeListenerCaptor.getValue().onUrlFocusChange(true);
        assertThat(mPropertyModel.get(IS_FAKE_SEARCH_BOX_VISIBLE), equalTo(false));
        mUrlFocusChangeListenerCaptor.getValue().onUrlFocusChange(false);
        assertThat(mPropertyModel.get(IS_FAKE_SEARCH_BOX_VISIBLE), equalTo(true));
        assertThat(mPropertyModel.get(IS_VOICE_RECOGNITION_BUTTON_VISIBLE), equalTo(true));

        mediator.hideOverview(true);
        verify(mMainTabGridController).hideOverview(eq(true));

        mOverviewModeObserverCaptor.getValue().startedHiding();
        assertThat(mPropertyModel.get(IS_SHOWING_OVERVIEW), equalTo(false));
        verify(mOmniboxStub).removeUrlFocusChangeListener(mUrlFocusChangeListenerCaptor.getValue());

        mOverviewModeObserverCaptor.getValue().finishedHiding();

        // TODO(crbug.com/1020223): Test the other SurfaceMode.OMNIBOX_ONLY operations.
    }

    @Test
    public void showAndHideTrendyTermsSurface() {
        doReturn(false).when(mTabModelSelector).isIncognitoSelected();
        doReturn(mVoiceRecognitionHandler).when(mOmniboxStub).getVoiceRecognitionHandler();
        doReturn(true).when(mVoiceRecognitionHandler).isVoiceSearchEnabled();

        StartSurfaceMediator mediator = createStartSurfaceMediator(SurfaceMode.TRENDY_TERMS, true);
        verify(mMainTabGridController)
                .addOverviewModeObserver(mOverviewModeObserverCaptor.capture());

        assertThat(mediator.getStartSurfaceState(), equalTo(StartSurfaceState.NOT_SHOWN));

        mediator.showOverview(false);
        verify(mMainTabGridController).showOverview(eq(false));
        verify(mOmniboxStub).addUrlFocusChangeListener(mUrlFocusChangeListenerCaptor.capture());
        assertThat(mediator.getStartSurfaceState(),
                equalTo(StartSurfaceState.SHOWN_TABSWITCHER_TRENDY_TERMS));
        assertThat(mPropertyModel.get(IS_INCOGNITO), equalTo(false));
        assertThat(mPropertyModel.get(IS_VOICE_RECOGNITION_BUTTON_VISIBLE), equalTo(true));
        assertThat(mPropertyModel.get(IS_EXPLORE_SURFACE_VISIBLE), equalTo(false));
        assertThat(mPropertyModel.get(MV_TILES_VISIBLE), equalTo(false));
        assertThat(mPropertyModel.get(IS_SHOWING_OVERVIEW), equalTo(true));
        assertThat(mPropertyModel.get(TRENDY_TERMS_VISIBLE), equalTo(true));

        mOverviewModeObserverCaptor.getValue().startedShowing();
        mOverviewModeObserverCaptor.getValue().finishedShowing();

        mUrlFocusChangeListenerCaptor.getValue().onUrlFocusChange(true);
        assertThat(mPropertyModel.get(IS_FAKE_SEARCH_BOX_VISIBLE), equalTo(false));
        mUrlFocusChangeListenerCaptor.getValue().onUrlFocusChange(false);
        assertThat(mPropertyModel.get(IS_FAKE_SEARCH_BOX_VISIBLE), equalTo(true));
        assertThat(mPropertyModel.get(IS_VOICE_RECOGNITION_BUTTON_VISIBLE), equalTo(true));

        mediator.hideOverview(true);
        verify(mMainTabGridController).hideOverview(eq(true));

        mOverviewModeObserverCaptor.getValue().startedHiding();
        assertThat(mPropertyModel.get(IS_SHOWING_OVERVIEW), equalTo(false));
        verify(mOmniboxStub).removeUrlFocusChangeListener(mUrlFocusChangeListenerCaptor.getValue());

        mOverviewModeObserverCaptor.getValue().finishedHiding();
    }

    @Test
    public void showAndHideSingleSurface() {
        doReturn(false).when(mTabModelSelector).isIncognitoSelected();
        doReturn(mVoiceRecognitionHandler).when(mOmniboxStub).getVoiceRecognitionHandler();
        doReturn(true).when(mVoiceRecognitionHandler).isVoiceSearchEnabled();

        StartSurfaceMediator mediator = createStartSurfaceMediator(SurfaceMode.SINGLE_PANE, false);
        verify(mMainTabGridController)
                .addOverviewModeObserver(mOverviewModeObserverCaptor.capture());

        assertThat(mediator.getStartSurfaceState(), equalTo(StartSurfaceState.NOT_SHOWN));
        // Sets the current StartSurfaceState to SHOWING_START before calling the
        // {@link StartSurfaceMediator#showOverview()}. This is because if the current
        // StartSurfaceState is NOT_SHOWN, the state will be set default to SHOWING_TABSWITCHER in
        // {@link StartSurfaceMediator#showOverview()}.
        mediator.setOverviewState(StartSurfaceState.SHOWING_START);

        mediator.showOverview(false);
        verify(mMainTabGridController).showOverview(eq(false));
        verify(mOmniboxStub).addUrlFocusChangeListener(mUrlFocusChangeListenerCaptor.capture());
        assertThat(mediator.getStartSurfaceState(), equalTo(StartSurfaceState.SHOWN_HOMEPAGE));
        assertThat(mPropertyModel.get(IS_INCOGNITO), equalTo(false));
        assertThat(mPropertyModel.get(IS_VOICE_RECOGNITION_BUTTON_VISIBLE), equalTo(true));
        assertThat(mPropertyModel.get(IS_EXPLORE_SURFACE_VISIBLE), equalTo(true));
        assertThat(mPropertyModel.get(MV_TILES_VISIBLE), equalTo(true));
        assertThat(mPropertyModel.get(IS_SHOWING_OVERVIEW), equalTo(true));
        assertThat(mPropertyModel.get(TRENDY_TERMS_VISIBLE), equalTo(false));

        mOverviewModeObserverCaptor.getValue().startedShowing();
        mOverviewModeObserverCaptor.getValue().finishedShowing();

        mUrlFocusChangeListenerCaptor.getValue().onUrlFocusChange(true);
        assertThat(mPropertyModel.get(IS_FAKE_SEARCH_BOX_VISIBLE), equalTo(false));
        mUrlFocusChangeListenerCaptor.getValue().onUrlFocusChange(false);
        assertThat(mPropertyModel.get(IS_FAKE_SEARCH_BOX_VISIBLE), equalTo(true));
        assertThat(mPropertyModel.get(IS_VOICE_RECOGNITION_BUTTON_VISIBLE), equalTo(true));

        mediator.hideOverview(true);
        verify(mMainTabGridController).hideOverview(eq(true));

        mOverviewModeObserverCaptor.getValue().startedHiding();
        assertThat(mPropertyModel.get(IS_SHOWING_OVERVIEW), equalTo(false));
        verify(mOmniboxStub).removeUrlFocusChangeListener(mUrlFocusChangeListenerCaptor.getValue());

        mOverviewModeObserverCaptor.getValue().finishedHiding();

        // TODO(crbug.com/1020223): Test the other SurfaceMode.SINGLE_PANE operations.
    }

    @Test
    public void showAndHideSingleSurfaceWithoutMVTiles() {
        doReturn(false).when(mTabModelSelector).isIncognitoSelected();
        doReturn(mVoiceRecognitionHandler).when(mOmniboxStub).getVoiceRecognitionHandler();
        doReturn(true).when(mVoiceRecognitionHandler).isVoiceSearchEnabled();

        StartSurfaceMediator mediator = createStartSurfaceMediator(SurfaceMode.SINGLE_PANE, true);
        verify(mMainTabGridController)
                .addOverviewModeObserver(mOverviewModeObserverCaptor.capture());

        assertThat(mediator.getStartSurfaceState(), equalTo(StartSurfaceState.NOT_SHOWN));
        // Sets the current StartSurfaceState to SHOWING_START before calling the
        // {@link StartSurfaceMediator#showOverview()}. This is because if the current
        // StartSurfaceState is NOT_SHOWN, the state will be set default to SHOWING_TABSWITCHER in
        // {@link StartSurfaceMediator#showOverview()}.
        mediator.setOverviewState(StartSurfaceState.SHOWING_START);

        mediator.showOverview(false);
        verify(mMainTabGridController).showOverview(eq(false));
        verify(mOmniboxStub).addUrlFocusChangeListener(mUrlFocusChangeListenerCaptor.capture());
        assertThat(mediator.getStartSurfaceState(), equalTo(StartSurfaceState.SHOWN_HOMEPAGE));
        assertThat(mPropertyModel.get(IS_INCOGNITO), equalTo(false));
        assertThat(mPropertyModel.get(IS_VOICE_RECOGNITION_BUTTON_VISIBLE), equalTo(true));
        assertThat(mPropertyModel.get(IS_EXPLORE_SURFACE_VISIBLE), equalTo(true));
        assertThat(mPropertyModel.get(MV_TILES_VISIBLE), equalTo(false));
        assertThat(mPropertyModel.get(IS_SHOWING_OVERVIEW), equalTo(true));
        assertThat(mPropertyModel.get(TRENDY_TERMS_VISIBLE), equalTo(false));

        mOverviewModeObserverCaptor.getValue().startedShowing();
        mOverviewModeObserverCaptor.getValue().finishedShowing();

        mUrlFocusChangeListenerCaptor.getValue().onUrlFocusChange(true);
        assertThat(mPropertyModel.get(IS_FAKE_SEARCH_BOX_VISIBLE), equalTo(false));
        mUrlFocusChangeListenerCaptor.getValue().onUrlFocusChange(false);
        assertThat(mPropertyModel.get(IS_FAKE_SEARCH_BOX_VISIBLE), equalTo(true));
        assertThat(mPropertyModel.get(IS_VOICE_RECOGNITION_BUTTON_VISIBLE), equalTo(true));

        mediator.hideOverview(true);
        verify(mMainTabGridController).hideOverview(eq(true));

        mOverviewModeObserverCaptor.getValue().startedHiding();
        assertThat(mPropertyModel.get(IS_SHOWING_OVERVIEW), equalTo(false));
        verify(mOmniboxStub).removeUrlFocusChangeListener(mUrlFocusChangeListenerCaptor.getValue());

        mOverviewModeObserverCaptor.getValue().finishedHiding();

        // TODO(crbug.com/1020223): Test the other SurfaceMode.SINGLE_PANE operations.
    }

    @Test
    public void showAndHideTwoPanesSurface() {
        doReturn(false).when(mTabModelSelector).isIncognitoSelected();
        doReturn(mVoiceRecognitionHandler).when(mOmniboxStub).getVoiceRecognitionHandler();
        doReturn(true).when(mVoiceRecognitionHandler).isVoiceSearchEnabled();

        StartSurfaceMediator mediator = createStartSurfaceMediator(SurfaceMode.TWO_PANES, false);
        verify(mMainTabGridController)
                .addOverviewModeObserver(mOverviewModeObserverCaptor.capture());

        assertThat(mediator.getStartSurfaceState(), equalTo(StartSurfaceState.NOT_SHOWN));
        assertTrue(mPropertyModel.get(BOTTOM_BAR_CLICKLISTENER) != null);

        mediator.showOverview(false);
        verify(mMainTabGridController).showOverview(eq(false));
        verify(mOmniboxStub).addUrlFocusChangeListener(mUrlFocusChangeListenerCaptor.capture());
        assertThat(mediator.getStartSurfaceState(),
                equalTo(StartSurfaceState.SHOWN_TABSWITCHER_TWO_PANES));
        assertThat(mPropertyModel.get(IS_INCOGNITO), equalTo(false));
        assertThat(mPropertyModel.get(IS_VOICE_RECOGNITION_BUTTON_VISIBLE), equalTo(true));
        assertThat(mPropertyModel.get(IS_EXPLORE_SURFACE_VISIBLE), equalTo(false));
        assertThat(mPropertyModel.get(IS_BOTTOM_BAR_VISIBLE), equalTo(true));
        assertThat(mPropertyModel.get(MV_TILES_VISIBLE), equalTo(true));
        assertThat(mPropertyModel.get(IS_SHOWING_OVERVIEW), equalTo(true));
        assertThat(mPropertyModel.get(TRENDY_TERMS_VISIBLE), equalTo(false));

        mOverviewModeObserverCaptor.getValue().startedShowing();
        mOverviewModeObserverCaptor.getValue().finishedShowing();

        mUrlFocusChangeListenerCaptor.getValue().onUrlFocusChange(true);
        assertThat(mPropertyModel.get(IS_FAKE_SEARCH_BOX_VISIBLE), equalTo(false));
        mUrlFocusChangeListenerCaptor.getValue().onUrlFocusChange(false);
        assertThat(mPropertyModel.get(IS_FAKE_SEARCH_BOX_VISIBLE), equalTo(true));
        assertThat(mPropertyModel.get(IS_VOICE_RECOGNITION_BUTTON_VISIBLE), equalTo(true));

        mediator.hideOverview(true);
        verify(mMainTabGridController).hideOverview(eq(true));

        mOverviewModeObserverCaptor.getValue().startedHiding();
        assertThat(mPropertyModel.get(IS_SHOWING_OVERVIEW), equalTo(false));
        verify(mOmniboxStub).removeUrlFocusChangeListener(mUrlFocusChangeListenerCaptor.getValue());

        mOverviewModeObserverCaptor.getValue().finishedHiding();
    }

    @Test
    public void switchBetweenHomeAndExplorePane() {
        doReturn(false).when(mTabModelSelector).isIncognitoSelected();
        doReturn(mVoiceRecognitionHandler).when(mOmniboxStub).getVoiceRecognitionHandler();
        doReturn(true).when(mVoiceRecognitionHandler).isVoiceSearchEnabled();

        StartSurfaceMediator mediator = createStartSurfaceMediator(SurfaceMode.TWO_PANES, false);
        verify(mMainTabGridController)
                .addOverviewModeObserver(mOverviewModeObserverCaptor.capture());
        mediator.showOverview(false);
        mOverviewModeObserverCaptor.getValue().startedShowing();
        mOverviewModeObserverCaptor.getValue().finishedShowing();

        assertThat(mPropertyModel.get(IS_EXPLORE_SURFACE_VISIBLE), equalTo(false));
        assertThat(mPropertyModel.get(BOTTOM_BAR_SELECTED_TAB_POSITION), equalTo(0));

        mPropertyModel.get(BOTTOM_BAR_CLICKLISTENER).onExploreButtonClicked();
        assertThat(mPropertyModel.get(IS_EXPLORE_SURFACE_VISIBLE), equalTo(true));
        assertThat(mPropertyModel.get(BOTTOM_BAR_SELECTED_TAB_POSITION), equalTo(1));
        assertThat(ReturnToStartSurfaceUtil.shouldShowExploreSurface(), equalTo(true));

        mPropertyModel.get(BOTTOM_BAR_CLICKLISTENER).onHomeButtonClicked();
        assertThat(mPropertyModel.get(IS_EXPLORE_SURFACE_VISIBLE), equalTo(false));
        assertThat(mPropertyModel.get(BOTTOM_BAR_SELECTED_TAB_POSITION), equalTo(0));
        assertThat(ReturnToStartSurfaceUtil.shouldShowExploreSurface(), equalTo(false));
    }

    @Test
    public void showExplorePaneByDefault() {
        doReturn(false).when(mTabModelSelector).isIncognitoSelected();
        doReturn(mVoiceRecognitionHandler).when(mOmniboxStub).getVoiceRecognitionHandler();
        doReturn(true).when(mVoiceRecognitionHandler).isVoiceSearchEnabled();

        StartSurfaceMediator mediator = createStartSurfaceMediator(SurfaceMode.TWO_PANES, false);
        verify(mMainTabGridController)
                .addOverviewModeObserver(mOverviewModeObserverCaptor.capture());

        mPropertyModel.set(BOTTOM_BAR_SELECTED_TAB_POSITION, 1);
        ReturnToStartSurfaceUtil.setExploreSurfaceVisibleLast(true);
        mediator.showOverview(false);
        mOverviewModeObserverCaptor.getValue().startedShowing();
        mOverviewModeObserverCaptor.getValue().finishedShowing();

        assertThat(mPropertyModel.get(IS_EXPLORE_SURFACE_VISIBLE), equalTo(true));
        assertThat(mPropertyModel.get(IS_SHOWING_OVERVIEW), equalTo(true));
    }

    @Test
    public void incognitoTwoPanesSurface() {
        doReturn(false).when(mTabModelSelector).isIncognitoSelected();
        doReturn(mVoiceRecognitionHandler).when(mOmniboxStub).getVoiceRecognitionHandler();
        doReturn(true).when(mVoiceRecognitionHandler).isVoiceSearchEnabled();

        StartSurfaceMediator mediator = createStartSurfaceMediator(SurfaceMode.TWO_PANES, false);
        verify(mMainTabGridController)
                .addOverviewModeObserver(mOverviewModeObserverCaptor.capture());
        mediator.showOverview(false);
        mOverviewModeObserverCaptor.getValue().startedShowing();
        mOverviewModeObserverCaptor.getValue().finishedShowing();

        verify(mTabModelSelector).addObserver(mTabModelSelectorObserverCaptor.capture());
        assertThat(mPropertyModel.get(IS_INCOGNITO), equalTo(false));
        assertThat(mPropertyModel.get(IS_EXPLORE_SURFACE_VISIBLE), equalTo(false));
        assertThat(mPropertyModel.get(MV_TILES_VISIBLE), equalTo(true));
        assertThat(mPropertyModel.get(IS_BOTTOM_BAR_VISIBLE), equalTo(true));
        assertThat(mPropertyModel.get(IS_SHOWING_OVERVIEW), equalTo(true));

        doReturn(true).when(mTabModelSelector).isIncognitoSelected();
        mTabModelSelector.selectModel(true);
        mTabModelSelectorObserverCaptor.getValue().onTabModelSelected(
                mIncognitoTabModel, mNormalTabModel);

        assertThat(mPropertyModel.get(IS_INCOGNITO), equalTo(true));
        assertThat(mPropertyModel.get(IS_EXPLORE_SURFACE_VISIBLE), equalTo(false));
        assertThat(mPropertyModel.get(MV_TILES_VISIBLE), equalTo(false));
        assertThat(mPropertyModel.get(IS_BOTTOM_BAR_VISIBLE), equalTo(false));
        assertThat(mPropertyModel.get(IS_SHOWING_OVERVIEW), equalTo(true));
    }

    // TODO(crbug.com/1020223): Test SurfaceMode.SINGLE_PANE and SurfaceMode.TWO_PANES modes.
    @Test
    public void hideTabCarouselWithNoTabs() {
        doReturn(false).when(mTabModelSelector).isIncognitoSelected();
        doReturn(mVoiceRecognitionHandler).when(mOmniboxStub).getVoiceRecognitionHandler();
        doReturn(true).when(mVoiceRecognitionHandler).isVoiceSearchEnabled();

        StartSurfaceMediator mediator = createStartSurfaceMediator(SurfaceMode.SINGLE_PANE, false);

        doReturn(0).when(mNormalTabModel).getCount();
        mediator.setOverviewState(StartSurfaceState.SHOWN_HOMEPAGE);
        mediator.showOverview(false);
        mediator.setOverviewState(StartSurfaceState.SHOWN_HOMEPAGE);
        verify(mNormalTabModel).addObserver(mTabModelObserverCaptor.capture());
        assertThat(mPropertyModel.get(IS_SHOWING_OVERVIEW), equalTo(true));
        assertThat(mPropertyModel.get(IS_TAB_CAROUSEL_VISIBLE), equalTo(false));
    }

    @Test
    public void hideTabCarouselWhenClosingLastTab() {
        doReturn(false).when(mTabModelSelector).isIncognitoSelected();
        doReturn(mVoiceRecognitionHandler).when(mOmniboxStub).getVoiceRecognitionHandler();
        doReturn(true).when(mVoiceRecognitionHandler).isVoiceSearchEnabled();

        StartSurfaceMediator mediator = createStartSurfaceMediator(SurfaceMode.SINGLE_PANE, false);

        doReturn(2).when(mNormalTabModel).getCount();
        mediator.setOverviewState(StartSurfaceState.SHOWING_HOMEPAGE);
        mediator.showOverview(false);
        mediator.setOverviewState(StartSurfaceState.SHOWN_HOMEPAGE);
        verify(mNormalTabModel).addObserver(mTabModelObserverCaptor.capture());
        assertThat(mPropertyModel.get(IS_SHOWING_OVERVIEW), equalTo(true));
        assertThat(mPropertyModel.get(IS_TAB_CAROUSEL_VISIBLE), equalTo(true));

        mTabModelObserverCaptor.getValue().willCloseTab(mock(Tab.class), false);
        assertThat(mPropertyModel.get(IS_SHOWING_OVERVIEW), equalTo(true));
        assertThat(mPropertyModel.get(IS_TAB_CAROUSEL_VISIBLE), equalTo(true));

        doReturn(1).when(mNormalTabModel).getCount();
        mTabModelObserverCaptor.getValue().willCloseTab(mock(Tab.class), false);
        assertThat(mPropertyModel.get(IS_SHOWING_OVERVIEW), equalTo(true));
        assertThat(mPropertyModel.get(IS_TAB_CAROUSEL_VISIBLE), equalTo(false));
    }

    @Test
    public void reshowTabCarouselWhenTabClosureUndone() {
        doReturn(false).when(mTabModelSelector).isIncognitoSelected();
        doReturn(mVoiceRecognitionHandler).when(mOmniboxStub).getVoiceRecognitionHandler();
        doReturn(true).when(mVoiceRecognitionHandler).isVoiceSearchEnabled();

        StartSurfaceMediator mediator = createStartSurfaceMediator(SurfaceMode.SINGLE_PANE, false);

        doReturn(1).when(mNormalTabModel).getCount();

        mediator.setSecondaryTasksSurfacePropertyModel(mSecondaryTasksSurfacePropertyModel);
        mediator.setOverviewState(StartSurfaceState.SHOWING_HOMEPAGE);
        mediator.showOverview(false);

        verify(mNormalTabModel).addObserver(mTabModelObserverCaptor.capture());

        mediator.setOverviewState(StartSurfaceState.SHOWN_HOMEPAGE);
        mTabModelObserverCaptor.getValue().willCloseTab(mock(Tab.class), false);
        assertThat(mPropertyModel.get(IS_SHOWING_OVERVIEW), equalTo(true));
        assertThat(mPropertyModel.get(IS_TAB_CAROUSEL_VISIBLE), equalTo(false));

        mTabModelObserverCaptor.getValue().tabClosureUndone(mock(Tab.class));
        assertThat(mPropertyModel.get(IS_SHOWING_OVERVIEW), equalTo(true));
        assertThat(mPropertyModel.get(IS_TAB_CAROUSEL_VISIBLE), equalTo(true));

        doReturn(2).when(mNormalTabModel).getCount();
        mediator.setOverviewState(StartSurfaceState.SHOWN_TABSWITCHER);
        mTabModelObserverCaptor.getValue().willCloseTab(mock(Tab.class), false);
        mTabModelObserverCaptor.getValue().tabClosureUndone(mock(Tab.class));
        doReturn(0).when(mNormalTabModel).getCount();
        mTabModelObserverCaptor.getValue().willCloseTab(mock(Tab.class), false);
        assertThat(mPropertyModel.get(IS_TAB_CAROUSEL_VISIBLE), equalTo(false));
        assertThat(mPropertyModel.get(IS_SECONDARY_SURFACE_VISIBLE), equalTo(true));
    }

    @Test
    public void pendingTabModelObserverWithBothShowOverviewAndHideBeforeTabModelInitialization() {
        doReturn(false).when(mTabModelSelector).isIncognitoSelected();
        doReturn(mVoiceRecognitionHandler).when(mOmniboxStub).getVoiceRecognitionHandler();
        doReturn(true).when(mVoiceRecognitionHandler).isVoiceSearchEnabled();

        mTabModels.clear();
        StartSurfaceMediator mediator = createStartSurfaceMediator(SurfaceMode.SINGLE_PANE, false);
        verify(mTabModelSelector).addObserver(mTabModelSelectorObserverCaptor.capture());

        mediator.setOverviewState(StartSurfaceState.SHOWN_HOMEPAGE);
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
        StartSurfaceMediator mediator = createStartSurfaceMediator(SurfaceMode.SINGLE_PANE, false);
        verify(mTabModelSelector).addObserver(mTabModelSelectorObserverCaptor.capture());

        mediator.setOverviewState(StartSurfaceState.SHOWN_HOMEPAGE);
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
        StartSurfaceMediator mediator = createStartSurfaceMediator(SurfaceMode.SINGLE_PANE, false);
        verify(mTabModelSelector).addObserver(mTabModelSelectorObserverCaptor.capture());

        mTabModels.add(mNormalTabModel);
        mTabModels.add(mIncognitoTabModel);
        when(mTabModelSelector.getTabModelFilterProvider()).thenReturn(mTabModelFilterProvider);
        when(mTabModelFilterProvider.getTabModelFilter(false)).thenReturn(mTabModelFilter);
        when(mTabModelFilterProvider.getTabModelFilter(true)).thenReturn(mTabModelFilter);
        mTabModelSelectorObserverCaptor.getValue().onChange();
        verify(mTabModelSelector).removeObserver(mTabModelSelectorObserverCaptor.capture());
        verify(mNormalTabModel, never()).addObserver(mTabModelObserverCaptor.capture());

        mediator.setOverviewState(StartSurfaceState.SHOWN_HOMEPAGE);
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

        StartSurfaceMediator mediator = createStartSurfaceMediator(SurfaceMode.SINGLE_PANE, false);
        verify(mNormalTabModel, never()).addObserver(mTabModelObserverCaptor.capture());

        mediator.setOverviewState(StartSurfaceState.SHOWING_HOMEPAGE);
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

        StartSurfaceMediator mediator = createStartSurfaceMediator(SurfaceMode.SINGLE_PANE, false);

        verify(mTabModelSelector, never()).addObserver(mTabModelSelectorObserverCaptor.capture());

        mediator.setOverviewState(StartSurfaceState.SHOWN_HOMEPAGE);
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

        StartSurfaceMediator mediator = createStartSurfaceMediator(SurfaceMode.SINGLE_PANE, false);
        assertThat(mediator.getStartSurfaceState(), equalTo(StartSurfaceState.NOT_SHOWN));

        doReturn(2).when(mNormalTabModel).getCount();
        mediator.setOverviewState(StartSurfaceState.SHOWING_HOMEPAGE);
        mediator.showOverview(false);
        assertThat(mediator.getStartSurfaceState(), equalTo(StartSurfaceState.SHOWN_HOMEPAGE));
        assertThat(mPropertyModel.get(IS_SHOWING_OVERVIEW), equalTo(true));
        assertThat(mPropertyModel.get(IS_INCOGNITO), equalTo(false));
        assertThat(mPropertyModel.get(IS_EXPLORE_SURFACE_VISIBLE), equalTo(true));
        assertThat(mPropertyModel.get(MV_TILES_VISIBLE), equalTo(true));
        assertThat(mPropertyModel.get(IS_TAB_CAROUSEL_VISIBLE), equalTo(true));
        assertThat(mPropertyModel.get(IS_SECONDARY_SURFACE_VISIBLE), equalTo(false));

        mediator.setSecondaryTasksSurfacePropertyModel(mSecondaryTasksSurfacePropertyModel);
        mediator.onClick(mock(View.class));
        assertThat(mediator.getStartSurfaceState(), equalTo(StartSurfaceState.SHOWN_TABSWITCHER));
        assertThat(mPropertyModel.get(IS_SHOWING_OVERVIEW), equalTo(true));
        assertThat(mPropertyModel.get(IS_INCOGNITO), equalTo(false));
        assertThat(mPropertyModel.get(IS_EXPLORE_SURFACE_VISIBLE), equalTo(false));
        assertThat(mPropertyModel.get(MV_TILES_VISIBLE), equalTo(false));
        assertThat(mPropertyModel.get(IS_TAB_CAROUSEL_VISIBLE), equalTo(false));
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

        StartSurfaceMediator mediator = createStartSurfaceMediator(SurfaceMode.SINGLE_PANE, false);
        mediator.setSecondaryTasksSurfacePropertyModel(mSecondaryTasksSurfacePropertyModel);

        assertThat(mediator.getStartSurfaceState(), equalTo(StartSurfaceState.NOT_SHOWN));

        doReturn(2).when(mNormalTabModel).getCount();
        mediator.setOverviewState(StartSurfaceState.SHOWING_HOMEPAGE);
        mediator.showOverview(false);
        assertThat(mediator.getStartSurfaceState(), equalTo(StartSurfaceState.SHOWN_HOMEPAGE));
        assertThat(mPropertyModel.get(IS_SHOWING_OVERVIEW), equalTo(true));
        assertThat(mPropertyModel.get(IS_INCOGNITO), equalTo(true));
        assertThat(mPropertyModel.get(IS_EXPLORE_SURFACE_VISIBLE), equalTo(false));
        assertThat(mPropertyModel.get(MV_TILES_VISIBLE), equalTo(false));
        assertThat(mPropertyModel.get(IS_TAB_CAROUSEL_VISIBLE), equalTo(false));
        assertThat(mPropertyModel.get(IS_SECONDARY_SURFACE_VISIBLE), equalTo(true));
        assertThat(
                mSecondaryTasksSurfacePropertyModel.get(IS_FAKE_SEARCH_BOX_VISIBLE), equalTo(true));
        assertThat(mSecondaryTasksSurfacePropertyModel.get(IS_VOICE_RECOGNITION_BUTTON_VISIBLE),
                equalTo(false));
        assertThat(mSecondaryTasksSurfacePropertyModel.get(IS_INCOGNITO), equalTo(true));

        mediator.setOverviewState(StartSurfaceState.SHOWN_TABSWITCHER);
        assertThat(mediator.getStartSurfaceState(), equalTo(StartSurfaceState.SHOWN_TABSWITCHER));
        assertThat(mPropertyModel.get(IS_SHOWING_OVERVIEW), equalTo(true));
        assertThat(mPropertyModel.get(IS_INCOGNITO), equalTo(true));
        assertThat(mPropertyModel.get(IS_EXPLORE_SURFACE_VISIBLE), equalTo(false));
        assertThat(mPropertyModel.get(MV_TILES_VISIBLE), equalTo(false));
        assertThat(mPropertyModel.get(IS_TAB_CAROUSEL_VISIBLE), equalTo(false));
        assertThat(mPropertyModel.get(IS_SECONDARY_SURFACE_VISIBLE), equalTo(true));
        assertThat(mSecondaryTasksSurfacePropertyModel.get(IS_FAKE_SEARCH_BOX_VISIBLE),
                equalTo(false));
        assertThat(mSecondaryTasksSurfacePropertyModel.get(IS_INCOGNITO), equalTo(true));

        mediator.setOverviewState(StartSurfaceState.SHOWN_HOMEPAGE);
        assertThat(mediator.getStartSurfaceState(), equalTo(StartSurfaceState.SHOWN_HOMEPAGE));
        assertThat(mPropertyModel.get(IS_SHOWING_OVERVIEW), equalTo(true));
        assertThat(mPropertyModel.get(IS_INCOGNITO), equalTo(true));
        assertThat(mPropertyModel.get(IS_EXPLORE_SURFACE_VISIBLE), equalTo(false));
        assertThat(mPropertyModel.get(MV_TILES_VISIBLE), equalTo(false));
        assertThat(mPropertyModel.get(IS_TAB_CAROUSEL_VISIBLE), equalTo(false));
        assertThat(mPropertyModel.get(IS_SECONDARY_SURFACE_VISIBLE), equalTo(true));
        assertThat(
                mSecondaryTasksSurfacePropertyModel.get(IS_FAKE_SEARCH_BOX_VISIBLE), equalTo(true));
        assertThat(mSecondaryTasksSurfacePropertyModel.get(IS_VOICE_RECOGNITION_BUTTON_VISIBLE),
                equalTo(false));
        assertThat(mSecondaryTasksSurfacePropertyModel.get(IS_INCOGNITO), equalTo(true));

        mediator.hideOverview(false);
        mediator.startedHiding();
        assertThat(mediator.getStartSurfaceState(), equalTo(StartSurfaceState.NOT_SHOWN));
        assertThat(mPropertyModel.get(IS_SECONDARY_SURFACE_VISIBLE), equalTo(false));
    }

    @Test
    public void overviewModeIncognitoModeTaskOnly() {
        doReturn(true).when(mTabModelSelector).isIncognitoSelected();
        doReturn(mVoiceRecognitionHandler).when(mOmniboxStub).getVoiceRecognitionHandler();
        doReturn(true).when(mVoiceRecognitionHandler).isVoiceSearchEnabled();

        StartSurfaceMediator mediator = createStartSurfaceMediator(SurfaceMode.TASKS_ONLY, false);
        assertThat(mediator.getStartSurfaceState(), equalTo(StartSurfaceState.NOT_SHOWN));

        mediator.showOverview(false);
        assertThat(mediator.getStartSurfaceState(),
                equalTo(StartSurfaceState.SHOWN_TABSWITCHER_TASKS_ONLY));
        assertThat(mPropertyModel.get(IS_SHOWING_OVERVIEW), equalTo(true));
        assertThat(mPropertyModel.get(IS_INCOGNITO), equalTo(true));
        assertThat(mPropertyModel.get(IS_EXPLORE_SURFACE_VISIBLE), equalTo(false));
        assertThat(mPropertyModel.get(MV_TILES_VISIBLE), equalTo(false));
        assertThat(mPropertyModel.get(IS_TAB_CAROUSEL_VISIBLE), equalTo(false));
        assertThat(mPropertyModel.get(IS_SECONDARY_SURFACE_VISIBLE), equalTo(false));
    }

    @Test
    public void overviewModeSwitchToIncognitoModeAndBackSinglePane() {
        doReturn(false).when(mTabModelSelector).isIncognitoSelected();
        doReturn(mVoiceRecognitionHandler).when(mOmniboxStub).getVoiceRecognitionHandler();
        doReturn(true).when(mVoiceRecognitionHandler).isVoiceSearchEnabled();

        StartSurfaceMediator mediator = createStartSurfaceMediator(SurfaceMode.SINGLE_PANE, false);
        mediator.setSecondaryTasksSurfacePropertyModel(mSecondaryTasksSurfacePropertyModel);
        assertThat(mediator.getStartSurfaceState(), equalTo(StartSurfaceState.NOT_SHOWN));

        doReturn(2).when(mNormalTabModel).getCount();
        mediator.setOverviewState(StartSurfaceState.SHOWING_HOMEPAGE);
        mediator.showOverview(false);
        verify(mTabModelSelector).addObserver(mTabModelSelectorObserverCaptor.capture());
        assertThat(mediator.getStartSurfaceState(), equalTo(StartSurfaceState.SHOWN_HOMEPAGE));
        assertThat(mPropertyModel.get(IS_SHOWING_OVERVIEW), equalTo(true));
        assertThat(mPropertyModel.get(IS_INCOGNITO), equalTo(false));
        assertThat(mPropertyModel.get(IS_EXPLORE_SURFACE_VISIBLE), equalTo(true));
        assertThat(mPropertyModel.get(MV_TILES_VISIBLE), equalTo(true));
        assertThat(mPropertyModel.get(IS_TAB_CAROUSEL_VISIBLE), equalTo(true));
        assertThat(mPropertyModel.get(IS_SECONDARY_SURFACE_VISIBLE), equalTo(false));

        doReturn(true).when(mTabModelSelector).isIncognitoSelected();
        mTabModelSelector.selectModel(true);
        mTabModelSelectorObserverCaptor.getValue().onTabModelSelected(
                mIncognitoTabModel, mNormalTabModel);

        assertThat(mediator.getStartSurfaceState(), equalTo(StartSurfaceState.SHOWN_HOMEPAGE));
        assertThat(mPropertyModel.get(IS_SHOWING_OVERVIEW), equalTo(true));
        assertThat(mPropertyModel.get(IS_INCOGNITO), equalTo(true));
        assertThat(mPropertyModel.get(IS_EXPLORE_SURFACE_VISIBLE), equalTo(false));
        assertThat(mPropertyModel.get(MV_TILES_VISIBLE), equalTo(false));
        assertThat(mPropertyModel.get(IS_TAB_CAROUSEL_VISIBLE), equalTo(false));
        assertThat(mPropertyModel.get(IS_SECONDARY_SURFACE_VISIBLE), equalTo(true));
        assertThat(
                mSecondaryTasksSurfacePropertyModel.get(IS_FAKE_SEARCH_BOX_VISIBLE), equalTo(true));
        assertThat(mSecondaryTasksSurfacePropertyModel.get(IS_VOICE_RECOGNITION_BUTTON_VISIBLE),
                equalTo(false));
        assertThat(mSecondaryTasksSurfacePropertyModel.get(IS_INCOGNITO), equalTo(true));

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
        assertThat(mPropertyModel.get(IS_SECONDARY_SURFACE_VISIBLE), equalTo(false));
    }

    @Test
    public void activityIsFinishingOrDestroyedSinglePane() {
        doReturn(false).when(mTabModelSelector).isIncognitoSelected();
        doReturn(mVoiceRecognitionHandler).when(mOmniboxStub).getVoiceRecognitionHandler();
        doReturn(true).when(mVoiceRecognitionHandler).isVoiceSearchEnabled();

        StartSurfaceMediator mediator = createStartSurfaceMediator(SurfaceMode.SINGLE_PANE, false);
        assertThat(mediator.getStartSurfaceState(), equalTo(StartSurfaceState.NOT_SHOWN));

        doReturn(2).when(mNormalTabModel).getCount();
        doReturn(true).when(mActivityStateChecker).isFinishingOrDestroyed();
        mediator.setOverviewState(StartSurfaceState.SHOWING_HOMEPAGE);
        mediator.showOverview(false);
        assertThat(mediator.getStartSurfaceState(), equalTo(StartSurfaceState.SHOWN_HOMEPAGE));
        assertThat(mPropertyModel.get(IS_SHOWING_OVERVIEW), equalTo(true));
        assertThat(mPropertyModel.get(IS_INCOGNITO), equalTo(false));
        assertThat(mPropertyModel.get(IS_EXPLORE_SURFACE_VISIBLE), equalTo(true));
        assertThat(mPropertyModel.get(MV_TILES_VISIBLE), equalTo(true));
        assertThat(mPropertyModel.get(IS_TAB_CAROUSEL_VISIBLE), equalTo(true));
        assertThat(mPropertyModel.get(IS_SECONDARY_SURFACE_VISIBLE), equalTo(false));
        assertThat(mPropertyModel.get(FEED_SURFACE_COORDINATOR), equalTo(null));

        mediator.setSecondaryTasksSurfacePropertyModel(mSecondaryTasksSurfacePropertyModel);
        mediator.onClick(mock(View.class));
        assertThat(mediator.getStartSurfaceState(), equalTo(StartSurfaceState.SHOWN_TABSWITCHER));
        assertThat(mPropertyModel.get(IS_SHOWING_OVERVIEW), equalTo(true));
        assertThat(mPropertyModel.get(IS_INCOGNITO), equalTo(false));
        assertThat(mPropertyModel.get(IS_EXPLORE_SURFACE_VISIBLE), equalTo(false));
        assertThat(mPropertyModel.get(MV_TILES_VISIBLE), equalTo(false));
        assertThat(mPropertyModel.get(IS_TAB_CAROUSEL_VISIBLE), equalTo(false));
        assertThat(mPropertyModel.get(IS_SECONDARY_SURFACE_VISIBLE), equalTo(true));
        assertThat(mSecondaryTasksSurfacePropertyModel.get(IS_FAKE_SEARCH_BOX_VISIBLE),
                equalTo(false));
        assertThat(mSecondaryTasksSurfacePropertyModel.get(IS_INCOGNITO), equalTo(false));
        assertThat(mPropertyModel.get(FEED_SURFACE_COORDINATOR), equalTo(null));

        mediator.onBackPressed();
        assertThat(mediator.getStartSurfaceState(), equalTo(StartSurfaceState.SHOWN_HOMEPAGE));
        assertThat(mPropertyModel.get(IS_SHOWING_OVERVIEW), equalTo(true));
        assertThat(mPropertyModel.get(IS_INCOGNITO), equalTo(false));
        assertThat(mPropertyModel.get(IS_EXPLORE_SURFACE_VISIBLE), equalTo(true));
        assertThat(mPropertyModel.get(MV_TILES_VISIBLE), equalTo(true));
        assertThat(mPropertyModel.get(IS_TAB_CAROUSEL_VISIBLE), equalTo(true));
        assertThat(mPropertyModel.get(IS_SECONDARY_SURFACE_VISIBLE), equalTo(false));
        assertThat(mPropertyModel.get(FEED_SURFACE_COORDINATOR), equalTo(null));

        mediator.startedHiding();
        assertThat(mediator.getStartSurfaceState(), equalTo(StartSurfaceState.NOT_SHOWN));
        assertThat(mPropertyModel.get(IS_SECONDARY_SURFACE_VISIBLE), equalTo(false));
    }

    @Test
    public void overviewModeSwitchToIncognitoModeAndBackTasksOnly() {
        doReturn(false).when(mTabModelSelector).isIncognitoSelected();
        doReturn(mVoiceRecognitionHandler).when(mOmniboxStub).getVoiceRecognitionHandler();
        doReturn(true).when(mVoiceRecognitionHandler).isVoiceSearchEnabled();

        StartSurfaceMediator mediator = createStartSurfaceMediator(SurfaceMode.TASKS_ONLY, false);
        mediator.setSecondaryTasksSurfacePropertyModel(mSecondaryTasksSurfacePropertyModel);

        assertThat(mediator.getStartSurfaceState(), equalTo(StartSurfaceState.NOT_SHOWN));

        doReturn(2).when(mNormalTabModel).getCount();
        mediator.showOverview(false);
        verify(mTabModelSelector).addObserver(mTabModelSelectorObserverCaptor.capture());

        assertThat(mediator.getStartSurfaceState(),
                equalTo(StartSurfaceState.SHOWN_TABSWITCHER_TASKS_ONLY));
        assertThat(mPropertyModel.get(IS_SHOWING_OVERVIEW), equalTo(true));
        assertThat(mPropertyModel.get(IS_INCOGNITO), equalTo(false));
        assertThat(mPropertyModel.get(IS_EXPLORE_SURFACE_VISIBLE), equalTo(false));
        assertThat(mPropertyModel.get(MV_TILES_VISIBLE), equalTo(true));
        assertThat(mPropertyModel.get(IS_TAB_CAROUSEL_VISIBLE), equalTo(false));
        assertThat(mPropertyModel.get(IS_SECONDARY_SURFACE_VISIBLE), equalTo(false));

        doReturn(true).when(mTabModelSelector).isIncognitoSelected();
        mTabModelSelector.selectModel(true);
        mTabModelSelectorObserverCaptor.getValue().onTabModelSelected(
                mIncognitoTabModel, mNormalTabModel);

        assertThat(mediator.getStartSurfaceState(),
                equalTo(StartSurfaceState.SHOWN_TABSWITCHER_TASKS_ONLY));
        assertThat(mPropertyModel.get(IS_SHOWING_OVERVIEW), equalTo(true));
        assertThat(mPropertyModel.get(IS_INCOGNITO), equalTo(true));
        assertThat(mPropertyModel.get(IS_EXPLORE_SURFACE_VISIBLE), equalTo(false));
        assertThat(mPropertyModel.get(MV_TILES_VISIBLE), equalTo(false));
        assertThat(mPropertyModel.get(IS_TAB_CAROUSEL_VISIBLE), equalTo(false));
        assertThat(mPropertyModel.get(IS_SECONDARY_SURFACE_VISIBLE), equalTo(false));

        doReturn(false).when(mTabModelSelector).isIncognitoSelected();
        mTabModelSelector.selectModel(false);
        mTabModelSelectorObserverCaptor.getValue().onTabModelSelected(
                mNormalTabModel, mIncognitoTabModel);

        assertThat(mediator.getStartSurfaceState(),
                equalTo(StartSurfaceState.SHOWN_TABSWITCHER_TASKS_ONLY));
        assertThat(mPropertyModel.get(IS_SHOWING_OVERVIEW), equalTo(true));
        assertThat(mPropertyModel.get(IS_INCOGNITO), equalTo(false));
        assertThat(mPropertyModel.get(IS_EXPLORE_SURFACE_VISIBLE), equalTo(false));
        assertThat(mPropertyModel.get(MV_TILES_VISIBLE), equalTo(true));
        assertThat(mPropertyModel.get(IS_TAB_CAROUSEL_VISIBLE), equalTo(false));
        assertThat(mPropertyModel.get(IS_SECONDARY_SURFACE_VISIBLE), equalTo(false));
    }

    @Test
    public void overviewModeIncognitoTabswitcher() {
        doReturn(false).when(mTabModelSelector).isIncognitoSelected();
        doReturn(mVoiceRecognitionHandler).when(mOmniboxStub).getVoiceRecognitionHandler();
        doReturn(true).when(mVoiceRecognitionHandler).isVoiceSearchEnabled();

        StartSurfaceMediator mediator = createStartSurfaceMediator(SurfaceMode.SINGLE_PANE, false);
        mediator.setSecondaryTasksSurfacePropertyModel(mSecondaryTasksSurfacePropertyModel);

        assertThat(mediator.getStartSurfaceState(), equalTo(StartSurfaceState.NOT_SHOWN));

        doReturn(2).when(mNormalTabModel).getCount();
        mediator.setOverviewState(StartSurfaceState.SHOWING_HOMEPAGE);
        mediator.showOverview(false);
        verify(mTabModelSelector).addObserver(mTabModelSelectorObserverCaptor.capture());
        assertThat(mediator.getStartSurfaceState(), equalTo(StartSurfaceState.SHOWN_HOMEPAGE));

        mediator.setOverviewState(StartSurfaceState.SHOWN_TABSWITCHER);
        assertThat(mSecondaryTasksSurfacePropertyModel.get(IS_INCOGNITO), equalTo(false));

        doReturn(true).when(mTabModelSelector).isIncognitoSelected();
        mTabModelSelector.selectModel(true);
        mTabModelSelectorObserverCaptor.getValue().onTabModelSelected(
                mIncognitoTabModel, mNormalTabModel);
        assertThat(mSecondaryTasksSurfacePropertyModel.get(IS_INCOGNITO), equalTo(true));

        mediator.setOverviewState(StartSurfaceState.SHOWN_HOMEPAGE);
        assertThat(mSecondaryTasksSurfacePropertyModel.get(IS_INCOGNITO), equalTo(true));
        assertThat(
                mSecondaryTasksSurfacePropertyModel.get(IS_FAKE_SEARCH_BOX_VISIBLE), equalTo(true));
        assertThat(mSecondaryTasksSurfacePropertyModel.get(IS_VOICE_RECOGNITION_BUTTON_VISIBLE),
                equalTo(false));
        assertThat(mSecondaryTasksSurfacePropertyModel.get(IS_INCOGNITO), equalTo(true));
    }

    @Test
    public void paddingForBottomBarSinglePane() {
        doReturn(false).when(mTabModelSelector).isIncognitoSelected();
        doReturn(mVoiceRecognitionHandler).when(mOmniboxStub).getVoiceRecognitionHandler();
        doReturn(true).when(mVoiceRecognitionHandler).isVoiceSearchEnabled();

        StartSurfaceMediator mediator = createStartSurfaceMediator(SurfaceMode.SINGLE_PANE, false);
        verify(mMainTabGridController)
                .addOverviewModeObserver(mOverviewModeObserverCaptor.capture());
        mediator.setSecondaryTasksSurfacePropertyModel(mSecondaryTasksSurfacePropertyModel);
        assertThat(mediator.getStartSurfaceState(), equalTo(StartSurfaceState.NOT_SHOWN));

        doReturn(30).when(mBrowserControlsStateProvider).getBottomControlsHeight();
        doReturn(2).when(mNormalTabModel).getCount();
        mediator.setOverviewState(StartSurfaceState.SHOWING_HOMEPAGE);
        mediator.showOverview(false);
        verify(mBrowserControlsStateProvider)
                .addObserver(mBrowserControlsStateProviderCaptor.capture());
        assertThat(mPropertyModel.get(BOTTOM_BAR_HEIGHT), equalTo(30));
        assertThat(mSecondaryTasksSurfacePropertyModel.get(BOTTOM_BAR_HEIGHT), equalTo(0));

        mOverviewModeObserverCaptor.getValue().startedShowing();
        mOverviewModeObserverCaptor.getValue().finishedShowing();

        mBrowserControlsStateProviderCaptor.getValue().onBottomControlsHeightChanged(0, 0);
        assertThat(mPropertyModel.get(BOTTOM_BAR_HEIGHT), equalTo(0));
        assertThat(mSecondaryTasksSurfacePropertyModel.get(BOTTOM_BAR_HEIGHT), equalTo(0));

        mBrowserControlsStateProviderCaptor.getValue().onBottomControlsHeightChanged(10, 10);
        assertThat(mPropertyModel.get(BOTTOM_BAR_HEIGHT), equalTo(10));
        assertThat(mSecondaryTasksSurfacePropertyModel.get(BOTTOM_BAR_HEIGHT), equalTo(0));

        mediator.hideOverview(false);
        mOverviewModeObserverCaptor.getValue().startedHiding();
        verify(mBrowserControlsStateProvider)
                .removeObserver(mBrowserControlsStateProviderCaptor.getValue());
    }

    @Test
    public void doNotPaddingForBottomBarTasksOnly() {
        doReturn(false).when(mTabModelSelector).isIncognitoSelected();
        doReturn(mVoiceRecognitionHandler).when(mOmniboxStub).getVoiceRecognitionHandler();
        doReturn(true).when(mVoiceRecognitionHandler).isVoiceSearchEnabled();

        StartSurfaceMediator mediator = createStartSurfaceMediator(SurfaceMode.TASKS_ONLY, false);
        mediator.setSecondaryTasksSurfacePropertyModel(mSecondaryTasksSurfacePropertyModel);
        assertThat(mediator.getStartSurfaceState(), equalTo(StartSurfaceState.NOT_SHOWN));

        doReturn(30).when(mBrowserControlsStateProvider).getBottomControlsHeight();
        doReturn(2).when(mNormalTabModel).getCount();
        mediator.showOverview(false);
        verify(mBrowserControlsStateProvider)
                .addObserver(mBrowserControlsStateProviderCaptor.capture());
        assertThat(mPropertyModel.get(BOTTOM_BAR_HEIGHT), equalTo(0));
        assertThat(mSecondaryTasksSurfacePropertyModel.get(BOTTOM_BAR_HEIGHT), equalTo(0));

        // Even though the BrowserControlsStateProvider.Observer is added, changes to the
        // bottom bar height should be ignored.
        mBrowserControlsStateProviderCaptor.getValue().onBottomControlsHeightChanged(100, 0);
        assertThat(mPropertyModel.get(BOTTOM_BAR_HEIGHT), equalTo(0));
        assertThat(mSecondaryTasksSurfacePropertyModel.get(BOTTOM_BAR_HEIGHT), equalTo(0));
    }

    @Test
    public void setIncognitoDescriptionShowTasksOnly() {
        doReturn(false).when(mTabModelSelector).isIncognitoSelected();
        doReturn(mVoiceRecognitionHandler).when(mOmniboxStub).getVoiceRecognitionHandler();
        doReturn(true).when(mVoiceRecognitionHandler).isVoiceSearchEnabled();

        StartSurfaceMediator mediator = createStartSurfaceMediator(SurfaceMode.TASKS_ONLY, false);
        mediator.showOverview(false);
        verify(mTabModelSelector).addObserver(mTabModelSelectorObserverCaptor.capture());

        assertThat(mPropertyModel.get(IS_INCOGNITO_DESCRIPTION_INITIALIZED), equalTo(false));
        assertThat(mPropertyModel.get(IS_INCOGNITO_DESCRIPTION_VISIBLE), equalTo(false));
        assertThat(mPropertyModel.get(IS_SURFACE_BODY_VISIBLE), equalTo(false));
        doReturn(0).when(mIncognitoTabModel).getCount();
        mTabModelSelectorObserverCaptor.getValue().onTabModelSelected(
                mIncognitoTabModel, mNormalTabModel);
        assertThat(mPropertyModel.get(IS_INCOGNITO_DESCRIPTION_INITIALIZED), equalTo(true));
        assertThat(mPropertyModel.get(IS_INCOGNITO_DESCRIPTION_VISIBLE), equalTo(true));
        assertThat(mPropertyModel.get(IS_SURFACE_BODY_VISIBLE), equalTo(false));

        mTabModelSelectorObserverCaptor.getValue().onTabModelSelected(
                mNormalTabModel, mIncognitoTabModel);
        assertThat(mPropertyModel.get(IS_INCOGNITO_DESCRIPTION_INITIALIZED), equalTo(true));
        assertThat(mPropertyModel.get(IS_INCOGNITO_DESCRIPTION_VISIBLE), equalTo(false));
        assertThat(mPropertyModel.get(IS_SURFACE_BODY_VISIBLE), equalTo(true));

        mediator.hideOverview(true);
    }

    @Test
    public void setIncognitoDescriptionHideTasksOnly() {
        doReturn(false).when(mTabModelSelector).isIncognitoSelected();
        doReturn(mVoiceRecognitionHandler).when(mOmniboxStub).getVoiceRecognitionHandler();
        doReturn(true).when(mVoiceRecognitionHandler).isVoiceSearchEnabled();

        StartSurfaceMediator mediator = createStartSurfaceMediator(SurfaceMode.TASKS_ONLY, false);
        mediator.showOverview(false);
        verify(mTabModelSelector).addObserver(mTabModelSelectorObserverCaptor.capture());

        assertThat(mPropertyModel.get(IS_INCOGNITO_DESCRIPTION_INITIALIZED), equalTo(false));
        assertThat(mPropertyModel.get(IS_INCOGNITO_DESCRIPTION_VISIBLE), equalTo(false));
        doReturn(1).when(mIncognitoTabModel).getCount();
        mTabModelSelectorObserverCaptor.getValue().onTabModelSelected(
                mIncognitoTabModel, mNormalTabModel);
        assertThat(mPropertyModel.get(IS_INCOGNITO_DESCRIPTION_INITIALIZED), equalTo(false));
        assertThat(mPropertyModel.get(IS_INCOGNITO_DESCRIPTION_VISIBLE), equalTo(false));

        mTabModelSelectorObserverCaptor.getValue().onTabModelSelected(
                mNormalTabModel, mIncognitoTabModel);
        assertThat(mPropertyModel.get(IS_INCOGNITO_DESCRIPTION_INITIALIZED), equalTo(false));
        assertThat(mPropertyModel.get(IS_INCOGNITO_DESCRIPTION_VISIBLE), equalTo(false));

        mediator.hideOverview(true);
    }

    @Test
    public void setIncognitoDescriptionShowSinglePane() {
        doReturn(false).when(mTabModelSelector).isIncognitoSelected();
        doReturn(mVoiceRecognitionHandler).when(mOmniboxStub).getVoiceRecognitionHandler();
        doReturn(true).when(mVoiceRecognitionHandler).isVoiceSearchEnabled();

        StartSurfaceMediator mediator = createStartSurfaceMediator(SurfaceMode.SINGLE_PANE, false);
        mediator.setOverviewState(StartSurfaceState.SHOWING_HOMEPAGE);
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

        mediator.hideOverview(true);
    }

    @Test
    public void setIncognitoDescriptionHideSinglePane() {
        doReturn(false).when(mTabModelSelector).isIncognitoSelected();
        doReturn(mVoiceRecognitionHandler).when(mOmniboxStub).getVoiceRecognitionHandler();
        doReturn(true).when(mVoiceRecognitionHandler).isVoiceSearchEnabled();

        StartSurfaceMediator mediator = createStartSurfaceMediator(SurfaceMode.SINGLE_PANE, false);
        mediator.setOverviewState(StartSurfaceState.SHOWN_HOMEPAGE);
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

        mediator.hideOverview(true);
    }

    @Test
    public void showAndHideTabSwitcherToolbarHomePage() {
        doReturn(false).when(mTabModelSelector).isIncognitoSelected();
        doReturn(mVoiceRecognitionHandler).when(mOmniboxStub).getVoiceRecognitionHandler();
        doReturn(true).when(mVoiceRecognitionHandler).isVoiceSearchEnabled();

        StartSurfaceMediator mediator = createStartSurfaceMediator(SurfaceMode.SINGLE_PANE, false);
        verify(mMainTabGridController)
                .addOverviewModeObserver(mOverviewModeObserverCaptor.capture());

        assertThat(mediator.getStartSurfaceState(), equalTo(StartSurfaceState.NOT_SHOWN));

        mediator.setSecondaryTasksSurfacePropertyModel(mSecondaryTasksSurfacePropertyModel);
        mediator.setOverviewState(StartSurfaceState.SHOWN_HOMEPAGE);
        mediator.showOverview(false);
        verify(mTabModelSelector).addObserver(mTabModelSelectorObserverCaptor.capture());
        verify(mMainTabGridController).showOverview(eq(false));
        verify(mOmniboxStub).addUrlFocusChangeListener(mUrlFocusChangeListenerCaptor.capture());
        assertThat(mediator.getStartSurfaceState(), equalTo(StartSurfaceState.SHOWN_HOMEPAGE));
        assertThat(mPropertyModel.get(IS_FAKE_SEARCH_BOX_VISIBLE), equalTo(true));
        assertThat(mediator.shouldShowTabSwitcherToolbar(), equalTo(true));

        mUrlFocusChangeListenerCaptor.getValue().onUrlFocusChange(true);
        assertThat(mPropertyModel.get(IS_FAKE_SEARCH_BOX_VISIBLE), equalTo(false));
        assertThat(mediator.shouldShowTabSwitcherToolbar(), equalTo(false));

        mUrlFocusChangeListenerCaptor.getValue().onUrlFocusChange(false);
        assertThat(mPropertyModel.get(IS_FAKE_SEARCH_BOX_VISIBLE), equalTo(true));
        assertThat(mPropertyModel.get(IS_VOICE_RECOGNITION_BUTTON_VISIBLE), equalTo(true));
        assertThat(mediator.shouldShowTabSwitcherToolbar(), equalTo(true));

        doReturn(true).when(mTabModelSelector).isIncognitoSelected();
        mTabModelSelectorObserverCaptor.getValue().onTabModelSelected(
                mIncognitoTabModel, mNormalTabModel);
        assertThat(
                mSecondaryTasksSurfacePropertyModel.get(IS_FAKE_SEARCH_BOX_VISIBLE), equalTo(true));
        assertThat(mediator.shouldShowTabSwitcherToolbar(), equalTo(true));

        mUrlFocusChangeListenerCaptor.getValue().onUrlFocusChange(true);
        assertThat(mSecondaryTasksSurfacePropertyModel.get(IS_FAKE_SEARCH_BOX_VISIBLE),
                equalTo(false));
        assertThat(mediator.shouldShowTabSwitcherToolbar(), equalTo(false));

        mUrlFocusChangeListenerCaptor.getValue().onUrlFocusChange(false);
        assertThat(
                mSecondaryTasksSurfacePropertyModel.get(IS_FAKE_SEARCH_BOX_VISIBLE), equalTo(true));
        assertThat(mediator.shouldShowTabSwitcherToolbar(), equalTo(true));
    }

    @Test
    public void defaultStateSinglePane() {
        doReturn(false).when(mTabModelSelector).isIncognitoSelected();
        doReturn(mVoiceRecognitionHandler).when(mOmniboxStub).getVoiceRecognitionHandler();
        doReturn(true).when(mVoiceRecognitionHandler).isVoiceSearchEnabled();

        StartSurfaceMediator mediator = createStartSurfaceMediator(SurfaceMode.SINGLE_PANE, false);
        verify(mMainTabGridController)
                .addOverviewModeObserver(mOverviewModeObserverCaptor.capture());

        assertThat(mediator.getStartSurfaceState(), equalTo(StartSurfaceState.NOT_SHOWN));
        mediator.showOverview(false);
        assertThat(mediator.getStartSurfaceState(), equalTo(StartSurfaceState.SHOWN_TABSWITCHER));
    }

    @Test
    public void defaultStateTaskOnly() {
        doReturn(false).when(mTabModelSelector).isIncognitoSelected();
        doReturn(mVoiceRecognitionHandler).when(mOmniboxStub).getVoiceRecognitionHandler();
        doReturn(true).when(mVoiceRecognitionHandler).isVoiceSearchEnabled();

        StartSurfaceMediator mediator = createStartSurfaceMediator(SurfaceMode.TASKS_ONLY, false);
        verify(mMainTabGridController)
                .addOverviewModeObserver(mOverviewModeObserverCaptor.capture());

        assertThat(mediator.getStartSurfaceState(), equalTo(StartSurfaceState.NOT_SHOWN));
        mediator.showOverview(false);
        assertThat(mediator.getStartSurfaceState(),
                equalTo(StartSurfaceState.SHOWN_TABSWITCHER_TASKS_ONLY));
    }

    @Test
    public void defaultStateTwoPanes() {
        doReturn(false).when(mTabModelSelector).isIncognitoSelected();
        doReturn(mVoiceRecognitionHandler).when(mOmniboxStub).getVoiceRecognitionHandler();
        doReturn(true).when(mVoiceRecognitionHandler).isVoiceSearchEnabled();

        StartSurfaceMediator mediator = createStartSurfaceMediator(SurfaceMode.TWO_PANES, false);
        verify(mMainTabGridController)
                .addOverviewModeObserver(mOverviewModeObserverCaptor.capture());

        assertThat(mediator.getStartSurfaceState(), equalTo(StartSurfaceState.NOT_SHOWN));
        mediator.showOverview(false);
        assertThat(mediator.getStartSurfaceState(),
                equalTo(StartSurfaceState.SHOWN_TABSWITCHER_TWO_PANES));
    }

    @Test
    public void showAndHideTabSwitcherToolbarTabswitcher() {
        doReturn(false).when(mTabModelSelector).isIncognitoSelected();
        doReturn(mVoiceRecognitionHandler).when(mOmniboxStub).getVoiceRecognitionHandler();
        doReturn(true).when(mVoiceRecognitionHandler).isVoiceSearchEnabled();

        StartSurfaceMediator mediator = createStartSurfaceMediator(SurfaceMode.SINGLE_PANE, false);
        verify(mMainTabGridController)
                .addOverviewModeObserver(mOverviewModeObserverCaptor.capture());

        assertThat(mediator.getStartSurfaceState(), equalTo(StartSurfaceState.NOT_SHOWN));

        mediator.setSecondaryTasksSurfacePropertyModel(mSecondaryTasksSurfacePropertyModel);
        mediator.setOverviewState(StartSurfaceState.SHOWING_HOMEPAGE);
        mediator.showOverview(false);
        verify(mMainTabGridController).showOverview(eq(false));
        verify(mOmniboxStub).addUrlFocusChangeListener(mUrlFocusChangeListenerCaptor.capture());
        assertThat(mediator.getStartSurfaceState(), equalTo(StartSurfaceState.SHOWN_HOMEPAGE));
        assertThat(mPropertyModel.get(IS_FAKE_SEARCH_BOX_VISIBLE), equalTo(true));
        assertThat(mediator.shouldShowTabSwitcherToolbar(), equalTo(true));

        mUrlFocusChangeListenerCaptor.getValue().onUrlFocusChange(true);
        assertThat(mPropertyModel.get(IS_FAKE_SEARCH_BOX_VISIBLE), equalTo(false));
        assertThat(mediator.shouldShowTabSwitcherToolbar(), equalTo(false));

        mUrlFocusChangeListenerCaptor.getValue().onUrlFocusChange(false);
        assertThat(mPropertyModel.get(IS_FAKE_SEARCH_BOX_VISIBLE), equalTo(true));
        assertThat(mPropertyModel.get(IS_VOICE_RECOGNITION_BUTTON_VISIBLE), equalTo(true));
        assertThat(mediator.shouldShowTabSwitcherToolbar(), equalTo(true));

        mediator.setOverviewState(StartSurfaceState.SHOWN_TABSWITCHER);
        assertThat(mSecondaryTasksSurfacePropertyModel.get(IS_FAKE_SEARCH_BOX_VISIBLE),
                equalTo(false));
        assertThat(mediator.shouldShowTabSwitcherToolbar(), equalTo(true));
    }

    @Test
    public void singleShowingPrevious() {
        doReturn(false).when(mTabModelSelector).isIncognitoSelected();
        doReturn(mVoiceRecognitionHandler).when(mOmniboxStub).getVoiceRecognitionHandler();
        doReturn(true).when(mVoiceRecognitionHandler).isVoiceSearchEnabled();

        StartSurfaceMediator mediator = createStartSurfaceMediator(SurfaceMode.SINGLE_PANE, false);
        InOrder mainTabGridController = inOrder(mMainTabGridController);
        mainTabGridController.verify(mMainTabGridController)
                .addOverviewModeObserver(mOverviewModeObserverCaptor.capture());

        assertThat(mediator.getStartSurfaceState(), equalTo(StartSurfaceState.NOT_SHOWN));

        mediator.setSecondaryTasksSurfacePropertyModel(mSecondaryTasksSurfacePropertyModel);
        mediator.setOverviewState(StartSurfaceState.SHOWING_PREVIOUS);
        mediator.showOverview(false);
        mainTabGridController.verify(mMainTabGridController).showOverview(eq(false));
        InOrder omniboxStub = inOrder(mOmniboxStub);
        omniboxStub.verify(mOmniboxStub)
                .addUrlFocusChangeListener(mUrlFocusChangeListenerCaptor.capture());
        assertThat(mediator.getStartSurfaceState(), equalTo(StartSurfaceState.SHOWN_HOMEPAGE));
        assertThat(mPropertyModel.get(IS_FAKE_SEARCH_BOX_VISIBLE), equalTo(true));
        assertThat(mediator.shouldShowTabSwitcherToolbar(), equalTo(true));

        mediator.hideOverview(true);
        mOverviewModeObserverCaptor.getValue().startedHiding();
        mOverviewModeObserverCaptor.getValue().finishedHiding();

        mediator.setOverviewState(StartSurfaceState.SHOWING_PREVIOUS);
        mediator.showOverview(false);
        mainTabGridController.verify(mMainTabGridController).showOverview(eq(false));
        omniboxStub.verify(mOmniboxStub)
                .addUrlFocusChangeListener(mUrlFocusChangeListenerCaptor.capture());
        assertThat(mediator.getStartSurfaceState(), equalTo(StartSurfaceState.SHOWN_HOMEPAGE));
        assertThat(mPropertyModel.get(IS_FAKE_SEARCH_BOX_VISIBLE), equalTo(true));
        assertThat(mediator.shouldShowTabSwitcherToolbar(), equalTo(true));

        mediator.setOverviewState(StartSurfaceState.SHOWN_TABSWITCHER);
        assertThat(mSecondaryTasksSurfacePropertyModel.get(IS_FAKE_SEARCH_BOX_VISIBLE),
                equalTo(false));
        assertThat(mediator.shouldShowTabSwitcherToolbar(), equalTo(true));
        assertThat(mediator.getStartSurfaceState(), equalTo(StartSurfaceState.SHOWN_TABSWITCHER));

        mediator.hideOverview(true);
        mOverviewModeObserverCaptor.getValue().startedHiding();
        mOverviewModeObserverCaptor.getValue().finishedHiding();

        mediator.setOverviewState(StartSurfaceState.SHOWING_PREVIOUS);
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

        StartSurfaceMediator mediator = createStartSurfaceMediator(SurfaceMode.SINGLE_PANE, false);
        InOrder mainTabGridController = inOrder(mMainTabGridController);
        mainTabGridController.verify(mMainTabGridController)
                .addOverviewModeObserver(mOverviewModeObserverCaptor.capture());
        assertThat(mediator.getStartSurfaceState(), equalTo(StartSurfaceState.NOT_SHOWN));

        mediator.setOverviewState(StartSurfaceState.SHOWING_HOMEPAGE);
        mPropertyModel.set(IS_EXPLORE_SURFACE_VISIBLE, true);
        when(mFeedSurfaceCreator.createFeedSurfaceCoordinator(anyBoolean(), anyBoolean()))
                .thenReturn(mFeedSurfaceCoordinator);
        mediator.showOverview(false);
        mainTabGridController.verify(mMainTabGridController).showOverview(eq(false));
        assertThat(mediator.getStartSurfaceState(), equalTo(StartSurfaceState.SHOWN_HOMEPAGE));
        assertThat(mPropertyModel.get(FEED_SURFACE_COORDINATOR), equalTo(mFeedSurfaceCoordinator));

        doReturn(TabLaunchType.FROM_START_SURFACE).when(mTab).getLaunchType();
        mediator.hideOverview(true);
        mOverviewModeObserverCaptor.getValue().startedHiding();
        mOverviewModeObserverCaptor.getValue().finishedHiding();
        assertThat(mPropertyModel.get(FEED_SURFACE_COORDINATOR), equalTo(mFeedSurfaceCoordinator));

        FeedSurfaceCoordinator feedSurfaceCoordinator = mock(FeedSurfaceCoordinator.class);
        assertNotEquals(mFeedSurfaceCoordinator, feedSurfaceCoordinator);
        when(mFeedSurfaceCreator.createFeedSurfaceCoordinator(anyBoolean(), anyBoolean()))
                .thenReturn(feedSurfaceCoordinator);
        mediator.setOverviewState(StartSurfaceState.SHOWING_PREVIOUS);
        mediator.showOverview(false);
        mainTabGridController.verify(mMainTabGridController).showOverview(eq(false));
        assertThat(mediator.getStartSurfaceState(), equalTo(StartSurfaceState.SHOWN_HOMEPAGE));
        assertThat(mPropertyModel.get(FEED_SURFACE_COORDINATOR), equalTo(mFeedSurfaceCoordinator));

        doReturn(TabLaunchType.FROM_LINK).when(mTab).getLaunchType();
        mediator.hideOverview(true);
        mOverviewModeObserverCaptor.getValue().startedHiding();
        mOverviewModeObserverCaptor.getValue().finishedHiding();
        assertNull(mPropertyModel.get(FEED_SURFACE_COORDINATOR));
    }

    @Test
    public void changeTopContentOffset() {
        doReturn(false).when(mTabModelSelector).isIncognitoSelected();
        doReturn(mVoiceRecognitionHandler).when(mOmniboxStub).getVoiceRecognitionHandler();
        doReturn(true).when(mVoiceRecognitionHandler).isVoiceSearchEnabled();

        doNothing()
                .when(mBrowserControlsStateProvider)
                .addObserver(mBrowserControlsStateProviderCaptor.capture());
        StartSurfaceMediator mediator = createStartSurfaceMediator(SurfaceMode.SINGLE_PANE, false);
        // Sets the current StartSurfaceState to SHOWING_START before calling the
        // {@link StartSurfaceMediator#showOverview()}. This is because if the current
        // StartSurfaceState is NOT_SHOWN, the state will be set default to SHOWING_TABSWITCHER in
        // {@link StartSurfaceMediator#showOverview()}.
        mediator.setOverviewState(StartSurfaceState.SHOWING_START);
        mediator.showOverview(false);

        verify(mBrowserControlsStateProvider).addObserver(ArgumentMatchers.any());

        mBrowserControlsStateProviderCaptor.getValue().onTopControlsHeightChanged(100, 20);
        doReturn(100).when(mBrowserControlsStateProvider).getContentOffset();
        mBrowserControlsStateProviderCaptor.getValue().onControlsOffsetChanged(
                100, 20, 0, 0, false);
        assertEquals("Wrong top content offset.", 100, mPropertyModel.get(TOP_MARGIN));

        doReturn(50).when(mBrowserControlsStateProvider).getContentOffset();
        mBrowserControlsStateProviderCaptor.getValue().onControlsOffsetChanged(50, 20, 0, 0, false);
        assertEquals("Wrong top content offset.", 50, mPropertyModel.get(TOP_MARGIN));
    }

    @Test
    public void exploreSurfaceInitializedAfterNativeInSinglePane() {
        doReturn(false).when(mTabModelSelector).isIncognitoSelected();
        doReturn(mVoiceRecognitionHandler).when(mOmniboxStub).getVoiceRecognitionHandler();
        doReturn(true).when(mVoiceRecognitionHandler).isVoiceSearchEnabled();

        StartSurfaceMediator mediator = createStartSurfaceMediatorWithoutInit(
                SurfaceMode.SINGLE_PANE, /* excludeMVTiles= */ false,
                /* hadWarmStart= */ false);
        verify(mMainTabGridController)
                .addOverviewModeObserver(mOverviewModeObserverCaptor.capture());

        assertThat(mediator.getStartSurfaceState(), equalTo(StartSurfaceState.NOT_SHOWN));
        // Sets the current StartSurfaceState to SHOWING_START before calling the
        // {@link StartSurfaceMediator#showOverview()}. This is because if the current
        // StartSurfaceState is NOT_SHOWN, the state will be set default to SHOWING_TABSWITCHER in
        // {@link StartSurfaceMediator#showOverview()}.
        mediator.setOverviewState(StartSurfaceState.SHOWING_START);
        mediator.showOverview(false);
        assertThat(mediator.getStartSurfaceState(), equalTo(StartSurfaceState.SHOWN_HOMEPAGE));
        assertThat(mPropertyModel.get(IS_EXPLORE_SURFACE_VISIBLE), equalTo(false));
        verify(mMainTabGridController).showOverview(eq(false));

        when(mMainTabGridController.overviewVisible()).thenReturn(true);
        mediator.initWithNative(mOmniboxStub, mFeedSurfaceCreator, mPrefService);
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

        createStartSurfaceMediatorWithoutInit(SurfaceMode.OMNIBOX_ONLY, /* excludeMVTiles= */ false,
                /* hadWarmStart= */ false);
        assertThat(mPropertyModel.get(TASKS_SURFACE_BODY_TOP_MARGIN), equalTo(0));
        assertThat(mPropertyModel.get(MV_TILES_CONTAINER_TOP_MARGIN), equalTo(0));
        assertThat(mPropertyModel.get(TAB_SWITCHER_TITLE_TOP_MARGIN), equalTo(0));

        createStartSurfaceMediatorWithoutInit(SurfaceMode.SINGLE_PANE, /* excludeMVTiles= */ false,
                /* hadWarmStart= */ false);
        assertThat(mPropertyModel.get(TASKS_SURFACE_BODY_TOP_MARGIN),
                equalTo(tasksSurfaceBodyTopMargin));
        assertThat(mPropertyModel.get(MV_TILES_CONTAINER_TOP_MARGIN),
                equalTo(mvTilesContainerTopMargin));
        assertThat(mPropertyModel.get(TAB_SWITCHER_TITLE_TOP_MARGIN),
                equalTo(tabSwitcherTitleTopMargin));
    }

    @Test
    @Features.EnableFeatures(INSTANT_START)
    public void feedPlaceholderFromWarmStart() {
        doReturn(false).when(mTabModelSelector).isIncognitoSelected();
        doReturn(mVoiceRecognitionHandler).when(mOmniboxStub).getVoiceRecognitionHandler();
        doReturn(true).when(mVoiceRecognitionHandler).isVoiceSearchEnabled();

        StartSurfaceMediator mediator =
                createStartSurfaceMediator(SurfaceMode.SINGLE_PANE, /* excludeMVTiles= */ false,
                        /* hadWarmStart= */ true);
        assertFalse(mediator.shouldShowFeedPlaceholder());

        mediator.setOverviewState(StartSurfaceState.SHOWING_HOMEPAGE);
        mPropertyModel.set(IS_EXPLORE_SURFACE_VISIBLE, true);
        when(mFeedSurfaceCreator.createFeedSurfaceCoordinator(anyBoolean(), anyBoolean()))
                .thenReturn(mFeedSurfaceCoordinator);
        mediator.showOverview(false);
        assertThat(mediator.getStartSurfaceState(), equalTo(StartSurfaceState.SHOWN_HOMEPAGE));

        FeedSurfaceCoordinator feedSurfaceCoordinator =
                mPropertyModel.get(FEED_SURFACE_COORDINATOR);
        assertThat(feedSurfaceCoordinator, equalTo(mFeedSurfaceCoordinator));

        assertFalse(feedSurfaceCoordinator.isPlaceholderShown());
    }

    private StartSurfaceMediator createStartSurfaceMediator(
            @SurfaceMode int mode, boolean excludeMVTiles) {
        return createStartSurfaceMediator(mode, excludeMVTiles, /* hadWarmStart= */ false);
    }

    private StartSurfaceMediator createStartSurfaceMediator(
            @SurfaceMode int mode, boolean excludeMVTiles, boolean hadWarmStart) {
        StartSurfaceMediator mediator =
                createStartSurfaceMediatorWithoutInit(mode, excludeMVTiles, hadWarmStart);
        mediator.initWithNative(mOmniboxStub,
                (mode == SurfaceMode.SINGLE_PANE || mode == SurfaceMode.TWO_PANES)
                        ? mFeedSurfaceCreator
                        : null,
                mPrefService);
        return mediator;
    }

    private StartSurfaceMediator createStartSurfaceMediatorWithoutInit(
            @SurfaceMode int mode, boolean excludeMVTiles, boolean hadWarmStart) {
        StartSurfaceMediator mediator = new StartSurfaceMediator(mMainTabGridController,
                mTabModelSelector, mode == SurfaceMode.NO_START_SURFACE ? null : mPropertyModel,
                mode == SurfaceMode.SINGLE_PANE ? mSecondaryTasksSurfaceInitializer : null, mode,
                ContextUtils.getApplicationContext(), mBrowserControlsStateProvider,
                mActivityStateChecker, excludeMVTiles, mStartSurfaceSupplier, hadWarmStart);
        return mediator;
    }
}
