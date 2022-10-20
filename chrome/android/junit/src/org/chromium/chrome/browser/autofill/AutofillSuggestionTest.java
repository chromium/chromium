// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.chrome.browser.autofill;

import static org.junit.Assert.assertEquals;
import static org.mockito.Mockito.mock;

import android.graphics.Bitmap;

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
        AutofillSuggestion suggestion =
                new AutofillSuggestion("label", "secondary_label", "sublabel", "secondary_sublabel",
                        "item_tag", 1, true, 1, true, true, true, "feature_for_iph",
                        mock(GURL.class), Bitmap.createBitmap(100, 200, Bitmap.Config.ARGB_8888));
        assertEquals(suggestion.toBuilder().build(), suggestion);
    }
}
