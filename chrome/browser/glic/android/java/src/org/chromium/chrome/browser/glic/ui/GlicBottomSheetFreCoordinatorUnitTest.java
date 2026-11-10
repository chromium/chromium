// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.glic.ui;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoInteractions;

import android.animation.ValueAnimator;
import android.app.Activity;
import android.graphics.Typeface;
import android.graphics.drawable.ColorDrawable;
import android.text.Spanned;
import android.text.style.StyleSpan;
import android.view.InputDevice;
import android.view.KeyEvent;
import android.view.MotionEvent;
import android.view.View;
import android.widget.FrameLayout;
import android.widget.TextView;

import androidx.core.view.ViewCompat;

import com.airbnb.lottie.LottieAnimationView;
import com.airbnb.lottie.LottieComposition;
import com.airbnb.lottie.LottieCompositionFactory;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Robolectric;
import org.robolectric.shadows.ShadowLooper;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.glic.R;
import org.chromium.components.browser_ui.settings.SettingsCustomTabLauncher;
import org.chromium.ui.base.TestActivity;
import org.chromium.ui.text.ChromeClickableSpan;
import org.chromium.ui.widget.ButtonCompat;
import org.chromium.ui.widget.ChromeImageButton;
import org.chromium.ui.widget.TextViewWithClickableSpans;

import java.util.concurrent.TimeUnit;

/** Unit tests for {@link GlicBottomSheetFreCoordinator}. */
@RunWith(BaseRobolectricTestRunner.class)
public class GlicBottomSheetFreCoordinatorUnitTest {
    private static final long FADE_ADVANCE_MS =
            GlicBottomSheetFreTransitionController.CROSSFADE_DURATION_MS + 50;

    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private Runnable mOnAccept;
    @Mock private Runnable mOnDecline;
    @Mock private SettingsCustomTabLauncher mCustomTabLauncher;

    private Activity mActivity;
    private FrameLayout mParentContainer;

    @Before
    public void setUp() {
        mActivity = Robolectric.buildActivity(TestActivity.class).setup().get();
        mActivity.setTheme(R.style.Theme_BrowserUI_DayNight);
        mParentContainer = new FrameLayout(mActivity);
        mActivity.setContentView(mParentContainer);
    }

    @After
    public void tearDown() {
        ValueAnimator.setDurationScale(1.0f);
    }

    private void ensureCompositionLoaded(LottieAnimationView view) {
        if (view.getComposition() == null) {
            LottieComposition composition =
                    LottieCompositionFactory.fromRawResSync(
                                    mActivity, R.raw.glic_fre_header_animation)
                            .getValue();
            if (composition != null) {
                view.setComposition(composition);
            }
        }
    }

    private static <T> T[] getSpans(TextView textView, Class<T> spanClass) {
        Spanned spanned = (Spanned) textView.getText();
        return spanned.getSpans(0, spanned.length(), spanClass);
    }

    private static void pressEnter(View view) {
        view.dispatchKeyEvent(new KeyEvent(KeyEvent.ACTION_DOWN, KeyEvent.KEYCODE_ENTER));
        view.dispatchKeyEvent(new KeyEvent(KeyEvent.ACTION_UP, KeyEvent.KEYCODE_ENTER));
    }

    private GlicBottomSheetFreCoordinator createCoordinator(boolean isEnterpriseAccount) {
        return createCoordinator(isEnterpriseAccount, /* backgroundColor= */ 0);
    }

    private GlicBottomSheetFreCoordinator createCoordinator(
            boolean isEnterpriseAccount, int backgroundColor) {
        return new GlicBottomSheetFreCoordinator(
                mActivity,
                mParentContainer,
                isEnterpriseAccount,
                backgroundColor,
                mCustomTabLauncher,
                mOnAccept,
                mOnDecline);
    }

