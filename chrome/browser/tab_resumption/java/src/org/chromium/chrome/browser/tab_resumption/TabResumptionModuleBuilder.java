// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab_resumption;

import android.content.Context;
import android.view.LayoutInflater;
import android.view.ViewGroup;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import org.chromium.base.Callback;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.chrome.browser.magic_stack.ModuleConfigChecker;
import org.chromium.chrome.browser.magic_stack.ModuleDelegate;
import org.chromium.chrome.browser.magic_stack.ModuleProvider;
import org.chromium.chrome.browser.magic_stack.ModuleProviderBuilder;
import org.chromium.chrome.browser.page_image_service.ImageServiceBridge;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.recent_tabs.ForeignSessionHelper;
import org.chromium.chrome.browser.tab_resumption.TabResumptionDataProvider.TabResumptionDataProviderFactory;
import org.chromium.chrome.browser.tab_resumption.TabResumptionModuleMetricsUtils.ModuleNotShownReason;
import org.chromium.chrome.browser.tab_ui.TabContentManager;
import org.chromium.chrome.browser.tab_ui.ThumbnailProvider;
import org.chromium.components.browser_ui.util.GlobalDiscardableReferencePool;
import org.chromium.components.image_fetcher.ImageFetcher;
import org.chromium.components.image_fetcher.ImageFetcherConfig;
import org.chromium.components.image_fetcher.ImageFetcherFactory;
import org.chromium.page_image_service.mojom.ClientId;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

public class TabResumptionModuleBuilder implements ModuleProviderBuilder, ModuleConfigChecker {
    private final Context mContext;
    private final ObservableSupplier<Profile> mProfileSupplier;
    private final ObservableSupplier<TabContentManager> mTabContentManagerSupplier;
    private final boolean mUseSalientImage;

    // SuggestionEntry data source that listens to login / sync status changes. Shared among data
    // providers to reduce resource use, and ref-counted to ensure proper resource management.
    private SyncDerivedSuggestionEntrySource mSuggestionEntrySource;
    private int mSuggestionEntrySourceRefCount;

    @Nullable private ImageServiceBridge mImageServiceBridge;

    public TabResumptionModuleBuilder(
            @NonNull Context context,
            @NonNull ObservableSupplier<Profile> profileSupplier,
            ObservableSupplier<TabContentManager> tabContentManagerSupplier) {
        mContext = context;
        mProfileSupplier = profileSupplier;
        mTabContentManagerSupplier = tabContentManagerSupplier;
        mUseSalientImage = TabResumptionModuleUtils.TAB_RESUMPTION_USE_SALIENT_IMAGE.getValue();
    }

    /** Build {@link ModuleProvider} for the tab resumption module. */
    @Override
    public boolean build(
            @NonNull ModuleDelegate moduleDelegate,
            @NonNull Callback<ModuleProvider> onModuleBuiltCallback) {
        Profile profile = getRegularProfile();

        Integer notShownReason =
                TabResumptionModuleEnablement.computeModuleNotShownReason(
                        moduleDelegate, getRegularProfile());
        if (notShownReason != null) {
            TabResumptionModuleMetricsUtils.recordModuleNotShownReason(notShownReason.intValue());
            return false;
        }

        TabResumptionDataProviderFactory dataProviderFactory =
                () -> makeDataProvider(profile, moduleDelegate);

        maybeInitImageServiceBridge(profile);

        UrlImageProvider urlImageProvider =
                new UrlImageProvider(profile, mContext, mImageServiceBridge);

        assert mTabContentManagerSupplier.hasValue();
        TabResumptionModuleCoordinator coordinator =
                new TabResumptionModuleCoordinator(
                        mContext,
                        moduleDelegate,
                        dataProviderFactory,
                        urlImageProvider,
                        getThumbnailProvider(mTabContentManagerSupplier.get()));
        onModuleBuiltCallback.onResult(coordinator);
        return true;
    }

    /** Create view for the tab resumption module. */
    @Override
    public ViewGroup createView(@NonNull ViewGroup parentView) {
        return (ViewGroup)
                LayoutInflater.from(mContext)
                        .inflate(R.layout.tab_resumption_module_layout, parentView, false);
    }

