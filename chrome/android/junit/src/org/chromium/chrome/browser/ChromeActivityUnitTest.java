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
import android.util.Pair;
import android.view.ViewGroup;

import org.json.JSONException;
import org.json.JSONObject;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentMatchers;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Robolectric;
import org.robolectric.annotation.Config;

import org.chromium.base.UserDataHost;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.supplier.OneshotSupplier;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.UserActionTester;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.app.ChromeActivity;
import org.chromium.chrome.browser.app.metrics.LaunchCauseMetrics;
import org.chromium.chrome.browser.app.tabmodel.TabModelOrchestrator;
import org.chromium.chrome.browser.compositor.layouts.LayoutManagerImpl;
import org.chromium.chrome.browser.dom_distiller.ReaderModeManager;
import org.chromium.chrome.browser.enterprise.util.EnterpriseInfo;
import org.chromium.chrome.browser.flags.ActivityType;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.media.FullscreenVideoPictureInPictureController;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileProvider;
import org.chromium.chrome.browser.readaloud.ReadAloudController;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabCreator;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.ui.BottomContainer;
import org.chromium.chrome.browser.ui.RootUiCoordinator;
import org.chromium.chrome.browser.ui.appmenu.AppMenuPropertiesDelegate;
import org.chromium.chrome.browser.ui.messages.snackbar.Snackbar;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.components.dom_distiller.core.DomDistillerUrlUtilsJni;
import org.chromium.components.ukm.UkmRecorder;
import org.chromium.components.ukm.UkmRecorderJni;
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
    @Mock ActivityTabProvider mActivityTabProvider;
    @Mock ReadAloudController mReadAloudController;
    @Mock ReaderModeManager mReaderModeManager;
    @Mock FullscreenVideoPictureInPictureController mFullscreenVideoPictureInPictureController;
    @Mock PictureInPictureUiState mPictureInPictureUiState;
    @Mock EnterpriseInfo mEnterpriseInfo;
    @Mock UkmRecorder.Natives mUkmRecorderJniMock;
    @Mock DomDistillerUrlUtilsJni mDomDistillerUrlUtilsJni;
    @Mock private ObservableSupplier<LayoutManagerImpl> mLayoutManagerSupplier;
    @Mock private TabStateThemeResourceProvider mThemeResourceProvider;

    ObservableSupplierImpl<ReadAloudController> mReadAloudControllerSupplier =
            new ObservableSupplierImpl<>();

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
        protected void destroyTabModels() {}

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
        protected FullscreenVideoPictureInPictureController
                ensureFullscreenVideoPictureInPictureController() {
            return mFullscreenVideoPictureInPictureController;
        }

        @Override
        protected void onPreCreate() {
            // Override the method in test so it can be accessible in test body.
            super.onPreCreate();
        }

        @Override
        public ObservableSupplier<LayoutManagerImpl> getLayoutManagerSupplier() {
            return mLayoutManagerSupplier;
        }

        @Override
        public ActivityTabProvider getActivityTabProvider() {
            return mActivityTabProvider;
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
                Mockito.spy(new SnackbarManager(mActivity, viewGroup, null));
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
    @EnableFeatures({ChromeFeatureList.PAGE_CONTENT_PROVIDER})
    @DisableFeatures({ChromeFeatureList.ANDROID_PDF_ASSIST_CONTENT})
    public void testPageContentStructuredData() throws JSONException {
        TestChromeActivity chromeActivity = Mockito.spy(new TestChromeActivity());
        when(chromeActivity.getActivityTab()).thenReturn(mActivityTab);
        when(chromeActivity.getActivityTabProvider()).thenReturn(mActivityTabProvider);
        when(mActivityTabProvider.get()).thenReturn(mActivityTab);
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
                (JSONObject) new org.json.JSONTokener(result.getStructuredData()).nextValue();
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
        TestChromeActivity chromeActivity = new TestChromeActivity();
        chromeActivity.onPreCreate();
        assertNotNull(
                "ThemeResourceProvider should be created.",
                chromeActivity.getThemeResourceProviderForTesting());
    }

    @Test
    @DisableFeatures(ChromeFeatureList.ANDROID_THEME_RESOURCE_PROVIDER)
    public void testThemeResourceProvider_disabled() {
        TestChromeActivity chromeActivity = new TestChromeActivity();
        chromeActivity.onPreCreate();
        assertNull(
                "ThemeResourceProvider should not be created.",
                chromeActivity.getThemeResourceProviderForTesting());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.ANDROID_THEME_RESOURCE_PROVIDER)
    public void testThemeResourceProvider_wrongActivityType() {
        TestChromeActivity chromeActivity = Mockito.spy(new TestChromeActivity());
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
}
