// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.any;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.eq;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.app.Activity;
import android.app.PictureInPictureUiState;
import android.app.assist.AssistContent;
import android.net.Uri;
import android.os.Build;
import android.os.Bundle;
import android.util.Pair;
import android.view.ViewGroup;
import android.window.OnBackInvokedDispatcher;

import androidx.annotation.Nullable;

import org.json.JSONException;
import org.json.JSONObject;
import org.json.JSONTokener;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.ArgumentMatchers;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Robolectric;
import org.robolectric.annotation.Config;

import org.chromium.base.UserDataHost;
import org.chromium.base.supplier.ObservableSuppliers;
import org.chromium.base.supplier.OneshotSupplier;
import org.chromium.base.supplier.SettableMonotonicObservableSupplier;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.UserActionTester;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.app.ChromeActivity;
import org.chromium.chrome.browser.app.appmenu.AppMenuPropertiesDelegateImpl;
import org.chromium.chrome.browser.app.metrics.LaunchCauseMetrics;
import org.chromium.chrome.browser.app.tabmodel.TabModelOrchestrator;
import org.chromium.chrome.browser.app.tabwindow.TabWindowManagerSingleton;
import org.chromium.chrome.browser.compositor.layouts.LayoutManagerImpl;
import org.chromium.chrome.browser.dom_distiller.ReaderModeManager;
import org.chromium.chrome.browser.flags.ActivityType;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.layouts.LayoutType;
import org.chromium.chrome.browser.media.FullscreenVideoPictureInPictureController;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileProvider;
import org.chromium.chrome.browser.readaloud.ReadAloudController;
import org.chromium.chrome.browser.settings.SettingsNavigationFactory;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabDestroyStatus;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tab.TabSelectionType;
import org.chromium.chrome.browser.tabmodel.TabCreator;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabwindow.TabWindowManager;
import org.chromium.chrome.browser.ui.BottomContainer;
import org.chromium.chrome.browser.ui.RootUiCoordinator;
import org.chromium.chrome.browser.ui.appmenu.AppMenuPropertiesDelegate;
import org.chromium.chrome.browser.ui.messages.snackbar.Snackbar;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.components.browser_ui.settings.SettingsNavigation;
import org.chromium.components.dom_distiller.core.DomDistillerUrlUtilsJni;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.components.policy.EnterpriseInfo;
import org.chromium.components.ukm.UkmRecorder;
import org.chromium.components.ukm.UkmRecorderJni;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.browser.RenderFrameHost;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.TestActivity;
import org.chromium.ui.theme.ThemeResourceWrapper;
import org.chromium.ui.theme.ThemeResourceWrapperProvider;
import org.chromium.url.JUnitTestGURLs;

/** Unit tests for ChromeActivity. */
@RunWith(BaseRobolectricTestRunner.class)
public class ChromeActivityUnitTest {
    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();
    Activity mActivity;

    @Mock RootUiCoordinator mRootUiCoordinatorMock;
    @Mock TabModel mTabModel;
    @Mock Profile mProfile;
    @Mock Tab mActivityTab;
    @Mock TabModelSelector mTabModelSelector;
    @Mock TabCreator mTabCreator;
    @Mock SettingsNavigation mSettingsNavigation;
    @Mock ReadAloudController mReadAloudController;
    @Mock ReaderModeManager mReaderModeManager;
    @Mock FullscreenVideoPictureInPictureController mFullscreenVideoPictureInPictureController;
    @Mock PictureInPictureUiState mPictureInPictureUiState;
    @Mock EnterpriseInfo mEnterpriseInfo;
    @Mock UkmRecorder.Natives mUkmRecorderJniMock;
    @Mock DomDistillerUrlUtilsJni mDomDistillerUrlUtilsJni;
    @Mock private TabStateThemeResourceProvider mThemeResourceProvider;
    @Mock LayoutManagerImpl mLayoutManagerMock;

    private final SettableMonotonicObservableSupplier<ReadAloudController>
            mReadAloudControllerSupplier = ObservableSuppliers.createMonotonic();

