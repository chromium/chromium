// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.password_check;

import android.content.Context;
import android.view.MenuItem;

import androidx.annotation.VisibleForTesting;
import androidx.lifecycle.LifecycleObserver;

import org.chromium.chrome.browser.feedback.HelpAndFeedbackLauncherFactory;
import org.chromium.chrome.browser.password_check.helper.PasswordCheckChangePasswordHelper;
import org.chromium.chrome.browser.password_check.helper.PasswordCheckIconHelper;
import org.chromium.chrome.browser.password_manager.settings.PasswordAccessReauthenticationHelper;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.favicon.LargeIconBridge;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

/**
 * Creates the PasswordCheckComponentUi. This class is responsible for managing the UI for the check
 * of the leaked password.
 */
class PasswordCheckCoordinator implements PasswordCheckComponentUi, LifecycleObserver {
    private final Profile mProfile;
    private final PasswordCheckFragmentView mFragmentView;
    private final PasswordAccessReauthenticationHelper mReauthenticationHelper;
    private final PasswordCheckMediator mMediator;
    private PropertyModel mModel;

    /** Blueprint for a class that handles interactions with credentials. */
    interface CredentialEventHandler {
        /**
         * Edits the given Credential in the password store.
         * @param credential A {@link CompromisedCredential} to be edited.
         * @param context The context to launch the editing UI from.
         */
        void onEdit(CompromisedCredential credential, Context context);

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
    }

    PasswordCheckCoordinator(
            PasswordCheckFragmentView fragmentView,
            CustomTabIntentHelper customTabIntentHelper,
            TrustedIntentHelper trustedIntentHelper,
            Profile profile) {
        mProfile = profile;
        mFragmentView = fragmentView;
        // TODO(crbug.com/40138266): If help is part of the view, make mediator the delegate.
        mFragmentView.setComponentDelegate(this);

        // TODO(crbug.com/40749164): Ideally, the following replaces the lifecycle event forwarding.
        //  Figure out why it isn't working and use the following lifecycle observer once it does:
        // mFragmentView.getLifecycle().addObserver(this);

        mReauthenticationHelper =
                new PasswordAccessReauthenticationHelper(
                        mFragmentView.getActivity(), mFragmentView.getParentFragmentManager());

        PasswordCheckChangePasswordHelper changePasswordHelper =
                new PasswordCheckChangePasswordHelper(
                        mFragmentView.getActivity(), customTabIntentHelper, trustedIntentHelper);
        PasswordCheckIconHelper iconHelper =
                new PasswordCheckIconHelper(
                        new LargeIconBridge(profile),
                        mFragmentView
                                .getResources()
                                .getDimensionPixelSize(R.dimen.default_favicon_size));
        mMediator =
                new PasswordCheckMediator(
                        changePasswordHelper, mReauthenticationHelper, iconHelper);
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
            mMediator.initialize(
                    mModel,
                    PasswordCheckFactory.getOrCreate(),
                    mFragmentView.getReferrer(),
                    this::launchCheckupInAccount);
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

    // TODO(crbug.com/40138266): Move to view code.
    @Override
    public boolean handleHelp(MenuItem item) {
        if (item.getItemId() == R.id.menu_id_targeted_help) {
            HelpAndFeedbackLauncherFactory.getForProfile(mProfile)
                    .show(
                            mFragmentView.getActivity(),
                            mFragmentView
                                    .getActivity()
                                    .getString(R.string.help_context_check_passwords),
                            null);
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
