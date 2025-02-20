// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.educational_tip;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.content.Context;
import android.text.StaticLayout;
import android.view.LayoutInflater;
import android.view.View.OnLayoutChangeListener;
import android.widget.TextView;

import androidx.test.core.app.ApplicationProvider;
import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;

@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class EducationalTipModuleViewUnitTest {

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private TextView mMockContentTitleView;
    @Mock private StaticLayout mMockContentTitleViewLayout;
    @Mock private TextView mMockContentDescriptionView;

    @Captor private ArgumentCaptor<OnLayoutChangeListener> mOnLayoutChangeListenerCaptor;

    private EducationalTipModuleView mModuleView;
    private Context mContext;

    @Before
    public void setUp() {
        mContext = ApplicationProvider.getApplicationContext();

        mModuleView =
                (EducationalTipModuleView)
                        LayoutInflater.from(mContext)
                                .inflate(R.layout.educational_tip_module_layout, null);
    }

    @After
    public void tearDown() {
        mModuleView.destroyForTesting();
        mModuleView = null;
    }

    @Test
    @SmallTest
    public void testSetContentTitleAndDescription() {
        String testTitle1 = "This is a test title";
        String testTitle2 = "Here is another test title";

        TextView contentTitleView =
                mModuleView.findViewById(R.id.educational_tip_module_content_title);
        TextView contentDescriptionView =
                mModuleView.findViewById(R.id.educational_tip_module_content_description);

        Assert.assertEquals("", contentTitleView.getText());
        mModuleView.setContentTitle(testTitle1);
        Assert.assertEquals(testTitle1, contentTitleView.getText());
        mModuleView.setContentTitle(testTitle2);
        Assert.assertEquals(testTitle2, contentTitleView.getText());

        Assert.assertEquals("", contentDescriptionView.getText());
        mModuleView.setContentDescription(testTitle1);
        Assert.assertEquals(testTitle1, contentDescriptionView.getText());
        mModuleView.setContentDescription(testTitle2);
        Assert.assertEquals(testTitle2, contentDescriptionView.getText());
    }

    @Test
    @SmallTest
    public void testUpdateContentTitleAndDescriptionMaxLines() {
        Assert.assertTrue(mModuleView.getIsTitleSingleLineForTesting());

        // Test if the title exceeds the available horizontal space, wrap it to two lines and limit
        // the description to a single line.
        when(mMockContentTitleView.getLayout()).thenReturn(mMockContentTitleViewLayout);
        when(mMockContentTitleViewLayout.getEllipsisCount(/* line= */ 0)).thenReturn(1);
        mModuleView.setContentTitleViewForTesting(mMockContentTitleView);
        mModuleView.setContentDescriptionViewForTesting(mMockContentDescriptionView);

        mModuleView.updateContentTitleAndDescriptionMaxLines();
        verify(mMockContentTitleView, times(1)).setMaxLines(2);
        verify(mMockContentDescriptionView, times(1)).setMaxLines(1);
        Assert.assertFalse(mModuleView.getIsTitleSingleLineForTesting());

        // Test if the title fits within a single line, the description should span two lines.
        when(mMockContentTitleView.getLineCount()).thenReturn(1);
        when(mMockContentTitleViewLayout.getEllipsisCount(/* line= */ 0)).thenReturn(0);

        mModuleView.updateContentTitleAndDescriptionMaxLines();
        verify(mMockContentTitleView, times(1)).setMaxLines(1);
        verify(mMockContentDescriptionView, times(1)).setMaxLines(2);
        Assert.assertTrue(mModuleView.getIsTitleSingleLineForTesting());
    }

    @Test
    @SmallTest
    public void testOnLayoutChangeListener() {
        mModuleView.setContentTitleViewForTesting(mMockContentTitleView);
        mModuleView.setContentTitleViewOnLayoutChangeListener();

        verify(mMockContentTitleView)
                .addOnLayoutChangeListener(mOnLayoutChangeListenerCaptor.capture());
        mOnLayoutChangeListenerCaptor
                .getValue()
                .onLayoutChange(mMockContentTitleView, 0, 0, 0, 0, 0, 0, 0, 0);

        verify(mMockContentTitleView, times(1)).post(any());
    }
}
