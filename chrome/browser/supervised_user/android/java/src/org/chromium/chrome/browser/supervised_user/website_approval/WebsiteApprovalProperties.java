// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.supervised_user.website_approval;

import android.graphics.Bitmap;
import android.view.View.OnClickListener;

import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.url.GURL;

/** Properties defined here reflect the visible state of the WebsiteApproval components. */
class WebsiteApprovalProperties {
    static final PropertyModel.WritableObjectPropertyKey<String> CHILD_NAME =
            new PropertyModel.WritableObjectPropertyKey<>("child_name");
    static final PropertyModel.WritableObjectPropertyKey<Bitmap> FAVICON =
            new PropertyModel.WritableObjectPropertyKey<>("favicon");
    static final PropertyModel.ReadableObjectPropertyKey<GURL> URL =
            new PropertyModel.ReadableObjectPropertyKey<>("url");
    static final PropertyModel.WritableObjectPropertyKey<OnClickListener> ON_CLICK_APPROVE =
            new PropertyModel.WritableObjectPropertyKey<>("on_click_approve");
    static final PropertyModel.WritableObjectPropertyKey<OnClickListener> ON_CLICK_DENY =
            new PropertyModel.WritableObjectPropertyKey<>("on_click_deny");

    public static final PropertyKey[] ALL_KEYS = {
        CHILD_NAME, FAVICON, URL, ON_CLICK_APPROVE, ON_CLICK_DENY
    };
}
