// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.features.start_surface;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.when;

import android.app.Activity;
import android.view.ViewGroup;
import android.view.ViewGroup.MarginLayoutParams;

import androidx.annotation.NonNull;

import org.junit.Assert;
import org.junit.Rule;
import org.junit.rules.TestRule;
import org.junit.runner.Description;
import org.junit.runners.model.Statement;
import org.mockito.Mockito;
import org.robolectric.Robolectric;

import org.chromium.base.ActivityState;
import org.chromium.base.ApplicationStatus;
import org.chromium.base.FeatureList;
import org.chromium.base.FeatureListJni;
import org.chromium.base.jank_tracker.PlaceholderJankTracker;
import org.chromium.base.library_loader.LibraryLoader;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.supplier.OneshotSupplierImpl;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.browser.app.tabmodel.ChromeTabModelFilterFactory;
import org.chromium.chrome.browser.back_press.BackPressManager;
import org.chromium.chrome.browser.bookmarks.BookmarkNativesMockRule;
import org.chromium.chrome.browser.feature_engagement.TrackerFactory;
import org.chromium.chrome.browser.feed.FeedServiceBridge;
import org.chromium.chrome.browser.feed.FeedServiceBridgeJni;
import org.chromium.chrome.browser.feed.FeedSurfaceMediator;
import org.chromium.chrome.browser.fullscreen.BrowserControlsManager;
import org.chromium.chrome.browser.incognito.reauth.IncognitoReauthController;
import org.chromium.chrome.browser.init.ActivityLifecycleDispatcherImpl;
import org.chromium.chrome.browser.init.ChromeActivityNativeDelegate;
import org.chromium.chrome.browser.omnibox.OmniboxStub;
import org.chromium.chrome.browser.omnibox.voice.VoiceRecognitionHandler;
import org.chromium.chrome.browser.preferences.PrefChangeRegistrar;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.search_engines.TemplateUrlServiceFactory;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.browser.signin.services.IdentityServicesProviderJni;
import org.chromium.chrome.browser.signin.services.SigninManager;
import org.chromium.chrome.browser.site_settings.CookieControlsServiceBridge;
import org.chromium.chrome.browser.site_settings.CookieControlsServiceBridgeJni;
import org.chromium.chrome.browser.tab_ui.TabContentManager;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelFilter;
import org.chromium.chrome.browser.tabmodel.TabModelFilterFactory;
import org.chromium.chrome.browser.tabmodel.TabModelFilterProvider;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabWindowManager;
import org.chromium.chrome.browser.tasks.tab_management.TabGridDialogView;
import org.chromium.chrome.browser.ui.favicon.FaviconHelper;
import org.chromium.chrome.browser.ui.favicon.FaviconHelperJni;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.chrome.test.util.browser.offlinepages.FakeOfflinePageBridge;
import org.chromium.chrome.test.util.browser.suggestions.SuggestionsDependenciesRule;
import org.chromium.chrome.test.util.browser.suggestions.mostvisited.FakeMostVisitedSites;
import org.chromium.chrome.test.util.browser.tabmodel.MockTabCreatorManager;
import org.chromium.chrome.test.util.browser.tabmodel.MockTabModelSelector;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.feature_engagement.Tracker;
import org.chromium.components.prefs.PrefService;
import org.chromium.components.search_engines.TemplateUrlService;
import org.chromium.components.user_prefs.UserPrefs;
import org.chromium.components.user_prefs.UserPrefsJni;
import org.chromium.ui.base.WindowAndroid;

import java.util.ArrayList;
import java.util.Collections;
import java.util.List;

/** Custom TestRule for tests using StartSurfaceCoordinator */
public class StartSurfaceCoordinatorUnitTestRule implements TestRule {
    @Rule public JniMocker mJniMocker = new JniMocker();
    @Rule public SuggestionsDependenciesRule mSuggestionsDeps = new SuggestionsDependenciesRule();
    @Rule public BookmarkNativesMockRule mBookmarkNativesMockRule = new BookmarkNativesMockRule();

    private Profile mProfile;
    private Profile mIncogonitoProfile;

    private TabModelSelector mTabModelSelector;
    private ViewGroup mContainerView;
    private TemplateUrlService mTemplateUrlService;
    private LibraryLoader mLibraryLoader;
    private TabWindowManager mTabWindowManager;

