// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabmodel;

import static org.chromium.chrome.browser.tab.TabStateStorageServiceFactory.createBatch;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.flags.ActivityType;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.CustomTabProfileType;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileProvider;
import org.chromium.chrome.browser.tab_ui.TabContentManager;
import org.chromium.chrome.browser.tabmodel.NextTabPolicy.NextTabPolicySupplier;

/**
 * Helper to create {@link TabModelInternal} and {@link IncognitoTabModelInternal}. This was a shim
 * class to allow seamlessly swapping the implementation between legacy implementation and {@link
 * TabCollectionTabModelImpl}. It remains convenient for testing, but could be inlined.
 */
@NullMarked
public class TabModelFactory {
    private TabModelFactory() {}

    /** Creates a regular mode {@link TabModelInternal}, {@see TabModelImpl}'s constructor. */
    public static TabModelInternal createTabModel(
            Profile profile,
            @ActivityType int activityType,
            @Nullable @CustomTabProfileType Integer customTabProfileType,
            TabCreator regularTabCreator,
            TabCreator incognitoTabCreator,
            TabModelOrderController orderController,
            TabContentManager tabContentManager,
            NextTabPolicySupplier nextTabPolicySupplier,
            AsyncTabParamsManager asyncTabParamsManager,
            TabModelDelegate modelDelegate,
            TabRemover tabRemover,
            boolean supportUndo,
            @TabModelType int tabModelType,
            TabUngrouperFactory tabUngrouperFactory,
            @SupportedProfileType int supportedProfileType) {
        if (ChromeFeatureList.isEnabled(ChromeFeatureList.ENFORCE_INCOGNITO_ISOLATION)
                && supportedProfileType == SupportedProfileType.OFF_THE_RECORD) {
            return new StubTabModel(/* isIncognito= */ false, profile);
        }

        return createCollectionTabModel(
                profile,
                activityType,
                customTabProfileType,
                tabModelType,
                regularTabCreator,
                incognitoTabCreator,
                orderController,
                tabContentManager,
                nextTabPolicySupplier,
                modelDelegate,
                asyncTabParamsManager,
                tabRemover,
                tabUngrouperFactory,
                supportUndo);
    }

    /**
     * Creates an incognito mode {@link IncognitoTabModelInternal}, {@see
     * IncognitoTabModelImplCreator}'s constructor.
     */
    public static IncognitoTabModelInternal createIncognitoTabModel(
            ProfileProvider profileProvider,
            TabCreator regularTabCreator,
            TabCreator incognitoTabCreator,
            TabModelOrderController orderController,
            TabContentManager tabContentManager,
            NextTabPolicySupplier nextTabPolicySupplier,
            AsyncTabParamsManager asyncTabParamsManager,
            @ActivityType int activityType,
            @Nullable @CustomTabProfileType Integer customTabProfileType,
            TabModelDelegate modelDelegate,
            TabRemover tabRemover,
            TabUngrouperFactory tabUngrouperFactory,
            @SupportedProfileType int supportedProfileType) {
        if (ChromeFeatureList.isEnabled(ChromeFeatureList.ENFORCE_INCOGNITO_ISOLATION)
                && supportedProfileType == SupportedProfileType.REGULAR) {
            return new StubTabModel(/* isIncognito= */ true, /* profile= */ null);
        }

        return createCollectionIncognitoTabModel(
                profileProvider,
                regularTabCreator,
                incognitoTabCreator,
                orderController,
                tabContentManager,
                nextTabPolicySupplier,
                asyncTabParamsManager,
                activityType,
                customTabProfileType,
                modelDelegate,
                tabRemover,
                tabUngrouperFactory);
    }

    /** Creates an empty {@link IncognitoTabModelInternal}. */
    public static IncognitoTabModelInternal createEmptyIncognitoTabModel() {
        return EmptyTabModel.getInstance(/* isIncognito= */ true);
    }

    private static TabModelInternal createCollectionTabModel(
            Profile profile,
            @ActivityType int activityType,
            @Nullable @CustomTabProfileType Integer customTabProfileType,
            @TabModelType int tabModelType,
            TabCreator regularTabCreator,
            TabCreator incognitoTabCreator,
            TabModelOrderController orderController,
            TabContentManager tabContentManager,
            NextTabPolicySupplier nextTabPolicySupplier,
            TabModelDelegate modelDelegate,
            AsyncTabParamsManager asyncTabParamsManager,
            TabRemover tabRemover,
            TabUngrouperFactory tabUngrouperFactory,
            boolean supportUndo) {
        return new TabCollectionTabModelImpl(
                profile,
                activityType,
                customTabProfileType,
                tabModelType,
                regularTabCreator,
                incognitoTabCreator,
                orderController,
                tabContentManager,
                nextTabPolicySupplier,
                modelDelegate,
                asyncTabParamsManager,
                tabRemover,
                /* isIncognitoBranded= */ false,
                tabUngrouperFactory,
                () -> createBatch(profile),
                supportUndo);
    }

    private static IncognitoTabModelInternal createCollectionIncognitoTabModel(
            ProfileProvider profileProvider,
            TabCreator regularTabCreator,
            TabCreator incognitoTabCreator,
            TabModelOrderController orderController,
            TabContentManager tabContentManager,
            NextTabPolicySupplier nextTabPolicySupplier,
            AsyncTabParamsManager asyncTabParamsManager,
            @ActivityType int activityType,
            @Nullable @CustomTabProfileType Integer customTabProfileType,
            TabModelDelegate modelDelegate,
            TabRemover tabRemover,
            TabUngrouperFactory tabUngrouperFactory) {
        IncognitoTabModelImplCreator incognitoCreator =
                new IncognitoTabModelImplCreator(
                        profileProvider,
                        regularTabCreator,
                        incognitoTabCreator,
                        orderController,
                        tabContentManager,
                        nextTabPolicySupplier,
                        asyncTabParamsManager,
                        activityType,
                        customTabProfileType,
                        modelDelegate,
                        tabRemover,
                        tabUngrouperFactory);
        return new IncognitoTabModelImpl(incognitoCreator);
    }
}