    @Test
    public void testConsumerCardSetupAndSpans() {
        GlicBottomSheetFreCoordinator coordinator =
                createCoordinator(/* isEnterpriseAccount= */ false);
        View rootView = coordinator.getView();
        assertEquals(mParentContainer, rootView.getParent());
        View contentContainer = coordinator.getContentContainer();
        assertEquals(
                mActivity.getString(R.string.glic_fre_dialog_title),
                ViewCompat.getAccessibilityPaneTitle(contentContainer));
        assertTrue(contentContainer.isImportantForAccessibility());

        TextViewWithClickableSpans bodyView = rootView.findViewById(R.id.glic_fre_body);
        StyleSpan[] styleSpans = getSpans(bodyView, StyleSpan.class);
        assertEquals(1, styleSpans.length);
        assertEquals(Typeface.BOLD, styleSpans[0].getStyle());

        ChromeClickableSpan[] clickableSpans = getSpans(bodyView, ChromeClickableSpan.class);
        assertEquals(2, clickableSpans.length);
    }

    @Test
    public void testEnterpriseCardSpans() {
        GlicBottomSheetFreCoordinator coordinator =
                createCoordinator(/* isEnterpriseAccount= */ true);
        TextViewWithClickableSpans bodyView =
                coordinator.getView().findViewById(R.id.glic_fre_body);

        StyleSpan[] styleSpans = getSpans(bodyView, StyleSpan.class);
        assertEquals(2, styleSpans.length);
        assertEquals(Typeface.BOLD, styleSpans[0].getStyle());
        assertEquals(Typeface.BOLD, styleSpans[1].getStyle());
    }

    @Test
    public void testLegalLinksOpenInCustomTab() {
        GlicBottomSheetFreCoordinator coordinator =
                createCoordinator(/* isEnterpriseAccount= */ false);
        TextViewWithClickableSpans bodyView =
                coordinator.getView().findViewById(R.id.glic_fre_body);
        ChromeClickableSpan[] clickableSpans = getSpans(bodyView, ChromeClickableSpan.class);

        clickableSpans[0].onClick(bodyView);
        verify(mCustomTabLauncher)
                .openUrlInCct(mActivity, mActivity.getString(R.string.glic_fre_dialog_terms_url));

        clickableSpans[1].onClick(bodyView);
        verify(mCustomTabLauncher)
                .openUrlInCct(
                        mActivity,
                        mActivity.getString(R.string.glic_fre_dialog_privacy_notice_url));
    }

    @Test
    public void testAcceptFiresOnceAndCloseStillWorks() {
        GlicBottomSheetFreCoordinator coordinator =
                createCoordinator(/* isEnterpriseAccount= */ false);

        ButtonCompat acceptButton = coordinator.getView().findViewById(R.id.glic_fre_accept_button);
        ChromeImageButton closeButton =
                coordinator.getView().findViewById(R.id.glic_fre_close_button);

        acceptButton.performClick();
        acceptButton.performClick();
        verify(mOnAccept).run();
        verifyNoInteractions(mOnDecline);

        closeButton.performClick();
        closeButton.performClick();
        verify(mOnDecline).run();
    }

    @Test
    public void testAcceptIgnoredAfterClose() {
        GlicBottomSheetFreCoordinator coordinator =
                createCoordinator(/* isEnterpriseAccount= */ false);

        coordinator.getView().findViewById(R.id.glic_fre_close_button).performClick();
        coordinator.getView().findViewById(R.id.glic_fre_accept_button).performClick();

        verifyNoInteractions(mOnAccept);
    }

