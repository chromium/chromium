// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.devtools;

import android.content.Context;
import android.content.Intent;
import android.util.DisplayMetrics;

import org.jni_zero.CalledByNative;
import org.jni_zero.JniType;

import org.chromium.base.ContextUtils;
import org.chromium.base.ThreadUtils;
import org.chromium.base.supplier.OneshotSupplierImpl;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.actor.background_tabs.OffscreenRenderingManager;
import org.chromium.chrome.browser.app.tabmodel.HeadlessBrowserControlsStateProvider;
import org.chromium.chrome.browser.app.tabwindow.TabWindowManagerSingleton;
import org.chromium.chrome.browser.externalnav.ExternalNavigationDelegateImpl;
import org.chromium.chrome.browser.flags.ActivityType;
import org.chromium.chrome.browser.pdf.PdfInfo;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileProvider;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabBuilder;
import org.chromium.chrome.browser.tab.TabCreationState;
import org.chromium.chrome.browser.tab.TabDelegateFactory;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tab.TabSelectionType;
import org.chromium.chrome.browser.tab.TabState;
import org.chromium.chrome.browser.tab.TabWebContentsDelegateAndroid;
import org.chromium.chrome.browser.tab_ui.TabContentManager;
import org.chromium.chrome.browser.tabmodel.AsyncTabParamsManagerFactory;
import org.chromium.chrome.browser.tabmodel.NextTabPolicy;
import org.chromium.chrome.browser.tabmodel.SupportedProfileType;
import org.chromium.chrome.browser.tabmodel.TabClosureParams;
import org.chromium.chrome.browser.tabmodel.TabCreator;
import org.chromium.chrome.browser.tabmodel.TabCreatorManager;
import org.chromium.chrome.browser.tabmodel.TabList;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorImpl;
import org.chromium.chrome.browser.tabmodel.TabModelType;
import org.chromium.chrome.browser.ui.native_page.NativePage;
import org.chromium.chrome.browser.util.PictureInPictureWindowOptions;
import org.chromium.chrome.browser.util.WindowFeatures;
import org.chromium.components.browser_ui.util.BrowserControlsVisibilityDelegate;
import org.chromium.components.embedder_support.contextmenu.ContextMenuPopulatorFactory;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.components.external_intents.ExternalNavigationHandler;
import org.chromium.components.external_intents.ExternalNavigationParams;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.url.GURL;

import java.util.concurrent.CompletableFuture;

/**
 * Owns an offscreen {@link TabModelSelectorImpl} for a disposable DevTools off-the-record {@link
 * Profile} created via {@code Target.createBrowserContext}, keeping DevTools OTR targets isolated
 * from UI browser windows' {@link TabModel}s.
 */
// TODO(crbug.com/586667896): Rename (e.g., to OffscreenOtrTabModelOrchestrator), move to
// org.chromium.chrome.browser.app.tabmodel, and expand isolated OTR target test coverage.
@NullMarked
class DevToolsBrowserContextTabModel {
    private final TabContentManager mTabContentManager;
    private final TabModelSelectorImpl mTabModelSelector;
    private final DevToolsTabCreator mIncognitoTabCreator;

    @CalledByNative
    static DevToolsBrowserContextTabModel create(@JniType("Profile*") Profile otrProfile) {
        return new DevToolsBrowserContextTabModel(otrProfile);
    }

    private DevToolsBrowserContextTabModel(Profile otrProfile) {
        assert otrProfile.isOffTheRecord();

        Context context = ContextUtils.getApplicationContext();
        DevToolsTabCreator regularTabCreator =
                new DevToolsTabCreator(otrProfile.getOriginalProfile(), /* incognito= */ false);
        mIncognitoTabCreator = new DevToolsTabCreator(otrProfile, /* incognito= */ true);
        TabCreatorManager tabCreatorManager =
                incognito -> incognito ? mIncognitoTabCreator : regularTabCreator;

        OneshotSupplierImpl<ProfileProvider> profileProviderSupplier = new OneshotSupplierImpl<>();
        profileProviderSupplier.set(
                new ProfileProvider() {
                    @Override
                    public Profile getOriginalProfile() {
                        return otrProfile.getOriginalProfile();
                    }

                    @Override
                    public @Nullable Profile getOffTheRecordProfile(boolean createIfNeeded) {
                        return otrProfile;
                    }
                });

        mTabModelSelector =
                new TabModelSelectorImpl(
                        context,
                        /* modalDialogManager= */ null,
                        profileProviderSupplier,
                        tabCreatorManager,
                        () -> NextTabPolicy.LOCATIONAL,
                        AsyncTabParamsManagerFactory.createAsyncTabParamsManager(),
                        /* supportUndo= */ false,
                        ActivityType.TABBED,
                        /* customTabProfileType= */ null,
                        TabModelType.STANDARD,
                        /* startIncognito= */ true,
                        SupportedProfileType.OFF_THE_RECORD) {
                    @Override
                    protected boolean shouldActivateModelOnInit() {
                        // Keep IsActiveModel() false so callers scanning TabModelList do not treat
                        // this offscreen model as an active foreground TabModel.
                        return false;
                    }

                    @Override
                    public void selectModel(boolean incognito) {
                        // Intentional noop so adding tabs does not activate the offscreen model.
                    }

                    @Override
                    public void requestToShowTab(@Nullable Tab tab, @TabSelectionType int type) {
                        // Intentional noop for offscreen DevTools browser contexts.
                    }
                };

        mTabContentManager =
                new TabContentManager(
                        context,
                        new HeadlessBrowserControlsStateProvider(),
                        /* snapshotsEnabled= */ false,
                        mTabModelSelector::getTabById,
                        TabWindowManagerSingleton.getInstance());
        mTabModelSelector.onNativeLibraryReady(mTabContentManager);
        mTabModelSelector.markTabStateInitialized();
    }

