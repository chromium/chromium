// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import android.view.View;
import android.view.View.OnClickListener;

import androidx.annotation.NonNull;
import androidx.annotation.StringRes;

import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.chrome.browser.hub.DelegateButtonData;
import org.chromium.chrome.browser.hub.DisplayButtonData;
import org.chromium.chrome.browser.hub.FadeHubLayoutAnimationFactory;
import org.chromium.chrome.browser.hub.FullButtonData;
import org.chromium.chrome.browser.hub.HubContainerView;
import org.chromium.chrome.browser.hub.HubLayoutAnimatorProvider;
import org.chromium.chrome.browser.hub.HubLayoutConstants;
import org.chromium.chrome.browser.hub.Pane;
import org.chromium.chrome.browser.hub.ResourceButtonData;
import org.chromium.ui.base.DeviceFormFactor;

/**
 * A abstract {@link Pane} representing a tab switcher for shared logic between the normal and
 * incognito modes. This is effectively an adapter layer between the {@link Pane} and {@link
 * TabSwitcher} APIs.
 */
public abstract class TabSwitcherPaneBase implements Pane {
    protected final TabSwitcher mTabSwitcher;
    protected final ObservableSupplierImpl<DisplayButtonData> mReferenceButtonDataSupplier =
            new ObservableSupplierImpl<>();
    protected final ObservableSupplierImpl<FullButtonData> mNewTabButtonDataSupplier =
            new ObservableSupplierImpl<>();

    private OnClickListener mNewTabButtonClickListener;

    /**
     * @param tabSwitcher The {@link TabSwitcher} hosted by the Pane.
     * @param newTabButtonClickListener The {@link OnClickListener} for the new tab button.
     * @param newTabButtonContentDescriptionRes The resource for the new tab button content
     *     description.
     */
    TabSwitcherPaneBase(
            @NonNull TabSwitcher tabSwitcher,
            @NonNull OnClickListener newTabButtonClickListener,
            @StringRes int newTabButtonContentDescriptionRes) {
        mTabSwitcher = tabSwitcher;

        mNewTabButtonDataSupplier.set(
                new DelegateButtonData(
                        new ResourceButtonData(
                                org.chromium.chrome.browser.toolbar.R.string.button_new_tab,
                                newTabButtonContentDescriptionRes,
                                org.chromium.chrome.browser.toolbar.R.drawable.new_tab_icon),
                        () -> newTabButtonClickListener.onClick(null)));
    }

    @Override
    public @NonNull View getRootView() {
        return mTabSwitcher.getController().getTabSwitcherContainer();
    }

    @Override
    public @NonNull ObservableSupplier<FullButtonData> getActionButtonDataSupplier() {
        return mNewTabButtonDataSupplier;
    }

    @Override
    public @NonNull ObservableSupplier<DisplayButtonData> getReferenceButtonDataSupplier() {
        return mReferenceButtonDataSupplier;
    }

    @Override
    public @NonNull HubLayoutAnimatorProvider createShowHubLayoutAnimatorProvider(
            @NonNull HubContainerView hubContainerView) {
        assert !DeviceFormFactor.isNonMultiDisplayContextOnTablet(hubContainerView.getContext());
        // TODO(crbug/1505772): Replace with shrink animator.
        return FadeHubLayoutAnimationFactory.createFadeInAnimatorProvider(
                hubContainerView, HubLayoutConstants.FADE_DURATION_MS);
    }

    @Override
    public @NonNull HubLayoutAnimatorProvider createHideHubLayoutAnimatorProvider(
            @NonNull HubContainerView hubContainerView) {
        assert !DeviceFormFactor.isNonMultiDisplayContextOnTablet(hubContainerView.getContext());
        // TODO(crbug/1505772): Replace with expand animator.
        return FadeHubLayoutAnimationFactory.createFadeOutAnimatorProvider(
                hubContainerView, HubLayoutConstants.FADE_DURATION_MS);
    }

    @Override
    public @BackPressResult int handleBackPress() {
        return mTabSwitcher.getController().handleBackPress();
    }

    @Override
    public ObservableSupplier<Boolean> getHandleBackPressChangedSupplier() {
        return mTabSwitcher.getController().getHandleBackPressChangedSupplier();
    }
}
