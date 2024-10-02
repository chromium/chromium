// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.touch_to_fill.password_generation;

import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.any;
import static org.mockito.Mockito.anyBoolean;
import static org.mockito.Mockito.doNothing;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import static org.chromium.chrome.browser.touch_to_fill.password_generation.TouchToFillPasswordGenerationCoordinator.INTERACTION_RESULT_HISTOGRAM;
import static org.chromium.chrome.browser.touch_to_fill.password_generation.TouchToFillPasswordGenerationCoordinator.InteractionResult.DISMISSED_FROM_NATIVE;
import static org.chromium.chrome.browser.touch_to_fill.password_generation.TouchToFillPasswordGenerationCoordinator.InteractionResult.DISMISSED_SHEET;
import static org.chromium.chrome.browser.touch_to_fill.password_generation.TouchToFillPasswordGenerationCoordinator.InteractionResult.REJECTED_GENERATED_PASSWORD;
import static org.chromium.chrome.browser.touch_to_fill.password_generation.TouchToFillPasswordGenerationCoordinator.InteractionResult.USED_GENERATED_PASSWORD;

import android.view.MotionEvent;
import android.view.ViewGroup;
import android.widget.Button;
import android.widget.FrameLayout;

import androidx.test.ext.junit.rules.ActivityScenarioRule;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.mockito.quality.Strictness;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.StateChangeReason;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetObserver;
import org.chromium.components.prefs.PrefService;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.KeyboardVisibilityDelegate;
import org.chromium.ui.base.TestActivity;
import org.chromium.ui.base.ViewAndroidDelegate;

/** Tests for {@link TouchToFillPasswordGenerationBridge} */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
@Batch(Batch.PER_CLASS)
@CommandLineFlags.Add({
    ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE,
    ChromeSwitches.DISABLE_NATIVE_INITIALIZATION
})
public class TouchToFillPasswordGenerationModuleTest {
    private TouchToFillPasswordGenerationCoordinator mCoordinator;
    private final ArgumentCaptor<BottomSheetObserver> mBottomSheetObserverCaptor =
            ArgumentCaptor.forClass(BottomSheetObserver.class);

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule().strictness(Strictness.STRICT_STUBS);

    @Rule
    public ActivityScenarioRule<TestActivity> mActivityScenarioRule =
            new ActivityScenarioRule<>(TestActivity.class);

    @Mock private WebContents mWebContents;
    @Mock private BottomSheetController mBottomSheetController;
    @Mock private TouchToFillPasswordGenerationCoordinator.Delegate mDelegate;
    @Mock private KeyboardVisibilityDelegate mKeyboardVisibilityDelegate;
    @Mock private PrefService mPrefService;
    @Mock private MotionEvent mMotionEvent;

    private static final String sTestEmailAddress = "test@email.com";
    private static final String sGeneratedPassword = "Strong generated password";
    private ViewGroup mContent;
    private TestActivity mActivity;

    @Before
    public void setUp() {
        MockitoAnnotations.openMocks(this);

        mActivityScenarioRule
                .getScenario()
                .onActivity(
                        activity -> {
                            setUpBottomSheetController();
                            mCoordinator =
                                    new TouchToFillPasswordGenerationCoordinator(
                                            mWebContents,
                                            mPrefService,
                                            mBottomSheetController,
                                            mKeyboardVisibilityDelegate,
                                            mDelegate);
                            mActivity = activity;
                            mContent = new FrameLayout(mActivity);
                            mActivity.setContentView(mContent);
                        });
    }

    private void setUpBottomSheetController() {
        when(mBottomSheetController.requestShowContent(any(), anyBoolean())).thenReturn(true);
        doNothing().when(mBottomSheetController).addObserver(mBottomSheetObserverCaptor.capture());
    }

    private void show() {
        mCoordinator.show(sGeneratedPassword, sTestEmailAddress, mActivity);
        mContent.addView(mCoordinator.getContentViewForTesting());
    }

    /**
     * Simulates a MotionEvent on the content view of the coordinator. This is necessary because
     * robolectric doesn't dispatch the event through the view hierarchy that this content view is a
     * part of.
     *
     * @return True iff that event was handled. If false, the event will be passed on.
     */
    private boolean simulateMotionEventOnSheet() {
        return mCoordinator.getContentViewForTesting().dispatchGenericMotionEvent(mMotionEvent);
    }

    @Test
    public void showsAndHidesBottomSheet() {
        show();
        verify(mBottomSheetController).requestShowContent(any(), anyBoolean());
        verify(mBottomSheetController).addObserver(any());

        mBottomSheetObserverCaptor.getValue().onSheetClosed(StateChangeReason.SWIPE);
        verify(mBottomSheetController).hideContent(any(), anyBoolean());
        verify(mBottomSheetController).removeObserver(mBottomSheetObserverCaptor.getValue());
    }

    @Test
    public void testConsumesGenericMotionEventsToPreventMouseClicksThroughSheet() {
        show();
        assertTrue(simulateMotionEventOnSheet());
    }

