// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;
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

import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.supplier.OneshotSupplier;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.app.ChromeActivity;
import org.chromium.chrome.browser.app.metrics.LaunchCauseMetrics;
import org.chromium.chrome.browser.app.tabmodel.TabModelOrchestrator;
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
import org.chromium.content_public.browser.RenderFrameHost;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.TestActivity;
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
    @Mock FullscreenVideoPictureInPictureController mFullscreenVideoPictureInPictureController;
    @Mock PictureInPictureUiState mPictureInPictureUiState;
    @Mock EnterpriseInfo mEnterpriseInfo;

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
    }

    @Before
    public void setup() {
        mActivity = Robolectric.buildActivity(TestActivity.class).setup().get();
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
        Assert.assertNull("Snackbar controller should be null.", snackbar.getController());
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
        when(mActivityTab.getWebContents()).thenReturn(mock(WebContents.class));
        when(mActivityTab.getWebContents().getMainFrame()).thenReturn(mock(RenderFrameHost.class));
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
}