    @CalledByNative
    @JniType("TabAndroid*")
    @Nullable Tab createTab(@JniType("content::WebContents*") WebContents webContents) {
        return mIncognitoTabCreator.createTabWithWebContents(
                /* parent= */ null,
                /* shouldPin= */ false,
                webContents,
                TabLaunchType.FROM_CHROME_UI,
                TabList.INVALID_TAB_INDEX,
                CompletableFuture.completedFuture(true));
    }

    @CalledByNative
    void destroy() {
        mIncognitoTabCreator.destroy();
        TabModel incognitoModel = mTabModelSelector.getModel(/* incognito= */ true);
        incognitoModel
                .getTabRemover()
                .forceCloseTabs(
                        TabClosureParams.closeAllTabs()
                                .allowUndo(false)
                                .allowUnloadHandlers(false)
                                .build());
        mTabModelSelector.destroy();
        mTabContentManager.destroy();
    }

    private static class DevToolsTabCreator implements TabCreator, TabCreator.NeedsTabModel {
        private final Profile mProfile;
        private final boolean mIncognito;
        private final TabDelegateFactory mTabDelegateFactory;
        private @Nullable TabModel mTabModel;

        DevToolsTabCreator(Profile profile, boolean incognito) {
            mProfile = profile;
            mIncognito = incognito;
            mTabDelegateFactory = new DevToolsTabDelegateFactory(this);
        }

        @Nullable TabModel getTabModel() {
            return mTabModel;
        }

        void destroy() {
            mTabModel = null;
        }

        @Override
        public void setTabModel(TabModel tabModel) {
            mTabModel = tabModel;
        }

        @Override
        public @Nullable Tab createNewTab(
                LoadUrlParams loadUrlParams, @TabLaunchType int type, @Nullable Tab parent) {
            return createNewTab(loadUrlParams, "", type, parent, TabList.INVALID_TAB_INDEX);
        }

        @Override
        public @Nullable Tab createNewTab(
                LoadUrlParams loadUrlParams,
                @TabLaunchType int type,
                @Nullable Tab parent,
                int position) {
            return createNewTab(loadUrlParams, "", type, parent, position);
        }

        @Override
        public @Nullable Tab createNewTab(
                LoadUrlParams loadUrlParams,
                String title,
                @TabLaunchType int type,
                @Nullable Tab parent,
                int position) {
            if (!mIncognito || mTabModel == null) return null;

            WindowAndroid windowAndroid =
                    OffscreenRenderingManager.getInstance().getOffscreenWindow();
            Tab tab =
                    TabBuilder.createLiveTab(mProfile, /* initiallyHidden= */ false)
                            .setParent(parent)
                            .setWindow(windowAndroid)
                            .setLaunchType(type)
                            .setDelegateFactory(mTabDelegateFactory)
                            .build();
            startOffscreenRendering(tab);
            tab.loadUrl(loadUrlParams);
            mTabModel.addTab(tab, position, type, TabCreationState.LIVE_IN_FOREGROUND);
            return tab;
        }

        @Override
        public @Nullable Tab createFrozenTab(
                TabState state, int id, int index, @TabLaunchType int type) {
            return null;
        }

        @Override
        public @Nullable Tab launchUrl(String url, @TabLaunchType int type) {
            return createNewTab(new LoadUrlParams(url), type, /* parent= */ null);
        }

