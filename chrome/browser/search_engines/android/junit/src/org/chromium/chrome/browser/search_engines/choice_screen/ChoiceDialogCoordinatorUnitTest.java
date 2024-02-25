// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.search_engines.choice_screen;

import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;

import android.app.Activity;
import android.view.View;

import com.google.common.collect.ImmutableList;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.mockito.quality.Strictness;

import org.chromium.base.Callback;
import org.chromium.base.Promise;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features;
import org.chromium.chrome.browser.app.ChromeActivity;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.search_engines.DefaultSearchEngineDialogHelper;
import org.chromium.chrome.browser.search_engines.SearchEnginePromoType;
import org.chromium.components.search_engines.FakeTemplateUrl;
import org.chromium.components.search_engines.TemplateUrl;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogManager.ModalDialogPriority;
import org.chromium.ui.modaldialog.ModalDialogManager.ModalDialogType;
import org.chromium.ui.modaldialog.ModalDialogProperties;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.List;

@RunWith(BaseRobolectricTestRunner.class)
@Features.EnableFeatures({ChromeFeatureList.SEARCH_ENGINE_CHOICE})
public class ChoiceDialogCoordinatorUnitTest {
    public @Rule TestRule mFeatureProcessor = new Features.JUnitProcessor();
    public @Rule MockitoRule mMockitoRule = MockitoJUnit.rule().strictness(Strictness.STRICT_STUBS);

    private @Mock ChromeActivity<?> mActivity;
    private @Mock View mContentView;
    private @Mock ModalDialogManager mModalDialogManager;
    private @Mock DefaultSearchEngineDialogHelper.Delegate mDialogHelperDelegate;
    private @Mock Callback<Boolean> mOnSuccessCallback;
    private @Mock ChoiceScreenCoordinator mContentCoordinator;
    private @Captor ArgumentCaptor<PropertyModel> mDialogModelCaptor;

    private static final TemplateUrl FAKE_SEARCH_ENGINE_A =
            new FakeTemplateUrl("Search Engine A", "sea");
    private static final TemplateUrl FAKE_SEARCH_ENGINE_B =
            new FakeTemplateUrl("Search Engine B", "seb");

    @Before
    public void setUp() {
        doReturn(mModalDialogManager).when(mActivity).getModalDialogManager();
        doReturn(mContentView).when(mContentCoordinator).getContentView();

        doReturn(ImmutableList.of(FAKE_SEARCH_ENGINE_A, FAKE_SEARCH_ENGINE_B))
                .when(mDialogHelperDelegate)
                .getSearchEnginesForPromoDialog(anyInt());
    }

    @Test
    public void testShowDialog() {
        ChoiceDialogCoordinator coordinator =
                new ChoiceDialogCoordinator(mActivity, mDialogHelperDelegate, mOnSuccessCallback) {
                    @Override
                    ChoiceScreenCoordinator buildContentCoordinator(
                            Activity activity, ChoiceScreenDelegate delegate) {
                        return mContentCoordinator;
                    }
                };

        verify(mModalDialogManager, never()).showDialog(any(), anyInt(), anyInt());
        coordinator.show();

        verify(mModalDialogManager)
                .showDialog(
                        mDialogModelCaptor.capture(),
                        eq(ModalDialogType.APP),
                        eq(ModalDialogPriority.VERY_HIGH));

        // Simulate an unexpected dialog closure.
        int unexpectedDismissalCause = ChoiceDialogCoordinator.SUCCESS_DISMISSAL_CAUSE + 1;
        PropertyModel dialogModel = mDialogModelCaptor.getValue();
        dialogModel
                .get(ModalDialogProperties.CONTROLLER)
                .onDismiss(dialogModel, unexpectedDismissalCause);

        verify(mOnSuccessCallback).onResult(false);
    }

    @Test
    public void testChooseOnDialog() {
        final Promise<ChoiceScreenDelegate> delegateCaptor = new Promise<>();

        ChoiceDialogCoordinator coordinator =
                new ChoiceDialogCoordinator(mActivity, mDialogHelperDelegate, mOnSuccessCallback) {
                    @Override
                    ChoiceScreenCoordinator buildContentCoordinator(
                            Activity activity, ChoiceScreenDelegate delegate) {
                        delegateCaptor.fulfill(delegate);
                        return mContentCoordinator;
                    }
                };

        verify(mModalDialogManager, never()).showDialog(any(), anyInt(), anyInt());
        coordinator.show();

        verify(mModalDialogManager)
                .showDialog(
                        mDialogModelCaptor.capture(),
                        eq(ModalDialogType.APP),
                        eq(ModalDialogPriority.VERY_HIGH));
        assertTrue(delegateCaptor.isFulfilled());

        // Simulate a user choice.
        delegateCaptor.getResult().onChoiceMade(FAKE_SEARCH_ENGINE_B.getKeyword());

        verify(mDialogHelperDelegate)
                .onUserSearchEngineChoice(
                        SearchEnginePromoType.SHOW_WAFFLE,
                        List.of(
                                FAKE_SEARCH_ENGINE_A.getKeyword(),
                                FAKE_SEARCH_ENGINE_B.getKeyword()),
                        FAKE_SEARCH_ENGINE_B.getKeyword());
        verify(mOnSuccessCallback).onResult(true);
    }
}
