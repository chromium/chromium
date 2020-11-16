// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions.header;

import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.notNull;
import static org.mockito.Mockito.reset;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.app.Activity;
import android.os.Bundle;
import android.view.accessibility.AccessibilityNodeInfo;
import android.view.accessibility.AccessibilityNodeInfo.AccessibilityAction;
import android.widget.ImageView;
import android.widget.TextView;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.MockitoAnnotations;
import org.robolectric.Robolectric;

import org.chromium.chrome.R;
import org.chromium.testing.local.LocalRobolectricTestRunner;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

/**
 * Tests for {@link BaseSuggestionViewProcessor}.
 */
@RunWith(LocalRobolectricTestRunner.class)
public class HeaderViewBinderUnitTest {
    Activity mActivity;
    PropertyModel mModel;

    HeaderView mHeaderView;
    @Mock
    TextView mHeaderText;
    @Mock
    ImageView mHeaderIcon;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        mActivity = Robolectric.buildActivity(Activity.class).setup().get();
        mActivity.setTheme(R.style.Light);

        mHeaderView = mock(HeaderView.class,
                Mockito.withSettings().useConstructor(mActivity).defaultAnswer(
                        Mockito.CALLS_REAL_METHODS));

        when(mHeaderView.getTextView()).thenReturn(mHeaderText);
        when(mHeaderView.getIconView()).thenReturn(mHeaderIcon);

        mModel = new PropertyModel(HeaderViewProperties.ALL_KEYS);
        PropertyModelChangeProcessor.create(mModel, mHeaderView, HeaderViewBinder::bind);
    }

    @Test
    public void actionIcon_iconReflectsExpandedState() {
        // Expand.
        mModel.set(HeaderViewProperties.IS_COLLAPSED, false);
        verify(mHeaderIcon, times(1)).setImageResource(R.drawable.ic_expand_less_black_24dp);

        // Collapse.
        mModel.set(HeaderViewProperties.IS_COLLAPSED, true);
        verify(mHeaderIcon, times(1)).setImageResource(R.drawable.ic_expand_more_black_24dp);
    }

    @Test
    public void headerView_accessibilityStringReflectsExpandedState() {
        // Expand without title.
        mModel.set(HeaderViewProperties.IS_COLLAPSED, false);
        verify(mHeaderView, times(1)).setCollapsedStateForAccessibility(false);

        mModel.set(HeaderViewProperties.IS_COLLAPSED, true);
        verify(mHeaderView, times(1)).setCollapsedStateForAccessibility(true);
    }

    @Test
    public void headerView_listenerInstalledWhenDelegateIsSet() {
        final HeaderViewProperties.Delegate delegate = mock(HeaderViewProperties.Delegate.class);

        // Install.
        mModel.set(HeaderViewProperties.DELEGATE, delegate);
        verify(mHeaderView, times(1)).setOnClickListener(notNull());
        verify(mHeaderView, times(1)).setOnSelectListener(notNull());

        // Call.
        Assert.assertTrue(mHeaderView.performClick());
        verify(delegate, times(1)).onHeaderClicked();

        // Select.
        mHeaderView.setSelected(false);
        verify(delegate, never()).onHeaderSelected();
        mHeaderView.setSelected(true);
        verify(delegate, times(1)).onHeaderSelected();

        // Remove.
        mModel.set(HeaderViewProperties.DELEGATE, null);
        verify(mHeaderView, times(1)).setOnClickListener(null);
        verify(mHeaderView, times(1)).setOnSelectListener(null);

        reset(delegate);

        // Call; check that click calls are no longer propagated.
        // Note: performClick returns value indicating whether onClickListener was invoked.
        Assert.assertFalse(mHeaderView.performClick());
        verify(delegate, never()).onHeaderClicked();

        // Select.
        mHeaderView.setSelected(true);
        verify(delegate, never()).onHeaderSelected();
    }

    @Test
    public void actionIcon_accessibilityAnnouncementsReflectExpandedState() {
        final AccessibilityNodeInfo info = mock(AccessibilityNodeInfo.class);
        Bundle infoExtras = new Bundle();
        when(info.getExtras()).thenReturn(infoExtras);

        // Expand.
        mModel.set(HeaderViewProperties.IS_COLLAPSED, false);
        mHeaderView.onInitializeAccessibilityNodeInfo(info);
        verify(info, times(1)).addAction(AccessibilityAction.ACTION_COLLAPSE);
        verify(info, never()).addAction(AccessibilityAction.ACTION_EXPAND);

        reset(info);
        when(info.getExtras()).thenReturn(infoExtras);

        // Collapse.
        mModel.set(HeaderViewProperties.IS_COLLAPSED, true);
        mHeaderView.onInitializeAccessibilityNodeInfo(info);
        verify(info, never()).addAction(AccessibilityAction.ACTION_COLLAPSE);
        verify(info, times(1)).addAction(AccessibilityAction.ACTION_EXPAND);
    }
}
