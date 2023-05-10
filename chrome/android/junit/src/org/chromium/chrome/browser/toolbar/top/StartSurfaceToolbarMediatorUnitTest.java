// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.top;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.AdditionalMatchers.not;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import static org.chromium.chrome.browser.toolbar.top.StartSurfaceToolbarProperties.BUTTONS_CLICKABLE;
import static org.chromium.chrome.browser.toolbar.top.StartSurfaceToolbarProperties.IDENTITY_DISC_AT_START;
import static org.chromium.chrome.browser.toolbar.top.StartSurfaceToolbarProperties.IDENTITY_DISC_CLICK_HANDLER;
import static org.chromium.chrome.browser.toolbar.top.StartSurfaceToolbarProperties.IDENTITY_DISC_DESCRIPTION;
import static org.chromium.chrome.browser.toolbar.top.StartSurfaceToolbarProperties.IDENTITY_DISC_IMAGE;
import static org.chromium.chrome.browser.toolbar.top.StartSurfaceToolbarProperties.IDENTITY_DISC_IS_VISIBLE;
import static org.chromium.chrome.browser.toolbar.top.StartSurfaceToolbarProperties.INCOGNITO_SWITCHER_VISIBLE;
import static org.chromium.chrome.browser.toolbar.top.StartSurfaceToolbarProperties.IS_VISIBLE;
import static org.chromium.chrome.browser.toolbar.top.StartSurfaceToolbarProperties.NEW_TAB_VIEW_IS_VISIBLE;
import static org.chromium.chrome.browser.toolbar.top.StartSurfaceToolbarProperties.NEW_TAB_VIEW_TEXT_IS_VISIBLE;
import static org.chromium.chrome.browser.toolbar.top.StartSurfaceToolbarProperties.TRANSLATION_Y;

import android.content.Context;
import android.content.res.Resources;
import android.graphics.drawable.Drawable;
import android.view.View;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;
import org.robolectric.annotation.LooperMode;

import org.chromium.base.Callback;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.browser.feature_engagement.TrackerFactory;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.identity_disc.IdentityDiscController;
import org.chromium.chrome.browser.layouts.LayoutType;
import org.chromium.chrome.browser.logo.LogoBridge;
import org.chromium.chrome.browser.logo.LogoBridgeJni;
import org.chromium.chrome.browser.logo.LogoView;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.SharedPreferencesManager;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.search_engines.TemplateUrlServiceFactory;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.IncognitoTabModelObserver;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorObserver;
import org.chromium.chrome.browser.toolbar.ButtonData.ButtonSpec;
import org.chromium.chrome.browser.toolbar.ButtonDataImpl;
import org.chromium.chrome.browser.toolbar.adaptive.AdaptiveToolbarButtonVariant;
import org.chromium.chrome.browser.toolbar.menu_button.MenuButtonCoordinator;
import org.chromium.chrome.browser.user_education.IPHCommandBuilder;
import org.chromium.chrome.browser.util.ChromeAccessibilityUtil;
import org.chromium.chrome.features.start_surface.StartSurfaceState;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.chrome.test.util.browser.Features.DisableFeatures;
import org.chromium.chrome.test.util.browser.Features.EnableFeatures;
import org.chromium.components.feature_engagement.Tracker;
import org.chromium.components.search_engines.TemplateUrlService;
import org.chromium.ui.modelutil.PropertyModel;