        @Override
        public @Nullable Tab createTabWithWebContents(
                @Nullable Tab parent,
                boolean shouldPin,
                WebContents webContents,
                @TabLaunchType int type,
                int index,
                CompletableFuture<Boolean> addTabToModel) {
            if (!mIncognito || mTabModel == null) return null;
            // Check synchronous rejection before TabBuilder.build() takes native ownership of
            // webContents via TabAndroid::InitWebContents(), avoiding a double-free in C++ callers.
            if (addTabToModel.isDone() && !addTabToModel.join()) {
                return null;
            }

            WindowAndroid windowAndroid =
                    OffscreenRenderingManager.getInstance().getOffscreenWindow();
            Tab tab;
            @TabCreationState int creationState;
            if (webContents.getMainFrame() == null
                    || !webContents.getMainFrame().isRenderFrameLive()) {
                tab =
                        TabBuilder.createLazyTabWithWebContents(mProfile)
                                .setParent(parent)
                                .setWindow(windowAndroid)
                                .setLaunchType(type)
                                .setWebContents(webContents)
                                .setDelegateFactory(mTabDelegateFactory)
                                .setInitialPinState(shouldPin)
                                .build();
                creationState = TabCreationState.FROZEN_FOR_LAZY_LOAD;
            } else {
                tab =
                        TabBuilder.createLiveTab(mProfile, /* initiallyHidden= */ false)
                                .setParent(parent)
                                .setWindow(windowAndroid)
                                .setLaunchType(type)
                                .setWebContents(webContents)
                                .setDelegateFactory(mTabDelegateFactory)
                                .setInitialPinState(shouldPin)
                                .build();
                creationState = TabCreationState.LIVE_IN_FOREGROUND;
            }
            startOffscreenRendering(tab);

            if (addTabToModel.isDone()) {
                mTabModel.addTab(tab, index, type, creationState);
            } else {
                addTabToModel.thenAccept(
                        shouldAdd -> {
                            ThreadUtils.assertOnUiThread();
                            if (shouldAdd && mTabModel != null) {
                                mTabModel.addTab(tab, index, type, creationState);
                            } else {
                                tab.destroy();
                            }
                        });
            }
            return tab;
        }

        @Override
        public @Nullable Tab createTabWithHistory(Tab parent, @TabLaunchType int type) {
            return null;
        }

        @Override
        public void launchNtp(@TabLaunchType int type) {
            launchUrl(UrlConstants.NTP_URL, type);
        }

        @Override
        public @Nullable TabDelegateFactory createDefaultTabDelegateFactory() {
            return mTabDelegateFactory;
        }

        private static void startOffscreenRendering(Tab tab) {
            DisplayMetrics displayMetrics =
                    ContextUtils.getApplicationContext().getResources().getDisplayMetrics();
            int width = Math.max(1, displayMetrics.widthPixels);
            int height = Math.max(1, displayMetrics.heightPixels);
            OffscreenRenderingManager.getInstance().startOffscreenRendering(tab, width, height);
        }
    }

    private static class DevToolsTabDelegateFactory implements TabDelegateFactory {
        private final DevToolsTabCreator mTabCreator;

        DevToolsTabDelegateFactory(DevToolsTabCreator tabCreator) {
            mTabCreator = tabCreator;
        }

        @Override
        public TabWebContentsDelegateAndroid createWebContentsDelegate(Tab tab) {
            return new DevToolsTabWebContentsDelegate(mTabCreator);
        }

        @Override
        public ExternalNavigationHandler createExternalNavigationHandler(Tab tab) {
            return new ExternalNavigationHandler(
                    new ExternalNavigationDelegateImpl(tab) {
                        @Override
                        public boolean shouldDisableExternalIntentRequestsForUrl(
                                ExternalNavigationParams params, Intent intent) {
                            return true;
                        }
                    });
        }

        @Override
        public @Nullable ContextMenuPopulatorFactory createContextMenuPopulatorFactory(Tab tab) {
            return null;
        }

        @Override
        public @Nullable BrowserControlsVisibilityDelegate createBrowserControlsVisibilityDelegate(
                Tab tab) {
            return null;
        }

        @Override
        public @Nullable NativePage createNativePage(
                String url,
                @Nullable NativePage candidatePage,
                Tab tab,
                @Nullable PdfInfo pdfInfo) {
            return null;
        }

        @Override
        public @TabModelType int getTabModelType() {
            return TabModelType.STANDARD;
        }

        @Override
        public boolean isCustomTab() {
            return false;
        }

        @Override
        public boolean isTabInPwa() {
            return false;
        }

        @Override
        public boolean isTabInBrowser() {
            return false;
        }

        @Override
        public boolean isTabInPopup() {
            return false;
        }
    }

    private static class DevToolsTabWebContentsDelegate extends TabWebContentsDelegateAndroid {
        private final DevToolsTabCreator mTabCreator;

        DevToolsTabWebContentsDelegate(DevToolsTabCreator tabCreator) {
            mTabCreator = tabCreator;
        }

        @Override
        public boolean shouldResumeRequestsForCreatedWindow() {
            return true;
        }

        @Override
        public boolean addNewContents(
                @Nullable WebContents sourceWebContents,
                WebContents webContents,
                GURL targetUrl,
                int disposition,
                WindowFeatures windowFeatures,
                boolean userGesture,
                @Nullable PictureInPictureWindowOptions pictureInPictureWindowOptions) {
            TabModel tabModel = mTabCreator.getTabModel();
            Tab parent = null;
            if (tabModel != null && sourceWebContents != null) {
                for (Tab candidate : tabModel) {
                    if (candidate.getWebContents() == sourceWebContents) {
                        parent = candidate;
                        break;
                    }
                }
            }
            Tab tab =
                    mTabCreator.createTabWithWebContents(
                            parent,
                            /* shouldPin= */ false,
                            webContents,
                            TabLaunchType.FROM_LONGPRESS_FOREGROUND,
                            CompletableFuture.completedFuture(true));
            return tab != null;
        }

        @Override
        public void setOverlayMode(boolean useOverlayMode) {}

        @Override
        public void destroy() {}
    }
}
