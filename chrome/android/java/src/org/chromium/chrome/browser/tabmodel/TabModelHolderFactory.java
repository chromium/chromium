// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabmodel;

import static org.chromium.build.NullUtil.assumeNonNull;

import org.chromium.base.Holder;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.flags.ActivityType;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileProvider;
import org.chromium.chrome.browser.tab_ui.TabContentManager;
import org.chromium.chrome.browser.tabmodel.NextTabPolicy.NextTabPolicySupplier;

/**
 * Helper to create {@link TabModelHolder} and {@link IncognitoTabModelHolder}. This is a shim class
 * to allow seamlessly swapping the implementation between legacy {@link TabModelImpl} and {@link
 * TabGroupModelImpl} and the new {@link TabCollectionTabModelImpl}.
 */
@NullMarked
public class TabModelHolderFactory {
    private TabModelHolderFactory() {}

    /** Creates a regular mode {@link TabModelHolder}, {@see TabModelImpl}'s constructor. */
    public static TabModelHolder createTabModelHolder(
            Profile profile,
            @ActivityType int activityType,
            TabCreator regularTabCreator,
            TabCreator incognitoTabCreator,
            TabModelOrderController orderController,
            TabContentManager tabContentManager,
            NextTabPolicySupplier nextTabPolicySupplier,
            AsyncTabParamsManager asyncTabParamsManager,
            TabModelDelegate modelDelegate,
            TabRemover tabRemover,
            boolean supportUndo,
            boolean isArchivedTabModel,
            TabUngrouperFactory tabUngrouperFactory,
            boolean wasTabCollectionsActive) {
        if (ChromeFeatureList.sTabCollectionAndroid.isEnabled()) {
            return createCollectionTabModelHolder(
                    profile,
                    activityType,
                    isArchivedTabModel,
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
        return createLegacyTabModelHolder(
                profile,
                activityType,
                regularTabCreator,
                incognitoTabCreator,
                orderController,
                tabContentManager,
                nextTabPolicySupplier,
                asyncTabParamsManager,
                modelDelegate,
                tabRemover,
                supportUndo,
                isArchivedTabModel,
                tabUngrouperFactory,
                wasTabCollectionsActive);
    }

    /**
     * Creates an incognito mode {@link IncognitoTabModelHolder}, {@see
     * IncognitoTabModelImplCreator}'s constructor.
     */
    public static IncognitoTabModelHolder createIncognitoTabModelHolder(
            ProfileProvider profileProvider,
            TabCreator regularTabCreator,
            TabCreator incognitoTabCreator,
            TabModelOrderController orderController,
            TabContentManager tabContentManager,
            NextTabPolicySupplier nextTabPolicySupplier,
            AsyncTabParamsManager asyncTabParamsManager,
            @ActivityType int activityType,
            TabModelDelegate modelDelegate,
            TabRemover tabRemover,
            TabUngrouperFactory tabUngrouperFactory,
            boolean wasTabCollectionsActive) {
        if (ChromeFeatureList.sTabCollectionAndroid.isEnabled()) {
            return createCollectionIncognitoTabModelHolder(
                    profileProvider,
                    regularTabCreator,
                    incognitoTabCreator,
                    orderController,
                    tabContentManager,
                    nextTabPolicySupplier,
                    asyncTabParamsManager,
                    activityType,
                    modelDelegate,
                    tabRemover,
                    tabUngrouperFactory);
        }
        return createLegacyIncognitoTabModelHolder(
                profileProvider,
                regularTabCreator,
                incognitoTabCreator,
                orderController,
                tabContentManager,
                nextTabPolicySupplier,
                asyncTabParamsManager,
                activityType,
                modelDelegate,
                tabRemover,
                tabUngrouperFactory,
                wasTabCollectionsActive);
    }

    /** Creates an empty {@link IncognitoTabModelHolder}. */
    public static IncognitoTabModelHolder createEmptyIncognitoTabModelHolder() {
        EmptyTabModel model = EmptyTabModel.getInstance(/* isIncognito= */ true);
        return new IncognitoTabModelHolder(model, new IncognitoTabGroupModelFilterImpl(model));
    }

    private static TabModelHolder createCollectionTabModelHolder(
            Profile profile,
            @ActivityType int activityType,
            boolean isArchivedTabModel,
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
        Holder<@Nullable TabGroupModelFilter> filterHolder = new Holder<>(null);
        TabUngrouper tabUngrouper =
                tabUngrouperFactory.create(/* isIncognitoBranded= */ false, filterHolder);
        TabCollectionTabModelImpl regularTabModel =
                new TabCollectionTabModelImpl(
                        profile,
                        activityType,
                        isArchivedTabModel,
                        regularTabCreator,
                        incognitoTabCreator,
                        orderController,
                        tabContentManager,
                        nextTabPolicySupplier,
                        modelDelegate,
                        asyncTabParamsManager,
                        tabRemover,
                        tabUngrouper,
                        supportUndo);
        filterHolder.value = regularTabModel;

        return new TabModelHolder(regularTabModel, regularTabModel);
    }

    private static IncognitoTabModelHolder createCollectionIncognitoTabModelHolder(
            ProfileProvider profileProvider,
            TabCreator regularTabCreator,
            TabCreator incognitoTabCreator,
            TabModelOrderController orderController,
            TabContentManager tabContentManager,
            NextTabPolicySupplier nextTabPolicySupplier,
            AsyncTabParamsManager asyncTabParamsManager,
            @ActivityType int activityType,
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
                        modelDelegate,
                        tabRemover,
                        tabUngrouperFactory);
        IncognitoTabModelImpl incognitoTabModel = new IncognitoTabModelImpl(incognitoCreator);

        return new IncognitoTabModelHolder(
                incognitoTabModel, new IncognitoTabGroupModelFilterImpl(incognitoTabModel));
    }

    private static TabModelHolder createLegacyTabModelHolder(
            Profile profile,
            @ActivityType int activityType,
            TabCreator regularTabCreator,
            TabCreator incognitoTabCreator,
            TabModelOrderController orderController,
            TabContentManager tabContentManager,
            NextTabPolicySupplier nextTabPolicySupplier,
            AsyncTabParamsManager asyncTabParamsManager,
            TabModelDelegate modelDelegate,
            TabRemover tabRemover,
            boolean supportUndo,
            boolean isArchivedTabModel,
            TabUngrouperFactory tabUngrouperFactory,
            boolean wasTabCollectionsActive) {
        TabModelImpl regularTabModel =
                new TabModelImpl(
                        profile,
                        activityType,
                        regularTabCreator,
                        incognitoTabCreator,
                        orderController,
                        tabContentManager,
                        nextTabPolicySupplier,
                        asyncTabParamsManager,
                        modelDelegate,
                        tabRemover,
                        supportUndo,
                        isArchivedTabModel);

        return new TabModelHolder(
                regularTabModel,
                createLegacyTabGroupModelFilterInternal(
                        regularTabModel, tabUngrouperFactory, wasTabCollectionsActive));
    }

    private static IncognitoTabModelHolder createLegacyIncognitoTabModelHolder(
            ProfileProvider profileProvider,
            TabCreator regularTabCreator,
            TabCreator incognitoTabCreator,
            TabModelOrderController orderController,
            TabContentManager tabContentManager,
            NextTabPolicySupplier nextTabPolicySupplier,
            AsyncTabParamsManager asyncTabParamsManager,
            @ActivityType int activityType,
            TabModelDelegate modelDelegate,
            TabRemover tabRemover,
            TabUngrouperFactory tabUngrouperFactory,
            boolean wasTabCollectionsActive) {
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
                        modelDelegate,
                        tabRemover,
                        tabUngrouperFactory);
        IncognitoTabModelImpl incognitoTabModel = new IncognitoTabModelImpl(incognitoCreator);

        return new IncognitoTabModelHolder(
                incognitoTabModel,
                createLegacyTabGroupModelFilterInternal(
                        incognitoTabModel, tabUngrouperFactory, wasTabCollectionsActive));
    }

