// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import android.content.Context;
import android.view.View.OnClickListener;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import org.chromium.base.CallbackController;
import org.chromium.base.supplier.OneshotSupplier;
import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.browser.hub.DelegateButtonData;
import org.chromium.chrome.browser.hub.FullButtonData;
import org.chromium.chrome.browser.hub.HubColorScheme;
import org.chromium.chrome.browser.hub.Pane;
import org.chromium.chrome.browser.hub.PaneHubController;
import org.chromium.chrome.browser.hub.PaneId;
import org.chromium.chrome.browser.hub.ResourceButtonData;
import org.chromium.chrome.browser.incognito.reauth.IncognitoReauthController;
import org.chromium.chrome.browser.incognito.reauth.IncognitoReauthManager.IncognitoReauthCallback;
import org.chromium.chrome.browser.tabmodel.IncognitoTabModel;
import org.chromium.chrome.browser.tabmodel.IncognitoTabModelObserver;
import org.chromium.chrome.browser.tabmodel.TabList;
import org.chromium.chrome.browser.tabmodel.TabModelFilter;
import org.chromium.chrome.browser.tabmodel.TabModelUtils;
import org.chromium.chrome.tab_ui.R;

/** A {@link Pane} representing the incognito tab switcher. */
public class IncognitoTabSwitcherPane extends TabSwitcherPaneBase {
    private final IncognitoTabModelObserver mIncognitoTabModelObserver =
            new IncognitoTabModelObserver() {
                @Override
                public void wasFirstTabCreated() {
                    mReferenceButtonDataSupplier.set(mReferenceButtonData);
                }

                @Override
                public void didBecomeEmpty() {
                    mReferenceButtonDataSupplier.set(null);
                    if (isFocused()) {
                        @Nullable PaneHubController controller = getPaneHubController();
                        assert controller != null
                                : "isFocused requires a non-null PaneHubController.";
                        controller.focusPane(PaneId.TAB_SWITCHER);
                    }
                    resetWithTabList(null, false);
                }
            };

    private final IncognitoReauthCallback mIncognitoReauthCallback =
            new IncognitoReauthCallback() {
                @Override
                public void onIncognitoReauthNotPossible() {}

                @Override
                public void onIncognitoReauthSuccess() {
                    TabModelFilter incognitoTabModelFilter = mIncognitoTabModelFilterSupplier.get();
                    @Nullable
                    TabSwitcherPaneCoordinator coordinator = getTabSwitcherPaneCoordinator();
                    if (!isVisible()
                            || coordinator == null
                            || !incognitoTabModelFilter.isCurrentlySelectedFilter()) {
                        return;
                    }

                    coordinator.resetWithTabList(incognitoTabModelFilter);
                    coordinator.setInitialScrollIndexOffset();
                    coordinator.requestAccessibilityFocusOnCurrentTab();

                    mNewTabButtonDataSupplier.set(mEnabledNewTabButtonData);
                }

                @Override
                public void onIncognitoReauthFailure() {}
            };

    /** Not safe to use until initWithNative. */
    private final @NonNull Supplier<TabModelFilter> mIncognitoTabModelFilterSupplier;

    private final @NonNull ResourceButtonData mReferenceButtonData;
    private final @NonNull FullButtonData mEnabledNewTabButtonData;
    private final @NonNull FullButtonData mDisabledNewTabButtonData;

    private boolean mIsNativeInitialized;
    private @Nullable IncognitoReauthController mIncognitoReauthController;
    private @Nullable CallbackController mCallbackController;

