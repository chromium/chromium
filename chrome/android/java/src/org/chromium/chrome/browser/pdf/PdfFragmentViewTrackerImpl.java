// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.pdf;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.text.TextUtils;
import android.view.View;
import android.view.ViewGroup;

import androidx.fragment.app.FragmentActivity;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorTabObserver;

import java.util.ArrayList;
import java.util.List;
import java.util.function.Supplier;

/** Implementation of {@link PdfFragmentViewTracker} that manages misplaced PdfFragment Views. */
@NullMarked
public class PdfFragmentViewTrackerImpl implements PdfFragmentViewTracker {

    private final @Nullable TabModelSelectorTabObserver mTabModelSelectorTabObserver;

    /**
     * Temporarily holds PdfViewerFragment's Views object that were (mis)placed in the current Tab's
     * PdfPage Views.
     */
    private final List<View> mPdfFragmentViews = new ArrayList<>();

    /**
     * List of tags of Fragment Views that found their Tabs. Used not to populate
     * |mPdfFragmentViews| again with already handled ones.
     */
    private final List<String> mHandledViews = new ArrayList<>();

    /** Provides the PdfFragment Views from the framework. */
    private Supplier<List<View>> mPdfFragmentSupplier;

    public PdfFragmentViewTrackerImpl(
            @Nullable TabModelSelector tabModelSelector, @Nullable FragmentActivity activity) {
        // CustomTab comes with a null selector/activity since it doesn't need to observe tabs.
        if (tabModelSelector == null || activity == null) {
            mTabModelSelectorTabObserver = null;
            mPdfFragmentSupplier = () -> List.of();
            return;
        }

        mTabModelSelectorTabObserver =
                new TabModelSelectorTabObserver(tabModelSelector) {
                    @Override
                    public void onDestroyed(Tab tab) {
                        if (tab.getNativePage() != null && tab.getNativePage().isPdf()) {
                            String tag = String.valueOf(tab.getId());
                            removeViewWithTag(tag);
                        }
                    }
                };
        mPdfFragmentSupplier = () -> PdfUtils.findAllPdfFragmentViews(activity);
    }

    @Override
    public void maybeRelocateViews(ViewGroup container, String tag) {
        if (container.getChildCount() == 1
                && TextUtils.equals(String.valueOf(container.getChildAt(0).getTag()), tag)) {
            mHandledViews.add(tag);
            return;
        }
        List<View> fragmentViews = assumeNonNull(mPdfFragmentSupplier).get();
        for (View view : fragmentViews) {
            if (view.getTag() == null) continue;
            if (!mHandledViews.contains(view.getTag()) && !mPdfFragmentViews.contains(view)) {
                mPdfFragmentViews.add(view);
            }
        }

        for (int i = container.getChildCount() - 1; i >= 0; i--) {
            View child = container.getChildAt(i);
            if (child.getTag() == null) continue;
            if (tag.equals(child.getTag())) {
                mPdfFragmentViews.remove(child);
            } else {
                container.removeView(child);
            }
        }

        View removedView = removeViewWithTag(tag);
        if (removedView != null) {
            if (removedView.getParent() != null) {
                ((ViewGroup) removedView.getParent()).removeView(removedView);
            }
            container.addView(removedView);
            mHandledViews.add(tag);
        }
    }

    private @Nullable View removeViewWithTag(String tag) {
        for (int i = mPdfFragmentViews.size() - 1; i >= 0; i--) {
            View child = mPdfFragmentViews.get(i);
            if (TextUtils.equals(tag, String.valueOf(child.getTag()))) {
                mPdfFragmentViews.remove(child);
                return child;
            }
        }
        return null;
    }

    public void destroy() {
        if (mTabModelSelectorTabObserver != null) mTabModelSelectorTabObserver.destroy();
        if (mPdfFragmentViews != null) mPdfFragmentViews.clear();
    }

    List<View> getViewsForTesting() {
        return mPdfFragmentViews;
    }

    void destroyPdfTabForTesting(Tab tab) {
        assumeNonNull(mTabModelSelectorTabObserver).onDestroyed(tab);
    }

    void setFragmentSupplierForTesting(Supplier<List<View>> supplier) {
        mPdfFragmentSupplier = supplier;
        mPdfFragmentViews.clear();
        mPdfFragmentViews.addAll(mPdfFragmentSupplier.get());
    }
}
