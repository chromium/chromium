// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.editors.autofill_ai;

import static org.chromium.build.NullUtil.assumeNonNull;
import static org.chromium.chrome.browser.autofill.editors.autofill_ai.EntityEditorProperties.VISIBLE;

import android.app.Activity;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.autofill.PersonalDataManagerFactory;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.components.autofill.autofill_ai.EntityInstance;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

/** Coordinator for the Entity Editor. */
@NullMarked
public class EntityEditorCoordinator {
    private final EntityEditorMediator mMediator;
    private final EntityEditorView mEditorView;
    private @Nullable PropertyModel mEditorModel;

    /** Delegate used to subscribe to AddressEditor user interactions. */
    public interface Delegate {
        /**
         * The user committed changes by pressing the "Done" button.
         *
         * @param entityInstance the entity instance with all updates applied.
         * @param descriptionStringId the resource ID of the string used to describe the storage
         *     used.
         * @param acceptButtonStringId the resource ID of the string used as the accept button for
         *     the consent.
         */
        default void onDone(
                EntityInstance entityInstance, int descriptionStringId, int acceptButtonStringId) {}

        /**
         * The user has confirmed deletion of this entity instance.
         *
         * @param entityInstance the initial entity instance with no user changes.
         */
        default void onDelete(EntityInstance entityInstance) {}

        /**
         * The user clicked the "manage your info" link in the source notice to open either the
         * Google Wallet passes page (when the entity is a public pass) or the help center article
         * (when the entity is a private pass).
         */
        default void onOpenGoogleWallet(boolean isPrivateEntity) {}
    }

    /**
     * Creates a new {@link EntityEditorCoordinator}.
     *
     * @param activity The activity for this component.
     * @param delegate The delegate to be notified of editor events.
     * @param profile The user's profile.
     * @param entityInstance The entity instance to be edited.
     */
    public EntityEditorCoordinator(
            Activity activity, Delegate delegate, Profile profile, EntityInstance entityInstance) {
        mMediator =
                new EntityEditorMediator(
                        activity,
                        delegate,
                        profile,
                        assumeNonNull(IdentityServicesProvider.get().getIdentityManager(profile)),
                        PersonalDataManagerFactory.getForProfile(profile),
                        entityInstance);
        mEditorView = new EntityEditorView(activity);
    }

    /** Notifies underlying view that device configuration has changed. */
    public void onConfigurationChanged() {
        mEditorView.onConfigurationChanged();
    }

    /** Initializes the editor's MCP and shows the dialog. */
    public void showEditorDialog() {
        mEditorModel = mMediator.getEditorModel();
        PropertyModelChangeProcessor.create(
                mEditorModel, mEditorView, EntityEditorViewBinder::bindEditorDialogView);
        mEditorModel.set(VISIBLE, true);
    }

    /**
     * Check if current editor dialog is visible to the user.
     *
     * @return true if this editor is visible to the user, false otherwise.
     */
    public boolean isShowing() {
        return mEditorView.isShowing();
    }

    /** Dismiss currently visible editor dialog. */
    public void dismiss() {
        mEditorView.dismiss();
    }

    EntityEditorView getEntityEditorViewForTest() {
        return mEditorView;
    }

    @Nullable PropertyModel getEditorModelForTest() {
        return mEditorModel;
    }
}
