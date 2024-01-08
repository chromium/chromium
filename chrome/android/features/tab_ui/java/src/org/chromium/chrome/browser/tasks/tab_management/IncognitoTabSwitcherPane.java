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
import org.chromium.chrome.browser.hub.Pane;
import org.chromium.chrome.browser.hub.PaneId;
import org.chromium.chrome.browser.hub.ResourceButtonData;
import org.chromium.chrome.browser.incognito.reauth.IncognitoReauthController;
import org.chromium.chrome.browser.incognito.reauth.IncognitoReauthManager.IncognitoReauthCallback;
import org.chromium.chrome.browser.tabmodel.IncognitoTabModel;
import org.chromium.chrome.browser.tabmodel.IncognitoTabModelObserver;
import org.chromium.chrome.browser.tabmodel.TabList;
import org.chromium.chrome.browser.tabmodel.TabModelFilter;
import org.chromium.chrome.tab_ui.R;
import org.chromium.components.browser_ui.widget.MenuOrKeyboardActionController;

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
                }
            };

    // TODO(crbug/1505772): Add and override TabSwitcherPaneBase#resetWithTabList to not load any
    // tabs if mIncognitoReauthController is pending a reauth.
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
                }

                @Override
                public void onIncognitoReauthFailure() {}
            };

    /** Not safe to use until initWithNative. */
    private final @NonNull Supplier<TabModelFilter> mIncognitoTabModelFilterSupplier;

    private final @NonNull ResourceButtonData mReferenceButtonData;

    private boolean mIsNativeInitialized;
    private @Nullable IncognitoReauthController mIncognitoReauthController;
    private @Nullable CallbackController mCallbackController;

    /**
     * @param context The activity context.
     * @param factory The factory used to construct {@link TabSwitcherPaneCoordinator}s.
     * @param incognitoTabModelFilter The incognito tab model filter.
     * @param newTabButtonClickListener The {@link OnClickListener} for the new tab button.
     * @param menuOrKeyboardActionController Allows access to menu or keyboard actions.
     * @param incognitoReauthControllerSupplier Supplier for the incognito reauth controller.
     */
    IncognitoTabSwitcherPane(
            @NonNull Context context,
            @NonNull TabSwitcherPaneCoordinatorFactory factory,
            @NonNull Supplier<TabModelFilter> incognitoTabModelFilterSupplier,
            @NonNull OnClickListener newTabButtonClickListener,
            @NonNull MenuOrKeyboardActionController menuOrKeyboardActionController,
            @Nullable
                    OneshotSupplier<IncognitoReauthController> incognitoReauthControllerSupplier) {
        super(
                context,
                factory,
                newTabButtonClickListener,
                menuOrKeyboardActionController,
                R.string.button_new_incognito_tab,
                /* isIncognito= */ true);

        mIncognitoTabModelFilterSupplier = incognitoTabModelFilterSupplier;

        // TODO(crbug/1505772): Update this string to not be an a11y string and it should probably
        // just say "Incognito".
        mReferenceButtonData =
                new ResourceButtonData(
                        R.string.accessibility_tab_switcher,
                        R.string.accessibility_tab_switcher,
                        R.drawable.incognito_small);

        if (incognitoReauthControllerSupplier != null) {
            mCallbackController = new CallbackController();
            incognitoReauthControllerSupplier.onAvailable(
                    mCallbackController.makeCancelable(
                            incognitoReauthController -> {
                                mIncognitoReauthController = incognitoReauthController;
                                incognitoReauthController.addIncognitoReauthCallback(
                                        mIncognitoReauthCallback);
                            }));
        }
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
    public @PaneId int getPaneId() {
        return PaneId.INCOGNITO_TAB_SWITCHER;
    }

    @Override
    public boolean resetWithTabList(@Nullable TabList tabList, boolean quickMode) {
        // TODO(crbug/1505772): Implement.
        return true;
    }

    private IncognitoTabModel getIncognitoTabModel() {
        if (!mIsNativeInitialized) return null;

        TabModelFilter incognitoTabModelFilter = mIncognitoTabModelFilterSupplier.get();
        assert incognitoTabModelFilter != null;
        return (IncognitoTabModel) incognitoTabModelFilter.getTabModel();
    }
}
