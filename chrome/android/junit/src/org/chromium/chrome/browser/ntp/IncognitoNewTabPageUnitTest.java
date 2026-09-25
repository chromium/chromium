// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.verify;

import android.text.Layout;
import android.text.Spanned;
import android.text.style.ClickableSpan;
import android.view.InputDevice;
import android.view.MotionEvent;
import android.view.View.MeasureSpec;
import android.widget.LinearLayout;
import android.widget.ScrollView;
import android.widget.TextView;

import androidx.test.ext.junit.rules.ActivityScenarioRule;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.lifetime.Destroyable;
import org.chromium.base.supplier.ObservableSuppliers;
import org.chromium.base.supplier.SettableMonotonicObservableSupplier;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.ntp.IncognitoNewTabPageView.IncognitoNewTabPageManager;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.ui.edge_to_edge.EdgeToEdgeController;
import org.chromium.chrome.browser.ui.native_page.NativePageHost;
import org.chromium.ui.base.TestActivity;
import org.chromium.ui.edge_to_edge.EdgeToEdgePadAdjuster;
import org.chromium.ui.widget.TextViewWithClickableSpans;

/** Unit test for {@link org.chromium.chrome.browser.ntp.IncognitoNewTabPage} */
@RunWith(BaseRobolectricTestRunner.class)
public class IncognitoNewTabPageUnitTest {
    @Rule
    public ActivityScenarioRule<TestActivity> mScenarioRule =
            new ActivityScenarioRule<>(TestActivity.class);

    @Rule public MockitoRule rule = MockitoJUnit.rule();

    @Mock NativePageHost mHost;
    @Mock Profile mProfile;
    @Mock TabModelSelector mTabModelSelector;
    @Mock Tab mTab;
    @Mock Destroyable mMarginSupplier;
    @Mock IncognitoNewTabPageManager mIncognitoNtpManager;

    @Mock EdgeToEdgeController mEdgeToEdgeController;
    @Captor ArgumentCaptor<EdgeToEdgePadAdjuster> mEdgePadAdjusterCaptor;

    private TestActivity mActivity;
    private IncognitoNewTabPage mIncognitoNtp;
    private final SettableMonotonicObservableSupplier<EdgeToEdgeController> mEdgeToEdgeSupplier =
            ObservableSuppliers.createMonotonic();

    @Before
    public void setup() {
        mScenarioRule.getScenario().onActivity(activity -> mActivity = activity);

        doReturn(true).when(mProfile).isOffTheRecord();

        doReturn(mActivity).when(mHost).getContext();
        doReturn(mMarginSupplier).when(mHost).createDefaultMarginAdapter(any());

        IncognitoNewTabPage.setIncognitoNtpManagerForTesting(mIncognitoNtpManager);

        mIncognitoNtp =
                new IncognitoNewTabPage(
                        mActivity, mHost, mProfile, mTabModelSelector, mTab, mEdgeToEdgeSupplier);
    }

    @Test
    public void setupEdgeToEdgeWithInsets() {
        mEdgeToEdgeSupplier.set(mEdgeToEdgeController);
        verify(mEdgeToEdgeController).registerAdjuster(mEdgePadAdjusterCaptor.capture());

        // Simulate a new bottom insets is set.
        mEdgePadAdjusterCaptor.getValue().overrideBottomInset(100);

        ScrollView view = mIncognitoNtp.mIncognitoNewTabPageView.getScrollView();
        assertEquals("Bottom padding should be set. ", 100, view.getPaddingBottom());
        assertFalse(
                "ScrollView should not clip to padding under E2E mode.", view.getClipToPadding());
    }

    @Test
    public void setupEdgeToEdgeWithoutInsets() {
        mEdgeToEdgeSupplier.set(mEdgeToEdgeController);
        verify(mEdgeToEdgeController).registerAdjuster(mEdgePadAdjusterCaptor.capture());
        assertTrue("Incognito NTP should support E2E.", mIncognitoNtp.supportsEdgeToEdge());

        // Simulate a new bottom insets is set.
        mEdgePadAdjusterCaptor.getValue().overrideBottomInset(0);

        ScrollView view = mIncognitoNtp.mIncognitoNewTabPageView.getScrollView();
        assertEquals("Bottom padding should be set. ", 0, view.getPaddingBottom());
        assertTrue(
                "ScrollView should be clip to padding where there's no bottom insets.",
                view.getClipToPadding());
    }

