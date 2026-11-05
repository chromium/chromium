// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.junit.Assert.assertEquals;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoInteractions;

import android.app.Activity;
import android.view.View;
import android.widget.FrameLayout;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.mockito.quality.Strictness;
import org.robolectric.Robolectric;

import org.chromium.base.Callback;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.user_education.IphCommand;
import org.chromium.chrome.browser.user_education.UserEducationHelper;
import org.chromium.chrome.tab_ui.R;
import org.chromium.components.feature_engagement.FeatureConstants;

/** Unit tests for {@link TabListEditorIphController}. */
@RunWith(BaseRobolectricTestRunner.class)
@NullMarked
public class TabListEditorIphControllerUnitTest {
    @Rule
    public final MockitoRule mMockitoRule = MockitoJUnit.rule().strictness(Strictness.STRICT_STUBS);

    @Mock private UserEducationHelper mUserEducationHelper;
    @Captor private ArgumentCaptor<IphCommand> mIphCommandCaptor;

    private FrameLayout mCardView;
    private View mTitleView;
    private @Nullable View mFirstTabView;
    private TabListEditorIphController mController;

    @Before
    public void setUp() {
        Activity activity = Robolectric.buildActivity(Activity.class).setup().get();
        mCardView = new FrameLayout(activity);
        mTitleView = new View(activity);
        mTitleView.setId(R.id.tab_title);
        mCardView.addView(mTitleView);
        activity.setContentView(mCardView);

        mFirstTabView = mCardView;
        TabListEditorLayout layout =
                new TabListEditorLayout(activity, Robolectric.buildAttributeSet().build()) {
                    @Override
                    void runOnFirstTabViewAvailable(Callback<@Nullable View> callback) {
                        callback.onResult(mFirstTabView);
                    }
                };
        mController = new TabListEditorIphController(mUserEducationHelper, layout);
    }

    @Test
    public void maybeShowSelectTabsIph_titleViewVisible_showsIphAnchoredToTitleView() {
        HistogramWatcher histogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        TabListEditorIphController.RESOLVED_ANCHOR_VIEW_HISTOGRAM, true);

        mController.maybeShowSelectTabsIph();

        histogramWatcher.assertExpected();
        verify(mUserEducationHelper).requestShowIph(mIphCommandCaptor.capture());
        IphCommand command = mIphCommandCaptor.getValue();
        assertEquals(FeatureConstants.TAB_PICKER_SELECT_TABS_FEATURE, command.featureName);
        assertEquals(mTitleView, command.anchorView);
        assertEquals(R.string.tabpicker_select_tabs_iph, command.stringId);
    }

    @Test
    public void maybeShowSelectTabsIph_nullCardView_dropsIph() {
        mFirstTabView = null;
        assertIphDropped();
    }

    @Test
    public void maybeShowSelectTabsIph_titleViewNotFound_dropsIph() {
        mCardView.removeView(mTitleView);
        assertIphDropped();
    }

    @Test
    public void maybeShowSelectTabsIph_titleViewNotShown_dropsIph() {
        mTitleView.setVisibility(View.GONE);
        assertIphDropped();
    }

    @Test
    public void destroy_dismissesIph() {
        mController.destroy();

        verify(mUserEducationHelper).dismissTextBubble();
    }

    private void assertIphDropped() {
        HistogramWatcher histogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        TabListEditorIphController.RESOLVED_ANCHOR_VIEW_HISTOGRAM, false);

        mController.maybeShowSelectTabsIph();

        histogramWatcher.assertExpected();
        verifyNoInteractions(mUserEducationHelper);
    }
}
