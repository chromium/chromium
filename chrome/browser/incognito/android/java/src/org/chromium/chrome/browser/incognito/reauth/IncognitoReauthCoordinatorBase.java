// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.incognito.reauth;

import static org.chromium.chrome.browser.incognito.reauth.IncognitoReauthProperties.createPropertyModel;

import android.content.Context;
import android.view.LayoutInflater;
import android.view.View;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.chrome.browser.incognito.R;
import org.chromium.chrome.browser.incognito.reauth.IncognitoReauthManager.IncognitoReauthCallback;
import org.chromium.ui.listmenu.ListMenuButtonDelegate;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

/**
 * An abstract base class responsible for setting up the mediator, property model and the re-auth
 * view.
 */
abstract class IncognitoReauthCoordinatorBase implements IncognitoReauthCoordinator {
    /** The context to use to inflate the re-auth view and access other resources.*/
    protected final @NonNull Context mContext;

    /** The mediator which is responsible for handling the interaction done within the view.*/
    private final IncognitoReauthMediator mIncognitoReauthMediator;

    /**
     * The model change processor for the re-auth view which triggers the binded method associated
     * with the {@link PropertyModel} which got changed.
     */
    private PropertyModelChangeProcessor mModelChangeProcessor;

    /** The property model container for {@link IncognitoReauthProperties}. */
    private PropertyModel mPropertyModel;

    /** The actual underlying re-auth view.*/
    private View mIncognitoReauthView;

    /**
     * Test-only method to ignore the assertion on null checks, and use the respective view, model
     * and processor override instead.
     */
    @VisibleForTesting protected boolean mIgnoreViewAndModelCreationForTesting;

    /**
     * @param context The {@link Context} to use for inflating the re-auth view.
     * @param incognitoReauthManager The {@link IncognitoReauthManager} instance which would be
     *                              used to initiate re-authentication.
     * @param incognitoReauthCallback The {@link IncognitoReauthCallback} which would be executed
     *                               after an authentication attempt.
     * @param seeOtherTabsRunnable A {@link Runnable} which is run when the user clicks on
     *                            "See other tabs" option.
     */
    public IncognitoReauthCoordinatorBase(
            @NonNull Context context,
            @NonNull IncognitoReauthManager incognitoReauthManager,
            @NonNull IncognitoReauthCallback incognitoReauthCallback,
            @NonNull Runnable seeOtherTabsRunnable) {
        mContext = context;
        mIncognitoReauthMediator =
                new IncognitoReauthMediator(
                        incognitoReauthCallback, incognitoReauthManager, seeOtherTabsRunnable);
    }

    /** A method to clean-up any unwanted resource. */
    @Override
    public void destroy() {
        assert mModelChangeProcessor != null : "Model must be created before its destroyed.";
        mModelChangeProcessor.destroy();
    }

    /**
     * A method to responsible for setting up the environment to show before the re-auth view.
     *
     * @param menuButtonDelegate The {@link ListMenuButtonDelegate} for the 3 dots menu. Non-null
     *     for full-screen coordinator.
     * @param fullscreen A boolean indicating whether the incognito re-auth view needs to be shown
     *     fullscreen style or tab-switcher style.
     */
    protected void prepareToShow(
            @Nullable ListMenuButtonDelegate menuButtonDelegate, boolean fullscreen) {
        assert !fullscreen || menuButtonDelegate != null
                : "Full screen should provide a valid menu" + " button delegate.";

        // Don't create anything below and instead simply return.
        // The client should provide the override for the same.
        if (mIgnoreViewAndModelCreationForTesting) return;

        assert mIncognitoReauthView == null : "Previous view was not removed.";
        mIncognitoReauthView =
                LayoutInflater.from(mContext)
                        .inflate(R.layout.incognito_reauth_view, /* root= */ null);

        // When the re-auth view is shown, then own all the on touch events happening on it.
        // This prevents the touch event to propagate to other children when our re-auth view
        // is part of a ViewGroup when shown inside tab-switcher.
        mIncognitoReauthView.setOnTouchListener(
                (view, motionEvent) -> {
                    // Consume the click event.
                    view.performClick();
                    return true;
                });

        assert mPropertyModel == null : "Property model must not be reused.";
        mPropertyModel =
                createPropertyModel(
                        mIncognitoReauthMediator::onUnlockIncognitoButtonClicked,
                        mIncognitoReauthMediator::onSeeOtherTabsButtonClicked,
                        fullscreen,
                        menuButtonDelegate);

        assert mModelChangeProcessor == null : "Model change processor must not be reused.";
        mModelChangeProcessor =
                PropertyModelChangeProcessor.create(
                        mPropertyModel, mIncognitoReauthView, IncognitoReauthViewBinder::bind);
    }

    /**
     * @return The underlying incognito re-auth {@link View}. Null, if #show was not previously
     * called.
     */
    protected @Nullable View getIncognitoReauthView() {
        return mIncognitoReauthView;
    }

    /**
     * Test-only method to set a mock view for testing, instead of inflating the view which is
     * not available for unit tests.
     *
     * @param incognitoReauthView The mock {@link View} to set instead of the actual re-auth view.
     */
    protected void setIncognitoReauthViewForTesting(View incognitoReauthView) {
        mIncognitoReauthView = incognitoReauthView;
    }

    /** Test-only method to set a mock {@link PropertyModel}. */
    protected void setPropertyModelForTesting(PropertyModel propertyModel) {
        mPropertyModel = propertyModel;
    }

    /** Test-only method to set a mock {@link PropertyModelChangeProcessor}. */
    protected void setModelChangeProcessorForTesting(
            PropertyModelChangeProcessor modelChangeProcessor) {
        mModelChangeProcessor = modelChangeProcessor;
    }
}
