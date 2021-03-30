// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.top;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.AdditionalMatchers.not;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import static org.chromium.chrome.browser.toolbar.top.StartSurfaceToolbarProperties.BUTTONS_CLICKABLE;
import static org.chromium.chrome.browser.toolbar.top.StartSurfaceToolbarProperties.HOME_BUTTON_IS_VISIBLE;
import static org.chromium.chrome.browser.toolbar.top.StartSurfaceToolbarProperties.IDENTITY_DISC_AT_START;
import static org.chromium.chrome.browser.toolbar.top.StartSurfaceToolbarProperties.IDENTITY_DISC_CLICK_HANDLER;
import static org.chromium.chrome.browser.toolbar.top.StartSurfaceToolbarProperties.IDENTITY_DISC_DESCRIPTION;
import static org.chromium.chrome.browser.toolbar.top.StartSurfaceToolbarProperties.IDENTITY_DISC_IMAGE;
import static org.chromium.chrome.browser.toolbar.top.StartSurfaceToolbarProperties.IDENTITY_DISC_IS_VISIBLE;
import static org.chromium.chrome.browser.toolbar.top.StartSurfaceToolbarProperties.INCOGNITO_SWITCHER_VISIBLE;
import static org.chromium.chrome.browser.toolbar.top.StartSurfaceToolbarProperties.IN_START_SURFACE_MODE;
import static org.chromium.chrome.browser.toolbar.top.StartSurfaceToolbarProperties.IS_VISIBLE;
import static org.chromium.chrome.browser.toolbar.top.StartSurfaceToolbarProperties.LOGO_IS_VISIBLE;
import static org.chromium.chrome.browser.toolbar.top.StartSurfaceToolbarProperties.MENU_IS_VISIBLE;
import static org.chromium.chrome.browser.toolbar.top.StartSurfaceToolbarProperties.NEW_TAB_BUTTON_IS_VISIBLE;
import static org.chromium.chrome.browser.toolbar.top.StartSurfaceToolbarProperties.TAB_SWITCHER_BUTTON_IS_VISIBLE;

import android.content.res.Resources;
import android.graphics.drawable.Drawable;
import android.view.View;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;

import org.chromium.base.Callback;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.identity_disc.IdentityDiscController;
import org.chromium.chrome.browser.layouts.LayoutStateProvider;
import org.chromium.chrome.browser.layouts.LayoutType;
import org.chromium.chrome.browser.search_engines.TemplateUrlServiceFactory;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorObserver;
import org.chromium.chrome.browser.toolbar.ButtonData.ButtonSpec;
import org.chromium.chrome.browser.toolbar.ButtonDataImpl;
import org.chromium.chrome.browser.toolbar.menu_button.MenuButtonCoordinator;
import org.chromium.chrome.browser.user_education.IPHCommandBuilder;
import org.chromium.chrome.browser.util.ChromeAccessibilityUtil;
import org.chromium.chrome.features.start_surface.StartSurfaceState;
import org.chromium.components.search_engines.TemplateUrlService;
import org.chromium.components.search_engines.TemplateUrlService.TemplateUrlServiceObserver;
import org.chromium.ui.modelutil.PropertyModel;

