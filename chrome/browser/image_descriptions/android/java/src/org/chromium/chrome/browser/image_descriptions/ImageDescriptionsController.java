// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.image_descriptions;

import org.chromium.base.annotations.NativeMethods;
import org.chromium.content_public.browser.WebContents;

/**
 * Singleton class to control the Image Descriptions feature. This class can be used to initiate the
 * user flow, to turn the feature on/off and to update settings as needed.
 */
public class ImageDescriptionsController {

    @NativeMethods
    interface Natives {
        void getImageDescriptionsOnce(WebContents webContents);
    }
}