    /**
     * @param context The activity context.
     * @param factory The factory used to construct {@link TabSwitcherPaneCoordinator}s.
     * @param incognitoTabModelFilter The incognito tab model filter.
     * @param newTabButtonClickListener The {@link OnClickListener} for the new tab button.
     * @param incognitoReauthControllerSupplier Supplier for the incognito reauth controller.
     */
    IncognitoTabSwitcherPane(
            @NonNull Context context,
            @NonNull TabSwitcherPaneCoordinatorFactory factory,
            @NonNull Supplier<TabModelFilter> incognitoTabModelFilterSupplier,
            @NonNull OnClickListener newTabButtonClickListener,
            @Nullable
                    OneshotSupplier<IncognitoReauthController> incognitoReauthControllerSupplier) {
        super(
                context,
                factory,
                /* isIncognito= */ true);

        mIncognitoTabModelFilterSupplier = incognitoTabModelFilterSupplier;

        // TODO(crbug/1505772): Update this string to not be an a11y string and it should probably
        // just say "Incognito".
        mReferenceButtonData =
                new ResourceButtonData(
                        R.string.accessibility_tab_switcher_incognito_stack,
                        R.string.accessibility_tab_switcher_incognito_stack,
                        R.drawable.incognito_small);

        ResourceButtonData newTabButtonData =
                new ResourceButtonData(
                        R.string.button_new_tab,
                        R.string.button_new_incognito_tab,
                        R.drawable.new_tab_icon);
        mEnabledNewTabButtonData =
                new DelegateButtonData(
                        newTabButtonData, () -> newTabButtonClickListener.onClick(null));
        mDisabledNewTabButtonData = new DelegateButtonData(newTabButtonData, null);

        if (incognitoReauthControllerSupplier != null) {
            mCallbackController = new CallbackController();
            incognitoReauthControllerSupplier.onAvailable(
                    mCallbackController.makeCancelable(
                            incognitoReauthController -> {
                                mIncognitoReauthController = incognitoReauthController;
                                incognitoReauthController.addIncognitoReauthCallback(
                                        mIncognitoReauthCallback);
                            }));
            mNewTabButtonDataSupplier.set(mDisabledNewTabButtonData);
        } else {
            mNewTabButtonDataSupplier.set(mEnabledNewTabButtonData);
        }
    }

    @Override
    public @PaneId int getPaneId() {
        return PaneId.INCOGNITO_TAB_SWITCHER;
    }

    @Override
    public @HubColorScheme int getColorScheme() {
        return HubColorScheme.INCOGNITO;
    }

    @Override
    public void destroy() {
        super.destroy();
        IncognitoTabModel incognitoTabModel = getIncognitoTabModel();
        if (incognitoTabModel != null) {
            incognitoTabModel.removeIncognitoObserver(mIncognitoTabModelObserver);
        }
        if (mIncognitoReauthController != null) {
            mIncognitoReauthController.removeIncognitoReauthCallback(mIncognitoReauthCallback);
        }
        if (mCallbackController != null) {
            mCallbackController.destroy();
        }
    }

    @Override
    public void initWithNative() {
        super.initWithNative();
        mIsNativeInitialized = true;
        IncognitoTabModel incognitoTabModel = getIncognitoTabModel();
        incognitoTabModel.addIncognitoObserver(mIncognitoTabModelObserver);
        if (incognitoTabModel.getCount() > 0) {
            mIncognitoTabModelObserver.wasFirstTabCreated();
        }
    }

    @Override
    public void showAllTabs() {
        resetWithTabList(mIncognitoTabModelFilterSupplier.get(), false);
    }

    @Override
    public int getCurrentTabId() {
        return TabModelUtils.getCurrentTabId(mIncognitoTabModelFilterSupplier.get().getTabModel());
    }

    @Override
    public boolean resetWithTabList(@Nullable TabList tabList, boolean quickMode) {
        @Nullable TabSwitcherPaneCoordinator coordinator = getTabSwitcherPaneCoordinator();
        if (coordinator == null) return false;

        boolean isNotVisibleOrSelected =
                !isVisible() || !mIncognitoTabModelFilterSupplier.get().isCurrentlySelectedFilter();
        boolean incognitoReauthShowing =
                mIncognitoReauthController != null
                        && mIncognitoReauthController.isIncognitoReauthPending();

        if (isNotVisibleOrSelected || incognitoReauthShowing) {
            coordinator.resetWithTabList(null);
        } else {
            coordinator.resetWithTabList(tabList);
        }

        mNewTabButtonDataSupplier.set(
                incognitoReauthShowing ? mDisabledNewTabButtonData : mEnabledNewTabButtonData);
        return true;
    }

    @Override
    protected void requestAccessibilityFocusOnCurrentTab() {
        if (mIncognitoReauthController != null
                && mIncognitoReauthController.isReauthPageShowing()) {
            return;
        }

        super.requestAccessibilityFocusOnCurrentTab();
    }

    private IncognitoTabModel getIncognitoTabModel() {
        if (!mIsNativeInitialized) return null;

        TabModelFilter incognitoTabModelFilter = mIncognitoTabModelFilterSupplier.get();
        assert incognitoTabModelFilter != null;
        return (IncognitoTabModel) incognitoTabModelFilter.getTabModel();
    }
}