    class TestChromeActivity extends ChromeActivity {
        public TestChromeActivity() {
            mRootUiCoordinator = mRootUiCoordinatorMock;
        }

        @Override
        protected TabModelOrchestrator createTabModelOrchestrator() {
            return null;
        }

        @Override
        protected void createTabModels() {}

        @Override
        protected @TabDestroyStatus int destroyTabModels() {
            return TabDestroyStatus.NO_SHUTDOWN;
        }

        @Override
        protected Pair<? extends TabCreator, ? extends TabCreator> createTabCreators() {
            return null;
        }

        @Override
        protected LaunchCauseMetrics createLaunchCauseMetrics() {
            return null;
        }

        @Override
        public AppMenuPropertiesDelegate createAppMenuPropertiesDelegate() {
            return null;
        }

        @Override
        public @ActivityType int getActivityType() {
            return ActivityType.TABBED;
        }

        @Override
        protected OneshotSupplier<ProfileProvider> createProfileProvider() {
            return null;
        }

        @Override
        protected RootUiCoordinator createRootUiCoordinator() {
            return null;
        }

        @Override
        protected @Nullable FullscreenVideoPictureInPictureController
                ensureFullscreenVideoPictureInPictureController() {
            if (!ChromeFeatureList.sFullscreenVideoPictureInPicture.isEnabled()) {
                return null;
            }
            return mFullscreenVideoPictureInPictureController;
        }

        @Override
        protected void onPreCreate() {
            // Override the method in test so it can be accessible in test body.
            super.onPreCreate();
        }
    }

    @Before
    public void setup() {
        mActivity = Robolectric.buildActivity(TestActivity.class).setup().get();
        DomDistillerUrlUtilsJni.setInstanceForTesting(mDomDistillerUrlUtilsJni);
    }

    @Test
    public void testCreateWindowErrorSnackbar() {
        String errorString = "Some error.";
        ViewGroup viewGroup = new BottomContainer(mActivity, null);
        SnackbarManager snackbarManager =
                Mockito.spy(new SnackbarManager(mActivity, viewGroup, null, null, null));
        ChromeActivity.createWindowErrorSnackbar(errorString, snackbarManager);
        Snackbar snackbar = snackbarManager.getCurrentSnackbarForTesting();
        Mockito.verify(snackbarManager).showSnackbar(ArgumentMatchers.any());
        assertNull("Snackbar controller should be null.", snackbar.getController());
        Assert.assertEquals(
                "Snackbar text should match.", errorString, snackbar.getTextForTesting());
        Assert.assertEquals(
                "Snackbar identifier should match.",
                Snackbar.UMA_WINDOW_ERROR,
                snackbar.getIdentifierForTesting());
        Assert.assertEquals(
                "Snackbar dismiss duration is incorrect.",
                SnackbarManager.DEFAULT_SNACKBAR_DURATION_LONG_MS,
                snackbar.getDuration());
        snackbarManager.dismissSnackbars(null);
    }

    @Test
    public void testReadAloudAppMenuItemClicked() {
        TestChromeActivity chromeActivity = Mockito.spy(new TestChromeActivity());

        doReturn(mActivityTab).when(chromeActivity).getActivityTab();
        doReturn(mTabModel).when(chromeActivity).getCurrentTabModel();
        when(mTabModel.getProfile()).thenReturn(mProfile);
        mReadAloudControllerSupplier.set(mReadAloudController);
        when(mRootUiCoordinatorMock.getReadAloudControllerSupplier())
                .thenReturn(mReadAloudControllerSupplier);

        assertTrue(
                chromeActivity.onMenuOrKeyboardAction(
                        R.id.readaloud_menu_id, /* fromMenu= */ true));
        verify(mReadAloudController)
                .playTab(eq(mActivityTab), eq(ReadAloudController.Entrypoint.OVERFLOW_MENU));
    }

