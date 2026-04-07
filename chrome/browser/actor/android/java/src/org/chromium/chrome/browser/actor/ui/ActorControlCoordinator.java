// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.actor.ui;

import android.content.Context;
import android.view.LayoutInflater;
import android.view.View;

import org.chromium.base.Log;
import org.chromium.base.supplier.NullableObservableSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.actor.ui.ActorUiTabController.UiTabState;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabSupplierObserver;
import org.chromium.chrome.browser.tab_bottom_sheet.TabBottomSheetManager;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

/** The Coordinator for the Actor Control component. */
@NullMarked
public class ActorControlCoordinator implements ActorUiTabController.Observer {
    private static final String TAG = "ActorControlCoordin";

    private final ActorControlMediator mMediator;
    private final Context mContext;
    private final PropertyModel mModel;
    private final TabSupplierObserver mTabObserver;
    private final TabBottomSheetManager mTabBottomSheetManager;

    private @Nullable PropertyModelChangeProcessor mViewBinder;
    private @Nullable ActorControlView mView;
    private @Nullable ActorUiTabController mActorUiTabController;

    /**
     * Constructs a new {@link ActorControlCoordinator}.
     *
     * @param context The {@link Context} used to inflate the layout.
     * @param playPauseListener The {@link View.OnClickListener} for the status button.
     * @param closeListener The {@link View.OnClickListener} for the close button.
     * @param tabSupplier The {@link ObservableSupplier<Tab>} for the activity.
     * @param tabBottomSheetManager The {@link TabBottomSheetManager} for the tab bottom sheet.
     */
    // TODO(crbug.com/491895203): Add render test for peek view.
    public ActorControlCoordinator(
            Context context,
            View.OnClickListener playPauseListener,
            View.OnClickListener closeListener,
            NullableObservableSupplier<Tab> tabSupplier,
            TabBottomSheetManager tabBottomSheetManager) {
        mContext = context;
        mTabBottomSheetManager = tabBottomSheetManager;
        mModel =
                new PropertyModel.Builder(ActorControlProperties.ALL_KEYS)
                        .with(ActorControlProperties.TASK_TITLE, "")
                        .with(ActorControlProperties.TASK_STEP_DESCRIPTION, "")
                        .with(
                                ActorControlProperties.STATUS_ICON_RESOURCE,
                                R.drawable.ic_pause_white_24dp)
                        .with(ActorControlProperties.ON_PLAY_PAUSE_CLICKED, playPauseListener)
                        .with(ActorControlProperties.ON_CLOSE_CLICKED, closeListener)
                        .build();

        mMediator = new ActorControlMediator(mModel);
        mTabObserver =
                new TabSupplierObserver(tabSupplier, /* shouldTrigger= */ true) {
                    @Override
                    protected void onObservingDifferentTab(@Nullable Tab tab) {
                        if (mActorUiTabController != null) {
                            mActorUiTabController.removeObserver(ActorControlCoordinator.this);
                        }
                        mActorUiTabController = tab != null ? ActorUiTabController.from(tab) : null;
                        if (mActorUiTabController != null) {
                            mActorUiTabController.addObserver(ActorControlCoordinator.this);
                            if (mView == null) {
                                attachPeekView();
                            }
                        }
                    }
                };
    }

    /**
     * Called when the UI tab state changes.
     *
     * @param state The new UI tab state.
     */
    @Override
    public void onUiTabStateChanged(UiTabState state) {
        if (!mTabBottomSheetManager.isSheetInitialized()) return;
        if (state.actorOverlay.isActive) {
            if (mView == null) {
                attachPeekView();
            }
            if (!mTabBottomSheetManager.showPeekView()) {
                Log.d(TAG, "onUiTabStateChanged: Failed to show peek view.");
            }
            return;
        }
        if (!mTabBottomSheetManager.hidePeekView()) {
            Log.d(TAG, "onUiTabStateChanged: Failed to hide peek view.");
        }
    }

    /**
     * Initializes peek view, if it is not already initialized, and attaches it to the bottom sheet.
     */
    public void attachPeekView() {
        assert mView == null;
        if (mTabBottomSheetManager == null || !mTabBottomSheetManager.isSheetInitialized()) return;
        mView =
                (ActorControlView)
                        LayoutInflater.from(mContext)
                                .inflate(R.layout.actor_control_layout, null, false);
        mTabBottomSheetManager.attachPeekView(mView);
        mViewBinder =
                PropertyModelChangeProcessor.create(mModel, mView, ActorControlViewBinder::bind);
    }

    /** Cleans up component */
    public void destroy() {
        if (mViewBinder != null) {
            mViewBinder.destroy();
        }
        if (mActorUiTabController != null) {
            mActorUiTabController.removeObserver(this);
        }
        mTabObserver.destroy();
    }

    /**
     * @return The {@link PropertyModel} for this component.
     */
    /* package */ PropertyModel getModelForTesting() {
        return mModel;
    }

    /**
     * @return The {@link ActorControlMediator} for this component.
     */
    /* package */ ActorControlMediator getMediatorForTesting() {
        return mMediator;
    }

    /**
     * @return The {@link View} for this component.
     */
    /* package */ @Nullable View getPeekViewForTesting() {
        return mView;
    }
}