/** Tests for {@link StartSurfaceToolbarMediator}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class StartSurfaceToolbarMediatorUnitTest {
    private PropertyModel mPropertyModel;
    private StartSurfaceToolbarMediator mMediator;
    @Mock
    private LayoutStateProvider mLayoutStateProvider;
    @Mock
    TemplateUrlService mTemplateUrlService;
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
    Callback<IPHCommandBuilder> mMockCallback;
    @Mock
    Tab mMockIncognitoTab;
    @Mock
    MenuButtonCoordinator mMenuButtonCoordinator;
    @Captor
    private ArgumentCaptor<LayoutStateProvider.LayoutStateObserver> mLayoutStateObserverCaptor;
    @Captor
    private ArgumentCaptor<TabModelSelectorObserver> mTabModelSelectorObserver;
    @Captor
    private ArgumentCaptor<TemplateUrlServiceObserver> mTemplateUrlServiceObserver;

    private ButtonDataImpl mButtonData;
    private ButtonDataImpl mDisabledButtonData;
    private ObservableSupplierImpl<Boolean> mIdentityDiscStateSupplier;
    private ObservableSupplierImpl<Boolean> mStartSurfaceAsHomepageSupplier;
    private ObservableSupplierImpl<Boolean> mHomepageEnabledSupplier;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);

        mPropertyModel = mPropertyModel =
                new PropertyModel.Builder(StartSurfaceToolbarProperties.ALL_KEYS)
                        .with(StartSurfaceToolbarProperties.INCOGNITO_SWITCHER_VISIBLE, true)
                        .with(StartSurfaceToolbarProperties.IN_START_SURFACE_MODE, false)
                        .with(StartSurfaceToolbarProperties.MENU_IS_VISIBLE, true)
                        .with(StartSurfaceToolbarProperties.IS_VISIBLE, true)
                        .build();
        mButtonData = new ButtonDataImpl(false, mDrawable, mOnClickListener, 0, false, null, true);
        mDisabledButtonData = new ButtonDataImpl(false, null, null, 0, false, null, true);
        mIdentityDiscStateSupplier = new ObservableSupplierImpl<>();
        mStartSurfaceAsHomepageSupplier = new ObservableSupplierImpl<>();
        mStartSurfaceAsHomepageSupplier.set(true);
        mHomepageEnabledSupplier = new ObservableSupplierImpl<>();
        mHomepageEnabledSupplier.set(true);

        doReturn(mButtonData)
                .when(mIdentityDiscController)
                .getForStartSurface(StartSurfaceState.SHOWN_HOMEPAGE);
        doReturn(mDisabledButtonData)
                .when(mIdentityDiscController)
                .getForStartSurface(not(eq(StartSurfaceState.SHOWN_HOMEPAGE)));

        mMockConstantState = mock(Drawable.ConstantState.class);
        doReturn(mMockConstantState).when(mDrawable).getConstantState();
        doReturn(mDrawable).when(mMockConstantState).newDrawable();

        TemplateUrlServiceFactory.setInstanceForTesting(mTemplateUrlService);
        doReturn(true).when(mTemplateUrlService).isDefaultSearchEngineGoogle();

        doReturn(false).when(mTabModelSelector).isIncognitoSelected();
        doReturn(mIncognitoTabModel).when(mTabModelSelector).getModel(true);
        doReturn(mMockIncognitoTab).when(mIncognitoTabModel).getTabAt(0);
        doReturn(false).when(mMockIncognitoTab).isClosing();
    }

    @After
    public void tearDown() {
    }

    @Test
    public void showAndHide() {
        createMediator(false);
        assertFalse(mPropertyModel.get(IN_START_SURFACE_MODE));
        assertTrue(mPropertyModel.get(IS_VISIBLE));

        mMediator.setStartSurfaceMode(true);
        assertTrue(mPropertyModel.get(IN_START_SURFACE_MODE));
        assertTrue(mPropertyModel.get(IS_VISIBLE));
        assertFalse(mPropertyModel.get(BUTTONS_CLICKABLE));
        assertFalse(mPropertyModel.get(LOGO_IS_VISIBLE));
        assertFalse(mPropertyModel.get(NEW_TAB_BUTTON_IS_VISIBLE));
        assertFalse(mPropertyModel.get(IDENTITY_DISC_IS_VISIBLE));
        assertTrue(mPropertyModel.get(MENU_IS_VISIBLE));
        assertEquals(true, mPropertyModel.get(INCOGNITO_SWITCHER_VISIBLE));

        mMediator.setStartSurfaceToolbarVisibility(false);
        assertTrue(mPropertyModel.get(IN_START_SURFACE_MODE));
        assertFalse(mPropertyModel.get(IS_VISIBLE));

        mMediator.setStartSurfaceToolbarVisibility(true);
        assertTrue(mPropertyModel.get(IN_START_SURFACE_MODE));
        assertTrue(mPropertyModel.get(IS_VISIBLE));

        mMediator.setStartSurfaceMode(false);
        assertFalse(mPropertyModel.get(IN_START_SURFACE_MODE));
        assertTrue(mPropertyModel.get(IS_VISIBLE));
        assertFalse(mPropertyModel.get(BUTTONS_CLICKABLE));
        assertFalse(mPropertyModel.get(LOGO_IS_VISIBLE));
        assertFalse(mPropertyModel.get(NEW_TAB_BUTTON_IS_VISIBLE));
        assertFalse(mPropertyModel.get(IDENTITY_DISC_IS_VISIBLE));
        assertTrue(mPropertyModel.get(MENU_IS_VISIBLE));
        assertEquals(true, mPropertyModel.get(INCOGNITO_SWITCHER_VISIBLE));
    }

    @Test
    public void showAndHideSetClickable() {
        createMediator(false);
        mMediator.setStartSurfaceMode(true);
        assertFalse(mPropertyModel.get(BUTTONS_CLICKABLE));

        mLayoutStateObserverCaptor.getValue().onFinishedShowing(LayoutType.TAB_SWITCHER);
        assertTrue(mPropertyModel.get(BUTTONS_CLICKABLE));

        mLayoutStateObserverCaptor.getValue().onStartedHiding(LayoutType.TAB_SWITCHER, true, false);
        assertFalse(mPropertyModel.get(BUTTONS_CLICKABLE));
    }

    @Test
    public void showAndHideHomePage() {
        createMediator(false);
        mMediator.setTabModelSelector(mTabModelSelector);

        doReturn(false).when(mTemplateUrlService).isDefaultSearchEngineGoogle();
        mMediator.onNativeLibraryReady();
        verify(mTemplateUrlService).addObserver(mTemplateUrlServiceObserver.capture());
        assertFalse(mPropertyModel.get(LOGO_IS_VISIBLE));
        assertFalse(mPropertyModel.get(IDENTITY_DISC_IS_VISIBLE));
        assertEquals(true, mPropertyModel.get(INCOGNITO_SWITCHER_VISIBLE));
        assertFalse(mPropertyModel.get(IN_START_SURFACE_MODE));
        assertTrue(mPropertyModel.get(IS_VISIBLE));

        mMediator.setStartSurfaceMode(true);
        assertFalse(mPropertyModel.get(LOGO_IS_VISIBLE));
        assertFalse(mPropertyModel.get(IDENTITY_DISC_IS_VISIBLE));
        assertEquals(true, mPropertyModel.get(INCOGNITO_SWITCHER_VISIBLE));
        assertTrue(mPropertyModel.get(IN_START_SURFACE_MODE));
        assertTrue(mPropertyModel.get(IS_VISIBLE));

        mLayoutStateObserverCaptor.getValue().onStartedShowing(LayoutType.TAB_SWITCHER, false);
        mLayoutStateObserverCaptor.getValue().onFinishedShowing(LayoutType.TAB_SWITCHER);
        mMediator.onStartSurfaceStateChanged(StartSurfaceState.SHOWN_HOMEPAGE, true);
        assertFalse(mPropertyModel.get(LOGO_IS_VISIBLE));
        assertFalse(mPropertyModel.get(IDENTITY_DISC_IS_VISIBLE));
        assertEquals(false, mPropertyModel.get(INCOGNITO_SWITCHER_VISIBLE));
        assertTrue(mPropertyModel.get(IN_START_SURFACE_MODE));
        assertTrue(mPropertyModel.get(IS_VISIBLE));
    }

    @Test
    public void showAndHideHomePage_HideIncognitoSwitch() {
        createMediator(false);
        mMediator.setTabModelSelector(mTabModelSelector);

        doReturn(true).when(mTemplateUrlService).isDefaultSearchEngineGoogle();
        mMediator.onNativeLibraryReady();
        verify(mTemplateUrlService).addObserver(mTemplateUrlServiceObserver.capture());
        mMediator.setStartSurfaceMode(true);

        mLayoutStateObserverCaptor.getValue().onStartedShowing(LayoutType.TAB_SWITCHER, false);
        mLayoutStateObserverCaptor.getValue().onFinishedShowing(LayoutType.TAB_SWITCHER);
        mMediator.onStartSurfaceStateChanged(StartSurfaceState.SHOWN_HOMEPAGE, true);
        assertTrue(mPropertyModel.get(LOGO_IS_VISIBLE));
        assertFalse(mPropertyModel.get(IDENTITY_DISC_IS_VISIBLE));
        assertEquals(false, mPropertyModel.get(INCOGNITO_SWITCHER_VISIBLE));
        assertTrue(mPropertyModel.get(IN_START_SURFACE_MODE));
        assertTrue(mPropertyModel.get(IS_VISIBLE));

        mMediator.onStartSurfaceStateChanged(StartSurfaceState.SHOWN_TABSWITCHER, true);
        assertFalse(mPropertyModel.get(LOGO_IS_VISIBLE));
        assertFalse(mPropertyModel.get(IDENTITY_DISC_IS_VISIBLE));
        assertEquals(true, mPropertyModel.get(INCOGNITO_SWITCHER_VISIBLE));
        assertTrue(mPropertyModel.get(IN_START_SURFACE_MODE));
        assertTrue(mPropertyModel.get(IS_VISIBLE));
    }

    @Test
    public void showAndHideHomePageNoIncognitoTabs() {
        createMediator(true);
        mMediator.setTabModelSelector(mTabModelSelector);
        doReturn(0).when(mIncognitoTabModel).getCount();

        doReturn(false).when(mTemplateUrlService).isDefaultSearchEngineGoogle();
        mMediator.onNativeLibraryReady();
        verify(mTemplateUrlService).addObserver(mTemplateUrlServiceObserver.capture());
        assertFalse(mPropertyModel.get(LOGO_IS_VISIBLE));
        assertFalse(mPropertyModel.get(IDENTITY_DISC_IS_VISIBLE));
        assertEquals(true, mPropertyModel.get(INCOGNITO_SWITCHER_VISIBLE));
        assertFalse(mPropertyModel.get(IN_START_SURFACE_MODE));
        assertTrue(mPropertyModel.get(IS_VISIBLE));

        mMediator.setStartSurfaceMode(true);
        assertFalse(mPropertyModel.get(LOGO_IS_VISIBLE));
        assertFalse(mPropertyModel.get(IDENTITY_DISC_IS_VISIBLE));
        assertEquals(true, mPropertyModel.get(INCOGNITO_SWITCHER_VISIBLE));
        assertTrue(mPropertyModel.get(IN_START_SURFACE_MODE));
        assertTrue(mPropertyModel.get(IS_VISIBLE));

        mLayoutStateObserverCaptor.getValue().onStartedShowing(LayoutType.TAB_SWITCHER, false);
        mLayoutStateObserverCaptor.getValue().onFinishedShowing(LayoutType.TAB_SWITCHER);
        mMediator.onStartSurfaceStateChanged(StartSurfaceState.SHOWN_HOMEPAGE, true);
        assertFalse(mPropertyModel.get(LOGO_IS_VISIBLE));
        assertFalse(mPropertyModel.get(IDENTITY_DISC_IS_VISIBLE));
        assertEquals(false, mPropertyModel.get(INCOGNITO_SWITCHER_VISIBLE));
        assertTrue(mPropertyModel.get(IN_START_SURFACE_MODE));
        assertTrue(mPropertyModel.get(IS_VISIBLE));

        mLayoutStateObserverCaptor.getValue().onStartedHiding(LayoutType.TAB_SWITCHER, true, false);
        mLayoutStateObserverCaptor.getValue().onFinishedHiding(LayoutType.TAB_SWITCHER);
        doReturn(1).when(mIncognitoTabModel).getCount();

        mLayoutStateObserverCaptor.getValue().onStartedShowing(LayoutType.TAB_SWITCHER, false);
        mLayoutStateObserverCaptor.getValue().onFinishedShowing(LayoutType.TAB_SWITCHER);
        mMediator.onStartSurfaceStateChanged(StartSurfaceState.SHOWN_HOMEPAGE, true);
        assertFalse(mPropertyModel.get(LOGO_IS_VISIBLE));
        assertFalse(mPropertyModel.get(IDENTITY_DISC_IS_VISIBLE));
        assertEquals(false, mPropertyModel.get(INCOGNITO_SWITCHER_VISIBLE));
        assertTrue(mPropertyModel.get(IN_START_SURFACE_MODE));
        assertTrue(mPropertyModel.get(IS_VISIBLE));
    }

    @Test
    public void showHomePageWithLogo() {
        createMediator(false);
        verify(mLayoutStateProvider).addObserver(mLayoutStateObserverCaptor.capture());

        mMediator.onNativeLibraryReady();
        verify(mTemplateUrlService).addObserver(mTemplateUrlServiceObserver.capture());
        assertFalse(mPropertyModel.get(LOGO_IS_VISIBLE));

        mMediator.setStartSurfaceMode(true);
        assertFalse(mPropertyModel.get(LOGO_IS_VISIBLE));

        mLayoutStateObserverCaptor.getValue().onStartedShowing(LayoutType.TAB_SWITCHER, false);
        mLayoutStateObserverCaptor.getValue().onFinishedShowing(LayoutType.TAB_SWITCHER);
        mMediator.onStartSurfaceStateChanged(StartSurfaceState.SHOWN_HOMEPAGE, true);
        assertTrue(mPropertyModel.get(LOGO_IS_VISIBLE));
    }

    @Test
    public void enableDisableLogo() {
        createMediator(false);
        doReturn(false).when(mTemplateUrlService).isDefaultSearchEngineGoogle();
        mMediator.onNativeLibraryReady();
        verify(mTemplateUrlService).addObserver(mTemplateUrlServiceObserver.capture());
        mMediator.onStartSurfaceStateChanged(StartSurfaceState.SHOWN_HOMEPAGE, true);
        mMediator.setStartSurfaceMode(true);
        assertFalse(mPropertyModel.get(LOGO_IS_VISIBLE));

        doReturn(true).when(mTemplateUrlService).isDefaultSearchEngineGoogle();
        mTemplateUrlServiceObserver.getValue().onTemplateURLServiceChanged();
        assertTrue(mPropertyModel.get(LOGO_IS_VISIBLE));

        doReturn(false).when(mTemplateUrlService).isDefaultSearchEngineGoogle();
        mTemplateUrlServiceObserver.getValue().onTemplateURLServiceChanged();
        assertFalse(mPropertyModel.get(LOGO_IS_VISIBLE));
    }

    @Test
    public void showHomePageWithIdentityDisc() {
        createMediator(false);
        mMediator.setTabModelSelector(mTabModelSelector);
        assertFalse(mPropertyModel.get(IDENTITY_DISC_IS_VISIBLE));

        mMediator.setStartSurfaceMode(true);
        mMediator.onStartSurfaceStateChanged(StartSurfaceState.SHOWN_HOMEPAGE, true);
        assertFalse(mPropertyModel.get(IDENTITY_DISC_IS_VISIBLE));

        mButtonData.setButtonSpec(
                new ButtonSpec(mDrawable, mOnClickListener, /*contentDescriptionResId=*/5,
                        /*supportsTinting=*/false, /*iphCommandBuilder=*/null));
        mButtonData.setCanShow(true);
        mMediator.updateIdentityDisc(mButtonData);
        assertTrue(mPropertyModel.get(IDENTITY_DISC_IS_VISIBLE));
        assertEquals(mOnClickListener, mPropertyModel.get(IDENTITY_DISC_CLICK_HANDLER));
        assertEquals(5, mPropertyModel.get(IDENTITY_DISC_DESCRIPTION));
        assertEquals(mDrawable, mPropertyModel.get(IDENTITY_DISC_IMAGE));

        Drawable testDrawable2 = mock(Drawable.class);
        doReturn(mMockConstantState).when(testDrawable2).getConstantState();
        doReturn(testDrawable2).when(mMockConstantState).newDrawable();
        mButtonData.setButtonSpec(
                new ButtonSpec(testDrawable2, mOnClickListener, /*contentDescriptionResId=*/5,
                        /*supportsTinting=*/false, /*iphCommandBuilder=*/null));
        mMediator.updateIdentityDisc(mButtonData);
        assertEquals(testDrawable2, mPropertyModel.get(IDENTITY_DISC_IMAGE));

        mButtonData.setCanShow(false);
        mMediator.updateIdentityDisc(mButtonData);
        assertFalse(mPropertyModel.get(IDENTITY_DISC_IS_VISIBLE));
    }

    @Test
    public void hideIdentityDiscInIncognito() {
        createMediator(false);
        mMediator.setTabModelSelector(mTabModelSelector);
        verify(mTabModelSelector).addObserver(mTabModelSelectorObserver.capture());

        assertFalse(mPropertyModel.get(IDENTITY_DISC_IS_VISIBLE));

        mMediator.setStartSurfaceMode(true);
        mMediator.onStartSurfaceStateChanged(StartSurfaceState.SHOWN_HOMEPAGE, true);
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
        mMediator.setTabModelSelector(mTabModelSelector);
        assertFalse(mPropertyModel.get(IDENTITY_DISC_IS_VISIBLE));

        mMediator.setStartSurfaceMode(true);
        mMediator.onStartSurfaceStateChanged(StartSurfaceState.SHOWN_HOMEPAGE, true);
        mButtonData.setCanShow(true);
        IPHCommandBuilder iphCommandBuilder =
                new IPHCommandBuilder(mMockResources, "IdentityDisc", 0, 0)
                        .setOnDismissCallback(mDismissedCallback);
        mButtonData.setButtonSpec(
                new ButtonSpec(mDrawable, mOnClickListener, /*contentDescriptionResId=*/0,
                        /*supportsTinting=*/false, /*iphCommandBuilder=*/iphCommandBuilder));

        mMediator.updateIdentityDisc(mButtonData);
        assertTrue(mPropertyModel.get(IDENTITY_DISC_IS_VISIBLE));

        verify(mMockCallback, times(1))
                .onResult(mButtonData.getButtonSpec().getIPHCommandBuilder());
    }

    @Test
    public void showTabSwitcher() {
        createMediator(false);
        mMediator.setTabModelSelector(mTabModelSelector);
        mMediator.onNativeLibraryReady();
        verify(mTemplateUrlService).addObserver(mTemplateUrlServiceObserver.capture());

        assertFalse(mPropertyModel.get(LOGO_IS_VISIBLE));
        assertFalse(mPropertyModel.get(IDENTITY_DISC_IS_VISIBLE));
        assertEquals(true, mPropertyModel.get(INCOGNITO_SWITCHER_VISIBLE));
        assertTrue(mPropertyModel.get(IS_VISIBLE));
        assertFalse(mPropertyModel.get(IN_START_SURFACE_MODE));

        mMediator.setStartSurfaceMode(true);
        assertFalse(mPropertyModel.get(LOGO_IS_VISIBLE));
        assertFalse(mPropertyModel.get(IDENTITY_DISC_IS_VISIBLE));
        assertEquals(true, mPropertyModel.get(INCOGNITO_SWITCHER_VISIBLE));
        assertTrue(mPropertyModel.get(IS_VISIBLE));
        assertTrue(mPropertyModel.get(IN_START_SURFACE_MODE));

        mLayoutStateObserverCaptor.getValue().onStartedShowing(LayoutType.TAB_SWITCHER, false);
        mLayoutStateObserverCaptor.getValue().onFinishedShowing(LayoutType.TAB_SWITCHER);
        mMediator.onStartSurfaceStateChanged(StartSurfaceState.SHOWN_TABSWITCHER, true);
        assertFalse(mPropertyModel.get(LOGO_IS_VISIBLE));
        assertFalse(mPropertyModel.get(IDENTITY_DISC_IS_VISIBLE));
        assertEquals(true, mPropertyModel.get(INCOGNITO_SWITCHER_VISIBLE));
        assertTrue(mPropertyModel.get(IS_VISIBLE));

        mMediator.updateIdentityDisc(mButtonData);
        assertFalse(mPropertyModel.get(IDENTITY_DISC_IS_VISIBLE));
    }

    @Test
    public void showTabSwitcherNoIncognitoTabs() {
        createMediator(true);
        mMediator.setTabModelSelector(mTabModelSelector);
        doReturn(0).when(mIncognitoTabModel).getCount();
        mMediator.onNativeLibraryReady();
        verify(mTemplateUrlService).addObserver(mTemplateUrlServiceObserver.capture());

        assertFalse(mPropertyModel.get(LOGO_IS_VISIBLE));
        assertFalse(mPropertyModel.get(IDENTITY_DISC_IS_VISIBLE));
        assertEquals(true, mPropertyModel.get(INCOGNITO_SWITCHER_VISIBLE));
        assertTrue(mPropertyModel.get(IS_VISIBLE));
        assertFalse(mPropertyModel.get(IN_START_SURFACE_MODE));

        mMediator.setStartSurfaceMode(true);
        assertFalse(mPropertyModel.get(LOGO_IS_VISIBLE));
        assertFalse(mPropertyModel.get(IDENTITY_DISC_IS_VISIBLE));
        assertEquals(true, mPropertyModel.get(INCOGNITO_SWITCHER_VISIBLE));
        assertTrue(mPropertyModel.get(IS_VISIBLE));
        assertTrue(mPropertyModel.get(IN_START_SURFACE_MODE));

        mLayoutStateObserverCaptor.getValue().onStartedShowing(LayoutType.TAB_SWITCHER, false);
        mLayoutStateObserverCaptor.getValue().onFinishedShowing(LayoutType.TAB_SWITCHER);
        mMediator.onStartSurfaceStateChanged(StartSurfaceState.SHOWN_TABSWITCHER, true);
        assertFalse(mPropertyModel.get(LOGO_IS_VISIBLE));
        assertFalse(mPropertyModel.get(IDENTITY_DISC_IS_VISIBLE));
        assertEquals(false, mPropertyModel.get(INCOGNITO_SWITCHER_VISIBLE));
        assertTrue(mPropertyModel.get(IS_VISIBLE));

        mMediator.updateIdentityDisc(mButtonData);
        assertFalse(mPropertyModel.get(IDENTITY_DISC_IS_VISIBLE));

        mLayoutStateObserverCaptor.getValue().onStartedHiding(LayoutType.TAB_SWITCHER, true, false);
        mLayoutStateObserverCaptor.getValue().onFinishedHiding(LayoutType.TAB_SWITCHER);
        doReturn(1).when(mIncognitoTabModel).getCount();

        mLayoutStateObserverCaptor.getValue().onStartedShowing(LayoutType.TAB_SWITCHER, false);
        mLayoutStateObserverCaptor.getValue().onFinishedShowing(LayoutType.TAB_SWITCHER);
        mMediator.onStartSurfaceStateChanged(StartSurfaceState.SHOWN_TABSWITCHER, true);
        assertFalse(mPropertyModel.get(LOGO_IS_VISIBLE));
        assertFalse(mPropertyModel.get(IDENTITY_DISC_IS_VISIBLE));
        assertEquals(true, mPropertyModel.get(INCOGNITO_SWITCHER_VISIBLE));
        assertTrue(mPropertyModel.get(IS_VISIBLE));
    }

    @Test
    public void homePageToTabswitcher() {
        createMediator(false);
        mMediator.setTabModelSelector(mTabModelSelector);

        mMediator.onNativeLibraryReady();
        verify(mTemplateUrlService).addObserver(mTemplateUrlServiceObserver.capture());
        mButtonData.setCanShow(true);
        mMediator.updateIdentityDisc(mButtonData);
        mMediator.setStartSurfaceMode(true);
        mMediator.onStartSurfaceStateChanged(StartSurfaceState.SHOWN_HOMEPAGE, true);
        assertTrue(mPropertyModel.get(LOGO_IS_VISIBLE));
        assertTrue(mPropertyModel.get(IDENTITY_DISC_IS_VISIBLE));

        mMediator.onStartSurfaceStateChanged(StartSurfaceState.SHOWN_TABSWITCHER, true);
        assertFalse(mPropertyModel.get(LOGO_IS_VISIBLE));
        assertFalse(mPropertyModel.get(IDENTITY_DISC_IS_VISIBLE));

        mMediator.onStartSurfaceStateChanged(StartSurfaceState.SHOWN_HOMEPAGE, true);
        assertTrue(mPropertyModel.get(LOGO_IS_VISIBLE));
        assertTrue(mPropertyModel.get(IDENTITY_DISC_IS_VISIBLE));
    }

    @Test
    public void showTabswitcherTasksOnly() {
        createMediator(false);
        mMediator.setTabModelSelector(mTabModelSelector);
        mMediator.onNativeLibraryReady();
        verify(mTemplateUrlService).addObserver(mTemplateUrlServiceObserver.capture());

        mMediator.setStartSurfaceMode(true);
        mLayoutStateObserverCaptor.getValue().onStartedShowing(LayoutType.TAB_SWITCHER, false);
        mLayoutStateObserverCaptor.getValue().onFinishedShowing(LayoutType.TAB_SWITCHER);
        mMediator.onStartSurfaceStateChanged(StartSurfaceState.SHOWN_TABSWITCHER_TASKS_ONLY, true);
        assertTrue(mPropertyModel.get(LOGO_IS_VISIBLE));
        assertFalse(mPropertyModel.get(IDENTITY_DISC_IS_VISIBLE));
        assertEquals(true, mPropertyModel.get(INCOGNITO_SWITCHER_VISIBLE));
        assertTrue(mPropertyModel.get(IN_START_SURFACE_MODE));
        assertTrue(mPropertyModel.get(IS_VISIBLE));
        assertTrue(mPropertyModel.get(IN_START_SURFACE_MODE));

        mMediator.setStartSurfaceMode(false);
        assertFalse(mPropertyModel.get(IN_START_SURFACE_MODE));
    }

    @Test
    public void showTabswitcherOmniboxOnlyNoIncognitoTabs() {
        createMediator(true);
        mMediator.setTabModelSelector(mTabModelSelector);
        doReturn(0).when(mIncognitoTabModel).getCount();
        mMediator.onNativeLibraryReady();
        verify(mTemplateUrlService).addObserver(mTemplateUrlServiceObserver.capture());

        mMediator.setStartSurfaceMode(true);
        mMediator.onStartSurfaceStateChanged(
                StartSurfaceState.SHOWN_TABSWITCHER_OMNIBOX_ONLY, true);
        mLayoutStateObserverCaptor.getValue().onStartedShowing(LayoutType.TAB_SWITCHER, false);
        assertTrue(mPropertyModel.get(LOGO_IS_VISIBLE));
        assertFalse(mPropertyModel.get(IDENTITY_DISC_IS_VISIBLE));
        assertEquals(false, mPropertyModel.get(INCOGNITO_SWITCHER_VISIBLE));
        assertTrue(mPropertyModel.get(IS_VISIBLE));
        assertTrue(mPropertyModel.get(IN_START_SURFACE_MODE));

        mMediator.setStartSurfaceMode(false);
        assertFalse(mPropertyModel.get(IN_START_SURFACE_MODE));
    }

    @Test
    public void testIdentityDiscStateChanges() {
        createMediator(false);
        mMediator.setTabModelSelector(mTabModelSelector);
        assertFalse(mPropertyModel.get(IDENTITY_DISC_IS_VISIBLE));

        mMediator.setStartSurfaceMode(true);
        mMediator.onStartSurfaceStateChanged(StartSurfaceState.SHOWN_HOMEPAGE, true);
        assertFalse(mPropertyModel.get(IDENTITY_DISC_IS_VISIBLE));

        mButtonData.setCanShow(true);

        mIdentityDiscStateSupplier.set(true);
        assertTrue(mPropertyModel.get(IDENTITY_DISC_IS_VISIBLE));

        mButtonData.setCanShow(false);
        mIdentityDiscStateSupplier.set(false);
        assertFalse(mPropertyModel.get(IDENTITY_DISC_IS_VISIBLE));

        // updateIdentityDisc() should properly handle a hint that contradicts the true value of
        // canShow.
        mButtonData.setCanShow(false);
        mIdentityDiscStateSupplier.set(true);
        assertFalse(mPropertyModel.get(IDENTITY_DISC_IS_VISIBLE));
    }

    @Test
    public void testShowAndHideHomePage() {
        createMediator(false);
        mMediator.setTabModelSelector(mTabModelSelector);
        doReturn(0).when(mIncognitoTabModel).getCount();

        doReturn(false).when(mTemplateUrlService).isDefaultSearchEngineGoogle();
        mMediator.onNativeLibraryReady();
        verify(mTemplateUrlService).addObserver(mTemplateUrlServiceObserver.capture());
        assertFalse(mPropertyModel.get(LOGO_IS_VISIBLE));
        assertFalse(mPropertyModel.get(IDENTITY_DISC_IS_VISIBLE));
        assertFalse(mPropertyModel.get(IDENTITY_DISC_AT_START));
        assertEquals(true, mPropertyModel.get(INCOGNITO_SWITCHER_VISIBLE));
        assertFalse(mPropertyModel.get(IN_START_SURFACE_MODE));
        assertTrue(mPropertyModel.get(IS_VISIBLE));

        mMediator.setStartSurfaceMode(true);
        assertFalse(mPropertyModel.get(LOGO_IS_VISIBLE));
        assertFalse(mPropertyModel.get(IDENTITY_DISC_IS_VISIBLE));
        assertFalse(mPropertyModel.get(IDENTITY_DISC_AT_START));
        assertEquals(true, mPropertyModel.get(INCOGNITO_SWITCHER_VISIBLE));
        assertTrue(mPropertyModel.get(IN_START_SURFACE_MODE));
        assertTrue(mPropertyModel.get(IS_VISIBLE));

        mLayoutStateObserverCaptor.getValue().onStartedShowing(LayoutType.TAB_SWITCHER, false);
        mLayoutStateObserverCaptor.getValue().onFinishedShowing(LayoutType.TAB_SWITCHER);
        mMediator.onStartSurfaceStateChanged(StartSurfaceState.SHOWN_HOMEPAGE, true);
        assertFalse(mPropertyModel.get(LOGO_IS_VISIBLE));
        assertFalse(mPropertyModel.get(IDENTITY_DISC_IS_VISIBLE));
        assertFalse(mPropertyModel.get(IDENTITY_DISC_AT_START));
        assertEquals(false, mPropertyModel.get(INCOGNITO_SWITCHER_VISIBLE));
        assertTrue(mPropertyModel.get(IN_START_SURFACE_MODE));
        assertTrue(mPropertyModel.get(IS_VISIBLE));

        mLayoutStateObserverCaptor.getValue().onStartedHiding(LayoutType.TAB_SWITCHER, true, false);
        mLayoutStateObserverCaptor.getValue().onFinishedHiding(LayoutType.TAB_SWITCHER);
        doReturn(1).when(mIncognitoTabModel).getCount();

        mLayoutStateObserverCaptor.getValue().onStartedShowing(LayoutType.TAB_SWITCHER, false);
        mLayoutStateObserverCaptor.getValue().onFinishedShowing(LayoutType.TAB_SWITCHER);
        mMediator.onStartSurfaceStateChanged(StartSurfaceState.SHOWN_HOMEPAGE, true);
        assertFalse(mPropertyModel.get(LOGO_IS_VISIBLE));
        assertFalse(mPropertyModel.get(IDENTITY_DISC_IS_VISIBLE));
        assertFalse(mPropertyModel.get(IDENTITY_DISC_AT_START));
        assertEquals(false, mPropertyModel.get(INCOGNITO_SWITCHER_VISIBLE));
        assertTrue(mPropertyModel.get(IN_START_SURFACE_MODE));
        assertTrue(mPropertyModel.get(IS_VISIBLE));
    }

    @Test
    public void testShowAndHideTabSwitcher() {
        createMediator(false);
        mMediator.setTabModelSelector(mTabModelSelector);
        doReturn(0).when(mIncognitoTabModel).getCount();
        mMediator.onNativeLibraryReady();
        verify(mTemplateUrlService).addObserver(mTemplateUrlServiceObserver.capture());

        assertFalse(mPropertyModel.get(LOGO_IS_VISIBLE));
        assertFalse(mPropertyModel.get(IDENTITY_DISC_IS_VISIBLE));
        assertFalse(mPropertyModel.get(IDENTITY_DISC_AT_START));
        assertEquals(true, mPropertyModel.get(INCOGNITO_SWITCHER_VISIBLE));
        assertTrue(mPropertyModel.get(IS_VISIBLE));
        assertFalse(mPropertyModel.get(IN_START_SURFACE_MODE));

        mMediator.setStartSurfaceMode(true);
        assertFalse(mPropertyModel.get(LOGO_IS_VISIBLE));
        assertFalse(mPropertyModel.get(IDENTITY_DISC_IS_VISIBLE));
        assertFalse(mPropertyModel.get(IDENTITY_DISC_AT_START));
        assertEquals(true, mPropertyModel.get(INCOGNITO_SWITCHER_VISIBLE));
        assertTrue(mPropertyModel.get(IS_VISIBLE));
        assertTrue(mPropertyModel.get(IN_START_SURFACE_MODE));

        mLayoutStateObserverCaptor.getValue().onStartedShowing(LayoutType.TAB_SWITCHER, false);
        mLayoutStateObserverCaptor.getValue().onFinishedShowing(LayoutType.TAB_SWITCHER);
        mMediator.onStartSurfaceStateChanged(StartSurfaceState.SHOWN_TABSWITCHER, true);
        assertFalse(mPropertyModel.get(LOGO_IS_VISIBLE));
        assertFalse(mPropertyModel.get(IDENTITY_DISC_IS_VISIBLE));
        assertFalse(mPropertyModel.get(IDENTITY_DISC_AT_START));
        assertEquals(true, mPropertyModel.get(INCOGNITO_SWITCHER_VISIBLE));
        assertTrue(mPropertyModel.get(IS_VISIBLE));

        mMediator.updateIdentityDisc(mButtonData);
        assertFalse(mPropertyModel.get(IDENTITY_DISC_IS_VISIBLE));

        mLayoutStateObserverCaptor.getValue().onStartedHiding(LayoutType.TAB_SWITCHER, true, false);
        mLayoutStateObserverCaptor.getValue().onFinishedHiding(LayoutType.TAB_SWITCHER);
        doReturn(1).when(mIncognitoTabModel).getCount();

        mLayoutStateObserverCaptor.getValue().onStartedShowing(LayoutType.TAB_SWITCHER, false);
        mLayoutStateObserverCaptor.getValue().onFinishedShowing(LayoutType.TAB_SWITCHER);
        mMediator.onStartSurfaceStateChanged(StartSurfaceState.SHOWN_TABSWITCHER, true);
        assertFalse(mPropertyModel.get(LOGO_IS_VISIBLE));
        assertFalse(mPropertyModel.get(IDENTITY_DISC_IS_VISIBLE));
        assertFalse(mPropertyModel.get(IDENTITY_DISC_AT_START));
        assertEquals(true, mPropertyModel.get(INCOGNITO_SWITCHER_VISIBLE));
        assertTrue(mPropertyModel.get(IS_VISIBLE));
    }

    @Test
    public void testShowHomeButtonInTabSwitcher() {
        createMediator(false, true, false, false);
        mMediator.setTabModelSelector(mTabModelSelector);
        doReturn(0).when(mIncognitoTabModel).getCount();
        mMediator.onNativeLibraryReady();
        verify(mTemplateUrlService).addObserver(mTemplateUrlServiceObserver.capture());
        assertFalse(mPropertyModel.get(HOME_BUTTON_IS_VISIBLE));

        mMediator.setStartSurfaceMode(true);
        mLayoutStateObserverCaptor.getValue().onStartedShowing(LayoutType.TAB_SWITCHER, false);
        mLayoutStateObserverCaptor.getValue().onFinishedShowing(LayoutType.TAB_SWITCHER);
        mMediator.onStartSurfaceStateChanged(StartSurfaceState.SHOWN_HOMEPAGE, true);
        assertTrue(mPropertyModel.get(IN_START_SURFACE_MODE));
        assertFalse(mPropertyModel.get(HOME_BUTTON_IS_VISIBLE));

        mLayoutStateObserverCaptor.getValue().onStartedShowing(LayoutType.TAB_SWITCHER, false);
        mLayoutStateObserverCaptor.getValue().onFinishedShowing(LayoutType.TAB_SWITCHER);
        mMediator.onStartSurfaceStateChanged(StartSurfaceState.SHOWN_TABSWITCHER, true);
        assertTrue(mPropertyModel.get(HOME_BUTTON_IS_VISIBLE));

        mMediator.setShowHomeButtonOnTabSwitcherForTesting(false);
        mLayoutStateObserverCaptor.getValue().onStartedShowing(LayoutType.TAB_SWITCHER, false);
        mLayoutStateObserverCaptor.getValue().onFinishedShowing(LayoutType.TAB_SWITCHER);
        mMediator.onStartSurfaceStateChanged(StartSurfaceState.SHOWN_TABSWITCHER, true);
        assertFalse(mPropertyModel.get(HOME_BUTTON_IS_VISIBLE));
    }

    @Test
    public void testNewHomeSurface() {
        createMediator(false, true, true, false);
        mMediator.setTabModelSelector(mTabModelSelector);
        assertFalse(mPropertyModel.get(IDENTITY_DISC_IS_VISIBLE));

        mMediator.setStartSurfaceMode(true);
        mMediator.onStartSurfaceStateChanged(StartSurfaceState.SHOWN_HOMEPAGE, true);

        // Identity disc should be shown at start on homepage.
        assertFalse(mPropertyModel.get(IDENTITY_DISC_IS_VISIBLE));
        mButtonData.setCanShow(true);
        mButtonData.setButtonSpec(
                new ButtonSpec(mDrawable, mOnClickListener, /*contentDescriptionResId=*/5,
                        /*supportsTinting=*/false, /*iphCommandBuilder=*/null));
        mMediator.updateIdentityDisc(mButtonData);
        assertTrue(mPropertyModel.get(IDENTITY_DISC_IS_VISIBLE));
        assertTrue(mPropertyModel.get(IDENTITY_DISC_AT_START));

        assertTrue(mPropertyModel.get(IS_VISIBLE));
        assertTrue(mPropertyModel.get(TAB_SWITCHER_BUTTON_IS_VISIBLE));

        mLayoutStateObserverCaptor.getValue().onStartedShowing(LayoutType.TAB_SWITCHER, false);
        mLayoutStateObserverCaptor.getValue().onFinishedShowing(LayoutType.TAB_SWITCHER);
        mMediator.onStartSurfaceStateChanged(StartSurfaceState.SHOWN_TABSWITCHER, true);
        assertFalse(mPropertyModel.get(TAB_SWITCHER_BUTTON_IS_VISIBLE));
        assertTrue(mPropertyModel.get(HOME_BUTTON_IS_VISIBLE));

        // Change homepage to customized.
        mStartSurfaceAsHomepageSupplier.set(false);
        assertFalse(mPropertyModel.get(HOME_BUTTON_IS_VISIBLE));

        // Disable homepage.
        mHomepageEnabledSupplier.set(false);
        assertFalse(mPropertyModel.get(HOME_BUTTON_IS_VISIBLE));
    }

    @Test
    public void testNewTabButtonWithAccessibilityOnAndContinuationOn() {
        ChromeAccessibilityUtil.get().setAccessibilityEnabledForTesting(true);

        createMediator(false, true, true, true);
        mMediator.setStartSurfaceMode(true);
        // When accessibility is turned on and TAB_GROUPS_CONTINUATION_ANDROID is enabled, new tab
        // button shouldn't show on homepage.
        mMediator.onStartSurfaceStateChanged(StartSurfaceState.SHOWN_HOMEPAGE, true);
        assertFalse(mPropertyModel.get(NEW_TAB_BUTTON_IS_VISIBLE));

        ChromeAccessibilityUtil.get().setAccessibilityEnabledForTesting(false);
    }

    @Test
    public void testNewTabButtonWithAccessibilityOnAndContinuationOff() {
        ChromeAccessibilityUtil.get().setAccessibilityEnabledForTesting(true);

        createMediator(false, true, true, false);
        mMediator.setStartSurfaceMode(true);
        // When accessibility is turned on and TAB_GROUPS_CONTINUATION_ANDROID is disabled, new tab
        // button should show on homepage.
        mMediator.onStartSurfaceStateChanged(StartSurfaceState.SHOWN_HOMEPAGE, true);
        assertTrue(mPropertyModel.get(NEW_TAB_BUTTON_IS_VISIBLE));

        ChromeAccessibilityUtil.get().setAccessibilityEnabledForTesting(false);
    }

    private void createMediator(boolean hideIncognitoSwitchWhenNoTabs) {
        createMediator(hideIncognitoSwitchWhenNoTabs, false, false, false);
    }

    private void createMediator(boolean hideIncognitoSwitchWhenNoTabs,
            boolean showHomeButtonOnTabSwitcher, boolean shouldShowTabSwitcherButtonOnHomepage,
            boolean isTabGroupsAndroidContinuationEnabled) {
        mMediator = new StartSurfaceToolbarMediator(mPropertyModel, mMockCallback,
                hideIncognitoSwitchWhenNoTabs, showHomeButtonOnTabSwitcher, mMenuButtonCoordinator,
                mIdentityDiscStateSupplier,
                ()
                        -> mIdentityDiscController.getForStartSurface(
                                mMediator.getOverviewModeStateForTesting()),
                mHomepageEnabledSupplier, mStartSurfaceAsHomepageSupplier,
                new ObservableSupplierImpl<>(), null, shouldShowTabSwitcherButtonOnHomepage,
                isTabGroupsAndroidContinuationEnabled);

        mMediator.setLayoutStateProvider(mLayoutStateProvider);
        verify(mLayoutStateProvider).addObserver(mLayoutStateObserverCaptor.capture());
    }
}