/** Tests for {@link StartSurfaceToolbarMediator}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
@LooperMode(LooperMode.Mode.LEGACY)
public class StartSurfaceToolbarMediatorUnitTest {
    private PropertyModel mPropertyModel;
    private StartSurfaceToolbarMediator mMediator;
    @Rule
    public TestRule mFeaturesProcessorRule = new Features.JUnitProcessor();
    @Rule
    public JniMocker mJniMocker = new JniMocker();
    @Mock
    private TabModelSelector mTabModelSelector;
    @Mock
    private TabModel mIncognitoTabModel;
    @Mock
    Runnable mDismissedCallback;
    @Mock
    View.OnClickListener mOnClickListener;
    @Mock
    IdentityDiscController mIdentityDiscController;
    @Mock
    private Resources mMockResources;
    @Mock
    private Drawable mDrawable;
    @Mock
    Drawable.ConstantState mMockConstantState;
    @Mock
    Callback<IPHCommandBuilder> mMockIdentityIPHCallback;
    @Mock
    Tab mMockIncognitoTab;
    @Mock
    MenuButtonCoordinator mMenuButtonCoordinator;
    @Mock
    private Profile mProfile;
    @Mock
    Tracker mTracker;
    @Mock
    private TemplateUrlService mTemplateUrlService;
    @Mock
    private Context mContext;
    @Mock
    private LogoView mLogoView;
    @Mock
    LogoBridge.Natives mLogoBridge;
    @Mock
    private Callback<Boolean> mFinishedShowingCallback;
    @Captor
    private ArgumentCaptor<TabModelSelectorObserver> mTabModelSelectorObserver;
    @Captor
    private ArgumentCaptor<IncognitoTabModelObserver> mIncognitoTabModelObserver;

    private ButtonDataImpl mButtonData;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);

        mPropertyModel =
                new PropertyModel.Builder(StartSurfaceToolbarProperties.ALL_KEYS)
                        .with(StartSurfaceToolbarProperties.INCOGNITO_SWITCHER_VISIBLE, true)
                        .with(StartSurfaceToolbarProperties.MENU_IS_VISIBLE, true)
                        .with(StartSurfaceToolbarProperties.IS_VISIBLE, false)
                        .with(StartSurfaceToolbarProperties.NEW_TAB_VIEW_IS_VISIBLE, false)
                        .with(StartSurfaceToolbarProperties.NEW_TAB_VIEW_TEXT_IS_VISIBLE, false)
                        .build();
        mButtonData = new ButtonDataImpl(false, mDrawable, mOnClickListener, "", false, null, true,
                AdaptiveToolbarButtonVariant.UNKNOWN);
        ButtonDataImpl disabledButtonData = new ButtonDataImpl(
                false, null, null, "", false, null, true, AdaptiveToolbarButtonVariant.UNKNOWN);

        Profile.setLastUsedProfileForTesting(mProfile);
        TrackerFactory.setTrackerForTests(mTracker);

        when(mLogoBridge.init(any(), any())).thenReturn(1L);
        mJniMocker.mock(LogoBridgeJni.TEST_HOOKS, mLogoBridge);

        doReturn(mButtonData)
                .when(mIdentityDiscController)
                .getForStartSurface(StartSurfaceState.SHOWN_HOMEPAGE, LayoutType.START_SURFACE);
        doReturn(disabledButtonData)
                .when(mIdentityDiscController)
                .getForStartSurface(not(eq(StartSurfaceState.SHOWN_HOMEPAGE)),
                        not(eq(LayoutType.START_SURFACE)));

        mMockConstantState = mock(Drawable.ConstantState.class);
        doReturn(mMockConstantState).when(mDrawable).getConstantState();
        doReturn(mDrawable).when(mMockConstantState).newDrawable();

        TemplateUrlServiceFactory.setInstanceForTesting(mTemplateUrlService);
        when(mTemplateUrlService.doesDefaultSearchEngineHaveLogo()).thenReturn(true);

        doReturn(false).when(mTabModelSelector).isIncognitoSelected();
        doReturn(mIncognitoTabModel).when(mTabModelSelector).getModel(true);
        doReturn(mMockIncognitoTab).when(mIncognitoTabModel).getTabAt(0);
        doReturn(false).when(mMockIncognitoTab).isClosing();
        doReturn(0).when(mIncognitoTabModel).getCount();
    }

    @After
    public void tearDown() {
        ChromeAccessibilityUtil.get().setAccessibilityEnabledForTesting(false);
    }

    @Test
    public void testShowAndHideHomePage() {
        createMediator(false);

        doReturn(0).when(mIncognitoTabModel).getCount();
        assertFalse(mMediator.isLogoVisibleForTesting());
        assertFalse(mPropertyModel.get(IDENTITY_DISC_IS_VISIBLE));
        assertFalse(mPropertyModel.get(IDENTITY_DISC_AT_START));
        assertFalse(mPropertyModel.get(NEW_TAB_VIEW_IS_VISIBLE));
        assertFalse(mPropertyModel.get(NEW_TAB_VIEW_TEXT_IS_VISIBLE));
        assertFalse(mPropertyModel.get(INCOGNITO_SWITCHER_VISIBLE));
        assertFalse(mPropertyModel.get(IS_VISIBLE));

        mMediator.onStartSurfaceStateChanged(
                StartSurfaceState.SHOWN_HOMEPAGE, true, LayoutType.START_SURFACE);
        assertTrue(mMediator.isLogoVisibleForTesting());
        assertFalse(mPropertyModel.get(IDENTITY_DISC_IS_VISIBLE));
        assertTrue(mPropertyModel.get(IDENTITY_DISC_AT_START));
        assertFalse(mPropertyModel.get(NEW_TAB_VIEW_IS_VISIBLE));
        assertFalse(mPropertyModel.get(NEW_TAB_VIEW_TEXT_IS_VISIBLE));
        assertFalse(mPropertyModel.get(INCOGNITO_SWITCHER_VISIBLE));
        assertTrue(mPropertyModel.get(IS_VISIBLE));

        doReturn(1).when(mIncognitoTabModel).getCount();
        mMediator.onStartSurfaceStateChanged(
                StartSurfaceState.SHOWN_HOMEPAGE, true, LayoutType.START_SURFACE);
        assertTrue(mMediator.isLogoVisibleForTesting());
        assertFalse(mPropertyModel.get(IDENTITY_DISC_IS_VISIBLE));
        assertTrue(mPropertyModel.get(IDENTITY_DISC_AT_START));
        assertFalse(mPropertyModel.get(NEW_TAB_VIEW_IS_VISIBLE));
        assertFalse(mPropertyModel.get(NEW_TAB_VIEW_TEXT_IS_VISIBLE));
        assertFalse(mPropertyModel.get(INCOGNITO_SWITCHER_VISIBLE));
        assertTrue(mPropertyModel.get(IS_VISIBLE));
    }

    @Test
    public void testShowAndHideTabSwitcher() {
        createMediator(false);

        doReturn(0).when(mIncognitoTabModel).getCount();
        assertFalse(mMediator.isLogoVisibleForTesting());
        assertFalse(mPropertyModel.get(IDENTITY_DISC_IS_VISIBLE));
        assertFalse(mPropertyModel.get(IDENTITY_DISC_AT_START));
        assertFalse(mPropertyModel.get(INCOGNITO_SWITCHER_VISIBLE));
        assertFalse(mPropertyModel.get(NEW_TAB_VIEW_IS_VISIBLE));
        assertFalse(mPropertyModel.get(NEW_TAB_VIEW_TEXT_IS_VISIBLE));
        assertFalse(mPropertyModel.get(IS_VISIBLE));

        mMediator.onStartSurfaceStateChanged(
                StartSurfaceState.SHOWN_TABSWITCHER, true, LayoutType.TAB_SWITCHER);
        assertFalse(mMediator.isLogoVisibleForTesting());
        assertFalse(mPropertyModel.get(IDENTITY_DISC_IS_VISIBLE));
        assertFalse(mPropertyModel.get(IDENTITY_DISC_AT_START));
        assertTrue(mPropertyModel.get(NEW_TAB_VIEW_IS_VISIBLE));
        assertFalse(mPropertyModel.get(NEW_TAB_VIEW_TEXT_IS_VISIBLE));
        assertTrue(mPropertyModel.get(INCOGNITO_SWITCHER_VISIBLE));
        assertTrue(mPropertyModel.get(IS_VISIBLE));
        verify(mFinishedShowingCallback).onResult(true);

        mMediator.updateIdentityDisc(mButtonData);
        assertFalse(mPropertyModel.get(IDENTITY_DISC_IS_VISIBLE));

        doReturn(1).when(mIncognitoTabModel).getCount();
        mMediator.onStartSurfaceStateChanged(
                StartSurfaceState.SHOWN_TABSWITCHER, true, LayoutType.TAB_SWITCHER);
        assertFalse(mMediator.isLogoVisibleForTesting());
        assertFalse(mPropertyModel.get(IDENTITY_DISC_IS_VISIBLE));
        assertFalse(mPropertyModel.get(IDENTITY_DISC_AT_START));
        assertTrue(mPropertyModel.get(NEW_TAB_VIEW_IS_VISIBLE));
        assertFalse(mPropertyModel.get(NEW_TAB_VIEW_TEXT_IS_VISIBLE));
        assertTrue(mPropertyModel.get(INCOGNITO_SWITCHER_VISIBLE));
        assertTrue(mPropertyModel.get(IS_VISIBLE));

        mMediator.onStartSurfaceStateChanged(
                StartSurfaceState.NOT_SHOWN, false, LayoutType.BROWSING);
        verify(mFinishedShowingCallback).onResult(false);
    }

    @Test
    public void testSwitchBetweenHomePageAndTabSwitcher() {
        createMediator(false);

        mButtonData.setCanShow(true);
        mMediator.updateIdentityDisc(mButtonData);
        mMediator.onStartSurfaceStateChanged(
                StartSurfaceState.SHOWN_HOMEPAGE, true, LayoutType.START_SURFACE);
        assertTrue(mMediator.isLogoVisibleForTesting());
        assertTrue(mPropertyModel.get(IDENTITY_DISC_IS_VISIBLE));
        assertTrue(mPropertyModel.get(IDENTITY_DISC_AT_START));
        assertFalse(mPropertyModel.get(NEW_TAB_VIEW_IS_VISIBLE));
        assertFalse(mPropertyModel.get(INCOGNITO_SWITCHER_VISIBLE));
        assertTrue(mPropertyModel.get(IS_VISIBLE));

        mMediator.onStartSurfaceStateChanged(
                StartSurfaceState.SHOWN_TABSWITCHER, true, LayoutType.TAB_SWITCHER);
        assertFalse(mMediator.isLogoVisibleForTesting());
        assertFalse(mPropertyModel.get(IDENTITY_DISC_IS_VISIBLE));
        assertFalse(mPropertyModel.get(IDENTITY_DISC_AT_START));
        assertTrue(mPropertyModel.get(NEW_TAB_VIEW_IS_VISIBLE));
        assertFalse(mPropertyModel.get(NEW_TAB_VIEW_TEXT_IS_VISIBLE));
        assertTrue(mPropertyModel.get(INCOGNITO_SWITCHER_VISIBLE));
        assertTrue(mPropertyModel.get(IS_VISIBLE));

        mMediator.onStartSurfaceStateChanged(
                StartSurfaceState.SHOWN_HOMEPAGE, true, LayoutType.START_SURFACE);
        assertTrue(mMediator.isLogoVisibleForTesting());
        assertTrue(mPropertyModel.get(IDENTITY_DISC_IS_VISIBLE));
        assertTrue(mPropertyModel.get(IDENTITY_DISC_AT_START));
        assertFalse(mPropertyModel.get(NEW_TAB_VIEW_IS_VISIBLE));
        assertFalse(mPropertyModel.get(INCOGNITO_SWITCHER_VISIBLE));
        assertTrue(mPropertyModel.get(IS_VISIBLE));
    }

    @Test
    public void testHidingIncognitoToggleWithoutIncognitoTabs() {
        createMediator(true);

        doReturn(0).when(mIncognitoTabModel).getCount();
        mMediator.onStartSurfaceStateChanged(
                StartSurfaceState.SHOWN_TABSWITCHER, true, LayoutType.TAB_SWITCHER);
        assertFalse(mPropertyModel.get(INCOGNITO_SWITCHER_VISIBLE));
        assertTrue(mPropertyModel.get(NEW_TAB_VIEW_IS_VISIBLE));
        assertTrue(mPropertyModel.get(NEW_TAB_VIEW_TEXT_IS_VISIBLE));

        doReturn(1).when(mIncognitoTabModel).getCount();
        mMediator.onStartSurfaceStateChanged(
                StartSurfaceState.SHOWN_TABSWITCHER, true, LayoutType.TAB_SWITCHER);
        assertTrue(mPropertyModel.get(INCOGNITO_SWITCHER_VISIBLE));
        assertTrue(mPropertyModel.get(NEW_TAB_VIEW_IS_VISIBLE));
        assertFalse(mPropertyModel.get(NEW_TAB_VIEW_TEXT_IS_VISIBLE));
    }

    @Test
    public void testIncognitoTabModelObserverUpdatesIncognitoToggle() {
        createMediator(true);
        mMediator.onStartSurfaceStateChanged(
                StartSurfaceState.SHOWN_TABSWITCHER, true, LayoutType.TAB_SWITCHER);

        doReturn(0).when(mIncognitoTabModel).getCount();
        mIncognitoTabModelObserver.getValue().didBecomeEmpty();
        assertFalse(mPropertyModel.get(INCOGNITO_SWITCHER_VISIBLE));
        assertTrue(mPropertyModel.get(NEW_TAB_VIEW_TEXT_IS_VISIBLE));

        doReturn(1).when(mIncognitoTabModel).getCount();
        mIncognitoTabModelObserver.getValue().wasFirstTabCreated();
        assertTrue(mPropertyModel.get(INCOGNITO_SWITCHER_VISIBLE));
        assertFalse(mPropertyModel.get(NEW_TAB_VIEW_TEXT_IS_VISIBLE));
    }

    @Test
    public void testSetButtonsClickable() {
        createMediator(false);
        assertFalse(mPropertyModel.get(BUTTONS_CLICKABLE));

        mMediator.onStartSurfaceStateChanged(
                StartSurfaceState.SHOWN_TABSWITCHER, true, LayoutType.TAB_SWITCHER);
        assertTrue(mPropertyModel.get(BUTTONS_CLICKABLE));

        mMediator.onStartSurfaceStateChanged(
                StartSurfaceState.NOT_SHOWN, false, LayoutType.BROWSING);
        assertFalse(mPropertyModel.get(BUTTONS_CLICKABLE));
    }

    @Test
    public void enableDisableSearchEngineHaveLogo() {
        createMediator(false);
        when(mTemplateUrlService.doesDefaultSearchEngineHaveLogo()).thenReturn(true);
        mMediator.onStartSurfaceStateChanged(
                StartSurfaceState.SHOWN_HOMEPAGE, true, LayoutType.START_SURFACE);

        // If default search engine doesn't have logo, logo shouldn't be visible.
        when(mTemplateUrlService.doesDefaultSearchEngineHaveLogo()).thenReturn(false);
        mMediator.getLogoCoordinatorForTesting().onTemplateURLServiceChangedForTesting();
        assertFalse(mMediator.isLogoVisibleForTesting());

        // If default search engine has logo, logo should be visible.
        when(mTemplateUrlService.doesDefaultSearchEngineHaveLogo()).thenReturn(true);
        mMediator.getLogoCoordinatorForTesting().onTemplateURLServiceChangedForTesting();
        assertTrue(mMediator.isLogoVisibleForTesting());
    }

    @Test
    public void showHomePageWithIdentityDisc() {
        createMediator(false);
        assertFalse(mPropertyModel.get(IDENTITY_DISC_IS_VISIBLE));

        mMediator.onStartSurfaceStateChanged(
                StartSurfaceState.SHOWN_HOMEPAGE, true, LayoutType.START_SURFACE);
        assertFalse(mPropertyModel.get(IDENTITY_DISC_IS_VISIBLE));

        mButtonData.setButtonSpec(new ButtonSpec(mDrawable, mOnClickListener,
                /*onLongClickListener*/ null, /*contentDescription=*/"description",
                /*supportsTinting=*/false, /*iphCommandBuilder=*/null,
                AdaptiveToolbarButtonVariant.UNKNOWN, /*actionChipLabelResId=*/Resources.ID_NULL));
        mButtonData.setCanShow(true);
        mMediator.updateIdentityDisc(mButtonData);
        assertTrue(mPropertyModel.get(IDENTITY_DISC_IS_VISIBLE));
        assertEquals(mOnClickListener, mPropertyModel.get(IDENTITY_DISC_CLICK_HANDLER));
        assertEquals("description", mPropertyModel.get(IDENTITY_DISC_DESCRIPTION));
        assertEquals(mDrawable, mPropertyModel.get(IDENTITY_DISC_IMAGE));

        Drawable testDrawable2 = mock(Drawable.class);
        doReturn(mMockConstantState).when(testDrawable2).getConstantState();
        doReturn(testDrawable2).when(mMockConstantState).newDrawable();
        mButtonData.setButtonSpec(new ButtonSpec(testDrawable2, mOnClickListener,
                /*onLongClickListener*/ null, /*contentDescription=*/"description",
                /*supportsTinting=*/false, /*iphCommandBuilder=*/null,
                AdaptiveToolbarButtonVariant.UNKNOWN, /*actionChipLabelResId=*/Resources.ID_NULL));
        mMediator.updateIdentityDisc(mButtonData);
        assertEquals(testDrawable2, mPropertyModel.get(IDENTITY_DISC_IMAGE));

        mButtonData.setCanShow(false);
        mMediator.updateIdentityDisc(mButtonData);
        assertFalse(mPropertyModel.get(IDENTITY_DISC_IS_VISIBLE));
    }

    @Test
    public void hideIdentityDiscInIncognito() {
        createMediator(false);
        assertFalse(mPropertyModel.get(IDENTITY_DISC_IS_VISIBLE));

        mMediator.onStartSurfaceStateChanged(
                StartSurfaceState.SHOWN_HOMEPAGE, true, LayoutType.START_SURFACE);
        assertFalse(mPropertyModel.get(IDENTITY_DISC_IS_VISIBLE));

        mButtonData.setCanShow(true);
        mMediator.updateIdentityDisc(mButtonData);
        assertTrue(mPropertyModel.get(IDENTITY_DISC_IS_VISIBLE));

        doReturn(true).when(mTabModelSelector).isIncognitoSelected();
        mTabModelSelectorObserver.getValue().onTabModelSelected(
                mock(TabModel.class), mock(TabModel.class));
        assertFalse(mPropertyModel.get(IDENTITY_DISC_IS_VISIBLE));
    }

    @Test
    public void showIPHOnIdentityDisc() {
        createMediator(false);
        assertFalse(mPropertyModel.get(IDENTITY_DISC_IS_VISIBLE));

        mMediator.onStartSurfaceStateChanged(
                StartSurfaceState.SHOWN_HOMEPAGE, true, LayoutType.START_SURFACE);
        mButtonData.setCanShow(true);
        IPHCommandBuilder iphCommandBuilder =
                new IPHCommandBuilder(mMockResources, "IdentityDisc", 0, 0)
                        .setOnDismissCallback(mDismissedCallback);
        mButtonData.setButtonSpec(new ButtonSpec(mDrawable, mOnClickListener,
                /*onLongClickListener*/ null, /*contentDescription=*/"",
                /*supportsTinting=*/false, /*iphCommandBuilder=*/iphCommandBuilder,
                AdaptiveToolbarButtonVariant.UNKNOWN, /*actionChipLabelResId=*/Resources.ID_NULL));

        mMediator.updateIdentityDisc(mButtonData);
        assertTrue(mPropertyModel.get(IDENTITY_DISC_IS_VISIBLE));

        verify(mMockIdentityIPHCallback, times(1))
                .onResult(mButtonData.getButtonSpec().getIPHCommandBuilder());
    }

    @Test
    public void testIdentityDiscStateChanges() {
        createMediator(false);
        assertFalse(mPropertyModel.get(IDENTITY_DISC_IS_VISIBLE));

        mMediator.onStartSurfaceStateChanged(
                StartSurfaceState.SHOWN_HOMEPAGE, true, LayoutType.START_SURFACE);
        assertFalse(mPropertyModel.get(IDENTITY_DISC_IS_VISIBLE));

        mButtonData.setCanShow(false);
        mMediator.buttonDataChanged(true);
        assertFalse(mPropertyModel.get(IDENTITY_DISC_IS_VISIBLE));

        mButtonData.setCanShow(true);
        mMediator.buttonDataChanged(true);
        assertTrue(mPropertyModel.get(IDENTITY_DISC_IS_VISIBLE));

        // updateIdentityDisc() should properly handle a hint that contradicts the true value of
        // canShow.
        mButtonData.setCanShow(false);
        mMediator.buttonDataChanged(true);
        assertFalse(mPropertyModel.get(IDENTITY_DISC_IS_VISIBLE));
    }

    @Test
    public void testNewTabButtonWithAccessibilityOnAndContinuationOn() {
        ChromeAccessibilityUtil.get().setAccessibilityEnabledForTesting(true);

        createMediator(false, true);
        mMediator.onStartSurfaceStateChanged(
                StartSurfaceState.SHOWN_HOMEPAGE, true, LayoutType.START_SURFACE);
        // When accessibility is turned on and TAB_GROUPS_CONTINUATION_ANDROID is enabled, new tab
        // button shouldn't show on homepage.
        assertFalse(mPropertyModel.get(NEW_TAB_VIEW_IS_VISIBLE));
    }

    @Test
    public void testNewTabButtonWithAccessibilityOnAndContinuationOff() {
        ChromeAccessibilityUtil.get().setAccessibilityEnabledForTesting(true);

        createMediator(false, false);
        mMediator.onStartSurfaceStateChanged(
                StartSurfaceState.SHOWN_HOMEPAGE, true, LayoutType.START_SURFACE);

        // When accessibility is turned on and TAB_GROUPS_CONTINUATION_ANDROID is disabled, new tab
        // button should show on homepage.
        assertTrue(mPropertyModel.get(NEW_TAB_VIEW_IS_VISIBLE));
    }

    @Test
    public void hideRealSearchBoxWhenStartSurfaceToolbarIsOnScreenTop() {
        createMediator(false);
        mMediator.onStartSurfaceStateChanged(
                StartSurfaceState.SHOWN_HOMEPAGE, true, LayoutType.START_SURFACE);
        int toolbarHeight = 10;

        assertEquals(0.0, mPropertyModel.get(TRANSLATION_Y), 0.0);
        assertFalse(mMediator.shouldShowRealSearchBox(toolbarHeight));
        assertTrue(mMediator.isOnHomepage());

        mPropertyModel.set(TRANSLATION_Y, -toolbarHeight);

        assertTrue(mMediator.shouldShowRealSearchBox(toolbarHeight));
        assertTrue(mMediator.isOnHomepage());
    }

    @Test
    @DisableFeatures(ChromeFeatureList.START_SURFACE_DISABLED_FEED_IMPROVEMENT)
    public void testLogoLoadOrDestroy() {
        createMediator(false);
        assertFalse(mMediator.isLogoVisibleForTesting());

        mMediator.onStartSurfaceStateChanged(
                StartSurfaceState.SHOWN_HOMEPAGE, true, LayoutType.START_SURFACE);
        assertTrue(mMediator.isLogoVisibleForTesting());

        mMediator.onStartSurfaceStateChanged(
                StartSurfaceState.SHOWN_TABSWITCHER, true, LayoutType.TAB_SWITCHER);
        assertFalse(mMediator.isLogoVisibleForTesting());
        verify(mLogoBridge).destroy(eq(1L), any());

        mMediator.onStartSurfaceStateChanged(
                StartSurfaceState.SHOWN_HOMEPAGE, true, LayoutType.START_SURFACE);
        assertTrue(mMediator.isLogoVisibleForTesting());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.START_SURFACE_DISABLED_FEED_IMPROVEMENT)
    public void testNotShowLogoWhenStartSurfaceDisabledFeedImprovementIsOn() {
        SharedPreferencesManager.getInstance().writeBoolean(
                ChromePreferenceKeys.FEED_ARTICLES_LIST_VISIBLE, false);
        createMediator(false);
        assertFalse(mMediator.isLogoVisibleForTesting());

        mMediator.onStartSurfaceStateChanged(
                StartSurfaceState.SHOWN_HOMEPAGE, true, LayoutType.START_SURFACE);
        assertFalse(mMediator.isLogoVisibleForTesting());

        mMediator.onStartSurfaceStateChanged(
                StartSurfaceState.SHOWN_TABSWITCHER, true, LayoutType.TAB_SWITCHER);
        assertFalse(mMediator.isLogoVisibleForTesting());
        verify(mLogoBridge, times(0)).destroy(eq(1L), any());

        mMediator.onStartSurfaceStateChanged(
                StartSurfaceState.SHOWN_HOMEPAGE, true, LayoutType.START_SURFACE);
        assertFalse(mMediator.isLogoVisibleForTesting());
    }

    private void createMediator(boolean hideIncognitoSwitchWhenNoTabs) {
        createMediator(hideIncognitoSwitchWhenNoTabs, false);
    }

    private void createMediator(
            boolean hideIncognitoSwitchWhenNoTabs, boolean isTabGroupsAndroidContinuationEnabled) {
        boolean shouldCreateLogoInToolbar =
                !ChromeFeatureList.sStartSurfaceDisabledFeedImprovement.isEnabled()
                || SharedPreferencesManager.getInstance().readBoolean(
                        ChromePreferenceKeys.FEED_ARTICLES_LIST_VISIBLE, true);
        mMediator = new StartSurfaceToolbarMediator(mContext, mPropertyModel,
                mMockIdentityIPHCallback, hideIncognitoSwitchWhenNoTabs, mMenuButtonCoordinator,
                mIdentityDiscController,
                ()
                        -> mIdentityDiscController.getForStartSurface(
                                mMediator.getOverviewModeStateForTesting(),
                                mMediator.getLayoutTypeForTesting()),
                /*isTabToGtsFadeAnimationEnabled=*/false, isTabGroupsAndroidContinuationEnabled,
                ()
                        -> false,
                /*logoClickedCallback=*/null,
                /*isRefactorEnabled=*/false, /*shouldFetchDoodle=*/false, shouldCreateLogoInToolbar,
                mFinishedShowingCallback, /*ToolbarColorObserverManager=*/null);

        mMediator.onLogoViewReady(mLogoView);
        mMediator.initLogoWithNative();

        mMediator.setTabModelSelector(mTabModelSelector);
        verify(mTabModelSelector).addObserver(mTabModelSelectorObserver.capture());
        verify(mTabModelSelector)
                .addIncognitoTabModelObserver(mIncognitoTabModelObserver.capture());

        mIncognitoTabModelObserver.getValue().didBecomeEmpty();
    }
}