    private Activity mActivity;
    private StartSurfaceCoordinator mCoordinator;

    private final OneshotSupplierImpl<IncognitoReauthController>
            mIncognitoReauthControllerSupplier = new OneshotSupplierImpl<>();
    private ObservableSupplierImpl<Profile> mProfileSupplier = new ObservableSupplierImpl<>();

    private static class MockTabModelFilterProvider extends TabModelFilterProvider {
        public MockTabModelFilterProvider(
                Activity activity, Profile profile, Profile incogonitoProfile) {
            List<TabModel> tabModels = new ArrayList<>();
            MockTabModelSelector selector =
                    new MockTabModelSelector(profile, incogonitoProfile, 0, 0, null);
            tabModels.add(selector.getModel(false));
            tabModels.add(selector.getModel(true));
            selector.selectModel(true);

            init(new ChromeTabModelFilterFactory(activity), selector, tabModels);
        }

        @Override
        public void init(
                @NonNull TabModelFilterFactory tabModelFilterFactory,
                @NonNull TabModelSelector tabModelSelector,
                @NonNull List<TabModel> tabModels) {
            assert mTabModelFilterList.isEmpty();
            assert tabModels.size() > 0;

            List<TabModelFilter> filters = new ArrayList<>();
            for (int i = 0; i < tabModels.size(); i++) {
                filters.add(tabModelFilterFactory.createTabModelFilter(tabModels.get(i)));
            }
            mTabModelFilterList = Collections.unmodifiableList(filters);

            assert mTabModelFilterList.get(1).isCurrentlySelectedFilter();
        }
    }

    @Override
    public Statement apply(Statement statement, Description description) {
        return new Statement() {
            @Override
            public void evaluate() {
                mProfile = Mockito.mock(Profile.class);
                mIncogonitoProfile = Mockito.mock(Profile.class);
                mTabModelSelector = Mockito.mock(TabModelSelector.class);
                mContainerView = Mockito.mock(ViewGroup.class);
                mTemplateUrlService = Mockito.mock(TemplateUrlService.class);
                mLibraryLoader = Mockito.mock(LibraryLoader.class);
                mTabWindowManager = Mockito.mock(TabWindowManager.class);

                initJniMocks();
                initViewsMocks();

                doReturn(new MockTabModelFilterProvider(mActivity, mProfile, mIncogonitoProfile))
                        .when(mTabModelSelector)
                        .getTabModelFilterProvider();

                setUpCoordinator();
            }
        };
    }

    public StartSurfaceCoordinator getCoordinator() {
        return mCoordinator;
    }

    private void initJniMocks() {
        when(mProfile.isOffTheRecord()).thenReturn(false);
        when(mIncogonitoProfile.isOffTheRecord()).thenReturn(true);
        when(mProfile.getPrimaryOTRProfile(Mockito.anyBoolean())).thenReturn(mIncogonitoProfile);
        PrefService prefService = Mockito.mock(PrefService.class);

        mSuggestionsDeps.getFactory().offlinePageBridge = new FakeOfflinePageBridge();
        mSuggestionsDeps.getFactory().mostVisitedSites = new FakeMostVisitedSites();

        FeedSurfaceMediator.setPrefForTest(Mockito.mock(PrefChangeRegistrar.class), prefService);
        TrackerFactory.setTrackerForTests(Mockito.mock(Tracker.class));

        // Mock template url service.
        TemplateUrlServiceFactory.setInstanceForTesting(mTemplateUrlService);
        when(mTemplateUrlService.doesDefaultSearchEngineHaveLogo()).thenReturn(true);

        // Mock library loader.
        when(mLibraryLoader.isInitialized()).thenReturn(false);
        LibraryLoader.setLibraryLoaderForTesting(mLibraryLoader);

        UserPrefs.Natives userPrefsJniMock = Mockito.mock(UserPrefs.Natives.class);
        Mockito.when(userPrefsJniMock.get(mProfile)).thenReturn(prefService);
        when(userPrefsJniMock.get(mProfile)).thenReturn(prefService);
        mJniMocker.mock(UserPrefsJni.TEST_HOOKS, userPrefsJniMock);

        IdentityServicesProvider.Natives identityServicesProviderJniMock =
                Mockito.mock(IdentityServicesProvider.Natives.class);
        when(identityServicesProviderJniMock.getSigninManager(any()))
                .thenReturn(Mockito.mock(SigninManager.class));
        mJniMocker.mock(IdentityServicesProviderJni.TEST_HOOKS, identityServicesProviderJniMock);

        mJniMocker.mock(FaviconHelperJni.TEST_HOOKS, Mockito.mock(FaviconHelper.Natives.class));
        mJniMocker.mock(
                FeedServiceBridgeJni.TEST_HOOKS, Mockito.mock(FeedServiceBridge.Natives.class));
        mJniMocker.mock(
                CookieControlsServiceBridgeJni.TEST_HOOKS,
                Mockito.mock(CookieControlsServiceBridge.Natives.class));
        mJniMocker.mock(FeatureListJni.TEST_HOOKS, Mockito.mock(FeatureList.Natives.class));
    }

