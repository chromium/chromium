// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.devtools;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.content.Context;
import android.content.pm.PackageManager;
import android.os.Process;

import org.jni_zero.CalledByNative;
import org.jni_zero.JniType;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.base.ContextUtils;
import org.chromium.base.supplier.OneshotSupplierImpl;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.app.tabmodel.HeadlessBrowserControlsStateProvider;
import org.chromium.chrome.browser.app.tabwindow.TabWindowManagerSingleton;
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
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.permissions.AndroidPermissionDelegate;
import org.chromium.ui.permissions.PermissionCallback;
import org.chromium.url.GURL;

import java.util.Arrays;
import java.util.concurrent.CompletableFuture;

/**
 * Owns a headless {@link TabModelSelectorImpl} for a disposable DevTools off-the-record {@link
 * Profile} created via {@code Target.createBrowserContext}, keeping DevTools OTR targets isolated
 * from UI browser windows' {@link TabModel}s.
 */
@NullMarked
class DevToolsBrowserContextTabModel {
    private final WindowAndroid mWindowAndroid;
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
        mWindowAndroid = createHeadlessWindowAndroid(context);
        DevToolsTabCreator regularTabCreator =
                new DevToolsTabCreator(
                        otrProfile.getOriginalProfile(), mWindowAndroid, /* incognito= */ false);
        mIncognitoTabCreator =
                new DevToolsTabCreator(otrProfile, mWindowAndroid, /* incognito= */ true);
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
                    public void requestToShowTab(@Nullable Tab tab, @TabSelectionType int type) {
                        // Intentional noop for headless DevTools browser contexts.
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
        TabModel incognitoModel = mTabModelSelector.getModel(/* incognito= */ true);
        incognitoModel
                .getTabRemover()
                .closeTabs(
                        TabClosureParams.closeAllTabs().allowUndo(false).build(),
                        /* allowDialog= */ false);
        mTabModelSelector.destroy();
        mTabContentManager.destroy();
        mWindowAndroid.destroy();
    }

    private static WindowAndroid createHeadlessWindowAndroid(Context context) {
        WindowAndroid windowAndroid =
                new WindowAndroid(context, /* occlusionTrackingAllowed= */ false);
        windowAndroid.setAndroidPermissionDelegate(
                new AndroidPermissionDelegate() {
                    @Override
                    public boolean hasPermission(String permission) {
                        return ApiCompatibilityUtils.checkPermission(
                                        context, permission, Process.myPid(), Process.myUid())
                                == PackageManager.PERMISSION_GRANTED;
                    }

                    @Override
                    public boolean canRequestPermission(String permission) {
                        return false;
                    }

                    @Override
                    public boolean isPermissionRevokedByPolicy(String permission) {
                        return context.getPackageManager()
                                .isPermissionRevokedByPolicy(permission, context.getPackageName());
                    }

                    @Override
                    public void requestPermissions(
                            String[] permissions, PermissionCallback callback) {
                        int[] grantResults = new int[permissions.length];
                        Arrays.fill(grantResults, PackageManager.PERMISSION_DENIED);
                        callback.onRequestPermissionsResult(permissions, grantResults);
                    }

                    @Override
                    public boolean handlePermissionResult(
                            int requestCode, String[] permissions, int[] grantResults) {
                        return false;
                    }
                });
        return windowAndroid;
    }

    private static class DevToolsTabCreator implements TabCreator, TabCreator.NeedsTabModel {
        private final Profile mProfile;
        private final WindowAndroid mWindowAndroid;
        private final boolean mIncognito;
        private final TabDelegateFactory mTabDelegateFactory;
        private @Nullable TabModel mTabModel;

        DevToolsTabCreator(Profile profile, WindowAndroid windowAndroid, boolean incognito) {
            mProfile = profile;
            mWindowAndroid = windowAndroid;
            mIncognito = incognito;
            mTabDelegateFactory = new DevToolsTabDelegateFactory(this);
        }

        @Nullable TabModel getTabModel() {
            return mTabModel;
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

            Tab tab =
                    TabBuilder.createLiveTab(mProfile, /* initiallyHidden= */ false)
                            .setParent(parent)
                            .setWindow(mWindowAndroid)
                            .setLaunchType(type)
                            .setDelegateFactory(mTabDelegateFactory)
                            .build();
            tab.loadUrl(loadUrlParams);
            mTabModel.addTab(tab, position, type, TabCreationState.LIVE_IN_FOREGROUND);
            return tab;
        }

        @Override
        public @Nullable Tab createFrozenTab(TabState state, int id, int index) {
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

            Tab tab;
            @TabCreationState int creationState;
            if (webContents.getMainFrame() == null
                    || !webContents.getMainFrame().isRenderFrameLive()) {
                tab =
                        TabBuilder.createLazyTabWithWebContents(mProfile)
                                .setParent(parent)
                                .setWindow(mWindowAndroid)
                                .setLaunchType(type)
                                .setWebContents(webContents)
                                .setDelegateFactory(mTabDelegateFactory)
                                .setInitiallyHidden(false)
                                .setInitialPinState(shouldPin)
                                .build();
                creationState = TabCreationState.FROZEN_FOR_LAZY_LOAD;
            } else {
                tab =
                        TabBuilder.createLiveTab(mProfile, /* initiallyHidden= */ false)
                                .setParent(parent)
                                .setWindow(mWindowAndroid)
                                .setLaunchType(type)
                                .setWebContents(webContents)
                                .setDelegateFactory(mTabDelegateFactory)
                                .setInitialPinState(shouldPin)
                                .build();
                creationState = TabCreationState.LIVE_IN_FOREGROUND;
            }

            addTabToModel.thenAccept(
                    shouldAdd -> {
                        if (shouldAdd) {
                            assumeNonNull(mTabModel);
                            mTabModel.addTab(tab, index, type, creationState);
                        }
                    });
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
        public @Nullable ExternalNavigationHandler createExternalNavigationHandler(Tab tab) {
            return null;
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