    private static TabGroupModelFilterInternal createLegacyTabGroupModelFilterInternal(
            TabModelInternal tabModel,
            TabUngrouperFactory tabUngrouperFactory,
            boolean wasTabCollectionsActive) {
        boolean isIncognitoBranded = tabModel.isIncognitoBranded();
        Holder<@Nullable TabGroupModelFilter> filterHolder = new Holder<>(null);
        TabUngrouper tabUngrouper = tabUngrouperFactory.create(isIncognitoBranded, filterHolder);
        TabGroupModelFilterInternal filter =
                new TabGroupModelFilterImpl(tabModel, tabUngrouper, wasTabCollectionsActive);
        filterHolder.value = filter;
        return filter;
    }

    /**
     * Creates a {@link TabModelHolder} for testing. The {@link TabGroupModelFilter} mostly no-ops.
     * This is primarily intended for unit tests that use a {@link MockTabModel} and don't care
     * about tab groups.
     */
    public static TabModelHolder createTabModelHolderForTesting(TabModelInternal tabModelInternal) {
        return new TabModelHolder(
                tabModelInternal, createStubTabGroupModelFilterForTesting(tabModelInternal));
    }

    /**
     * Creates a {@link IncognitoTabModelHolder} for testing. The {@link TabGroupModelFilter} mostly
     * no-ops. This is primarily intended for unit tests that use a {@link MockTabModel} and don't
     * care about tab groups.
     */
    public static IncognitoTabModelHolder createIncognitoTabModelHolderForTesting(
            IncognitoTabModelInternal incognitoTabModelInternal) {
        return new IncognitoTabModelHolder(
                incognitoTabModelInternal,
                createStubTabGroupModelFilterForTesting(incognitoTabModelInternal));
    }

    private static TabGroupModelFilterInternal createStubTabGroupModelFilterForTesting(
            TabModelInternal tabModel) {
        Holder<@Nullable TabGroupModelFilter> filterHolder = new Holder<>(null);
        TabUngrouper tabUngrouper =
                new PassthroughTabUngrouper(() -> assumeNonNull(filterHolder.get()));
        TabGroupModelFilterInternal filter =
                new StubTabGroupModelFilterImpl(tabModel, tabUngrouper);
        filterHolder.value = filter;
        return filter;
    }
}
