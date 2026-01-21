// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.media.document_picture_in_picture_header;

import androidx.core.graphics.Insets;

import org.chromium.build.annotations.NullMarked;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableBooleanPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableIntPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableObjectPropertyKey;

/**
 * Properties for the Document Picture-in-Picture (PiP) header.
 *
 * <p>This class defines the keys for the property model used by the header.
 */
@NullMarked
public class DocumentPictureInPictureHeaderProperties {
    public static final WritableBooleanPropertyKey IS_SHOWN = new WritableBooleanPropertyKey();
    public static final WritableIntPropertyKey HEADER_HEIGHT = new WritableIntPropertyKey();
    public static final WritableObjectPropertyKey<Insets> HEADER_SPACING =
            new WritableObjectPropertyKey<>();
    public static final WritableIntPropertyKey BACKGROUND_COLOR = new WritableIntPropertyKey();

    public static final PropertyKey[] ALL_KEYS = {
        IS_SHOWN, HEADER_HEIGHT, HEADER_SPACING, BACKGROUND_COLOR
    };
}