    @Test
    public void testBottomSheetForceHide() {
        show();
        verify(mBottomSheetController).requestShowContent(any(), anyBoolean());

        mCoordinator.hideFromNative();
        verify(mBottomSheetController).hideContent(any(), anyBoolean());
        verify(mDelegate, times(0)).onDismissed(/* passwordAccepted= */ anyBoolean());
    }

    @Test
    public void testGeneratedPasswordAcceptedCalled() {
        show();

        Button acceptPasswordButton = mContent.findViewById(R.id.use_password_button);
        acceptPasswordButton.performClick();
        verify(mDelegate).onGeneratedPasswordAccepted(sGeneratedPassword);
    }

    @Test
    public void testBottomSheetIsHiddenAfterAcceptingPassword() {
        show();

        Button acceptPasswordButton = mContent.findViewById(R.id.use_password_button);
        acceptPasswordButton.performClick();
        verify(mBottomSheetController).hideContent(any(), anyBoolean());
        verify(mDelegate).onDismissed(/* passwordAccepted= */ true);
    }

    @Test
    public void testGenerationBottomSheetDismissCountMustResetAfterAcceptance() {
        show();

        Button acceptPasswordButton = mContent.findViewById(R.id.use_password_button);
        acceptPasswordButton.performClick();
        verify(mPrefService).setInteger(Pref.PASSWORD_GENERATION_BOTTOM_SHEET_DISMISS_COUNT, 0);
    }

    @Test
    public void testGeneratedPasswordRejectedCalled() {
        show();

        Button rejectPasswordButton = mContent.findViewById(R.id.reject_password_button);
        rejectPasswordButton.performClick();
        verify(mDelegate).onGeneratedPasswordRejected();
    }

    @Test
    public void testGenerationBottomSheetDismissCountMustIncrementAfterRejection() {
        show();

        Button rejectPasswordButton = mContent.findViewById(R.id.reject_password_button);
        rejectPasswordButton.performClick();
        verify(mPrefService).setInteger(Pref.PASSWORD_GENERATION_BOTTOM_SHEET_DISMISS_COUNT, 1);
    }

    @Test
    public void testKeyboardIsHiddenWhenBottomSheetIsDisplayed() {
        ViewAndroidDelegate viewAndroidDelegate = ViewAndroidDelegate.createBasicDelegate(mContent);
        when(mWebContents.getViewAndroidDelegate()).thenReturn(viewAndroidDelegate);

        show();
        verify(mKeyboardVisibilityDelegate).hideKeyboard(mContent);
    }

    @Test
    public void testBottomSheetIsHiddenAfterRejectingPassword() {
        show();

        Button rejectPasswordButton = mContent.findViewById(R.id.reject_password_button);
        rejectPasswordButton.performClick();
        verify(mBottomSheetController).hideContent(any(), anyBoolean());
        verify(mDelegate).onDismissed(/* passwordAccepted= */ false);
    }

    @Test
    public void testGenerationBottomSheetDismissCountMustNotChangeWhenDismissedFromNative() {
        show();

        mCoordinator.hideFromNative();
        verify(mPrefService, never())
                .setInteger(eq(Pref.PASSWORD_GENERATION_BOTTOM_SHEET_DISMISS_COUNT), anyInt());
    }

    @Test
    public void recordsMetricWhenPasswordAccepted() {
        HistogramWatcher histogramExpectation =
                HistogramWatcher.newSingleRecordWatcher(
                        INTERACTION_RESULT_HISTOGRAM, USED_GENERATED_PASSWORD);
        show();

        Button acceptPasswordButton = mContent.findViewById(R.id.use_password_button);
        acceptPasswordButton.performClick();

        histogramExpectation.assertExpected();
    }

    @Test
    public void recordsMetricWhenPasswordRejected() {
        HistogramWatcher histogramExpectation =
                HistogramWatcher.newSingleRecordWatcher(
                        INTERACTION_RESULT_HISTOGRAM, REJECTED_GENERATED_PASSWORD);
        show();

        Button rejectPasswordButton = mContent.findViewById(R.id.reject_password_button);
        rejectPasswordButton.performClick();

        histogramExpectation.assertExpected();
    }

    @Test
    public void recordsMetricWhenDismissedFromNative() {
        HistogramWatcher histogramExpectation =
                HistogramWatcher.newSingleRecordWatcher(
                        INTERACTION_RESULT_HISTOGRAM, DISMISSED_FROM_NATIVE);
        show();

        mCoordinator.hideFromNative();

        histogramExpectation.assertExpected();
    }

    @Test
    public void recordsMetricWhenDismissedByUser() {
        HistogramWatcher histogramExpectation =
                HistogramWatcher.newSingleRecordWatcher(
                        INTERACTION_RESULT_HISTOGRAM, DISMISSED_SHEET);
        show();

        ArgumentCaptor<BottomSheetObserver> observer =
                ArgumentCaptor.forClass(BottomSheetObserver.class);
        verify(mBottomSheetController).addObserver(observer.capture());

        observer.getValue().onSheetClosed(StateChangeReason.TAP_SCRIM);
        histogramExpectation.assertExpected();
    }
}
