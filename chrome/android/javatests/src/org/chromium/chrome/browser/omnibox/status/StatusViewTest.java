// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.status;

import static android.support.test.espresso.Espresso.onView;
import static android.support.test.espresso.assertion.ViewAssertions.doesNotExist;
import static android.support.test.espresso.assertion.ViewAssertions.matches;
import static android.support.test.espresso.matcher.ViewMatchers.isCompletelyDisplayed;
import static android.support.test.espresso.matcher.ViewMatchers.isDisplayed;
import static android.support.test.espresso.matcher.ViewMatchers.withId;

import static org.hamcrest.Matchers.not;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;

import static org.chromium.content_public.browser.test.util.TestThreadUtils.runOnUiThreadBlocking;

import android.support.test.filters.MediumTest;
import android.view.ViewGroup;
import android.widget.FrameLayout;
import android.widget.LinearLayout;

import org.junit.Assert;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.MockitoAnnotations;

import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.toolbar.LocationBarModel;
import org.chromium.chrome.browser.ui.widget.CompositeTouchDelegate;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ui.DummyUiActivityTestCase;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;
import org.chromium.ui.test.util.UiRestriction;

/**
 * Tests for {@link StatusView} and {@link StatusViewBinder}.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
public class StatusViewTest extends DummyUiActivityTestCase {
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
            mStatusView.setToolbarCommonPropertiesModel(
                    new LocationBarModel(mStatusView.getContext()));
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

        runOnUiThreadBlocking(() -> { mStatusModel.set(StatusProperties.STATUS_ICON_RES, 0); });
        onView(withId(R.id.location_bar_status_icon))
                .check((view, e) -> Assert.assertNull(mStatusView.getTouchDelegateForTesting()));
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
            mStatusModel.set(StatusProperties.STATUS_ICON_RES, R.drawable.ic_logo_googleg_24dp);
        });
        onView(withId(R.id.location_bar_status_icon))
                .check((view, e) -> assertNotNull(mStatusView.getTouchDelegateForTesting()));
    }

    @Test
    @MediumTest
    @Restriction(UiRestriction.RESTRICTION_TYPE_PHONE)
    @Feature({"Omnibox"})
    public void testSearchEngineLogo_noIncognitoPadding() {
        // Set incognito badge visible.
        runOnUiThreadBlocking(
                () -> { mStatusModel.set(StatusProperties.INCOGNITO_BADGE_VISIBLE, true); });
        onView(withId(R.id.location_bar_incognito_badge)).check(matches(isCompletelyDisplayed()));

        runOnUiThreadBlocking(() -> {
            mStatusModel.set(StatusProperties.STATUS_ICON_RES, R.drawable.ic_logo_googleg_24dp);
        });
        onView(withId(R.id.location_bar_incognito_badge)).check((view, e) -> {
            ViewGroup.MarginLayoutParams params =
                    (ViewGroup.MarginLayoutParams) view.getLayoutParams();
            assertEquals(params.getMarginEnd(), 0);
        });
    }
}
