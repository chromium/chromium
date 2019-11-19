// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp.cards;

import android.content.Context;
import android.graphics.Region;

import org.chromium.chrome.browser.ntp.FakeboxDelegate;
import org.chromium.chrome.browser.ntp.NewTabPageLayout;
import org.chromium.chrome.browser.ntp.SnapScrollHelper;
import org.chromium.chrome.browser.suggestions.SuggestionsRecyclerView;
import org.chromium.chrome.browser.util.ViewUtils;
import org.chromium.ui.base.DeviceFormFactor;

/**
 * Simple wrapper on top of a RecyclerView that will acquire focus when tapped.  Ensures the
 * New Tab page receives focus when clicked.
 */
public class NewTabPageRecyclerView
        extends SuggestionsRecyclerView implements NewTabPageLayout.ScrollDelegate {
    /** The helper for calculating the snap scroll offset. */
    private SnapScrollHelper mSnapScrollHelper;

    /** The fake search box delegate used for determining if the url bar has focus. */
    private FakeboxDelegate mFakeboxDelegate;

    public NewTabPageRecyclerView(Context context) {
        super(context);
    }

    public void setSnapScrollHelper(SnapScrollHelper snapScrollHelper) {
        mSnapScrollHelper = snapScrollHelper;
    }

    /**
     * Sets the {@link FakeboxDelegate} associated with the new tab page.
     * @param fakeboxDelegate The {@link FakeboxDelegate} used to determine whether the URL bar
     *                        has focus.
     */
    public void setFakeboxDelegate(FakeboxDelegate fakeboxDelegate) {
        mFakeboxDelegate = fakeboxDelegate;
    }

    @Override
    protected boolean getTouchEnabled() {
        if (!super.getTouchEnabled()) return false;

        if (DeviceFormFactor.isTablet()) return true;

        // The RecyclerView should not accept touch events while the URL bar is focused. This
        // prevents the RecyclerView from requesting focus during the URL focus animation, which
        // would cause the focus animation to be canceled. See https://crbug.com/798084.
        return mFakeboxDelegate == null || !mFakeboxDelegate.isUrlBarFocused();
    }

    @Override
    public boolean gatherTransparentRegion(Region region) {
        ViewUtils.gatherTransparentRegionsForOpaqueView(this, region);
        return true;
    }

    // NewTabPageLayout.ScrollDelegate interface.

    @Override
    public boolean isScrollViewInitialized() {
        // During startup the view may not be fully initialized, so we check to see if some basic
        // view properties (height of the RecyclerView) are sane.
        return getHeight() > 0;
    }

    @Override
    public int getVerticalScrollOffset() {
        return computeVerticalScrollOffset();
    }

    @Override
    public boolean isChildVisibleAtPosition(int position) {
        return position >= getLinearLayoutManager().findFirstVisibleItemPosition()
                && position <= getLinearLayoutManager().findLastVisibleItemPosition();
    }

    @Override
    public void snapScroll() {
        int initialScroll = computeVerticalScrollOffset();

        int scrollTo = mSnapScrollHelper.calculateSnapPosition(initialScroll);

        // Calculating the snap position should be idempotent.
        assert scrollTo == mSnapScrollHelper.calculateSnapPosition(scrollTo);

        smoothScrollBy(0, scrollTo - initialScroll);
    }
}
