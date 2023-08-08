// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.options;

import static org.chromium.chrome.browser.autofill.options.AutofillOptionsProperties.ON_THIRD_PARTY_TOGGLE_CHANGED;
import static org.chromium.chrome.browser.autofill.options.AutofillOptionsProperties.THIRD_PARTY_AUTOFILL_ENABLED;

import androidx.annotation.VisibleForTesting;
import androidx.lifecycle.DefaultLifecycleObserver;
import androidx.lifecycle.Lifecycle;
import androidx.lifecycle.LifecycleObserver;
import androidx.lifecycle.LifecycleOwner;
import androidx.lifecycle.Observer;

import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.components.user_prefs.UserPrefs;
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
    final LifecycleObserver mFragmentLifeCycleObserver = new DefaultLifecycleObserver() {
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
     */
    public static void createFor(AutofillOptionsFragment fragment) {
        new AutofillOptionsCoordinator(fragment).initializeOnViewCreated();
    };

    @VisibleForTesting
    AutofillOptionsCoordinator(AutofillOptionsFragment fragment) {
        assert ChromeFeatureList.isEnabled(
                ChromeFeatureList.AUTOFILL_VIRTUAL_VIEW_STRUCTURE_ANDROID);
        mFragment = fragment;
        mMediator = new AutofillOptionsMediator(mFragment.getProfile());
    }

    /**
     * Create the model and MCP with {@link initializeNow} once the view is created.
     *
     * The view's lifecycle is not available at this point, so observe the {@link LiveData} for it
     * to get notified when {@link onCreateView} is called. Then stop observing the lifecycle owner
     * and start observing the view lifecycle.
     */
    private void initializeOnViewCreated() {
        mFragment.getViewLifecycleOwnerLiveData().observe(
                mFragment, mFragmentLifeCycleOwnerObserver);
    }

    /**
     * Creates the model and returns it for testing. Assigns the model to the mediator and connects
     * the view to the model.
     */
    @VisibleForTesting
    PropertyModel initializeNow() {
        PropertyModel model =
                new PropertyModel.Builder(AutofillOptionsProperties.ALL_KEYS)
                        .with(THIRD_PARTY_AUTOFILL_ENABLED,
                                UserPrefs.get(mFragment.getProfile())
                                        .getBoolean(Pref.AUTOFILL_USING_VIRTUAL_VIEW_STRUCTURE))
                        .with(ON_THIRD_PARTY_TOGGLE_CHANGED, mMediator::onThirdPartyToggleChanged)
                        .build();
        mMediator.initialize(model);

        PropertyModelChangeProcessor.create(model, mFragment, AutofillOptionsViewBinder::bind);
        return model;
    }

    private void onLifeCycleOwnerChanged(LifecycleOwner lifecycleOwner) {
        if (lifecycleOwner == null) {
            return; // Ignore events before {@link onCreateView} creates a new lifeCycleOwner.
        }
        // If it hasn't happened yet, initialize all subcomponents with the available view now.
        if (!mMediator.isInitialized()) {
            mFragment.getViewLifecycleOwnerLiveData().removeObserver(
                    mFragmentLifeCycleOwnerObserver);
            initializeNow();
            observeLifecycle(lifecycleOwner.getLifecycle());
        }
    }

    @VisibleForTesting
    void observeLifecycle(Lifecycle lifecycle) {
        lifecycle.addObserver(mFragmentLifeCycleObserver);
    }
}
