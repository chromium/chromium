// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.safety_promo;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.verify;

import android.content.Context;
import android.view.LayoutInflater;
import android.widget.TextView;

import androidx.recyclerview.widget.LinearLayoutManager;
import androidx.recyclerview.widget.RecyclerView;
import androidx.test.core.app.ApplicationProvider;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;

import java.util.List;

/** Unit tests for {@link SafetyPromoCarouselCoordinator}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class SafetyPromoCarouselCoordinatorUnitTest {
    private static final List<SafetyPromoItem> TEST_ITEMS =
            List.of(
                    SafetyPromoItem.PASSWORD_MANAGER,
                    SafetyPromoItem.ENHANCED_SAFE_BROWSING,
                    SafetyPromoItem.INCOGNITO);

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private Runnable mAdvancePage;

    private Context mContext;
    private SafetyPromoCarouselView mView;
    private SafetyPromoCarouselCoordinator mCoordinator;

    @Before
    public void setUp() {
        mContext = ApplicationProvider.getApplicationContext();
        mView =
                (SafetyPromoCarouselView)
                        LayoutInflater.from(mContext)
                                .inflate(
                                        R.layout.safety_promo_fre_carousel_portrait_view,
                                        /* root= */ null);
        mCoordinator =
                new SafetyPromoCarouselCoordinator(mContext, mView, mAdvancePage, TEST_ITEMS);
    }

    @Test
    public void testInitialization() {
        RecyclerView recyclerView = mView.getRecyclerView();
        assertNotNull(recyclerView.getAdapter());
        assertEquals(3, recyclerView.getAdapter().getItemCount());

        assertTrue(recyclerView.getLayoutManager() instanceof LinearLayoutManager);
        LinearLayoutManager layoutManager = (LinearLayoutManager) recyclerView.getLayoutManager();
        assertEquals(LinearLayoutManager.HORIZONTAL, layoutManager.getOrientation());

        TextView titleView = mView.findViewById(R.id.safety_promo_carousel_title);
        assertEquals(
                mContext.getString(R.string.safety_fre_promo_password_manager_carousel_title),
                titleView.getText().toString());

        TextView subtitleView = mView.findViewById(R.id.safety_promo_carousel_subtitle);
        assertEquals(
                mContext.getString(R.string.safety_fre_promo_password_manager_carousel_subtitle),
                subtitleView.getText().toString());
    }

    @Test
    public void testContinueButton_triggersCallback() {
        mView.findViewById(R.id.fre_continue_button).performClick();
        verify(mAdvancePage).run();
    }

    @Test
    public void testUpdateHeaderForPosition() {
        TextView titleView = mView.findViewById(R.id.safety_promo_carousel_title);
        TextView subtitleView = mView.findViewById(R.id.safety_promo_carousel_subtitle);

        // Update to position 1 (Enhanced Safe Browsing)
        mCoordinator.updateHeaderForPosition(1);
        assertEquals(
                mContext.getString(R.string.safety_fre_promo_enhanced_safe_browsing_carousel_title),
                titleView.getText().toString());
        assertEquals(
                mContext.getString(
                        R.string.safety_fre_promo_enhanced_safe_browsing_carousel_subtitle),
                subtitleView.getText().toString());

        // Update to position 2 (Incognito)
        mCoordinator.updateHeaderForPosition(2);
        assertEquals(
                mContext.getString(R.string.safety_fre_promo_incognito_carousel_title),
                titleView.getText().toString());
        assertEquals(
                mContext.getString(R.string.safety_fre_promo_incognito_carousel_subtitle),
                subtitleView.getText().toString());

        // Update back to position 0 (Password Manager)
        mCoordinator.updateHeaderForPosition(0);
        assertEquals(
                mContext.getString(R.string.safety_fre_promo_password_manager_carousel_title),
                titleView.getText().toString());
        assertEquals(
                mContext.getString(R.string.safety_fre_promo_password_manager_carousel_subtitle),
                subtitleView.getText().toString());

        // Out of bounds positions should not change the header
        mCoordinator.updateHeaderForPosition(-1);
        assertEquals(
                mContext.getString(R.string.safety_fre_promo_password_manager_carousel_title),
                titleView.getText().toString());

        mCoordinator.updateHeaderForPosition(10);
        assertEquals(
                mContext.getString(R.string.safety_fre_promo_password_manager_carousel_title),
                titleView.getText().toString());
    }
}