    @Test
    public void testNonClickableRootIgnoresHoverAndKeyEvents() {
        GlicBottomSheetFreCoordinator coordinator =
                createCoordinator(/* isEnterpriseAccount= */ false);
        View rootView = coordinator.getView();
        View closeButton = rootView.findViewById(R.id.glic_fre_close_button);
        ShadowLooper.idleMainLooper();
        closeButton.setFocusableInTouchMode(true);
        assertTrue(closeButton.requestFocus());
        MotionEvent hover =
                MotionEvent.obtain(
                        0,
                        0,
                        MotionEvent.ACTION_HOVER_ENTER,
                        closeButton.getX() + 1,
                        closeButton.getY() + 1,
                        0);
        hover.setSource(InputDevice.SOURCE_TOUCHSCREEN);

        rootView.setClickable(false);
        assertFalse(rootView.dispatchGenericMotionEvent(hover));
        pressEnter(rootView);
        verifyNoInteractions(mOnDecline);

        rootView.setClickable(true);
        assertTrue(rootView.dispatchGenericMotionEvent(hover));
        pressEnter(rootView);
        verify(mOnDecline).run();
        hover.recycle();
    }

    @Test
    public void testExplicitBackgroundColor() {
        int customColor = 0xFF123456;
        GlicBottomSheetFreCoordinator coordinator =
                createCoordinator(/* isEnterpriseAccount= */ false, customColor);

        assertEquals(
                customColor, ((ColorDrawable) coordinator.getView().getBackground()).getColor());
    }

    @Test
    public void testHeaderAnimationPinsSettledFrameWhenReducedMotion() {
        ValueAnimator.setDurationScale(0.0f);
        GlicBottomSheetFreCoordinator coordinator =
                createCoordinator(/* isEnterpriseAccount= */ false);
        LottieAnimationView headerAnim =
                coordinator.getView().findViewById(R.id.glic_fre_header_animation);
        ensureCompositionLoaded(headerAnim);

        assertFalse(headerAnim.isAnimating());
        assertEquals(1.0f, headerAnim.getProgress(), 0.001f);
    }

    @Test
    public void testDestroyRemovesViewDisablesClicksAndCancelsAnimation() {
        GlicBottomSheetFreCoordinator coordinator =
                createCoordinator(/* isEnterpriseAccount= */ false);
        View rootView = coordinator.getView();
        TextViewWithClickableSpans bodyView = rootView.findViewById(R.id.glic_fre_body);
        ChromeClickableSpan[] clickableSpans = getSpans(bodyView, ChromeClickableSpan.class);
        ButtonCompat acceptButton = rootView.findViewById(R.id.glic_fre_accept_button);
        ChromeImageButton closeButton = rootView.findViewById(R.id.glic_fre_close_button);
        LottieAnimationView headerAnim = rootView.findViewById(R.id.glic_fre_header_animation);

        ensureCompositionLoaded(headerAnim);
        assertTrue(headerAnim.isAnimating());

        coordinator.destroy();
        coordinator.destroy();

        assertNull(rootView.getParent());
        assertFalse(headerAnim.isAnimating());

        acceptButton.performClick();
        closeButton.performClick();
        clickableSpans[0].onClick(bodyView);

        verifyNoInteractions(mOnAccept, mOnDecline, mCustomTabLauncher);
    }

    @Test
    public void testFadeOutRunsCallbackAfterFade() {
        GlicBottomSheetFreCoordinator coordinator =
                createCoordinator(/* isEnterpriseAccount= */ false);
        View rootView = coordinator.getView();
        Runnable onDone = mock(Runnable.class);

        coordinator.fadeOut(onDone);
        coordinator.fadeOut(onDone);

        assertFalse(rootView.isClickable());
        verifyNoInteractions(onDone);
        ShadowLooper.idleMainLooper(FADE_ADVANCE_MS, TimeUnit.MILLISECONDS);
        verify(onDone).run();
        assertEquals(View.GONE, rootView.getVisibility());
    }

    @Test
    public void testDestroyCancelsFadeOut() {
        GlicBottomSheetFreCoordinator coordinator =
                createCoordinator(/* isEnterpriseAccount= */ false);
        Runnable onDone = mock(Runnable.class);

        coordinator.fadeOut(onDone);
        coordinator.destroy();
        ShadowLooper.idleMainLooper(FADE_ADVANCE_MS, TimeUnit.MILLISECONDS);

        verifyNoInteractions(onDone);
        assertNull(coordinator.getView().getParent());
    }
}