    @Test
    @Config(sdk = 31)
    @EnableFeatures(ChromeFeatureList.FULLSCREEN_VIDEO_PICTURE_IN_PICTURE)
    public void testPictureInPictureStashing() {
        // Verify that ChromeActivity reports `isStashed` correctly to the controller.
        TestChromeActivity chromeActivity = Mockito.spy(new TestChromeActivity());

        // Test "not stashed".
        when(mPictureInPictureUiState.isStashed()).thenReturn(false);
        chromeActivity.onPictureInPictureUiStateChanged(mPictureInPictureUiState);
        Mockito.verify(mFullscreenVideoPictureInPictureController).onStashReported(false);

        // Test "is stashed".
        when(mPictureInPictureUiState.isStashed()).thenReturn(true);
        chromeActivity.onPictureInPictureUiStateChanged(mPictureInPictureUiState);
        Mockito.verify(mFullscreenVideoPictureInPictureController).onStashReported(true);
    }

    @Test
    @Config(sdk = 31)
    @DisableFeatures(ChromeFeatureList.FULLSCREEN_VIDEO_PICTURE_IN_PICTURE)
    public void testPictureInPictureStashing_Disabled() {
        // Verify that ChromeActivity does not report `isStashed` when the feature is disabled.
        TestChromeActivity chromeActivity = Mockito.spy(new TestChromeActivity());

        when(mPictureInPictureUiState.isStashed()).thenReturn(true);
        chromeActivity.onPictureInPictureUiStateChanged(mPictureInPictureUiState);
        Mockito.verify(mFullscreenVideoPictureInPictureController, Mockito.never())
                .onStashReported(Mockito.anyBoolean());
    }

    @Test
    @EnableFeatures({ChromeFeatureList.PAGE_CONTENT_PROVIDER})
    public void testPageContentStructuredData() throws JSONException {
        TestChromeActivity chromeActivity = Mockito.spy(new TestChromeActivity());
        chromeActivity.getActivityTabProvider().setForTesting(mActivityTab);
        when(chromeActivity.getActivityTab()).thenReturn(mActivityTab);
        when(mActivityTab.getUrl()).thenReturn(JUnitTestGURLs.GOOGLE_URL);
        WebContents webContents = mock(WebContents.class);
        when(webContents.getMainFrame()).thenReturn(mock(RenderFrameHost.class));
        when(mActivityTab.getWebContents()).thenReturn(webContents);
        UkmRecorderJni.setInstanceForTesting(mUkmRecorderJniMock);

        // Set enterprise info to report as enterprise owned.
        EnterpriseInfo.setInstanceForTest(mEnterpriseInfo);
        EnterpriseInfo.OwnedState enterpriseInfoState =
                new EnterpriseInfo.OwnedState(
                        /* isDeviceOwned= */ true, /* isProfileOwned= */ true);
        when(mEnterpriseInfo.getDeviceEnterpriseInfoSync()).thenReturn(enterpriseInfoState);

        AssistContent result = new AssistContent();
        chromeActivity.onProvideAssistContent(result);

        assertNotNull(result.getStructuredData());

        JSONObject jsonObject =
                (JSONObject) new JSONTokener(result.getStructuredData()).nextValue();
        var pageMetadata = jsonObject.getJSONObject("page_metadata");
        var isWorkProfile = pageMetadata.getBoolean("is_work_profile");
        var contentUri = pageMetadata.getString("content_uri");
        assertTrue(isWorkProfile);
        assertEquals("content", Uri.parse(contentUri).getScheme());
    }

    @Test
    public void testReaderModeMenuItemClicked_ShowReadingMode() {
        TestChromeActivity chromeActivity = Mockito.spy(new TestChromeActivity());
        UserActionTester userActionTester = new UserActionTester();

        UserDataHost userDataHost = new UserDataHost();
        userDataHost.setUserData(ReaderModeManager.class, mReaderModeManager);

        doReturn(mActivityTab).when(chromeActivity).getActivityTab();
        doReturn(mTabModel).when(chromeActivity).getCurrentTabModel();
        when(mTabModel.getProfile()).thenReturn(mProfile);
        when(mActivityTab.getUserDataHost()).thenReturn(userDataHost);
        when(mActivityTab.getUrl()).thenReturn(JUnitTestGURLs.EXAMPLE_URL);
        when(mDomDistillerUrlUtilsJni.isDistilledPage(any())).thenReturn(false);

        assertTrue(
                chromeActivity.onMenuOrKeyboardAction(
                        R.id.reader_mode_menu_id, /* fromMenu= */ true));
        verify(mReaderModeManager).activateReaderMode(ReaderModeManager.EntryPoint.APP_MENU);
        assertEquals(1, userActionTester.getActionCount("MobileMenuShowReaderMode"));
    }