    @Test
    public void testDescriptionViewLayoutAdaptsToViewWidth() {
        IncognitoDescriptionView descriptionView =
                mIncognitoNtp.getView().findViewById(R.id.new_tab_incognito_container);
        LinearLayout bulletpointsContainer =
                descriptionView.findViewById(R.id.new_tab_incognito_bulletpoints_container);

        float density = mActivity.getResources().getDisplayMetrics().density;

        // Simulate measuring the view at a narrow width (e.g. 580dp, as when vertical tabs is
        // shown).
        int narrowWidthPx = Math.round(580 * density);
        int heightPx = Math.round(800 * density);

        descriptionView.measure(
                MeasureSpec.makeMeasureSpec(narrowWidthPx, MeasureSpec.EXACTLY),
                MeasureSpec.makeMeasureSpec(heightPx, MeasureSpec.EXACTLY));

        // When view width is <= 720dp, bullet points should be stacked vertically (narrow layout).
        assertEquals(
                "Bullet points should be vertical when view width is narrow.",
                LinearLayout.VERTICAL,
                bulletpointsContainer.getOrientation());

        // Simulate measuring the view at a wide width (e.g. 800dp).
        int wideWidthPx = Math.round(800 * density);
        descriptionView.measure(
                MeasureSpec.makeMeasureSpec(wideWidthPx, MeasureSpec.EXACTLY),
                MeasureSpec.makeMeasureSpec(heightPx, MeasureSpec.EXACTLY));

        // When view width is > 720dp, bullet points should be horizontal (wide layout).
        assertEquals(
                "Bullet points should be horizontal when view width is wide.",
                LinearLayout.HORIZONTAL,
                bulletpointsContainer.getOrientation());
    }

    @Test
    public void testContextClickLearnMore_NarrowLayout() {
        mActivity.setContentView(mIncognitoNtp.getView());
        IncognitoDescriptionView descriptionView =
                mIncognitoNtp.getView().findViewById(R.id.new_tab_incognito_container);

        float density = mActivity.getResources().getDisplayMetrics().density;
        int narrowWidthPx = Math.round(580 * density);
        int heightPx = Math.round(800 * density);
        descriptionView.measure(
                MeasureSpec.makeMeasureSpec(narrowWidthPx, MeasureSpec.EXACTLY),
                MeasureSpec.makeMeasureSpec(heightPx, MeasureSpec.EXACTLY));
        descriptionView.layout(/* l= */ 0, /* t= */ 0, narrowWidthPx, heightPx);

        TextViewWithClickableSpans learnMore = descriptionView.findViewById(R.id.learn_more);
        float[] coords = getClickableSpanCenterCoordinates(learnMore);
        MotionEvent event = createSecondaryMouseClickEvent(coords[0], coords[1]);

        assertTrue(
                "Secondary mouse click (right-click) on Learn More in narrow layout should show a"
                        + " context menu.",
                learnMore.onGenericMotionEvent(event));
        event.recycle();
    }

    @Test
    public void testContextClickLearnMore_WideLayout() {
        mActivity.setContentView(mIncognitoNtp.getView());
        IncognitoDescriptionView descriptionView =
                mIncognitoNtp.getView().findViewById(R.id.new_tab_incognito_container);

        float density = mActivity.getResources().getDisplayMetrics().density;
        int wideWidthPx = Math.round(800 * density);
        int heightPx = Math.round(800 * density);
        descriptionView.measure(
                MeasureSpec.makeMeasureSpec(wideWidthPx, MeasureSpec.EXACTLY),
                MeasureSpec.makeMeasureSpec(heightPx, MeasureSpec.EXACTLY));
        descriptionView.layout(/* l= */ 0, /* t= */ 0, wideWidthPx, heightPx);

        TextViewWithClickableSpans subtitle =
                descriptionView.findViewById(R.id.new_tab_incognito_subtitle);
        float[] coords = getClickableSpanCenterCoordinates(subtitle);
        MotionEvent event = createSecondaryMouseClickEvent(coords[0], coords[1]);

        assertTrue(
                "Secondary mouse click (right-click) on Learn More in wide layout subtitle should"
                        + " show a context menu.",
                subtitle.onGenericMotionEvent(event));
        event.recycle();
    }

