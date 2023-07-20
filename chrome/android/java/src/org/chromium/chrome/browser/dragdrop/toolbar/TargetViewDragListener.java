// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.dragdrop.toolbar;

import android.content.ClipData;
import android.content.Context;
import android.content.res.ColorStateList;
import android.os.SystemClock;
import android.view.DragEvent;
import android.view.View;
import android.view.View.OnDragListener;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.omnibox.suggestions.AutocompleteDelegate;
import org.chromium.ui.base.PageTransition;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.url.GURL;

/**
 * A drag listener for the target view that handles events during drag and drop to Omnibox
 */
class TargetViewDragListener implements OnDragListener {
    private AutocompleteDelegate mAutocompleteDelegate;
    private PropertyModel mModel;

    /**
     * Create the drag listener for the target view.
     *
     * @param model {@link PropertyModel} built with {@link TargetViewProperties}
     * @param autocompleteDelegate Used to navigate on a successful drop.
     */
    public TargetViewDragListener(PropertyModel model, AutocompleteDelegate autocompleteDelegate) {
        mModel = model;
        mAutocompleteDelegate = autocompleteDelegate;
    }

    @Override
    public boolean onDrag(View v, DragEvent event) {
        switch (event.getAction()) {
            case DragEvent.ACTION_DRAG_STARTED:
                return true;
            case DragEvent.ACTION_DRAG_ENTERED:
                mModel.set(TargetViewProperties.TARGET_VIEW_COLOR,
                        ColorStateList.valueOf(
                                v.getResources().getColor(R.color.baseline_primary_40)));
                break;
            case DragEvent.ACTION_DRAG_EXITED:
                mModel.set(TargetViewProperties.TARGET_VIEW_COLOR, null);
                break;
            case DragEvent.ACTION_DROP:
                String url = getUrlFromClipData(event.getClipData(), v.getContext());
                // TODO(https://crbug.com/1465940): Handle invalid url and fix parsing
                mAutocompleteDelegate.loadUrl(
                        url, PageTransition.TYPED, SystemClock.uptimeMillis());
                mModel.set(TargetViewProperties.TARGET_VIEW_COLOR, null);
                return true;
        }
        return false;
    }

    public static String getUrlFromClipData(ClipData clipData, Context context) {
        String text = clipData.getItemAt(0).coerceToText(context).toString();
        GURL gurl = new GURL(text);
        return gurl.isValid() ? gurl.getSpec() : "";
    }
}
