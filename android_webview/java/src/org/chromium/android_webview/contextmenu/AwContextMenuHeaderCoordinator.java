// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.contextmenu;

import org.chromium.components.embedder_support.contextmenu.ContextMenuParams;
import org.chromium.ui.modelutil.PropertyModel;

/** Coordinator for creating the context menu header */
public class AwContextMenuHeaderCoordinator {
    private PropertyModel mModel;

    public AwContextMenuHeaderCoordinator(ContextMenuParams params) {
        mModel = buildModel(params.getUnfilteredLinkUrl().getSpec());
    }

    private PropertyModel buildModel(String title) {
        PropertyModel model =
                new PropertyModel.Builder(AwContextMenuHeaderProperties.ALL_KEYS)
                        .with(AwContextMenuHeaderProperties.TITLE, title)
                        .build();
        return model;
    }

    PropertyModel getModel() {
        return mModel;
    }

    public String getTitle() {
        return mModel.get(AwContextMenuHeaderProperties.TITLE);
    }
}
