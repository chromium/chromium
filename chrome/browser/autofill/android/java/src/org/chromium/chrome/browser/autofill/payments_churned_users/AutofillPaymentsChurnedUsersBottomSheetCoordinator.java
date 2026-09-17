// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.payments_churned_users;

import android.content.Context;
import android.view.View;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.autofill.R;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

/** Coordinator for assembling and controlling the Payments Churned Users bottom sheet. */
@NullMarked
public class AutofillPaymentsChurnedUsersBottomSheetCoordinator {
    private final AutofillPaymentsChurnedUsersBottomSheetMediator mMediator;
    private final AutofillPaymentsChurnedUsersBottomSheetView mView;
    private final PropertyModel mModel;
    private @Nullable
            PropertyModelChangeProcessor<
                    PropertyModel, AutofillPaymentsChurnedUsersBottomSheetView, PropertyKey>
            mModelChangeProcessor;

    public AutofillPaymentsChurnedUsersBottomSheetCoordinator(
            Context context, BottomSheetController bottomSheetController) {
        mView = new AutofillPaymentsChurnedUsersBottomSheetView(context);

        AutofillPaymentsChurnedUsersBottomSheetContent content =
                new AutofillPaymentsChurnedUsersBottomSheetContent(mView.getContentView());

        // TODO(crbug.com/558881009): The title string is currently hardcoded to the convenience
        // title for this skeleton. Modify this to dynamically select the appropriate title.
        mModel =
                new PropertyModel.Builder(
                                AutofillPaymentsChurnedUsersBottomSheetProperties.ALL_KEYS)
                        .with(
                                AutofillPaymentsChurnedUsersBottomSheetProperties.TITLE,
                                context.getString(
                                        R.string.autofill_churned_users_bubble_convenience_title))
                        .build();

        mMediator =
                new AutofillPaymentsChurnedUsersBottomSheetMediator(bottomSheetController, content);

        mModelChangeProcessor =
                PropertyModelChangeProcessor.create(
                        mModel, mView, AutofillPaymentsChurnedUsersBottomSheetViewBinder::bind);
    }

    public void requestShowContent() {
        mMediator.requestShowContent();
    }

    public void destroy() {
        if (mModelChangeProcessor != null) {
            mModelChangeProcessor.destroy();
            mModelChangeProcessor = null;
        }
        mMediator.destroy();
    }

    View getContentViewForTesting() {
        return mView.getContentView();
    }
}
