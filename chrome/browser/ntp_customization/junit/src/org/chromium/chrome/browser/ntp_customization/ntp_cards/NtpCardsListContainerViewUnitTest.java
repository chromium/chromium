// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp_customization.ntp_cards;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import static org.chromium.chrome.browser.magic_stack.ModuleDelegate.ModuleType.PRICE_CHANGE;
import static org.chromium.chrome.browser.magic_stack.ModuleDelegate.ModuleType.SAFETY_HUB;
import static org.chromium.chrome.browser.magic_stack.ModuleDelegate.ModuleType.SINGLE_TAB;

import android.content.Context;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.CompoundButton;

import androidx.test.core.app.ApplicationProvider;
import androidx.test.filters.SmallTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.chrome.browser.magic_stack.HomeModulesConfigManager;
import org.chromium.chrome.browser.ntp_customization.ListContainerViewDelegate;
import org.chromium.chrome.browser.ntp_customization.NtpCustomizationMetricsUtils;
import org.chromium.chrome.browser.ntp_customization.R;

import java.util.List;

/** Unit tests for {@link NtpCardsListContainerView}. */
@RunWith(BaseRobolectricTestRunner.class)
public class NtpCardsListContainerViewUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock ListContainerViewDelegate mDelegate;
    @Mock NtpCardsListItemView mListItemView;

    @Captor
    private ArgumentCaptor<CompoundButton.OnCheckedChangeListener> mOnCheckedChangeListenerCaptor;

    private NtpCardsListContainerView mContainerView;
    private List<Integer> mListContent;

    @Before
    public void setUp() {
        Context context = ApplicationProvider.getApplicationContext();
        View view =
                LayoutInflater.from(context)
                        .inflate(
                                org.chromium.chrome.browser.ntp_customization.R.layout
                                        .ntp_customization_ntp_cards_bottom_sheet,
                                null,
                                false);
        mContainerView = spy(view.findViewById(R.id.ntp_cards_container));
        mListContent = List.of(SINGLE_TAB, SAFETY_HUB, PRICE_CHANGE);
        when(mDelegate.getListItems()).thenReturn(mListContent);
        doReturn(mListItemView).when(mContainerView).createListItemView();
    }

    @Test
    @SmallTest
    public void testDelegateInRenderAllListItems() {
        mContainerView.renderAllListItems(mDelegate);

        // Verifies that only mDelegate.getListItems() and mDelegate.getListItemTitle() are called
        // when creating list items.
        verify(mDelegate).getListItems();
        for (int type : mListContent) {
            verify(mDelegate).getListItemTitle(eq(type), any(Context.class));
        }
        verify(mDelegate, never()).getListener(anyInt());
        verify(mDelegate, never()).getTrailingIcon(anyInt());
        verify(mDelegate, never()).getListItemSubtitle(anyInt(), any(Context.class));
    }

    @Test
    @SmallTest
    public void testRenderAllListItems() {
        mContainerView.renderAllListItems(mDelegate);

        // Verifies the title, background, and switch are set.
        int itemListSize = mListContent.size();
        verify(mListItemView, times(itemListSize)).setTitle(any());
        verify(mListItemView, times(itemListSize)).setBackground(any());
        verify(mContainerView, times(itemListSize))
                .setUpSwitch(
                        any(HomeModulesConfigManager.class),
                        any(NtpCardsListItemView.class),
                        anyInt());
    }

    @Test
    @SmallTest
    public void testSetUpSwitch() {
        HomeModulesConfigManager manager = mock(HomeModulesConfigManager.class);
        NtpCardsListItemView listItemView = mock(NtpCardsListItemView.class);
        String histogramTurnOnName = "NewTabPage.Customization.TurnOnModule";
        String histogramTurnOffName = "NewTabPage.Customization.TurnOffModule";

        // Verifies that getPrefModuleTypeEnabled() is called.
        mContainerView.setUpSwitch(manager, listItemView, SINGLE_TAB);
        verify(manager).getPrefModuleTypeEnabled(SINGLE_TAB);

        // Verifies setPrefModuleTypeEnabled() is called inside the OnCheckedChangeListener to
        // update the checked state of the switch.
        verify(listItemView).setOnCheckedChangeListener(mOnCheckedChangeListenerCaptor.capture());
        mOnCheckedChangeListenerCaptor
                .getValue()
                .onCheckedChanged(mock(CompoundButton.class), true);
        verify(manager).setPrefModuleTypeEnabled(SINGLE_TAB, true);
        HistogramWatcher histogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(histogramTurnOnName, SINGLE_TAB);
        NtpCustomizationMetricsUtils.recordModuleToggledInBottomSheet(SINGLE_TAB, true);
        histogramWatcher.assertExpected();

        mOnCheckedChangeListenerCaptor
                .getValue()
                .onCheckedChanged(mock(CompoundButton.class), false);
        verify(manager).setPrefModuleTypeEnabled(SINGLE_TAB, false);
        histogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(histogramTurnOffName, SINGLE_TAB);
        NtpCustomizationMetricsUtils.recordModuleToggledInBottomSheet(SINGLE_TAB, false);
        histogramWatcher.assertExpected();
    }
}