    /** Bind the property model for the tab resumption module. */
    @Override
    public void bind(
            @NonNull PropertyModel model,
            @NonNull ViewGroup view,
            @NonNull PropertyKey propertyKey) {
        TabResumptionModuleViewBinder.bind(model, view, propertyKey);
    }

    @Override
    public void destroy() {
        if (mImageServiceBridge != null) {
            mImageServiceBridge.destroy();
            mImageServiceBridge = null;
        }
    }

    @Override
    public void onPauseWithNative() {
        if (mImageServiceBridge != null) {
            mImageServiceBridge.clear();
        }
    }

    // ModuleEligibilityChecker implementation:

    @Override
    public boolean isEligible() {
        // This function may be called by MainSettings when a profile hasn't been initialized yet.
        // See b/324138242.
        if (!mProfileSupplier.hasValue()) return false;

        if (TabResumptionModuleEnablement.isFeatureEnabled()) return true;

        TabResumptionModuleMetricsUtils.recordModuleNotShownReason(
                ModuleNotShownReason.FEATURE_DISABLED);
        return false;
    }

    /** Gets the regular profile if exists. */
    private Profile getRegularProfile() {
        assert mProfileSupplier.hasValue();

        Profile profile = mProfileSupplier.get();
        // It is possible that an incognito profile is provided by the supplier. See b/326619334.
        return profile.isOffTheRecord() ? profile.getOriginalProfile() : profile;
    }

    private void addRefToSuggestionEntrySource() {
        if (mSuggestionEntrySourceRefCount == 0) {
            assert mSuggestionEntrySource == null;
            Profile profile = getRegularProfile();
            boolean isV2Enabled = TabResumptionModuleUtils.TAB_RESUMPTION_V2.getValue();
            SuggestionBackend suggestionBackend =
                    isV2Enabled
                            ? new VisitedUrlRankingBackend(profile)
                            : new ForeignSessionSuggestionBackend(
                                    new ForeignSessionHelper(profile));
            mSuggestionEntrySource =
                    SyncDerivedSuggestionEntrySource.createFromProfile(profile, suggestionBackend);
        }
        ++mSuggestionEntrySourceRefCount;
    }

    private void removeRefToSuggestionEntrySource() {
        assert mSuggestionEntrySource != null;
        --mSuggestionEntrySourceRefCount;
        if (mSuggestionEntrySourceRefCount == 0) {
            mSuggestionEntrySource.destroy();
            mSuggestionEntrySource = null;
        }
    }

    private TabResumptionDataProvider makeDataProvider(
            Profile profile, @NonNull ModuleDelegate moduleDelegate) {
        LocalTabTabResumptionDataProvider localTabProvider =
                TabResumptionModuleEnablement.LocalTab.shouldMakeProvider(moduleDelegate)
                        ? new LocalTabTabResumptionDataProvider(moduleDelegate.getTrackingTab())
                        : null;

        SyncDerivedTabResumptionDataProvider foreignSessionProvider = null;

        if (TabResumptionModuleEnablement.SyncDerived.shouldMakeProvider(profile)) {
            addRefToSuggestionEntrySource();
            foreignSessionProvider =
                    new SyncDerivedTabResumptionDataProvider(
                            mSuggestionEntrySource, this::removeRefToSuggestionEntrySource);
        }
        return new MixedTabResumptionDataProvider(localTabProvider, foreignSessionProvider);
    }

    static ThumbnailProvider getThumbnailProvider(TabContentManager tabContentManager) {
        return (tabId, thumbnailSize, callback, forceUpdate, writeBack, isSelected) -> {
            tabContentManager.getTabThumbnailWithCallback(
                    tabId, thumbnailSize, callback, forceUpdate, writeBack);
        };
    }

    private void maybeInitImageServiceBridge(Profile profile) {
        if (!mUseSalientImage || mImageServiceBridge != null) return;

        ImageFetcher imageFetcher =
                ImageFetcherFactory.createImageFetcher(
                        ImageFetcherConfig.IN_MEMORY_WITH_DISK_CACHE,
                        profile.getProfileKey(),
                        GlobalDiscardableReferencePool.getReferencePool());
        mImageServiceBridge =
                new ImageServiceBridge(
                        ClientId.NTP_TAB_RESUMPTION,
                        ImageFetcher.TAB_RESUMPTION_MODULE_NAME,
                        profile,
                        imageFetcher);
    }
}
