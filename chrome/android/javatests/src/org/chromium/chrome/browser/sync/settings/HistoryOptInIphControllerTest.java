// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.sync.settings;

import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

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
import org.mockito.quality.Strictness;

import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.ui.theme.ChromeSemanticColorUtils;
import org.chromium.chrome.browser.user_education.IphCommand;
import org.chromium.chrome.browser.user_education.UserEducationHelper;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.components.browser_ui.settings.ChromeSwitchPreference;

/** Tests for {@link HistoryOptInIphController}. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@Batch(Batch.PER_CLASS)
public class HistoryOptInIphControllerTest {
    @Rule
    public final MockitoRule mMockitoRule = MockitoJUnit.rule().strictness(Strictness.STRICT_STUBS);

    public final ChromeTabbedActivityTestRule mActivityTestRule =
            new ChromeTabbedActivityTestRule();

    @Mock private ChromeSwitchPreference mPreferenceMock;
    @Mock private UserEducationHelper mUserEducationHelperMock;
    @Captor private ArgumentCaptor<IphCommand> mIphCommandCaptor;

    private HistoryOptInIphController mController;

    @Before
    public void setUp() {
        mActivityTestRule.startMainActivityOnBlankPage();
        when(mPreferenceMock.getContext()).thenReturn(mActivityTestRule.getActivity());

        mController = new HistoryOptInIphController(mUserEducationHelperMock);
    }

    @Test
    @SmallTest
    public void testShowIph() {
        mController.showIph(
                mPreferenceMock,
                mActivityTestRule.getActivity().findViewById(R.id.account_section_history_toggle));
        verify(mUserEducationHelperMock).requestShowIph(mIphCommandCaptor.capture());

        IphCommand command = mIphCommandCaptor.getValue();
        command.onShowCallback.run();
        verify(mPreferenceMock)
                .setBackgroundColor(
                        ChromeSemanticColorUtils.getIphHighlightColor(
                                mActivityTestRule.getActivity()));

        command.onDismissCallback.run();
        verify(mPreferenceMock).clearBackgroundColor();
    }
}
