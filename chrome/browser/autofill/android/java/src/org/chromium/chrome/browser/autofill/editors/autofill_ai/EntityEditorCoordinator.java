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
         */
        default void onDone(EntityInstance entityInstance) {}

        /**
         * The user has confirmed deletion of this entity instance.
         *
         * @param entityInstance the initial entity instance with no user changes.
         */
        default void onDelete(EntityInstance entityInstance) {}
    }

    public EntityEditorCoordinator(
            Activity activity, Delegate delegate, Profile profile, EntityInstance entityInstance) {
        mMediator =
                new EntityEditorMediator(
                        activity,
                        delegate,
                        assumeNonNull(IdentityServicesProvider.get().getIdentityManager(profile)),
                        PersonalDataManagerFactory.getForProfile(profile),
                        entityInstance);
        mEditorView = new EntityEditorView(activity);
    }

    public void showEditorDialog() {
        mEditorModel = mMediator.getEditorModel();
        PropertyModelChangeProcessor.create(
                mEditorModel, mEditorView, EntityEditorViewBinder::bindEditorDialogView);
        mEditorModel.set(VISIBLE, true);
    }

    EntityEditorView getEntityEditorViewForTest() {
        return mEditorView;
    }

    @Nullable PropertyModel getEditorModelForTest() {
        return mEditorModel;
    }
}
