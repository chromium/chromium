// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.readaloud.miniplayer;

import android.view.View;
import android.view.ViewStub;
import android.widget.LinearLayout;

import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

/** Coordinator responsible for Read Aloud mini player lifecycle. */
public class MiniPlayerCoordinator {
    private final ViewStub mViewStub;
    private PropertyModel mModel;
    private PropertyModelChangeProcessor<PropertyModel, LinearLayout, PropertyKey>
            mModelChangeProcessor;
    private MiniPlayerMediator mMediator;
    private LinearLayout mLayout;

    public MiniPlayerCoordinator(ViewStub viewStub) {
        assert viewStub != null;
        mViewStub = viewStub;
    }

    public void show() {
        if (mLayout == null) {
            mLayout = (LinearLayout) mViewStub.inflate();
            mModel = new PropertyModel.Builder(MiniPlayerProperties.ALL_KEYS)
                             .with(MiniPlayerProperties.VIEW_VISIBILITY_KEY, View.GONE)
                             .with(MiniPlayerProperties.TITLE_KEY, "Title")
                             .with(MiniPlayerProperties.PUBLISHER_KEY, "Publisher")
                             .build();
            mModelChangeProcessor = PropertyModelChangeProcessor.create(
                    mModel, mLayout, MiniPlayerViewBinder::bind);
            mMediator = new MiniPlayerMediator(mModel);
        }
        mMediator.show();
    }

    public void dismiss() {
        mMediator.dismiss();
    }
}