    private void initViewsMocks() {
        mActivity = spy(Robolectric.buildActivity(Activity.class).setup().get());
        mActivity.setTheme(org.chromium.chrome.tab_ui.R.style.Theme_BrowserUI_DayNight);
        ApplicationStatus.onStateChangeForTesting(mActivity, ActivityState.CREATED);

        when(mContainerView.getContext()).thenReturn(mActivity);
        when(mContainerView.getLayoutParams()).thenReturn(new MarginLayoutParams(0, 0));

        ViewGroup coordinatorView = Mockito.mock(ViewGroup.class);
        when(coordinatorView.generateLayoutParams(any()))
                .thenReturn(
                        new ViewGroup.LayoutParams(
                                ViewGroup.LayoutParams.MATCH_PARENT,
                                ViewGroup.LayoutParams.MATCH_PARENT));
        when(coordinatorView.findViewById(org.chromium.chrome.tab_ui.R.id.dialog_parent_view))
                .thenReturn(Mockito.mock(TabGridDialogView.class));
        when(mActivity.findViewById(org.chromium.chrome.tab_ui.R.id.coordinator))
                .thenReturn(coordinatorView);
    }

    private void setUpCoordinator() {
        WindowAndroid windowAndroid = Mockito.mock(WindowAndroid.class);
        BrowserControlsManager browserControlsManager = new BrowserControlsManager(mActivity, 0);
        SnackbarManager snackbarManager =
                new SnackbarManager(mActivity, mContainerView, windowAndroid);
        TabContentManager tabContentManager =
                new TabContentManager(mActivity, null, false, null, mTabWindowManager);

        VoiceRecognitionHandler voiceRecognitionHandler =
                Mockito.mock(VoiceRecognitionHandler.class);
        OmniboxStub omniboxStub = Mockito.mock(OmniboxStub.class);
        when(omniboxStub.getVoiceRecognitionHandler()).thenReturn(voiceRecognitionHandler);
        when(voiceRecognitionHandler.isVoiceSearchEnabled()).thenReturn(true);
        mIncognitoReauthControllerSupplier.set(Mockito.mock(IncognitoReauthController.class));

        var tabStripHeightSupplier = new ObservableSupplierImpl<Integer>();
        tabStripHeightSupplier.set(0);

        mCoordinator =
                new StartSurfaceCoordinator(
                        mActivity,
                        Mockito.mock(BottomSheetController.class),
                        new OneshotSupplierImpl<>(),
                        new ObservableSupplierImpl<>(),
                        false,
                        windowAndroid,
                        new PlaceholderJankTracker(),
                        mContainerView,
                        mTabModelSelector,
                        browserControlsManager,
                        snackbarManager,
                        new ObservableSupplierImpl<>(),
                        () -> omniboxStub,
                        tabContentManager,
                        Mockito.mock(ChromeActivityNativeDelegate.class),
                        new ActivityLifecycleDispatcherImpl(mActivity),
                        new MockTabCreatorManager(),
                        new ObservableSupplierImpl<>(),
                        new BackPressManager(),
                        mProfileSupplier,
                        tabStripHeightSupplier,
                        new OneshotSupplierImpl<>());

        Assert.assertFalse(LibraryLoader.getInstance().isLoaded());
        when(mLibraryLoader.isInitialized()).thenReturn(true);
    }
}
