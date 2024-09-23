// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.chrome.browser.autofill;

import static org.junit.Assert.assertEquals;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.when;

import android.graphics.Bitmap;
import android.graphics.drawable.BitmapDrawable;

import androidx.test.filters.SmallTest;

import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.components.autofill.AutofillSuggestion;
import org.chromium.url.GURL;

/** Unit tests for {@link AutofillSuggestion} */
@RunWith(BaseRobolectricTestRunner.class)
@SuppressWarnings("DoNotMock") // Mocks GURL.
public class AutofillSuggestionTest {
    @Test
    @SmallTest
    public void testAutofillSuggestion_toBuilder() {
        Bitmap bitmapIcon = mock(Bitmap.class);
        when(bitmapIcon.sameAs(any(Bitmap.class))).thenReturn(true);
        BitmapDrawable drawableIcon = mock(BitmapDrawable.class);
        when(drawableIcon.getBitmap()).thenReturn(bitmapIcon);

        AutofillSuggestion suggestion =
                new AutofillSuggestion(
                        "label",
                        "secondary_label",
                        "sublabel",
                        "secondary_sublabel",
                        "item_tag",
                        /* iconId= */ 1,
                        /* isIconAtStart= */ true,
                        /* popupItemId= */ 1,
                        /* isDeletable= */ true,
                        /* isMultilineLabel= */ true,
                        /* isBoldLabel= */ true,
                        /* applyDeactivatedStyle= */ false,
                        /* shouldDisplayTermsAvailable= */ false,
                        "feature_for_iph",
                        "iph_description",
                        mock(GURL.class),
                        drawableIcon);
        assertEquals(suggestion.toBuilder().build(), suggestion);
    }
}