    @Test
    public void testReaderModeMenuItemClicked_HideReadingMode() {
        TestChromeActivity chromeActivity = Mockito.spy(new TestChromeActivity());
        UserActionTester userActionTester = new UserActionTester();

        UserDataHost userDataHost = new UserDataHost();
        userDataHost.setUserData(ReaderModeManager.class, mReaderModeManager);

        doReturn(mActivityTab).when(chromeActivity).getActivityTab();
        doReturn(mTabModel).when(chromeActivity).getCurrentTabModel();
        when(mTabModel.getProfile()).thenReturn(mProfile);
        when(mActivityTab.getUserDataHost()).thenReturn(userDataHost);
        when(mActivityTab.getUrl()).thenReturn(JUnitTestGURLs.CHROME_DISTILLER_EXAMPLE_URL);
        when(mDomDistillerUrlUtilsJni.isDistilledPage(any())).thenReturn(true);

        assertTrue(
                chromeActivity.onMenuOrKeyboardAction(
                        R.id.reader_mode_menu_id, /* fromMenu= */ true));
        verify(mReaderModeManager).hideReaderMode();
        assertEquals(1, userActionTester.getActionCount("MobileMenuHideReaderMode"));
    }

    @Test
    @EnableFeatures(ChromeFeatureList.ANDROID_THEME_RESOURCE_PROVIDER)
    public void testThemeResourceProvider_enabled() {
        TestChromeActivity chromeActivity = Mockito.spy(new TestChromeActivity());
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.BAKLAVA) {
            doReturn(Mockito.mock(OnBackInvokedDispatcher.class))
                    .when(chromeActivity)
                    .getOnBackInvokedDispatcher();
        }
        chromeActivity.onPreCreate();
        assertNotNull(
                "ThemeResourceProvider should be created.",
                chromeActivity.getThemeResourceProviderForTesting());
    }

    @Test
    @DisableFeatures(ChromeFeatureList.ANDROID_THEME_RESOURCE_PROVIDER)
    public void testThemeResourceProvider_disabled() {
        TestChromeActivity chromeActivity = Mockito.spy(new TestChromeActivity());
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.BAKLAVA) {
            doReturn(Mockito.mock(OnBackInvokedDispatcher.class))
                    .when(chromeActivity)
                    .getOnBackInvokedDispatcher();
        }
        chromeActivity.onPreCreate();
        assertNull(
                "ThemeResourceProvider should not be created.",
                chromeActivity.getThemeResourceProviderForTesting());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.ANDROID_THEME_RESOURCE_PROVIDER)
    public void testThemeResourceProvider_wrongActivityType() {
        TestChromeActivity chromeActivity = Mockito.spy(new TestChromeActivity());
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.BAKLAVA) {
            doReturn(Mockito.mock(OnBackInvokedDispatcher.class))
                    .when(chromeActivity)
                    .getOnBackInvokedDispatcher();
        }
        doReturn(ActivityType.CUSTOM_TAB).when(chromeActivity).getActivityType();
        chromeActivity.onPreCreate();
        assertNull(
                "ThemeResourceProvider should not be created.",
                chromeActivity.getThemeResourceProviderForTesting());
    }

    // Bare minimum test to ensure #getResource call is delegate to the theme resource provider.
    // Real use case covered by java integration test.
    @Test
    @EnableFeatures(ChromeFeatureList.ANDROID_THEME_RESOURCE_PROVIDER)
    public void testGetResources() {

        TestChromeActivity chromeActivity = new TestChromeActivity();
        chromeActivity.setThemeResourceProviderForTesting(mThemeResourceProvider);
        chromeActivity.getResources();

        verify(mThemeResourceProvider).getResources();
    }

    @Test
    @EnableFeatures(ChromeFeatureList.ANDROID_THEME_RESOURCE_PROVIDER)
    public void testHasThemeResourceWrapper() {
        TestChromeActivity chromeActivity = new TestChromeActivity();
        chromeActivity.setThemeResourceProviderForTesting(mThemeResourceProvider);
        Assert.assertTrue(
                "Should be changeable with a provider.", chromeActivity.hasThemeResourceWrapper());

        chromeActivity.setThemeResourceProviderForTesting(null);
        Assert.assertFalse(
                "Should not be changeable without a provider.",
                chromeActivity.hasThemeResourceWrapper());

        assertEquals(chromeActivity, ThemeResourceWrapperProvider.getFromContext(chromeActivity));
    }

    @Test
    @EnableFeatures(ChromeFeatureList.ANDROID_THEME_RESOURCE_PROVIDER)
    public void testAttachThemeObserver() {
        TestChromeActivity chromeActivity = new TestChromeActivity();
        chromeActivity.setThemeResourceProviderForTesting(mThemeResourceProvider);

        ThemeResourceWrapper.ThemeObserver observer =
                mock(ThemeResourceWrapper.ThemeObserver.class);
        chromeActivity.attachThemeObserver(observer);
        verify(mThemeResourceProvider).addObserver(observer);

        chromeActivity.detachThemeObserver(observer);
        verify(mThemeResourceProvider).removeObserver(observer);
    }

    @Test
    @EnableFeatures(ChromeFeatureList.GLIC)
    public void testExitOverviewModeOnActorPiPExpand() {
        TestChromeActivity activity = new TestChromeActivity();
        TestChromeActivity chromeActivity = Mockito.spy(activity);

        ((SettableMonotonicObservableSupplier<LayoutManagerImpl>)
                        chromeActivity.getLayoutManagerSupplier())
                .set(mLayoutManagerMock);

        doReturn(true).when(chromeActivity).isInOverviewMode();
        chromeActivity.exitOverviewModeOnActorPiPExpand();
        verify(mLayoutManagerMock).showLayout(eq(LayoutType.BROWSING), eq(false));
    }

    @Test
    @EnableFeatures(ChromeFeatureList.SETTINGS_IN_TAB)
    @Config(qualifiers = "sw600dp")
    public void testPreferencesMenuItem_SettingsInTabEnabled() {
        TestChromeActivity chromeActivity = Mockito.spy(new TestChromeActivity());

        doReturn(mActivityTab).when(chromeActivity).getActivityTab();
        doReturn(mTabModel).when(chromeActivity).getCurrentTabModel();
        doReturn(mTabCreator).when(chromeActivity).getTabCreator(eq(false));

        when(mTabModel.getProfile()).thenReturn(mProfile);
        when(mProfile.isOffTheRecord()).thenReturn(false);

        assertTrue(
                chromeActivity.onMenuOrKeyboardAction(R.id.preferences_id, /* fromMenu= */ true));

        // Verify that createNewTab was called with the settings URL.
        ArgumentCaptor<LoadUrlParams> paramsCaptor = ArgumentCaptor.forClass(LoadUrlParams.class);
        verify(mTabCreator)
                .createNewTab(
                        paramsCaptor.capture(), eq(TabLaunchType.FROM_CHROME_UI), eq(mActivityTab));
        assertEquals(UrlConstants.SETTINGS_URL, paramsCaptor.getValue().getUrl());
    }

    @Test
    @DisableFeatures({ChromeFeatureList.SETTINGS_IN_TAB, ChromeFeatureList.SETTINGS_IN_TAB_DESKTOP})
    public void testPreferencesMenuItem_SettingsInTabDisabled() {
        TestChromeActivity chromeActivity = Mockito.spy(new TestChromeActivity());

        doReturn(mTabModel).when(chromeActivity).getCurrentTabModel();
        when(mTabModel.getProfile()).thenReturn(mProfile);
        when(mProfile.isOffTheRecord()).thenReturn(false);

        SettingsNavigationFactory.setInstanceForTesting(mSettingsNavigation);

        assertTrue(
                chromeActivity.onMenuOrKeyboardAction(R.id.preferences_id, /* fromMenu= */ true));

        // Verify that the standard settings activity was launched.
        verify(mSettingsNavigation).startSettings(chromeActivity);
    }

    @Test
    @EnableFeatures(ChromeFeatureList.CROSS_WINDOW_TAB_GROUP_OPERATIONS)
    public void testSelectTabFromGroup_CrossWindowOperationsEnabled() {
        TestChromeActivity chromeActivity = Mockito.spy(new TestChromeActivity());
        UserActionTester userActionTester = new UserActionTester();

        doReturn(mActivityTab).when(chromeActivity).getActivityTab();
        doReturn(mTabModel).when(chromeActivity).getCurrentTabModel();
        when(mTabModel.getProfile()).thenReturn(mProfile);
        doReturn(mTabCreator).when(chromeActivity).getTabCreator(eq(false));

        int tabId = 123;
        Tab targetTab = mock(Tab.class);
        when(targetTab.getId()).thenReturn(tabId);
        when(targetTab.getUrl()).thenReturn(JUnitTestGURLs.URL_1);
        when(targetTab.isIncognito()).thenReturn(false);

        TabWindowManager tabWindowManager = mock(TabWindowManager.class);
        when(tabWindowManager.getTabById(tabId)).thenReturn(targetTab);
        TabWindowManagerSingleton.setTabWindowManagerForTesting(tabWindowManager);

        Bundle menuItemData = new Bundle();
        menuItemData.putInt(AppMenuPropertiesDelegateImpl.TAB_ID_BUNDLE_KEY, tabId);

        assertTrue(
                chromeActivity.onMenuOrKeyboardAction(
                        R.id.tab_group_tab_menu_item,
                        /* fromMenu= */ true,
                        menuItemData,
                        /* triggeringMotion= */ null));

        ArgumentCaptor<LoadUrlParams> paramsCaptor = ArgumentCaptor.forClass(LoadUrlParams.class);
        verify(mTabCreator)
                .createNewTab(paramsCaptor.capture(), eq(TabLaunchType.FROM_CHROME_UI), eq(null));
        assertEquals(JUnitTestGURLs.URL_1.getSpec(), paramsCaptor.getValue().getUrl());
        assertEquals(1, userActionTester.getActionCount("MobileMenuSelectTabFromGroup"));
    }

    @Test
    @DisableFeatures(ChromeFeatureList.CROSS_WINDOW_TAB_GROUP_OPERATIONS)
    public void testSelectTabFromGroup_CrossWindowOperationsDisabled() {
        TestChromeActivity chromeActivity = Mockito.spy(new TestChromeActivity());
        UserActionTester userActionTester = new UserActionTester();

        doReturn(mActivityTab).when(chromeActivity).getActivityTab();
        doReturn(mTabModel).when(chromeActivity).getCurrentTabModel();
        when(mTabModel.getProfile()).thenReturn(mProfile);
        doReturn(mTabModelSelector).when(chromeActivity).getTabModelSelector();

        int tabId = 123;
        Tab targetTab = mock(Tab.class);
        when(targetTab.getId()).thenReturn(tabId);

        when(mTabModelSelector.getModelForTabId(tabId)).thenReturn(mTabModel);
        when(mTabModel.getTabById(tabId)).thenReturn(targetTab);
        when(mTabModel.indexOf(targetTab)).thenReturn(1);

        Bundle menuItemData = new Bundle();
        menuItemData.putInt(AppMenuPropertiesDelegateImpl.TAB_ID_BUNDLE_KEY, tabId);

        assertTrue(
                chromeActivity.onMenuOrKeyboardAction(
                        R.id.tab_group_tab_menu_item,
                        /* fromMenu= */ true,
                        menuItemData,
                        /* triggeringMotion= */ null));

        verify(mTabModel).setIndex(1, TabSelectionType.FROM_USER);
        assertEquals(1, userActionTester.getActionCount("MobileMenuSelectTabFromGroup"));
    }
}
