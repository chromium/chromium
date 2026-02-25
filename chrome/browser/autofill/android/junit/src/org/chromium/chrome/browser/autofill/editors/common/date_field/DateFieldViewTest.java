// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.editors.common.date_field;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;

import android.app.Activity;
import android.view.View;
import android.widget.TextView;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.Robolectric;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.autofill.R;
import org.chromium.chrome.browser.autofill.editors.common.field.FieldView;

/** Unit test for {@link DateFieldView}. */
@RunWith(BaseRobolectricTestRunner.class)
public class DateFieldViewTest {
    private Activity mActivity;
    private DateFieldView mDateFieldView;

    @Before
    public void setUp() {
        mActivity = Robolectric.setupActivity(Activity.class);
        mDateFieldView = new DateFieldView(mActivity);
    }

    @Test
    public void testSetLabel() {
        mDateFieldView.setLabel("Test Label", /* isRequired= */ false);
        TextView labelView = mDateFieldView.findViewById(R.id.date_field_label);
        assertNotNull(labelView);
        assertEquals(View.VISIBLE, labelView.getVisibility());
        assertEquals("Test Label", labelView.getText());
    }

    @Test
    public void testSetLabel_Required() {
        mDateFieldView.setLabel("Test Label", /* isRequired= */ true);
        TextView labelView = mDateFieldView.findViewById(R.id.date_field_label);
        assertNotNull(labelView);
        assertEquals(View.VISIBLE, labelView.getVisibility());
        assertEquals("Test Label" + FieldView.REQUIRED_FIELD_INDICATOR, labelView.getText());
    }

    @Test
    public void testDropdownsExistence() {
        assertNotNull(mDateFieldView.findViewById(R.id.date_field_month_dropdown));
        assertNotNull(mDateFieldView.findViewById(R.id.date_field_day_dropdown));
        assertNotNull(mDateFieldView.findViewById(R.id.date_field_year_dropdown));
    }
}
