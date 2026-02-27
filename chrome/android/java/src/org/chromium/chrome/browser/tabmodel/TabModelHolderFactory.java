// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabmodel;

import static org.chromium.build.NullUtil.assumeNonNull;
import static org.chromium.chrome.browser.tab.TabStateStorageServiceFactory.createBatch;

import org.chromium.base.Holder;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.flags.ActivityType;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileProvider;
import org.chromium.chrome.browser.tab_ui.TabContentManager;
import org.chromium.chrome.browser.tabmodel.NextTabPolicy.NextTabPolicySupplier;

/**
 * Helper to create {@link TabModelHolder} and {@link IncognitoTabModelHolder}. This was a shim
 * class to allow seamlessly swapping the implementation between legacy implementation and {@link
 * TabCollectionTabModelImpl}. It remains convenient for testing, but could be inlined.
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
            @TabModelType int tabModelType,
            TabUngrouperFactory tabUngrouperFactory) {
        return createCollectionTabModelHolder(
                profile,
                activityType,
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
            TabUngrouperFactory tabUngrouperFactory) {
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

    /** Creates an empty {@link IncognitoTabModelHolder}. */
    public static IncognitoTabModelHolder createEmptyIncognitoTabModelHolder() {
        EmptyTabModel model = EmptyTabModel.getInstance(/* isIncognito= */ true);
        return new IncognitoTabModelHolder(model, new IncognitoTabGroupModelFilterImpl(model));
    }

    private static TabModelHolder createCollectionTabModelHolder(
            Profile profile,
            @ActivityType int activityType,
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
        Holder<@Nullable TabGroupModelFilter> filterHolder = new Holder<>(null);
        TabUngrouper tabUngrouper =
                tabUngrouperFactory.create(/* isIncognitoBranded= */ false, filterHolder);
        TabCollectionTabModelImpl regularTabModel =
                new TabCollectionTabModelImpl(
                        profile,
                        activityType,
                        tabModelType,
                        regularTabCreator,
                        incognitoTabCreator,
                        orderController,
                        tabContentManager,
                        nextTabPolicySupplier,
                        modelDelegate,
                        asyncTabParamsManager,
                        tabRemover,
                        tabUngrouper,
                        () -> createBatch(profile),
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
