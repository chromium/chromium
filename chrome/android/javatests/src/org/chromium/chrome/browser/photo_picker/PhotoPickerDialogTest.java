// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.photo_picker;

import static org.chromium.base.test.util.ScalableTimeout.scaleTimeout;

import android.os.Build;
import android.support.test.filters.LargeTest;
import android.support.v7.widget.RecyclerView;
import android.view.View;
import android.widget.Button;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.DisableIf;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.browser.widget.selection.SelectionDelegate;
import org.chromium.chrome.browser.widget.selection.SelectionDelegate.SelectionObserver;
import org.chromium.chrome.test.ChromeActivityTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.util.browser.RecyclerViewTestUtils;
import org.chromium.content_public.browser.test.util.TouchCommon;
import org.chromium.ui.PhotoPickerListener;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;
import java.util.concurrent.Callable;
import java.util.concurrent.TimeUnit;

/**
 * Tests for the PhotoPickerDialog class.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class PhotoPickerDialogTest implements PhotoPickerListener, SelectionObserver<PickerBitmap>,
                                              DecoderServiceHost.ServiceReadyCallback {
    // The timeout (in seconds) to wait for the decoder service to be ready.
    private static final long WAIT_TIMEOUT_SECONDS = scaleTimeout(30);

    @Rule
    public ChromeActivityTestRule<ChromeActivity> mActivityTestRule =
            new ChromeActivityTestRule<>(ChromeActivity.class);

    // The dialog we are testing.
    private PhotoPickerDialog mDialog;

    // The data to show in the dialog (A map of filepath to last-modified time).
    // Map<String, Long> mTestFiles;
    private List<PickerBitmap> mTestFiles;

    // The selection delegate for the dialog.
    private SelectionDelegate<PickerBitmap> mSelectionDelegate;

    // The last action recorded in the dialog (e.g. photo selected).
    private @PhotoPickerAction int mLastActionRecorded;

    // The final set of photos picked by the dialog. Can be an empty array, if
    // nothing was selected.
    private String[] mLastSelectedPhotos;

    // The list of currently selected photos (built piecemeal).
    private List<PickerBitmap> mCurrentPhotoSelection;

    // A callback that fires when something is selected in the dialog.
    public final CallbackHelper onSelectionCallback = new CallbackHelper();

    // A callback that fires when an action is taken in the dialog (cancel/done etc).
    public final CallbackHelper onActionCallback = new CallbackHelper();

    // A callback that fires when the decoder is ready.
    public final CallbackHelper onDecoderReadyCallback = new CallbackHelper();

    @Before
    public void setUp() throws Exception {
        mActivityTestRule.startMainActivityOnBlankPage();
        mTestFiles = new ArrayList<>();
        mTestFiles.add(new PickerBitmap("a", 5L, PickerBitmap.TileTypes.PICTURE));
        mTestFiles.add(new PickerBitmap("b", 4L, PickerBitmap.TileTypes.PICTURE));
        mTestFiles.add(new PickerBitmap("c", 3L, PickerBitmap.TileTypes.PICTURE));
        mTestFiles.add(new PickerBitmap("d", 2L, PickerBitmap.TileTypes.PICTURE));
        mTestFiles.add(new PickerBitmap("e", 1L, PickerBitmap.TileTypes.PICTURE));
        mTestFiles.add(new PickerBitmap("f", 0L, PickerBitmap.TileTypes.PICTURE));
        PickerCategoryView.setTestFiles(mTestFiles);

        DecoderServiceHost.setReadyCallback(this);
    }

    // PhotoPickerDialog.PhotoPickerListener:

    @Override
    public void onPhotoPickerUserAction(@PhotoPickerAction int action, String[] photos) {
        mLastActionRecorded = action;
        mLastSelectedPhotos = photos != null ? photos.clone() : null;
        if (mLastSelectedPhotos != null) Arrays.sort(mLastSelectedPhotos);
        onActionCallback.notifyCalled();
    }

    // DecoderServiceHost.ServiceReadyCallback:

    @Override
    public void serviceReady() {
        onDecoderReadyCallback.notifyCalled();
    }

    // SelectionObserver:

    @Override
    public void onSelectionStateChange(List<PickerBitmap> photosSelected) {
        mCurrentPhotoSelection = new ArrayList<>(photosSelected);
        onSelectionCallback.notifyCalled();
    }

    private RecyclerView getRecyclerView() {
        return (RecyclerView) mDialog.findViewById(R.id.recycler_view);
    }

    private PhotoPickerDialog createDialog(final boolean multiselect, final List<String> mimeTypes)
            throws Exception {
        final PhotoPickerDialog dialog =
                ThreadUtils.runOnUiThreadBlocking(new Callable<PhotoPickerDialog>() {
                    @Override
                    public PhotoPickerDialog call() {
                        final PhotoPickerDialog dialog =
                                new PhotoPickerDialog(mActivityTestRule.getActivity(),
                                        PhotoPickerDialogTest.this, multiselect, mimeTypes);
                        dialog.show();
                        return dialog;
                    }
                });

        mSelectionDelegate = dialog.getCategoryViewForTesting().getSelectionDelegateForTesting();
        if (!multiselect) mSelectionDelegate.setSingleSelectionMode();
        mSelectionDelegate.addObserver(this);
        mDialog = dialog;

        return dialog;
    }

    private void waitForDecoder() throws Exception {
        int callCount = onSelectionCallback.getCallCount();
        onDecoderReadyCallback.waitForCallback(
                callCount, 1, WAIT_TIMEOUT_SECONDS, TimeUnit.SECONDS);
    }

    private void clickView(final int position, final int expectedSelectionCount) throws Exception {
        RecyclerView recyclerView = getRecyclerView();
        RecyclerViewTestUtils.waitForView(recyclerView, position);

        int callCount = onSelectionCallback.getCallCount();
        TouchCommon.singleClickView(
                recyclerView.findViewHolderForAdapterPosition(position).itemView);
        onSelectionCallback.waitForCallback(callCount, 1);

        // Validate the correct selection took place.
        Assert.assertEquals(expectedSelectionCount, mCurrentPhotoSelection.size());
        Assert.assertTrue(mSelectionDelegate.isItemSelected(mTestFiles.get(position)));
    }

    private void clickDone() throws Exception {
        mLastActionRecorded = PhotoPickerAction.NUM_ENTRIES;

        PhotoPickerToolbar toolbar = (PhotoPickerToolbar) mDialog.findViewById(R.id.action_bar);
        Button done = (Button) toolbar.findViewById(R.id.done);
        int callCount = onActionCallback.getCallCount();
        TouchCommon.singleClickView(done);
        onActionCallback.waitForCallback(callCount, 1);
        Assert.assertEquals(PhotoPickerAction.PHOTOS_SELECTED, mLastActionRecorded);
    }

    public void clickCancel() throws Exception {
        mLastActionRecorded = PhotoPickerAction.NUM_ENTRIES;

        PickerCategoryView categoryView = mDialog.getCategoryViewForTesting();
        View cancel = new View(mActivityTestRule.getActivity());
        int callCount = onActionCallback.getCallCount();
        categoryView.onClick(cancel);
        onActionCallback.waitForCallback(callCount, 1);
        Assert.assertEquals(PhotoPickerAction.CANCEL, mLastActionRecorded);
    }

    private void dismissDialog() {
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                mDialog.dismiss();
            }
        });
    }

    @Test
    @DisableIf.Build(sdk_is_less_than = Build.VERSION_CODES.LOLLIPOP, message = "crbug.com/761060")
    @LargeTest
    public void testNoSelection() throws Throwable {
        createDialog(false, Arrays.asList("image/*")); // Multi-select = false.
        Assert.assertTrue(mDialog.isShowing());
        waitForDecoder();

        int expectedSelectionCount = 1;
        clickView(0, expectedSelectionCount);
        clickCancel();

        Assert.assertNull(mLastSelectedPhotos);
        Assert.assertEquals(PhotoPickerAction.CANCEL, mLastActionRecorded);

        dismissDialog();
    }

    @Test
    @DisableIf.Build(sdk_is_less_than = Build.VERSION_CODES.LOLLIPOP, message = "crbug.com/761060")
    @LargeTest
    public void testSingleSelectionPhoto() throws Throwable {
        createDialog(false, Arrays.asList("image/*")); // Multi-select = false.
        Assert.assertTrue(mDialog.isShowing());
        waitForDecoder();

        // Expected selection count is 1 because clicking on a new view unselects other.
        int expectedSelectionCount = 1;
        clickView(0, expectedSelectionCount);
        clickView(1, expectedSelectionCount);
        clickDone();

        Assert.assertEquals(1, mLastSelectedPhotos.length);
        Assert.assertEquals(PhotoPickerAction.PHOTOS_SELECTED, mLastActionRecorded);
        Assert.assertEquals(mTestFiles.get(1).getFilePath(), mLastSelectedPhotos[0]);

        dismissDialog();
    }

    @Test
    @DisableIf.Build(sdk_is_less_than = Build.VERSION_CODES.LOLLIPOP, message = "crbug.com/761060")
    @LargeTest
    public void testMultiSelectionPhoto() throws Throwable {
        createDialog(true, Arrays.asList("image/*")); // Multi-select = true.
        Assert.assertTrue(mDialog.isShowing());
        waitForDecoder();

        // Multi-selection is enabled, so each click is counted.
        int expectedSelectionCount = 1;
        clickView(0, expectedSelectionCount++);
        clickView(2, expectedSelectionCount++);
        clickView(4, expectedSelectionCount++);
        clickDone();

        Assert.assertEquals(3, mLastSelectedPhotos.length);
        Assert.assertEquals(PhotoPickerAction.PHOTOS_SELECTED, mLastActionRecorded);
        Assert.assertEquals(mTestFiles.get(0).getFilePath(), mLastSelectedPhotos[0]);
        Assert.assertEquals(mTestFiles.get(2).getFilePath(), mLastSelectedPhotos[1]);
        Assert.assertEquals(mTestFiles.get(4).getFilePath(), mLastSelectedPhotos[2]);

        dismissDialog();
    }
}
