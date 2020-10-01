// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.password_check;

import android.view.MenuItem;

import androidx.annotation.VisibleForTesting;
import androidx.lifecycle.LifecycleObserver;

import org.chromium.chrome.browser.feedback.HelpAndFeedbackLauncherImpl;
import org.chromium.chrome.browser.password_check.helper.PasswordCheckChangePasswordHelper;
import org.chromium.chrome.browser.password_check.helper.PasswordCheckIconHelper;
import org.chromium.chrome.browser.password_check.helper.PasswordCheckReauthenticationHelper;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.ui.favicon.LargeIconBridge;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

/**
 * Creates the PasswordCheckComponentUi. This class is responsible for managing the UI for the check
 * of the leaked password.
 */
class PasswordCheckCoordinator implements PasswordCheckComponentUi, LifecycleObserver {
    private final PasswordCheckFragmentView mFragmentView;
    private final PasswordCheckReauthenticationHelper mReauthenticationHelper;
    private final PasswordCheckMediator mMediator;
    private PropertyModel mModel;

    /**
     * Blueprint for a class that handles interactions with credentials.
     */
    interface CredentialEventHandler {
        /**
         * Edits the given Credential in the password store.
         * @param credential A {@link CompromisedCredential} to be edited.
         */
        void onEdit(CompromisedCredential credential);

        /**
         * Removes the given Credential from the password store.
         * @param credential A {@link CompromisedCredential} to be removed.
         */
        void onRemove(CompromisedCredential credential);

        /**
         * View the given Credential.
         * @param credential A {@link CompromisedCredential} to be viewed.
         */
        void onView(CompromisedCredential credential);

        /**
         * Opens a password change form or home page of |credential|'s origin or an app.
         * @param credential A {@link CompromisedCredential} to be changed.
         */
        void onChangePasswordButtonClick(CompromisedCredential credential);

        /**
         * Starts a script to change a {@link CompromisedCredential}. Can be called only if {@link
         * CompromisedCredential#hasScript()}.
         * @param credential A {@link CompromisedCredential} to be change with a script.
         */
        void onChangePasswordWithScriptButtonClick(CompromisedCredential credential);
    }

    PasswordCheckCoordinator(PasswordCheckFragmentView fragmentView) {
        mFragmentView = fragmentView;
        // TODO(crbug.com/1101256): If help is part of the view, make mediator the delegate.
        mFragmentView.setComponentDelegate(this);

        // TODO(crbug.com/1092444): Ideally, the following replaces the lifecycle event forwarding.
        //  Figure out why it isn't working and use the following lifecycle observer once it does:
        // mFragmentView.getLifecycle().addObserver(this);

        mReauthenticationHelper = new PasswordCheckReauthenticationHelper(
                mFragmentView.getActivity(), mFragmentView.getParentFragmentManager());

        mMediator = new PasswordCheckMediator(
                new PasswordCheckChangePasswordHelper(mFragmentView.getActivity()),
                mReauthenticationHelper,
                new PasswordCheckIconHelper(
                        new LargeIconBridge(Profile.getLastUsedRegularProfile()),
                        mFragmentView.getResources().getDimensionPixelSize(
                                org.chromium.chrome.browser.ui.favicon.R.dimen
                                        .default_favicon_size)));
    }

    private void launchCheckupInAccount() {
        PasswordCheckFactory.getOrCreate().launchCheckupInAccount(mFragmentView.getActivity());
    }

    @Override
    public void onStartFragment() {
        // In the rare case of a restarted activity, don't recreate the model and mediator.
        if (mModel == null) {
            mModel = PasswordCheckProperties.createDefaultModel();
            PasswordCheckCoordinator.setUpModelChangeProcessors(mModel, mFragmentView);
            mMediator.initialize(mModel, PasswordCheckFactory.getOrCreate(),
                    mFragmentView.getReferrer(), this::launchCheckupInAccount);
        }
    }

    @Override
    public void onResumeFragment() {
        mMediator.onResumeFragment();
        mReauthenticationHelper.onReauthenticationMaybeHappened();
    }

    @Override
    public void onDestroyFragment() {
        mMediator.stopCheck();
        if (mFragmentView.getActivity() == null || mFragmentView.getActivity().isFinishing()) {
            mMediator
                    .onUserLeavesCheckPage(); // Should be called only if the activity is finishing.
            mMediator.destroy();
            mModel = null;
        }
    }

    // TODO(crbug.com/1101256): Move to view code.
    @Override
    public boolean handleHelp(MenuItem item) {
        if (item.getItemId() == org.chromium.chrome.R.id.menu_id_targeted_help) {
            HelpAndFeedbackLauncherImpl.getInstance().show(mFragmentView.getActivity(),
                    mFragmentView.getActivity().getString(
                            org.chromium.chrome.R.string.help_context_check_passwords),
                    Profile.getLastUsedRegularProfile(), null);
            return true;
        }
        return false;
    }

    @Override
    public void destroy() {
        PasswordCheckFactory.destroy();
    }

    /**
     * Connects the given model with the given view using Model Change Processors.
     * @param model A {@link PropertyModel} built with {@link PasswordCheckProperties}.
     * @param view A {@link PasswordCheckFragmentView}.
     */
    @VisibleForTesting
    static void setUpModelChangeProcessors(PropertyModel model, PasswordCheckFragmentView view) {
        PropertyModelChangeProcessor.create(
                model, view, PasswordCheckViewBinder::bindPasswordCheckView);
    }
}
