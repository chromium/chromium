// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.safety_hub;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;

import android.app.Activity;
import android.graphics.drawable.Drawable;
import android.view.View;
import android.widget.ImageView;
import android.widget.TextView;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Robolectric;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.ui.base.TestActivity;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;
import org.chromium.ui.widget.ButtonCompat;

import java.util.concurrent.atomic.AtomicBoolean;

/** Test relating to binding for the Safety Hub Magic Stack view. */
@RunWith(BaseRobolectricTestRunner.class)
@Batch(Batch.UNIT_TESTS)
public class SafetyHubMagicStackViewBinderTest {
    private static final String HEADER_STRING = "header";
    private static final String TITLE_STRING = "title";
    private static final String SUMMARY_STRING = "summary";
    private static final String BUTTON_STRING = "button";

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private Drawable mDrawable;

    private Activity mActivity;
    private SafetyHubMagicStackView mView;
    private PropertyModel mModel;
    private PropertyModelChangeProcessor mPropertyModelChangeProcessor;

    @Before
    public void setUp() {
        mActivity = Robolectric.buildActivity(TestActivity.class).get();
        mView =
                (SafetyHubMagicStackView)
                        mActivity
                                .getLayoutInflater()
                                .inflate(R.layout.safety_hub_magic_stack_view, null);

        mModel = new PropertyModel(SafetyHubMagicStackViewProperties.ALL_KEYS);
        mPropertyModelChangeProcessor =
                PropertyModelChangeProcessor.create(
                        mModel, mView, SafetyHubMagicStackViewBinder::bind);
    }

    @After
    public void tearDown() {
        mPropertyModelChangeProcessor.destroy();
    }

    @Test
    public void testSetHeader() {
        TextView headerView = mView.findViewById(R.id.header);
        assertEquals("", headerView.getText());

        mModel.set(SafetyHubMagicStackViewProperties.HEADER, HEADER_STRING);
        assertEquals(HEADER_STRING, headerView.getText());
    }

    @Test
    public void testSetTitle() {
        TextView titleView = mView.findViewById(R.id.title);
        assertEquals("", titleView.getText());

        mModel.set(SafetyHubMagicStackViewProperties.TITLE, TITLE_STRING);
        assertEquals(TITLE_STRING, titleView.getText());
    }

    @Test
    public void testSetSummary() {
        TextView summaryView = mView.findViewById(R.id.summary);
        assertEquals("", summaryView.getText());
        assertEquals(View.GONE, summaryView.getVisibility());

        mModel.set(SafetyHubMagicStackViewProperties.SUMMARY, SUMMARY_STRING);
        assertEquals(SUMMARY_STRING, summaryView.getText());
        assertEquals(View.VISIBLE, summaryView.getVisibility());

        mModel.set(SafetyHubMagicStackViewProperties.SUMMARY, null);
        assertEquals("", summaryView.getText());
        assertEquals(View.GONE, summaryView.getVisibility());

        mModel.set(SafetyHubMagicStackViewProperties.SUMMARY, "");
        assertEquals("", summaryView.getText());
        assertEquals(View.GONE, summaryView.getVisibility());
    }

    @Test
    public void testSetIconDrawable() {
        ImageView iconView = mView.findViewById(R.id.icon);
        assertNull(iconView.getDrawable());

        mModel.set(SafetyHubMagicStackViewProperties.ICON_DRAWABLE, mDrawable);
        assertNotNull(iconView.getDrawable());
    }

    @Test
    public void testSetButtonText() {
        ButtonCompat buttonView = mView.findViewById(R.id.button);
        assertEquals("", buttonView.getText());

        mModel.set(SafetyHubMagicStackViewProperties.BUTTON_TEXT, BUTTON_STRING);
        assertEquals(BUTTON_STRING, buttonView.getText());
    }

    @Test
    public void testSetButtonOnClickListener() {
        AtomicBoolean buttonClicked = new AtomicBoolean();
        buttonClicked.set(false);

        ButtonCompat buttonView = mView.findViewById(R.id.button);
        buttonView.performClick();
        assertFalse(buttonClicked.get());

        mModel.set(
                SafetyHubMagicStackViewProperties.BUTTON_ON_CLICK_LISTENER,
                (view) -> buttonClicked.set(true));

        buttonView.performClick();
        assertTrue(buttonClicked.get());
    }
}
