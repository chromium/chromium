// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.status;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.assertion.ViewAssertions.doesNotExist;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.matcher.ViewMatchers.isCompletelyDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.withId;

import static org.hamcrest.Matchers.not;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.mockito.Mockito.doReturn;

import static org.chromium.content_public.browser.test.util.TestThreadUtils.runOnUiThreadBlocking;

import android.view.ViewGroup;
import android.widget.FrameLayout;
import android.widget.LinearLayout;

import androidx.test.filters.MediumTest;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;

import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.omnibox.NewTabPageDelegate;
import org.chromium.chrome.browser.omnibox.SearchEngineLogoUtils;
import org.chromium.chrome.browser.omnibox.status.StatusProperties.StatusIconResource;
import org.chromium.chrome.browser.toolbar.LocationBarModel;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.util.ToolbarTestUtils;
import org.chromium.chrome.test.util.browser.Features.EnableFeatures;
import org.chromium.components.browser_ui.widget.CompositeTouchDelegate;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;
import org.chromium.ui.test.util.DummyUiActivityTestCase;
import org.chromium.ui.test.util.UiRestriction;

/**
 * Tests for {@link StatusView} and {@link StatusViewBinder}.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
public class StatusViewTest extends DummyUiActivityTestCase {
    @Mock
    private SearchEngineLogoUtils mSearchEngineLogoUtils;

    private StatusView mStatusView;
    private PropertyModel mStatusModel;
    private PropertyModelChangeProcessor mStatusMCP;

    @Override
    public void setUpTest() throws Exception {
        super.setUpTest();
        MockitoAnnotations.initMocks(this);

        runOnUiThreadBlocking(() -> {
            ViewGroup view = new LinearLayout(getActivity());

            FrameLayout.LayoutParams params = new FrameLayout.LayoutParams(
                    ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.MATCH_PARENT);

            getActivity().setContentView(view, params);

            mStatusView = getActivity()
                                  .getLayoutInflater()
                                  .inflate(R.layout.location_status, view, true)
                                  .findViewById(R.id.location_bar_status);
            mStatusView.setCompositeTouchDelegate(new CompositeTouchDelegate(view));
            // clang-format off
            mStatusView.setLocationBarDataProvider(new LocationBarModel(
                    mStatusView.getContext(), NewTabPageDelegate.EMPTY, (url) -> url.getSpec(),
                    (window) -> null, ToolbarTestUtils.OFFLINE_STATUS, mSearchEngineLogoUtils));
            // clang-format on
            mStatusView.setSearchEngineLogoUtils(mSearchEngineLogoUtils);
            mStatusModel = new PropertyModel.Builder(StatusProperties.ALL_KEYS).build();
            mStatusMCP = PropertyModelChangeProcessor.create(
                    mStatusModel, mStatusView, new StatusViewBinder());
        });
    }

    @Override
    public void tearDownTest() throws Exception {
        mStatusMCP.destroy();
        super.tearDownTest();
    }

    @Test
    @MediumTest
    @Feature({"Omnibox"})
    public void testIncognitoBadgeVisibility() {
        // Verify that the incognito badge is not inflated by default.
        assertFalse(mStatusModel.get(StatusProperties.INCOGNITO_BADGE_VISIBLE));
        onView(withId(R.id.location_bar_incognito_badge)).check(doesNotExist());

        // Set incognito badge visible.
        runOnUiThreadBlocking(
                () -> { mStatusModel.set(StatusProperties.INCOGNITO_BADGE_VISIBLE, true); });
        onView(withId(R.id.location_bar_incognito_badge)).check(matches(isCompletelyDisplayed()));

        // Set incognito badge gone.
        runOnUiThreadBlocking(
                () -> { mStatusModel.set(StatusProperties.INCOGNITO_BADGE_VISIBLE, false); });
        onView(withId(R.id.location_bar_incognito_badge)).check(matches(not(isDisplayed())));
    }

    @Test
    @MediumTest
    @Feature({"Omnibox"})
    public void testTouchDelegate_nullWhenIncognitoStatusIconInvisible() {
        // Verify that the incognito badge is not inflated by default.
        assertFalse(mStatusModel.get(StatusProperties.INCOGNITO_BADGE_VISIBLE));
        onView(withId(R.id.location_bar_incognito_badge)).check(doesNotExist());

        // Set incognito badge visible.
        runOnUiThreadBlocking(
                () -> { mStatusModel.set(StatusProperties.INCOGNITO_BADGE_VISIBLE, true); });
        onView(withId(R.id.location_bar_incognito_badge)).check(matches(isCompletelyDisplayed()));

        runOnUiThreadBlocking(
                () -> { mStatusModel.set(StatusProperties.STATUS_ICON_RESOURCE, null); });
        onView(withId(R.id.location_bar_status_icon))
                .check((view, e) -> assertNull(mStatusView.getTouchDelegateForTesting()));
    }

    @Test
    @MediumTest
    @Feature({"Omnibox"})
    public void testTouchDelegate_notNullWhenIncognitoStatusIconVisible() {
        // Verify that the incognito badge is not inflated by default.
        assertFalse(mStatusModel.get(StatusProperties.INCOGNITO_BADGE_VISIBLE));
        onView(withId(R.id.location_bar_incognito_badge)).check(doesNotExist());

        // Set incognito badge visible.
        runOnUiThreadBlocking(
                () -> { mStatusModel.set(StatusProperties.INCOGNITO_BADGE_VISIBLE, true); });
        onView(withId(R.id.location_bar_incognito_badge)).check(matches(isCompletelyDisplayed()));

        runOnUiThreadBlocking(() -> {
            mStatusModel.set(StatusProperties.STATUS_ICON_RESOURCE,
                    new StatusIconResource(R.drawable.ic_search, 0));
        });
        onView(withId(R.id.location_bar_status_icon))
                .check((view, e) -> assertNotNull(mStatusView.getTouchDelegateForTesting()));
    }

    @Test
    @MediumTest
    @Restriction(UiRestriction.RESTRICTION_TYPE_PHONE)
    @Feature({"Omnibox"})
    @EnableFeatures("OmniboxSearchEngineLogo")
    public void testSearchEngineLogo_incognito_noMarginEnd() {
        // Set incognito badge visible.
        runOnUiThreadBlocking(
                () -> { mStatusModel.set(StatusProperties.INCOGNITO_BADGE_VISIBLE, true); });
        onView(withId(R.id.location_bar_incognito_badge)).check(matches(isCompletelyDisplayed()));

        runOnUiThreadBlocking(() -> {
            mStatusModel.set(StatusProperties.STATUS_ICON_RESOURCE,
                    new StatusIconResource(R.drawable.ic_logo_googleg_24dp, 0));
        });
        onView(withId(R.id.location_bar_incognito_badge)).check((view, e) -> {
            ViewGroup.MarginLayoutParams params =
                    (ViewGroup.MarginLayoutParams) view.getLayoutParams();
            assertEquals(0, params.getMarginEnd());
        });
    }

    @Test
    @MediumTest
    @Feature({"Omnibox"})
    @EnableFeatures("OmniboxSearchEngineLogo")
    public void testSearchEngineLogo_noIncognito_statusDimensions() {
        doReturn(true).when(mSearchEngineLogoUtils).isSearchEngineLogoEnabled();
        doReturn(true).when(mSearchEngineLogoUtils).shouldShowSearchEngineLogo(false);
        runOnUiThreadBlocking(() -> {
            mStatusModel.set(StatusProperties.STATUS_ICON_RESOURCE,
                    new StatusIconResource(R.drawable.ic_logo_googleg_24dp, 0));
            mStatusModel.set(StatusProperties.SHOW_STATUS_ICON, true);
            mStatusView.updateSearchEngineStatusIcon();
        });
        int expectedWidth = getActivity().getResources().getDimensionPixelSize(
                R.dimen.location_bar_status_icon_width);
        onView(withId(R.id.location_bar_status_icon)).check((view, e) -> {
            assertEquals(expectedWidth, view.getMeasuredWidth());
        });
        int expectedPadding = getActivity().getResources().getDimensionPixelOffset(
                R.dimen.sei_location_bar_icon_end_padding);
        onView(withId(R.id.location_bar_status)).check((view, e) -> {
            assertEquals(expectedPadding, view.getPaddingEnd());
        });
    }
}
