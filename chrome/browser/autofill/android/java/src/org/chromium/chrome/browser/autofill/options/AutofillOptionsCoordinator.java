// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.options;

import static org.chromium.chrome.browser.autofill.options.AutofillOptionsProperties.ON_THIRD_PARTY_TOGGLE_CHANGED;

import androidx.annotation.StringRes;
import androidx.annotation.VisibleForTesting;
import androidx.lifecycle.DefaultLifecycleObserver;
import androidx.lifecycle.Lifecycle;
import androidx.lifecycle.LifecycleObserver;
import androidx.lifecycle.LifecycleOwner;
import androidx.lifecycle.Observer;

import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.browser.autofill.R;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.components.autofill.AutofillFeatures;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogProperties;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

/**
 * Coordinator for the autofill options settings screen. Connects the settings fragment with ...
 *   ... a model keeping track of the settings state, and
 *   ... a mediator to ensure the settings UI is consistent with prefs.
 */
public class AutofillOptionsCoordinator {
    final AutofillOptionsFragment mFragment;
    final AutofillOptionsMediator mMediator;
    final Observer<LifecycleOwner> mFragmentLifeCycleOwnerObserver = this::onLifeCycleOwnerChanged;
    final LifecycleObserver mFragmentLifeCycleObserver =
            new DefaultLifecycleObserver() {
                @Override
                public void onResume(LifecycleOwner lifecycleOwner) {
                    mMediator.updateToggleStateFromPref();
                }

                @Override
                public void onDestroy(LifecycleOwner lifecycleOwner) {
                    lifecycleOwner.getLifecycle().removeObserver(this);
                }
            };

    /**
     * Creates a new coordinator and attaches it to the fragment. Waits until the fragment is ready
     * before completing initialization.
     *
     * @param fragment An @{link AutofillOptionsFragment} hosting all settings.
     * @param restartRunnable A @{link Runnable} to restart Chrome when settings change.
     */
    public static void createFor(
            AutofillOptionsFragment fragment,
            Supplier<ModalDialogManager> modalDialogManagerSupplier,
            Runnable restartRunnable) {
        new AutofillOptionsCoordinator(fragment, modalDialogManagerSupplier, restartRunnable)
                .initializeOnViewCreated();
    }

    @VisibleForTesting
    AutofillOptionsCoordinator(
            AutofillOptionsFragment fragment,
            Supplier<ModalDialogManager> modalDialogManagerSupplier,
            Runnable restartRunnable) {
        assert ChromeFeatureList.isEnabled(
                AutofillFeatures.AUTOFILL_VIRTUAL_VIEW_STRUCTURE_ANDROID);
        mFragment = fragment;
        mMediator =
                new AutofillOptionsMediator(
                        mFragment.getProfile(),
                        modalDialogManagerSupplier,
                        this::buildRestartConfirmationDialog,
                        restartRunnable);
    }

    /**
     * Create the model and MCP with {@link initializeNow} once the view is created.
     *
     * <p>The view's lifecycle is not available at this point, so observe the {@link LiveData} for
     * it to get notified when {@link onCreateView} is called. Then stop observing the lifecycle
     * owner and start observing the view lifecycle.
     */
    private void initializeOnViewCreated() {
        mFragment
                .getViewLifecycleOwnerLiveData()
                .observe(mFragment, mFragmentLifeCycleOwnerObserver);
    }

    /**
     * Creates the model and returns it for testing. Assigns the model to the mediator and connects
     * the view to the model.
     */
    @VisibleForTesting
    PropertyModel initializeNow() {
        PropertyModel model =
                new PropertyModel.Builder(AutofillOptionsProperties.ALL_KEYS)
                        .with(ON_THIRD_PARTY_TOGGLE_CHANGED, mMediator::onThirdPartyToggleChanged)
                        .build();
        mMediator.initialize(model, mFragment.getReferrer(), mFragment.getContext());

        PropertyModelChangeProcessor.create(model, mFragment, AutofillOptionsViewBinder::bind);
        return model;
    }

    private void onLifeCycleOwnerChanged(LifecycleOwner lifecycleOwner) {
        if (lifecycleOwner == null) {
            return; // Ignore events before {@link onCreateView} creates a new lifeCycleOwner.
        }
        // If it hasn't happened yet, initialize all subcomponents with the available view now.
        if (!mMediator.isInitialized()) {
            mFragment
                    .getViewLifecycleOwnerLiveData()
                    .removeObserver(mFragmentLifeCycleOwnerObserver);
            initializeNow();
            observeLifecycle(lifecycleOwner.getLifecycle());
        }
    }

    private PropertyModel buildRestartConfirmationDialog() {
        return new PropertyModel.Builder(ModalDialogProperties.ALL_KEYS)
                .with(
                        ModalDialogProperties.TITLE,
                        getString(R.string.autofill_options_restart_prompt_title))
                .with(
                        ModalDialogProperties.MESSAGE_PARAGRAPH_1,
                        getString(R.string.autofill_options_restart_prompt_text))
                .with(
                        ModalDialogProperties.POSITIVE_BUTTON_TEXT,
                        getString(R.string.autofill_options_confirm_restart))
                .with(
                        ModalDialogProperties.NEGATIVE_BUTTON_TEXT,
                        getString(R.string.autofill_options_undo_toggle_change))
                .with(ModalDialogProperties.CANCEL_ON_TOUCH_OUTSIDE, true)
                .with(ModalDialogProperties.CONTROLLER, mMediator)
                .build();
    }

    @VisibleForTesting
    void observeLifecycle(Lifecycle lifecycle) {
        lifecycle.addObserver(mFragmentLifeCycleObserver);
    }

    private String getString(@StringRes int messageId) {
        return mFragment.getResources().getString(messageId);
    }
}
