// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.dragdrop.toolbar;

import android.os.SystemClock;
import android.view.DragEvent;
import android.view.View;
import android.view.View.OnDragListener;
import android.widget.FrameLayout;

import androidx.annotation.IntDef;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.browser.omnibox.OmniboxFocusReason;
import org.chromium.chrome.browser.omnibox.OmniboxStub;
import org.chromium.chrome.browser.omnibox.suggestions.AutocompleteDelegate;
import org.chromium.chrome.browser.omnibox.suggestions.OmniboxLoadUrlParams;
import org.chromium.components.search_engines.TemplateUrlService;
import org.chromium.ui.base.MimeTypeUtils;
import org.chromium.ui.base.PageTransition;
import org.chromium.ui.dragdrop.DropDataAndroid;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;
import org.chromium.ui.widget.Toast;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/** ToolbarDragDrop Coordinator owns the view and the model change processor. */
public class ToolbarDragDropCoordinator implements OnDragListener {
    /**
     * Java Enum of AndroidToolbarDropType used for histogram recording for
     * Android.DragDrop.ToOmnibox.DropType. This is used for histograms and should therefore
     * be treated as append-only.
     */
    @IntDef({
        DropType.INVALID,
        DropType.TEXT,
        DropType.CHROME_TEXT,
        DropType.CHROME_LINK,
        DropType.CHROME_IMAGE,
        DropType.NUM_ENTRIES
    })
    @Retention(RetentionPolicy.SOURCE)
    @interface DropType {
        int INVALID = 0;
        int CHROME_TEXT = 1;
        int TEXT = 2;
        int CHROME_LINK = 3;
        int CHROME_IMAGE = 4;

        int NUM_ENTRIES = 5;
    }

    // TODO(crbug.com/40277406): Swap error message to a translated string.
    private static final String ERROR_MESSAGE = "Unable to handle drop";
    private AutocompleteDelegate mAutocompleteDelegate;
    private PropertyModelChangeProcessor mModelChangeProcessor;
    private FrameLayout mTargetView;
    private TargetViewDragListener mTargetViewDragListener;
    private PropertyModel mModel;
    private OmniboxStub mOmniboxStub;
    private Supplier<TemplateUrlService> mTemplateUrlServiceSupplier;

    interface OnDropCallback {
        /** Handles parsing DragEvents on a drop to the Omnibox */
        void parseDragEvent(DragEvent event);
    }

    /**
     * Create the Coordinator of TargetView that owns the view and the change process.
     *
     * @param targetView is the view displayed during the drag and drop process.
     * @param autocompleteDelegate Used to navigate on a successful link drop.
     * @param omniboxStub Used to navigate a successful text drop.
     * @param templateUrlServiceSupplier Used to obtain the TemplateUrlService needed on an image
     * drop.
     *
     */
    public ToolbarDragDropCoordinator(
            FrameLayout targetView,
            AutocompleteDelegate autocompleteDelegate,
            OmniboxStub omniboxStub,
            Supplier<TemplateUrlService> templateUrlServiceSupplier) {
        // TargetView is inflated in order to have the onDragListener attached before it is visible
        mTargetView = targetView;
        mModel =
                new PropertyModel.Builder(TargetViewProperties.ALL_KEYS)
                        .with(TargetViewProperties.TARGET_VIEW_VISIBLE, View.GONE)
                        .build();
        mAutocompleteDelegate = autocompleteDelegate;
        mTargetViewDragListener = new TargetViewDragListener(mModel, this::parseDragEvent);
        mModel.set(TargetViewProperties.ON_DRAG_LISTENER, mTargetViewDragListener);
        mModelChangeProcessor =
                PropertyModelChangeProcessor.create(mModel, mTargetView, new TargetViewBinder());
        mOmniboxStub = omniboxStub;
        mTemplateUrlServiceSupplier = templateUrlServiceSupplier;
    }

    @Override
    public boolean onDrag(View v, DragEvent event) {
        switch (event.getAction()) {
            case DragEvent.ACTION_DRAG_STARTED:
                if (isValidMimeType(event)) {
                    mModel.set(TargetViewProperties.TARGET_VIEW_VISIBLE, View.VISIBLE);
                    return true;
                }
                return false;
            case DragEvent.ACTION_DRAG_ENDED:
                mModel.set(TargetViewProperties.TARGET_VIEW_VISIBLE, View.GONE);
                // fall through
            default:
                return false;
        }
    }

