// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.common;

import androidx.annotation.IntDef;

import org.chromium.content_public.common.IsolatedWorldIds;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * The Java copy of //chrome/common/chrome_isolated_world_ids.
 * Please check there for details about each id.
 *
 * Both files must be kept in sync.
 */
// LINT.IfChange
@IntDef({
    ChromeIsolatedWorldIds.ISOLATED_WORLD_ID_TRANSLATE,
    ChromeIsolatedWorldIds.ISOLATED_WORLD_ID_CHROME_INTERNAL,
    ChromeIsolatedWorldIds.ISOLATED_WORLD_UNUSED_MAC,
    ChromeIsolatedWorldIds.ISOLATED_WORLD_ID_UNUSED_EXTENSIONS
})
@Retention(RetentionPolicy.SOURCE)
public @interface ChromeIsolatedWorldIds {
    int ISOLATED_WORLD_ID_TRANSLATE = IsolatedWorldIds.ISOLATED_WORLD_ID_CONTENT_END + 1;
    int ISOLATED_WORLD_ID_CHROME_INTERNAL = IsolatedWorldIds.ISOLATED_WORLD_ID_CONTENT_END + 2;
    int ISOLATED_WORLD_UNUSED_MAC = IsolatedWorldIds.ISOLATED_WORLD_ID_CONTENT_END + 3;
    int ISOLATED_WORLD_ID_UNUSED_EXTENSIONS = IsolatedWorldIds.ISOLATED_WORLD_ID_CONTENT_END + 4;
}
// LINT.ThenChange(//chrome/common/chrome_isolated_world_ids.h)