    @Test
    public void testContextClickTrackingProtectionLink() {
        mActivity.setContentView(mIncognitoNtp.getView());
        IncognitoDescriptionView descriptionView =
                mIncognitoNtp.getView().findViewById(R.id.new_tab_incognito_container);

        float density = mActivity.getResources().getDisplayMetrics().density;
        int narrowWidthPx = Math.round(580 * density);
        descriptionView.measure(
                MeasureSpec.makeMeasureSpec(narrowWidthPx, MeasureSpec.EXACTLY),
                MeasureSpec.makeMeasureSpec(/* size= */ 0, MeasureSpec.UNSPECIFIED));
        descriptionView.layout(
                /* l= */ 0,
                /* t= */ 0,
                descriptionView.getMeasuredWidth(),
                descriptionView.getMeasuredHeight());

        TextViewWithClickableSpans trackingProtectionDescription =
                descriptionView.findViewById(R.id.tracking_protection_card_description);
        float[] coords = getClickableSpanCenterCoordinates(trackingProtectionDescription);
        MotionEvent event = createSecondaryMouseClickEvent(coords[0], coords[1]);

        assertTrue(
                "Secondary mouse click (right-click) on Tracking Protection third-party cookies"
                        + " link should show a context menu.",
                trackingProtectionDescription.onGenericMotionEvent(event));
        event.recycle();
    }

    private static MotionEvent createSecondaryMouseClickEvent(float x, float y) {
        MotionEvent.PointerProperties props = new MotionEvent.PointerProperties();
        props.id = 0;
        props.toolType = MotionEvent.TOOL_TYPE_MOUSE;

        MotionEvent.PointerCoords coords = new MotionEvent.PointerCoords();
        coords.x = x;
        coords.y = y;

        return MotionEvent.obtain(
                /* downTime= */ 0,
                /* eventTime= */ 0,
                MotionEvent.ACTION_BUTTON_PRESS,
                /* pointerCount= */ 1,
                new MotionEvent.PointerProperties[] {props},
                new MotionEvent.PointerCoords[] {coords},
                /* metaState= */ 0,
                MotionEvent.BUTTON_SECONDARY,
                /* xPrecision= */ 1f,
                /* yPrecision= */ 1f,
                /* deviceId= */ 0,
                /* edgeFlags= */ 0,
                InputDevice.SOURCE_MOUSE,
                /* flags= */ 0);
    }

    private static float[] getClickableSpanCenterCoordinates(TextView view) {
        Spanned text = (Spanned) view.getText();
        ClickableSpan[] spans =
                text.getSpans(/* queryStart= */ 0, text.length(), ClickableSpan.class);
        assertTrue("Expected at least one ClickableSpan in view.", spans.length > 0);
        int spanStart = text.getSpanStart(spans[0]);
        int spanEnd = text.getSpanEnd(spans[0]);

        Layout layout = view.getLayout();
        int line = layout.getLineForOffset(spanStart);
        float endX =
                spanEnd >= layout.getLineEnd(line)
                        ? layout.getLineRight(line)
                        : layout.getPrimaryHorizontal(spanEnd);
        float x =
                (layout.getPrimaryHorizontal(spanStart) + endX) / 2f
                        + view.getTotalPaddingLeft()
                        - view.getScrollX();
        float y =
                (layout.getLineTop(line) + layout.getLineBottom(line)) / 2f
                        + view.getTotalPaddingTop()
                        - view.getScrollY();
        return new float[] {x, y};
    }
}
