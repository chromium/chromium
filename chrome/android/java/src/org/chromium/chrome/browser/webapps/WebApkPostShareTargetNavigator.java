// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webapps;

import org.chromium.base.annotations.NativeMethods;
import org.chromium.content_public.browser.WebContents;

/**
 * Perform navigation for share target with POST request.
 */
public class WebApkPostShareTargetNavigator {
    public boolean navigateIfPostShareTarget(
            String url,
            WebApkInfo.ShareTarget target,
            WebApkInfo.ShareData data, WebContents webContents) {
        WebApkShareTargetUtil.PostData postData =
                WebApkShareTargetUtil.computePostData(target, data);
        if (postData == null) {
            return false;
        }

        boolean[] isValueFileUris = new boolean[postData.isValueFileUri.size()];
        for (int i = 0; i < isValueFileUris.length; i++) {
            isValueFileUris[i] = postData.isValueFileUri.get(i);
        }

        WebApkPostShareTargetNavigatorJni.get().nativeLoadViewForShareTargetPost(
                postData.isMultipartEncoding, postData.names.toArray(new String[0]),
                postData.values.toArray(new String[0]), isValueFileUris,
                postData.filenames.toArray(new String[0]), postData.types.toArray(new String[0]),
                url, webContents);
        return true;
    }

    @NativeMethods
    public interface Natives {
        void nativeLoadViewForShareTargetPost(boolean isMultipartEncoding,
                String[] names, String[] values, boolean[] isValueFileUris, String[] filenames,
                String[] types, String startUrl, WebContents webContents);
    }
}
