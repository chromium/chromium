// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox;

import static android.view.View.GONE;
import static android.view.View.VISIBLE;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.withId;

import static org.hamcrest.Matchers.not;
import static org.mockito.ArgumentMatchers.anyString;
import static org.mockito.Mockito.doReturn;

import android.app.Activity;
import android.view.View;
import android.view.View.MeasureSpec;
import android.view.ViewGroup.MarginLayoutParams;
import android.widget.ImageButton;

import androidx.core.view.MarginLayoutParamsCompat;
import androidx.test.filters.MediumTest;
import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.MathUtils;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.Matchers;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.omnibox.styles.OmniboxResourceProvider;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.toolbar.LocationBarModel;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.R;
import org.chromium.chrome.test.util.OmniboxTestUtils;
import org.chromium.chrome.test.util.browser.Features.DisableFeatures;
import org.chromium.chrome.test.util.browser.Features.EnableFeatures;
import org.chromium.content_public.browser.test.util.ClickUtils;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.ui.permissions.AndroidPermissionDelegate;
import org.chromium.ui.test.util.UiRestriction;

import java.util.concurrent.Callable;
import java.util.concurrent.ExecutionException;

/** Unit tests for {@link LocationBarLayout}. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class LocationBarLayoutTest {
    private static final String SEARCH_TERMS = "machine learning";
    private static final String SEARCH_TERMS_URL = "testing.com";

    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock AndroidPermissionDelegate mAndroidPermissionDelegate;

    private OmniboxTestUtils mOmnibox;

    public static final LocationBarModel.OfflineStatus OFFLINE_STATUS =
            new LocationBarModel.OfflineStatus() {
                @Override
                public boolean isShowingTrustedOfflinePage(Tab tab) {
                    return false;
                }

                @Override
                public boolean isOfflinePage(Tab tab) {
                    return false;
                }
            };

    @Before
    public void setUp() throws InterruptedException {
        mActivityTestRule.startMainActivityOnBlankPage();
        mOmnibox = new OmniboxTestUtils(mActivityTestRule.getActivity());

        doReturn(true).when(mAndroidPermissionDelegate).hasPermission(anyString());
        mActivityTestRule
                .getActivity()
                .getWindowAndroid()
                .setAndroidPermissionDelegate(mAndroidPermissionDelegate);
    }

    private String getUrlText(UrlBar urlBar) {
        try {
            return TestThreadUtils.runOnUiThreadBlocking(() -> urlBar.getText().toString());
        } catch (ExecutionException ex) {
            throw new RuntimeException(
                    "Failed to get the UrlBar's text! Exception below:\n" + ex.toString());
        }
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
                        mActivityTestRule
                                .getActivity()
                                .getToolbarManager()
                                .getLocationBarForTesting();
        return locationBarCoordinator.getMediatorForTesting();
    }

    private ImageButton getDeleteButton() {
        return mActivityTestRule.getActivity().findViewById(R.id.delete_button);
    }

    private ImageButton getMicButton() {
        return mActivityTestRule.getActivity().findViewById(R.id.mic_button);
    }

    private View getStatusIconView() {
        return mActivityTestRule.getActivity().findViewById(R.id.location_bar_status_icon_frame);
    }

    private void setUrlBarTextAndFocus(String text) {
        final UrlBar urlBar = getUrlBar();
        TestThreadUtils.runOnUiThreadBlocking(
                () -> {
                    urlBar.requestFocus();
                });
        CriteriaHelper.pollUiThread(() -> urlBar.hasFocus());

        try {
            TestThreadUtils.runOnUiThreadBlocking(
                    new Callable<Void>() {
                        @Override
                        public Void call() throws InterruptedException {
                            mActivityTestRule.typeInOmnibox(text, false);
                            return null;
                        }
                    });
        } catch (ExecutionException e) {
            throw new RuntimeException("Failed to type \"" + text + "\" into the omnibox!");
        }
    }

    @Test
    @SmallTest
    @Restriction(UiRestriction.RESTRICTION_TYPE_PHONE)
    public void testNotShowingVoiceSearchButtonIfUrlBarContainsText() throws ExecutionException {
        // When there is text, the delete button should be visible.
        setUrlBarTextAndFocus("testing");

        onView(withId(R.id.delete_button)).check(matches(isDisplayed()));
        onView(withId(R.id.mic_button)).check(matches(not(isDisplayed())));
    }

    @Test
    @SmallTest
    @Restriction(UiRestriction.RESTRICTION_TYPE_PHONE)
    public void testShowingVoiceSearchButtonIfUrlBarIsEmpty() throws ExecutionException {
        // When there's no text, the mic button should be visible.
        setUrlBarTextAndFocus("");

        onView(withId(R.id.mic_button)).check(matches(isDisplayed()));
        onView(withId(R.id.delete_button)).check(matches(not(isDisplayed())));
    }

    @Test
    @SmallTest
    public void testDeleteButton() throws ExecutionException {
        setUrlBarTextAndFocus("testing");
        Assert.assertEquals(getDeleteButton().getVisibility(), VISIBLE);
        ClickUtils.clickButton(getDeleteButton());
        CriteriaHelper.pollUiThread(
                () -> {
                    Criteria.checkThat(getDeleteButton().getVisibility(), Matchers.not(VISIBLE));
                });
        Assert.assertEquals("", getUrlText(getUrlBar()));
    }

    @Test
    @SmallTest
    public void testSetUrlBarFocus() {
        LocationBarMediator locationBarMediator = getLocationBarMediator();

        Assert.assertEquals(
                0, RecordHistogram.getHistogramTotalCountForTesting("Android.OmniboxFocusReason"));
        TestThreadUtils.runOnUiThreadBlocking(
                () -> {
                    locationBarMediator.setUrlBarFocus(
                            true, SEARCH_TERMS_URL, OmniboxFocusReason.FAKE_BOX_LONG_PRESS);
                });
        Assert.assertTrue(getLocationBarMediator().isUrlBarFocused());
        Assert.assertTrue(getLocationBarMediator().didFocusUrlFromFakebox());
        Assert.assertEquals(SEARCH_TERMS_URL, getUrlText(getUrlBar()));
        Assert.assertEquals(
                1, RecordHistogram.getHistogramTotalCountForTesting("Android.OmniboxFocusReason"));

        TestThreadUtils.runOnUiThreadBlocking(
                () -> {
                    locationBarMediator.setUrlBarFocus(
                            true, SEARCH_TERMS, OmniboxFocusReason.SEARCH_QUERY);
                });
        Assert.assertTrue(getLocationBarMediator().isUrlBarFocused());
        Assert.assertTrue(getLocationBarMediator().didFocusUrlFromFakebox());
        Assert.assertEquals(SEARCH_TERMS, getUrlText(getUrlBar()));
        Assert.assertEquals(
                1, RecordHistogram.getHistogramTotalCountForTesting("Android.OmniboxFocusReason"));

        TestThreadUtils.runOnUiThreadBlocking(
                () -> {
                    locationBarMediator.setUrlBarFocus(false, null, OmniboxFocusReason.UNFOCUS);
                });
        Assert.assertFalse(getLocationBarMediator().isUrlBarFocused());
        Assert.assertFalse(getLocationBarMediator().didFocusUrlFromFakebox());
        Assert.assertEquals(
                1, RecordHistogram.getHistogramTotalCountForTesting("Android.OmniboxFocusReason"));

        TestThreadUtils.runOnUiThreadBlocking(
                () -> {
                    locationBarMediator.setUrlBarFocus(true, null, OmniboxFocusReason.OMNIBOX_TAP);
                });
        Assert.assertTrue(getLocationBarMediator().isUrlBarFocused());
        Assert.assertFalse(getLocationBarMediator().didFocusUrlFromFakebox());
        Assert.assertEquals(
                2, RecordHistogram.getHistogramTotalCountForTesting("Android.OmniboxFocusReason"));
    }

    @Test
    @MediumTest
    public void testUpdateLayoutParams() {
        LocationBarLayout locationBar = (LocationBarLayout) getLocationBar();
        View statusIcon = getStatusIconView();
        View urlContainer = getUrlBar();

        TestThreadUtils.runOnUiThreadBlocking(
                () -> {
                    getUrlBar().requestFocus();

                    MarginLayoutParams urlLayoutParams =
                            (MarginLayoutParams) urlContainer.getLayoutParams();
                    MarginLayoutParamsCompat.setMarginEnd(
                            urlLayoutParams, /* very random, and only used to fail a check */
                            13047);
                    urlContainer.setLayoutParams(urlLayoutParams);

                    statusIcon.setVisibility(GONE);
                    locationBar.updateLayoutParams(
                            MeasureSpec.makeMeasureSpec(1000, MeasureSpec.EXACTLY));
                    urlLayoutParams = (MarginLayoutParams) urlContainer.getLayoutParams();
                    int endMarginNoIcon = MarginLayoutParamsCompat.getMarginEnd(urlLayoutParams);

                    MarginLayoutParamsCompat.setMarginEnd(
                            urlLayoutParams, /* very random, and only used to fail a check */
                            13047);
                    urlContainer.setLayoutParams(urlLayoutParams);

                    statusIcon.setVisibility(VISIBLE);
                    locationBar.updateLayoutParams(
                            MeasureSpec.makeMeasureSpec(1000, MeasureSpec.EXACTLY));
                    urlLayoutParams = (MarginLayoutParams) urlContainer.getLayoutParams();
                    int endMarginWithIcon = MarginLayoutParamsCompat.getMarginEnd(urlLayoutParams);

                    Assert.assertEquals(
                            endMarginNoIcon + locationBar.getEndPaddingPixelSizeOnFocusDelta(),
                            endMarginWithIcon);
                });
    }

    @Test
    @MediumTest
    public void testEnforceMinimumUrlBarWidth() {
        setUrlBarTextAndFocus("");

        TestThreadUtils.runOnUiThreadBlocking(
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
                    Assert.assertEquals(
                            locationBar.findViewById(R.id.url_action_container).getVisibility(),
                            View.INVISIBLE);

                    locationBar.measure(
                            MeasureSpec.makeMeasureSpec(originalWidth, MeasureSpec.EXACTLY),
                            MeasureSpec.makeMeasureSpec(200, MeasureSpec.EXACTLY));
                    Assert.assertEquals(
                            locationBar.findViewById(R.id.url_action_container).getVisibility(),
                            View.VISIBLE);

                    locationBar.measure(
                            MeasureSpec.makeMeasureSpec(
                                    constrainedWidth + urlContainerMarginEnd - 1,
                                    MeasureSpec.EXACTLY),
                            MeasureSpec.makeMeasureSpec(200, MeasureSpec.EXACTLY));
                    Assert.assertEquals(
                            locationBar.findViewById(R.id.url_action_container).getVisibility(),
                            View.INVISIBLE);

                    locationBar.setUrlActionContainerVisibility(VISIBLE);
                    Assert.assertEquals(
                            locationBar.findViewById(R.id.url_action_container).getVisibility(),
                            View.INVISIBLE);
                });
    }

    @Test
    @MediumTest
    @EnableFeatures(ChromeFeatureList.OMNIBOX_MODERNIZE_VISUAL_UPDATE)
    @Restriction({UiRestriction.RESTRICTION_TYPE_TABLET})
    public void testTabletUrlBarTranslation_revampEnabled() {
        OmniboxFeatures.ENABLE_MODERNIZE_VISUAL_UPDATE_ON_TABLET.setForTesting(true);
        OmniboxFeatures.MODERNIZE_VISUAL_UPDATE_SMALLEST_MARGINS.setForTesting(true);

        TestThreadUtils.runOnUiThreadBlocking(
                () -> {
                    LocationBarLayout locationBar = getLocationBar();
                    View urlBar = getUrlBar();

                    urlBar.requestFocus();
                    int marginStart =
                            ((MarginLayoutParams) urlBar.getLayoutParams()).getMarginStart();
                    locationBar.updateLayoutParams(
                            MeasureSpec.makeMeasureSpec(
                                    locationBar.getMeasuredWidth(), MeasureSpec.EXACTLY));
                    locationBar.setUrlFocusChangePercent(
                            /* ntpSearchBoxScrollFraction= */ 0,
                            /* startSurfaceScrollFraction= */ 0,
                            /* urlFocusChangeFraction= */ MathUtils.EPSILON,
                            /* isUrlFocusChangeInProgress= */ true);

                    Assert.assertEquals(
                            marginStart + locationBar.getFocusedStatusViewSpacingDelta(),
                            ((MarginLayoutParams) urlBar.getLayoutParams()).getMarginStart());
                    Assert.assertEquals(
                            locationBar.getFocusedStatusViewSpacingDelta()
                                    * (-1 + MathUtils.EPSILON),
                            urlBar.getTranslationX(),
                            MathUtils.EPSILON);

                    locationBar.setUrlFocusChangePercent(
                            /* ntpSearchBoxScrollFraction= */ 0.5f,
                            /* startSurfaceScrollFraction= */ 0.5f,
                            /* urlFocusChangeFraction= */ 0.5f,
                            /* isUrlFocusChangeInProgress= */ false);
                    Assert.assertEquals(
                            locationBar.getFocusedStatusViewSpacingDelta() * -0.5,
                            urlBar.getTranslationX(),
                            MathUtils.EPSILON);

                    locationBar.setUrlFocusChangePercent(
                            /* ntpSearchBoxScrollFraction= */ 1.0f,
                            /* startSurfaceScrollFraction= */ 1.0f,
                            /* urlFocusChangeFraction= */ 1.0f,
                            /* isUrlFocusChangeInProgress= */ false);
                    Assert.assertEquals(0f, urlBar.getTranslationX(), MathUtils.EPSILON);
                });
    }

    @Test
    @MediumTest
    @DisableFeatures(ChromeFeatureList.OMNIBOX_MODERNIZE_VISUAL_UPDATE)
    @Restriction({UiRestriction.RESTRICTION_TYPE_TABLET})
    public void testTabletUrlBarTranslation_revampDisabled() {
        OmniboxFeatures.ENABLE_MODERNIZE_VISUAL_UPDATE_ON_TABLET.setForTesting(false);
        OmniboxFeatures.MODERNIZE_VISUAL_UPDATE_SMALLEST_MARGINS.setForTesting(false);

        Assert.assertEquals(0, getLocationBar().getEndPaddingPixelSizeOnFocusDelta());

        TestThreadUtils.runOnUiThreadBlocking(
                () -> {
                    LocationBarLayout locationBar = getLocationBar();
                    View urlBar = getUrlBar();

                    urlBar.requestFocus();
                    int marginStart =
                            ((MarginLayoutParams) urlBar.getLayoutParams()).getMarginStart();
                    locationBar.setUrlFocusChangePercent(
                            /* ntpSearchBoxScrollFraction= */ 0,
                            /* startSurfaceScrollFraction= */ 0,
                            /* urlFocusChangeFraction= */ 0.5f,
                            /* isUrlFocusChangeInProgress= */ true);
                    Assert.assertEquals(
                            marginStart,
                            ((MarginLayoutParams) urlBar.getLayoutParams()).getMarginStart());
                    Assert.assertEquals(0f, urlBar.getTranslationX(), MathUtils.EPSILON);

                    locationBar.updateLayoutParams(
                            MeasureSpec.makeMeasureSpec(
                                    locationBar.getMeasuredWidth(), MeasureSpec.EXACTLY));
                    Assert.assertEquals(
                            marginStart,
                            ((MarginLayoutParams) urlBar.getLayoutParams()).getMarginStart());
                });
    }

    @Test
    @MediumTest
    @DisableFeatures(ChromeFeatureList.AVOID_RELAYOUT_DURING_FOCUS_ANIMATION)
    @EnableFeatures(ChromeFeatureList.OMNIBOX_MODERNIZE_VISUAL_UPDATE)
    @Restriction({UiRestriction.RESTRICTION_TYPE_TABLET})
    public void testTabletUrlBarTranslation_revampEnabled_avoidRelayoutDisabled() {
        TestThreadUtils.runOnUiThreadBlocking(
                () -> {
                    LocationBarLayout locationBar = getLocationBar();
                    View urlBar = getUrlBar();

                    urlBar.requestFocus();
                    // Setting focus percent shouldn't crash.
                    locationBar.setUrlFocusChangePercent(
                            /* ntpSearchBoxScrollFraction= */ 0,
                            /* startSurfaceScrollFraction= */ 0,
                            /* urlFocusChangeFraction= */ MathUtils.EPSILON,
                            /* isUrlFocusChangeInProgress= */ true);
                    locationBar.setUrlFocusChangePercent(
                            /* ntpSearchBoxScrollFraction= */ 0.5f,
                            /* startSurfaceScrollFraction= */ 0.5f,
                            /* urlFocusChangeFraction= */ 0.5f,
                            /* isUrlFocusChangeInProgress= */ false);
                });
    }

    @Test
    @MediumTest
    @EnableFeatures({
        ChromeFeatureList.OMNIBOX_MODERNIZE_VISUAL_UPDATE,
        ChromeFeatureList.SURFACE_POLISH
    })
    @Restriction({UiRestriction.RESTRICTION_TYPE_PHONE})
    public void testPhoneUrlBarAndStatusViewTranslation_SurfacePolishEnabled() {
        TestThreadUtils.runOnUiThreadBlocking(
                () -> {
                    Activity activity = mActivityTestRule.getActivity();
                    int statusIconAndUrlBarOffsetForSurfacePolish =
                            OmniboxResourceProvider.getToolbarSidePaddingForStartSurfaceOrNtp(
                                            activity)
                                    - OmniboxResourceProvider.getToolbarSidePadding(activity);
                    LocationBarLayout locationBar = getLocationBar();
                    View urlBar = getUrlBar();
                    View statusView = locationBar.findViewById(R.id.location_bar_status);

                    urlBar.requestFocus();
                    locationBar.updateLayoutParams(
                            MeasureSpec.makeMeasureSpec(
                                    locationBar.getMeasuredWidth(), MeasureSpec.EXACTLY));
                    locationBar.setUrlFocusChangePercent(
                            /* ntpSearchBoxScrollFraction= */ 1,
                            /* startSurfaceScrollFraction= */ 0,
                            /* urlFocusChangeFraction= */ MathUtils.EPSILON,
                            /* isUrlFocusChangeInProgress= */ true);

                    Assert.assertEquals(
                            statusIconAndUrlBarOffsetForSurfacePolish * (1 - MathUtils.EPSILON),
                            urlBar.getTranslationX(),
                            MathUtils.EPSILON);
                    Assert.assertEquals(
                            OmniboxResourceProvider.getFocusedStatusViewLeftSpacing(activity)
                                    + statusIconAndUrlBarOffsetForSurfacePolish
                                            * (1 - MathUtils.EPSILON),
                            statusView.getTranslationX(),
                            MathUtils.EPSILON);

                    locationBar.setUrlFocusChangePercent(
                            /* ntpSearchBoxScrollFraction= */ 1,
                            /* startSurfaceScrollFraction= */ 0,
                            /* urlFocusChangeFraction= */ 0.5f,
                            /* isUrlFocusChangeInProgress= */ true);
                    Assert.assertEquals(
                            statusIconAndUrlBarOffsetForSurfacePolish * 0.5,
                            urlBar.getTranslationX(),
                            MathUtils.EPSILON);
                    Assert.assertEquals(
                            OmniboxResourceProvider.getFocusedStatusViewLeftSpacing(activity)
                                    + statusIconAndUrlBarOffsetForSurfacePolish * 0.5,
                            statusView.getTranslationX(),
                            MathUtils.EPSILON);

                    locationBar.setUrlFocusChangePercent(
                            /* ntpSearchBoxScrollFraction= */ 1.0f,
                            /* startSurfaceScrollFraction= */ 0,
                            /* urlFocusChangeFraction= */ 1.0f,
                            /* isUrlFocusChangeInProgress= */ true);
                    Assert.assertEquals(0f, urlBar.getTranslationX(), MathUtils.EPSILON);
                    Assert.assertEquals(
                            OmniboxResourceProvider.getFocusedStatusViewLeftSpacing(activity),
                            statusView.getTranslationX(),
                            MathUtils.EPSILON);

                    locationBar.setUrlFocusChangePercent(
                            /* ntpSearchBoxScrollFraction= */ 0,
                            /* startSurfaceScrollFraction= */ 1,
                            /* urlFocusChangeFraction= */ MathUtils.EPSILON,
                            /* isUrlFocusChangeInProgress= */ true);

                    Assert.assertEquals(
                            statusIconAndUrlBarOffsetForSurfacePolish * (1 - MathUtils.EPSILON),
                            urlBar.getTranslationX(),
                            MathUtils.EPSILON);
                    Assert.assertEquals(
                            OmniboxResourceProvider.getFocusedStatusViewLeftSpacing(activity)
                                    + statusIconAndUrlBarOffsetForSurfacePolish
                                            * (1 - MathUtils.EPSILON),
                            statusView.getTranslationX(),
                            MathUtils.EPSILON);
                });
    }
}
