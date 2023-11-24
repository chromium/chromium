// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.features.start_surface;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.when;

import android.app.Activity;
import android.graphics.Color;
import android.view.ViewGroup;
import android.view.ViewGroup.MarginLayoutParams;

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
import org.chromium.chrome.browser.compositor.layouts.content.TabContentManager;
import org.chromium.chrome.browser.feature_engagement.TrackerFactory;
import org.chromium.chrome.browser.feed.FeedServiceBridge;
import org.chromium.chrome.browser.feed.FeedServiceBridgeJni;
import org.chromium.chrome.browser.feed.FeedSurfaceMediator;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.fullscreen.BrowserControlsManager;
import org.chromium.chrome.browser.incognito.reauth.IncognitoReauthController;
import org.chromium.chrome.browser.init.ActivityLifecycleDispatcherImpl;
import org.chromium.chrome.browser.init.ChromeActivityNativeDelegate;
import org.chromium.chrome.browser.multiwindow.MultiWindowModeStateDispatcherImpl;
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
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelFilter;
import org.chromium.chrome.browser.tabmodel.TabModelFilterFactory;
import org.chromium.chrome.browser.tabmodel.TabModelFilterProvider;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tasks.ReturnToChromeUtil;
import org.chromium.chrome.browser.tasks.tab_management.TabGridDialogView;
import org.chromium.chrome.browser.ui.favicon.FaviconHelper;
import org.chromium.chrome.browser.ui.favicon.FaviconHelperJni;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.chrome.test.util.browser.offlinepages.FakeOfflinePageBridge;
import org.chromium.chrome.test.util.browser.suggestions.SuggestionsDependenciesRule;
import org.chromium.chrome.test.util.browser.suggestions.mostvisited.FakeMostVisitedSites;
import org.chromium.chrome.test.util.browser.tabmodel.MockTabCreatorManager;
import org.chromium.chrome.test.util.browser.tabmodel.MockTabModel;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.widget.MenuOrKeyboardActionController;
import org.chromium.components.browser_ui.widget.scrim.ScrimCoordinator;
import org.chromium.components.feature_engagement.Tracker;
import org.chromium.components.prefs.PrefService;
import org.chromium.components.search_engines.TemplateUrlService;
import org.chromium.components.user_prefs.UserPrefs;
import org.chromium.components.user_prefs.UserPrefsJni;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modaldialog.ModalDialogManager.ModalDialogType;
import org.chromium.ui.test.util.modaldialog.FakeModalDialogManager;

import java.util.ArrayList;
import java.util.Collections;
import java.util.List;

/** Custom TestRule for tests using StartSurfaceCoordinator */
public class StartSurfaceCoordinatorUnitTestRule implements TestRule {
    @Rule public JniMocker mJniMocker = new JniMocker();
    @Rule public TestRule mProcessor = new Features.JUnitProcessor();
    @Rule public SuggestionsDependenciesRule mSuggestionsDeps = new SuggestionsDependenciesRule();
    @Rule public BookmarkNativesMockRule mBookmarkNativesMockRule = new BookmarkNativesMockRule();

    private TabModelSelector mTabModelSelector;
    private ViewGroup mContainerView;
    private TemplateUrlService mTemplateUrlService;
    private LibraryLoader mLibraryLoader;

    private Activity mActivity;
    private StartSurfaceCoordinator mCoordinator;

    private final OneshotSupplierImpl<IncognitoReauthController>
            mIncognitoReauthControllerSupplier = new OneshotSupplierImpl<>();
    private ObservableSupplierImpl<Profile> mProfileSupplier = new ObservableSupplierImpl<>();

    private static class MockTabModelFilterProvider extends TabModelFilterProvider {
        public MockTabModelFilterProvider(Activity activity) {
            List<TabModel> tabModels = new ArrayList<>();
            tabModels.add(new MockTabModel(Profile.getLastUsedRegularProfile(), null));
            MockTabModel incognitoTabModel =
                    new MockTabModel(
                            Profile.getLastUsedRegularProfile().getPrimaryOTRProfile(true), null);
            incognitoTabModel.setAsActiveModelForTesting();
            tabModels.add(incognitoTabModel);

            init(new ChromeTabModelFilterFactory(activity), tabModels);
        }

