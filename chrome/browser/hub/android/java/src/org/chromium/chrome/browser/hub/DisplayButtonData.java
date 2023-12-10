// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.hub;

import android.content.Context;
import android.graphics.drawable.Drawable;

/**
 * Combination of text, content description, and icon. Any combination of text and icon both may
 * ultimately be used for the actual button. Allows implementors to delay resolving the underlying
 * Android resources until bind time. This interface should be used directly when implementor is not
 * responsible for handling effect of the button.
 */
public interface DisplayButtonData {
    /** Returns the text that should be shown for this button. */
    String resolveText(Context context);

    /** Returns the content description for this button. */
    String resolveContentDescription(Context context);

    /** Returns the icon that should be shown for this button. */
    Drawable resolveIcon(Context context);
}
