// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import android.content.Context;
import android.view.View;
import android.view.ViewGroup;
import android.widget.ImageView;
import android.widget.TextView;

import org.chromium.chrome.tab_ui.R;
import org.chromium.ui.base.DeviceFormFactor;
import org.chromium.ui.modelutil.ListObservable;
import org.chromium.ui.modelutil.ListObservable.ListObserver;

// @TODO(crbug.com/1442335) Add instrumentation test for TabListEmptyCoordinator class.
public class TabListEmptyCoordinator {
    private ViewGroup mRootView;
    private View mEmptyView;
    private Context mContext;
    private TabListModel mModel;
    private ListObserver<Void> mListObserver;

    public TabListEmptyCoordinator(ViewGroup rootView, TabListModel model) {
        mRootView = rootView;
        mContext = rootView.getContext();

        // Initialize tab switcher Empty State resources.
        mEmptyView = (ViewGroup) android.view.LayoutInflater.from(mContext).inflate(
                R.layout.empty_state_view, null);
        TextView emptyStateHeading = mEmptyView.findViewById(R.id.empty_state_text_title);
        TextView emptyStateSubheading = mEmptyView.findViewById(R.id.empty_state_text_description);
        ImageView imageView = mEmptyView.findViewById(R.id.empty_state_icon);

        // Set properties.
        emptyStateHeading.setText(R.string.tabswitcher_no_tabs_empty_state);
        emptyStateSubheading.setText(R.string.tabswitcher_no_tabs_open_to_visit_different_pages);

        // Apply image illustrations based on form factors.
        if (DeviceFormFactor.isNonMultiDisplayContextOnTablet(mContext)) {
            imageView.setImageResource(R.drawable.tablet_tab_switcher_empty_state_illustration);
        } else {
            imageView.setImageResource(R.drawable.phone_tab_switcher_empty_state_illustration);
        }

        // Observe TabListModel to determine when to add / remove empty state view.
        mModel = model;
        mListObserver = new ListObserver<Void>() {
            @Override
            public void onItemRangeInserted(ListObservable source, int index, int count) {
                updateEmptyView();
            }

            @Override
            public void onItemRangeRemoved(ListObservable source, int index, int count) {
                updateEmptyView();
            }
        };
        mModel.addObserver(mListObserver);
    }

    private void updateEmptyView() {
        boolean showEmptyView = mModel.size() == 0;
        boolean isEmptyViewAttached = mEmptyView.getParent() != null;

        if (showEmptyView && !isEmptyViewAttached) {
            mRootView.addView(mEmptyView);
        } else if (!showEmptyView && isEmptyViewAttached) {
            mRootView.removeView(mEmptyView);
        }
    }

    public void destroy() {
        if (mListObserver != null) {
            mModel.removeObserver(mListObserver);
        }
    }
}
