// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox;

import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.anyString;
import static org.mockito.Mockito.doReturn;

import static org.chromium.base.test.util.Batch.PER_CLASS;

import android.app.Activity;
import android.view.View;
import android.view.View.MeasureSpec;
import android.view.ViewGroup.MarginLayoutParams;

import androidx.test.filters.MediumTest;
import androidx.test.filters.SmallTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.MathUtils;
import org.chromium.base.ThreadUtils;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.omnibox.styles.OmniboxResourceProvider;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.R;
import org.chromium.chrome.test.transit.ChromeTransitTestRules;
import org.chromium.chrome.test.transit.FreshCtaTransitTestRule;
import org.chromium.chrome.test.transit.omnibox.OmniboxFacility;
import org.chromium.chrome.test.transit.page.WebPageStation;
import org.chromium.components.omnibox.AutocompleteInput;
import org.chromium.components.omnibox.OmniboxFocusReason;
import org.chromium.ui.base.DeviceFormFactor;
import org.chromium.ui.permissions.AndroidPermissionDelegate;

/** Unit tests for {@link LocationBarLayout}. */
@Batch(PER_CLASS)
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class LocationBarLayoutTest {
    private static final String SEARCH_TERMS = "machine learning";
    private static final String SEARCH_TERMS_URL = "testing.com";

    @Rule
    public FreshCtaTransitTestRule mActivityTestRule =
            ChromeTransitTestRules.freshChromeTabbedActivityRule();

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock AndroidPermissionDelegate mAndroidPermissionDelegate;
    private WebPageStation mPage;

    @Before
    public void setUp() throws InterruptedException {
        mPage = mActivityTestRule.startOnBlankPage();

        doReturn(true).when(mAndroidPermissionDelegate).hasPermission(anyString());
        mActivityTestRule
                .getActivity()
                .getWindowAndroid()
                .setAndroidPermissionDelegate(mAndroidPermissionDelegate);
    }

    private String getUrlText(UrlBar urlBar) {
        return ThreadUtils.runOnUiThreadBlocking(() -> urlBar.getText().toString());
    }

    private UrlBar getUrlBar() {
        return (UrlBar) mActivityTestRule.getActivity().findViewById(R.id.url_bar);
    }

    private LocationBarLayout getLocationBar() {
        return (LocationBarLayout) mActivityTestRule.getActivity().findViewById(R.id.location_bar);
    }

    private LocationBarMediator getLocationBarMediator() {
        LocationBarCoordinator locationBarCoordinator =
                (LocationBarCoordinator)
                        mActivityTestRule.getActivity().getToolbarManager().getLocationBar();
        return locationBarCoordinator.getMediatorForTesting();
    }

    @Test
    @SmallTest
    @Restriction(DeviceFormFactor.PHONE)
    public void testNotShowingVoiceSearchButtonIfUrlBarContainsText() {
        // When there is text, the delete button should be visible.
        OmniboxFacility omnibox = mPage.openOmnibox();
        omnibox.setText("testing");

        omnibox.deleteButtonElement.checkPresent();
        omnibox.micButtonElement.checkAbsent();
    }

    @Test
    @SmallTest
    @Restriction(DeviceFormFactor.PHONE)
    public void testShowingVoiceSearchButtonIfUrlBarIsEmpty() {
        // When there's no text, the mic button should be visible.
        OmniboxFacility omnibox = mPage.openOmnibox();

        omnibox.micButtonElement.checkPresent();
        omnibox.deleteButtonElement.checkAbsent();
    }

    @Test
    @SmallTest
    public void testDeleteButton() {
        OmniboxFacility omnibox = mPage.openOmnibox();
        omnibox.setText("testing").clickDelete();

        omnibox.deleteButtonElement.checkAbsent();
        omnibox.urlBarElement.check(matches(withText("")));
    }

    @Test
    @SmallTest
    @DisabledTest(message = "crbug.com/455509545")
    public void testSetUrlBarFocus() {
        LocationBarMediator locationBarMediator = getLocationBarMediator();

        assertEquals(
                0, RecordHistogram.getHistogramTotalCountForTesting("Android.OmniboxFocusReason"));
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    locationBarMediator.beginInput(
                            new AutocompleteInput()
                                    .setUserText(SEARCH_TERMS_URL)
                                    .setFocusReason(OmniboxFocusReason.FAKE_BOX_LONG_PRESS));
                });
        assertTrue(getLocationBarMediator().isUrlBarFocused());
        assertTrue(getLocationBarMediator().didFocusUrlFromFakebox());
        assertEquals(SEARCH_TERMS_URL, getUrlText(getUrlBar()));
        assertEquals(
                1, RecordHistogram.getHistogramTotalCountForTesting("Android.OmniboxFocusReason"));

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    locationBarMediator.beginInput(
                            new AutocompleteInput()
                                    .setUserText(SEARCH_TERMS_URL)
                                    .setFocusReason(OmniboxFocusReason.SEARCH_QUERY));
                });
        assertTrue(getLocationBarMediator().isUrlBarFocused());
        assertTrue(getLocationBarMediator().didFocusUrlFromFakebox());
        assertEquals(SEARCH_TERMS, getUrlText(getUrlBar()));
        assertEquals(
                1, RecordHistogram.getHistogramTotalCountForTesting("Android.OmniboxFocusReason"));

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    locationBarMediator.endInput();
                });
        assertFalse(getLocationBarMediator().isUrlBarFocused());
        assertFalse(getLocationBarMediator().didFocusUrlFromFakebox());
        assertEquals(
                1, RecordHistogram.getHistogramTotalCountForTesting("Android.OmniboxFocusReason"));

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    locationBarMediator.beginInput(
                            new AutocompleteInput().setFocusReason(OmniboxFocusReason.OMNIBOX_TAP));
                });
        assertTrue(getLocationBarMediator().isUrlBarFocused());
        assertFalse(getLocationBarMediator().didFocusUrlFromFakebox());
        assertEquals(
                2, RecordHistogram.getHistogramTotalCountForTesting("Android.OmniboxFocusReason"));
    }

    @Test
    @MediumTest
    @DisabledTest(message = "flaky, see crbug.com/359597342")
    public void testEnforceMinimumUrlBarWidth() {
        mPage.openOmnibox();

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    View urlBar = getUrlBar();
                    LocationBarLayout locationBar = getLocationBar();

                    int originalWidth = locationBar.getMeasuredWidth();
                    int constrainedWidth =
                            ((MarginLayoutParams) urlBar.getLayoutParams()).getMarginStart()
                                    + locationBar
                                            .getResources()
                                            .getDimensionPixelSize(
                                                    R.dimen.location_bar_min_url_width);
                    int urlContainerMarginEnd =
                            ((MarginLayoutParams) urlBar.getLayoutParams()).getMarginEnd();

                    locationBar.measure(
                            MeasureSpec.makeMeasureSpec(constrainedWidth, MeasureSpec.EXACTLY),
                            MeasureSpec.makeMeasureSpec(200, MeasureSpec.EXACTLY));
                    assertFalse(locationBar.getLocationBarButtonsVisibilityForTesting());

                    locationBar.measure(
                            MeasureSpec.makeMeasureSpec(originalWidth, MeasureSpec.EXACTLY),
                            MeasureSpec.makeMeasureSpec(200, MeasureSpec.EXACTLY));
                    assertTrue(locationBar.getLocationBarButtonsVisibilityForTesting());

                    locationBar.measure(
                            MeasureSpec.makeMeasureSpec(
                                    constrainedWidth + urlContainerMarginEnd - 1,
                                    MeasureSpec.EXACTLY),
                            MeasureSpec.makeMeasureSpec(200, MeasureSpec.EXACTLY));
                    assertFalse(locationBar.getLocationBarButtonsVisibilityForTesting());

                    locationBar.setUrlActionContainerVisibility(true);
                    assertFalse(locationBar.getLocationBarButtonsVisibilityForTesting());
                });
    }

    @Test
    @MediumTest
    @Restriction(DeviceFormFactor.TABLET_OR_DESKTOP)
    public void testTabletUrlBarTranslation_revampEnabled() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    LocationBarLayout locationBar = getLocationBar();
                    View urlBar = getUrlBar();

                    urlBar.requestFocus();
                    locationBar.setUrlFocusChangePercent(
                            /* ntpSearchBoxScrollFraction= */ 0,
                            /* urlFocusChangeFraction= */ MathUtils.EPSILON,
                            /* isUrlFocusChangeInProgress= */ true);

                    assertEquals(
                            locationBar.getFocusedStatusViewSpacingDelta(),
                            ((MarginLayoutParams) urlBar.getLayoutParams()).getMarginStart());
                    assertEquals(
                            locationBar.getFocusedStatusViewSpacingDelta()
                                    * (-1 + MathUtils.EPSILON),
                            urlBar.getTranslationX(),
                            MathUtils.EPSILON);

                    locationBar.setUrlFocusChangePercent(
                            /* ntpSearchBoxScrollFraction= */ 0.5f,
                            /* urlFocusChangeFraction= */ 0.5f,
                            /* isUrlFocusChangeInProgress= */ false);
                    assertEquals(
                            locationBar.getFocusedStatusViewSpacingDelta() * -0.5,
                            urlBar.getTranslationX(),
                            MathUtils.EPSILON);

                    locationBar.setUrlFocusChangePercent(
                            /* ntpSearchBoxScrollFraction= */ 1.0f,
                            /* urlFocusChangeFraction= */ 1.0f,
                            /* isUrlFocusChangeInProgress= */ false);
                    assertEquals(0f, urlBar.getTranslationX(), MathUtils.EPSILON);
                });
    }

    @Test
    @MediumTest
    @Restriction({DeviceFormFactor.PHONE})
    public void testPhoneUrlBarAndStatusViewTranslation() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    Activity activity = mActivityTestRule.getActivity();
                    int statusIconAndUrlBarOffset =
                            OmniboxResourceProvider.getToolbarSidePaddingForNtp(activity)
                                    - OmniboxResourceProvider.getToolbarSidePadding(activity);
                    LocationBarLayout locationBar = getLocationBar();
                    View urlBar = getUrlBar();
                    View statusView = locationBar.findViewById(R.id.location_bar_status);

                    urlBar.requestFocus();
                    locationBar.setUrlFocusChangePercent(
                            /* ntpSearchBoxScrollFraction= */ 1,
                            /* urlFocusChangeFraction= */ MathUtils.EPSILON,
                            /* isUrlFocusChangeInProgress= */ true);

                    assertEquals(
                            statusIconAndUrlBarOffset * (1 - MathUtils.EPSILON),
                            urlBar.getTranslationX(),
                            MathUtils.EPSILON);
                    assertEquals(
                            OmniboxResourceProvider.getFocusedStatusViewLeftSpacing(activity)
                                    + statusIconAndUrlBarOffset * (1 - MathUtils.EPSILON),
                            statusView.getTranslationX(),
                            MathUtils.EPSILON);

                    locationBar.setUrlFocusChangePercent(
                            /* ntpSearchBoxScrollFraction= */ 1,
                            /* urlFocusChangeFraction= */ 0.5f,
                            /* isUrlFocusChangeInProgress= */ true);
                    assertEquals(
                            statusIconAndUrlBarOffset * 0.5,
                            urlBar.getTranslationX(),
                            MathUtils.EPSILON);
                    assertEquals(
                            OmniboxResourceProvider.getFocusedStatusViewLeftSpacing(activity)
                                    + statusIconAndUrlBarOffset * 0.5,
                            statusView.getTranslationX(),
                            MathUtils.EPSILON);

                    locationBar.setUrlFocusChangePercent(
                            /* ntpSearchBoxScrollFraction= */ 1.0f,
                            /* urlFocusChangeFraction= */ 1.0f,
                            /* isUrlFocusChangeInProgress= */ true);
                    assertEquals(0f, urlBar.getTranslationX(), MathUtils.EPSILON);
                    assertEquals(
                            OmniboxResourceProvider.getFocusedStatusViewLeftSpacing(activity),
                            statusView.getTranslationX(),
                            MathUtils.EPSILON);
                });
    }
}