        @Override
        public void init(TabModelFilterFactory tabModelFilterFactory, List<TabModel> tabModels) {
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
                ChromeFeatureList.sStartSurfaceAndroid.setForTesting(true);
                ChromeFeatureList.sShowNtpAtStartupAndroid.setForTesting(false);

                mTabModelSelector = Mockito.mock(TabModelSelector.class);
                mContainerView = Mockito.mock(ViewGroup.class);
                mTemplateUrlService = Mockito.mock(TemplateUrlService.class);
                mLibraryLoader = Mockito.mock(LibraryLoader.class);

                initJniMocks();
                initViewsMocks();

                doReturn(new MockTabModelFilterProvider(mActivity))
                        .when(mTabModelSelector)
                        .getTabModelFilterProvider();

                Assert.assertTrue(ReturnToChromeUtil.isStartSurfaceEnabled(mActivity));
                setUpCoordinator();
            }
        };
    }

    public StartSurfaceCoordinator getCoordinator() {
        return mCoordinator;
    }

    private void initJniMocks() {
        Profile incognitoProfile = Mockito.mock(Profile.class);
        Mockito.when(incognitoProfile.isOffTheRecord()).thenReturn(true);
        Profile profile = Mockito.mock(Profile.class);
        Mockito.when(profile.getPrimaryOTRProfile(Mockito.anyBoolean()))
                .thenReturn(incognitoProfile);
        PrefService prefService = Mockito.mock(PrefService.class);
        Profile.setLastUsedProfileForTesting(profile);

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
        Mockito.when(userPrefsJniMock.get(profile)).thenReturn(prefService);
        when(userPrefsJniMock.get(profile)).thenReturn(prefService);
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
        ScrimCoordinator scrimCoordinator =
                new ScrimCoordinator(
                        mActivity,
                        new ScrimCoordinator.SystemUiScrimDelegate() {
                            @Override
                            public void setStatusBarScrimFraction(float scrimFraction) {
                                // Intentional noop
                            }

                            @Override
                            public void setNavigationBarScrimFraction(float scrimFraction) {
                                // Intentional noop
                            }
                        },
                        mContainerView,
                        Color.WHITE);

        WindowAndroid windowAndroid = Mockito.mock(WindowAndroid.class);
        BrowserControlsManager browserControlsManager = new BrowserControlsManager(mActivity, 0);
        SnackbarManager snackbarManager =
                new SnackbarManager(mActivity, mContainerView, windowAndroid);
        TabContentManager tabContentManager = new TabContentManager(mActivity, null, false, null);

        VoiceRecognitionHandler voiceRecognitionHandler =
                Mockito.mock(VoiceRecognitionHandler.class);
        OmniboxStub omniboxStub = Mockito.mock(OmniboxStub.class);
        when(omniboxStub.getVoiceRecognitionHandler()).thenReturn(voiceRecognitionHandler);
        when(voiceRecognitionHandler.isVoiceSearchEnabled()).thenReturn(true);
        mIncognitoReauthControllerSupplier.set(Mockito.mock(IncognitoReauthController.class));

        mCoordinator =
                new StartSurfaceCoordinator(
                        mActivity,
                        scrimCoordinator,
                        Mockito.mock(BottomSheetController.class),
                        new OneshotSupplierImpl<>(),
                        new ObservableSupplierImpl<>(),
                        false,
                        windowAndroid,
                        new PlaceholderJankTracker(),
                        mContainerView,
                        new ObservableSupplierImpl<>(),
                        mTabModelSelector,
                        browserControlsManager,
                        snackbarManager,
                        new ObservableSupplierImpl<>(),
                        () -> omniboxStub,
                        tabContentManager,
                        new FakeModalDialogManager(ModalDialogType.APP),
                        Mockito.mock(ChromeActivityNativeDelegate.class),
                        new ActivityLifecycleDispatcherImpl(mActivity),
                        new MockTabCreatorManager(),
                        Mockito.mock(MenuOrKeyboardActionController.class),
                        new MultiWindowModeStateDispatcherImpl(mActivity),
                        new ObservableSupplierImpl<>(),
                        new BackPressManager(),
                        mIncognitoReauthControllerSupplier,
                        null,
                        mProfileSupplier);

        Assert.assertFalse(LibraryLoader.getInstance().isLoaded());
        when(mLibraryLoader.isInitialized()).thenReturn(true);
        Assert.assertTrue(ReturnToChromeUtil.isStartSurfaceEnabled(mActivity));
    }
}
