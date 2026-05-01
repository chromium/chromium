// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.actions.glic;

import android.view.View;

import org.chromium.base.Callback;
import org.chromium.base.supplier.NullableObservableSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.tab.CurrentTabObserver;
import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.ui.actions.ActionId;
import org.chromium.chrome.browser.ui.actions.ActionProperties;
import org.chromium.chrome.browser.ui.actions.ActionRegistry;
import org.chromium.chrome.browser.ui.actions.button.ButtonState;
import org.chromium.components.embedder_support.util.UrlUtilities;
import org.chromium.ui.modelutil.PropertyModel;

/** Coordinator for the Glic action. */
@NullMarked
public class GlicActionCoordinator {
    private final NullableObservableSupplier<PropertyModel> mGlicActionModelSupplier;
    private final Callback<@Nullable PropertyModel> mModelCallback = this::onModelChanged;
    private final Runnable mToggleGlicCallback;
    private final NullableObservableSupplier<Tab> mTabSupplier;
    private final CurrentTabObserver mCurrentTabObserver;

    public GlicActionCoordinator(
            ActionRegistry actionRegistry,
            Runnable toggleGlicCallback,
            NullableObservableSupplier<Tab> tabSupplier) {
        mToggleGlicCallback = toggleGlicCallback;
        mTabSupplier = tabSupplier;
        mGlicActionModelSupplier = actionRegistry.get(ActionId.GLIC);

        if (mGlicActionModelSupplier != null) {
            mGlicActionModelSupplier.addSyncObserverAndCallIfNonNull(mModelCallback);
        }

        mCurrentTabObserver =
                new CurrentTabObserver(
                        tabSupplier,
                        new EmptyTabObserver() {
                            @Override
                            public void onUrlUpdated(Tab tab) {
                                updateButtonState();
                            }
                        },
                        (tab) -> updateButtonState());
    }

    private void onModelChanged(@Nullable PropertyModel model) {
        if (model == null) return;
        model.set(ActionProperties.ON_PRESS_CALLBACK, this::onGlicActionPressed);
        updateButtonState();
    }

    private void onGlicActionPressed(View view) {
        // TODO(crbug.com/491509952): Check for active tasks and show menu if needed.
        mToggleGlicCallback.run();
    }

    private void updateButtonState() {
        PropertyModel model =
                mGlicActionModelSupplier != null ? mGlicActionModelSupplier.get() : null;
        if (model == null) return;

        Tab currentTab = mTabSupplier.get();
        boolean isEnabled =
                currentTab != null
                        && !currentTab.isOffTheRecord()
                        && !UrlUtilities.isNtpUrl(currentTab.getUrl());

        model.set(
                ActionProperties.BUTTON_STATE,
                isEnabled ? ButtonState.DEFAULT : ButtonState.UNCLICKABLE);
    }

    public void destroy() {
        if (mGlicActionModelSupplier != null) {
            mGlicActionModelSupplier.removeObserver(mModelCallback);
        }
        mCurrentTabObserver.destroy();
    }
}