    /** Destroy the Toolbar Drag and Drop Coordinator and its components. */
    public void destroy() {
        mModel.set(TargetViewProperties.ON_DRAG_LISTENER, null);
        mTargetView.setVisibility(View.GONE);
        mModelChangeProcessor.destroy();
    }

    // Checks if the dragged object is supported for dragging into Omnibox
    public static boolean isValidMimeType(DragEvent event) {
        return event.getClipDescription().filterMimeTypes(MimeTypeUtils.TEXT_MIME_TYPE) != null;
    }

    /** Handles parsing DragEvents on a drop to the Omnibox. */
    @VisibleForTesting
    void parseDragEvent(DragEvent event) {
        if (event.getClipDescription().filterMimeTypes(MimeTypeUtils.IMAGE_MIME_TYPE) != null) {
            // Try to get the byte array needed for image search from local state. If drag and drop
            // is started from the same Chrome activity, then DropDataAndroid should be set as a
            // local state.
            //  TODO(crbug.com/40277338): Read the image bytes from localState using a util method
            Object dropData = event.getLocalState();
            TemplateUrlService urlService = mTemplateUrlServiceSupplier.get();
            if (!urlService.isSearchByImageAvailable() || !(dropData instanceof DropDataAndroid)) {
                handleErrorToast();
                return;
            }
            String[] postData = urlService.getImageUrlAndPostContent();
            // TODO(crbug.com/40278861): Pass in correct imageByteArray to AutocompleteDelegate
            byte[] imageByteArray = new byte[0];
            mAutocompleteDelegate.loadUrl(
                    new OmniboxLoadUrlParams.Builder(postData[0], PageTransition.GENERATED)
                            .setInputStartTimestamp(SystemClock.uptimeMillis())
                            .setpostDataAndType(imageByteArray, postData[1])
                            .build());
            recordDropType(DropType.CHROME_IMAGE);
        } else if (event.getClipDescription().hasMimeType(MimeTypeUtils.CHROME_MIMETYPE_TEXT)) {
            mOmniboxStub.setUrlBarFocus(
                    true,
                    event.getClipData()
                            .getItemAt(0)
                            .coerceToText(mTargetView.getContext())
                            .toString(),
                    OmniboxFocusReason.DRAG_DROP_TO_OMNIBOX);
            recordDropType(DropType.CHROME_TEXT);
        } else if (event.getClipDescription().hasMimeType(MimeTypeUtils.CHROME_MIMETYPE_LINK)) {
            /*
             * This parsing is based on the implementation in DragAndDropDelegateImpl#BuildClipData.
             * Ideally we should handle build / parsing in a similar place to keep things
             * consistent. TODO(crbug.com/40277338): Build ClipData and parse link URL using a
             * static helper method
             */
            String url = event.getClipData().getItemAt(0).getIntent().getData().toString();
            mAutocompleteDelegate.loadUrl(
                    new OmniboxLoadUrlParams.Builder(url, PageTransition.TYPED)
                            .setInputStartTimestamp(SystemClock.uptimeMillis())
                            .setOpenInNewTab(false)
                            .build());
            recordDropType(DropType.CHROME_LINK);
        } else {
            // case where dragged object is not from Chrome
            event.getClipDescription().filterMimeTypes(MimeTypeUtils.TEXT_MIME_TYPE);
            if (event.getClipData() == null) {
                handleErrorToast();
                recordDropType(DropType.INVALID);
            } else {
                mOmniboxStub.setUrlBarFocus(
                        true,
                        event.getClipData()
                                .getItemAt(0)
                                .coerceToText(mTargetView.getContext())
                                .toString(),
                        OmniboxFocusReason.DRAG_DROP_TO_OMNIBOX);
                recordDropType(DropType.TEXT);
            }
        }
    }

    private void handleErrorToast() {
        Toast errorMessage =
                Toast.makeText(mTargetView.getContext(), ERROR_MESSAGE, Toast.LENGTH_SHORT);
        errorMessage.show();
    }

    private void recordDropType(@DropType int type) {
        RecordHistogram.recordEnumeratedHistogram(
                "Android.DragDrop.ToOmnibox.DropType", type, DropType.NUM_ENTRIES);
    }
}
