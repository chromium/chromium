// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.share.screenshot;

import android.view.View;
import android.view.ViewGroup;
import android.widget.FrameLayout;
import android.widget.ImageView;
import android.widget.LinearLayout;

import androidx.test.annotation.UiThreadTest;
import androidx.test.filters.MediumTest;

import org.junit.Assert;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.Callback;
import org.chromium.base.ThreadUtils;
import org.chromium.chrome.R;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;
import org.chromium.ui.test.util.BlankUiTestActivityTestCase;

import java.util.concurrent.atomic.AtomicBoolean;

/** Tests for the {@link ScreenshotShareSheetView}. */
@RunWith(ChromeJUnit4ClassRunner.class)
public class ScreenshotShareSheetViewTest extends BlankUiTestActivityTestCase {
    private ScreenshotShareSheetView mScreenshotView;
    private PropertyModel mScreenshotModel;
    private PropertyModelChangeProcessor mScreenshotMCP;

    private AtomicBoolean mCloseClicked = new AtomicBoolean();
    private AtomicBoolean mShareClicked = new AtomicBoolean();
    private AtomicBoolean mSaveClicked = new AtomicBoolean();

    private Callback<Integer> mMockNoArgListener =
            new Callback<Integer>() {
                @Override
                public void onResult(
                        @ScreenshotShareSheetViewProperties.NoArgOperation Integer operation) {
                    if (ScreenshotShareSheetViewProperties.NoArgOperation.SHARE == operation) {
                        mShareClicked.set(true);
                    } else if (ScreenshotShareSheetViewProperties.NoArgOperation.SAVE
                            == operation) {
                        mSaveClicked.set(true);
                    } else if (ScreenshotShareSheetViewProperties.NoArgOperation.DELETE
                            == operation) {
                        mCloseClicked.set(true);
                    }
                }
            };

    @Override
    public void setUpTest() throws Exception {
        super.setUpTest();

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    ViewGroup view = new LinearLayout(getActivity());
                    FrameLayout.LayoutParams params =
                            new FrameLayout.LayoutParams(
                                    ViewGroup.LayoutParams.MATCH_PARENT,
                                    ViewGroup.LayoutParams.MATCH_PARENT);
                    getActivity().setContentView(view, params);

                    mScreenshotView =
                            (ScreenshotShareSheetView)
                                    getActivity()
                                            .getLayoutInflater()
                                            .inflate(R.layout.screenshot_share_sheet, null);

                    view.addView(mScreenshotView);

                    mScreenshotModel =
                            new PropertyModel.Builder(ScreenshotShareSheetViewProperties.ALL_KEYS)
                                    .with(
                                            ScreenshotShareSheetViewProperties
                                                    .NO_ARG_OPERATION_LISTENER,
                                            mMockNoArgListener)
                                    .build();
                    mScreenshotMCP =
                            PropertyModelChangeProcessor.create(
                                    mScreenshotModel,
                                    mScreenshotView,
                                    ScreenshotShareSheetViewBinder::bind);
                });
    }

    @Test
    @MediumTest
    @UiThreadTest
    public void testClickToClose() {
        ImageView closeButton = mScreenshotView.findViewById(R.id.close_button);

        Assert.assertFalse(mCloseClicked.get());
        closeButton.performClick();
        Assert.assertTrue(mCloseClicked.get());
        mCloseClicked.set(false);
    }

    @Test
    @MediumTest
    @UiThreadTest
    public void testClickDeleteToClose() {
        View deleteButton = mScreenshotView.findViewById(R.id.delete);

        Assert.assertFalse(mCloseClicked.get());
        deleteButton.performClick();
        Assert.assertTrue(mCloseClicked.get());
        mCloseClicked.set(false);
    }

    @Test
    @MediumTest
    @UiThreadTest
    public void testShare() {
        View shareButton = mScreenshotView.findViewById(R.id.share);

        Assert.assertFalse(mShareClicked.get());
        shareButton.performClick();
        Assert.assertTrue(mShareClicked.get());
        mShareClicked.set(false);
    }

    @Test
    @MediumTest
    @UiThreadTest
    public void testSave() {
        View saveButton = mScreenshotView.findViewById(R.id.save);

        Assert.assertFalse(mSaveClicked.get());
        saveButton.performClick();
        Assert.assertTrue(mSaveClicked.get());
        mSaveClicked.set(false);
    }

    @Override
    public void tearDownTest() throws Exception {
        ThreadUtils.runOnUiThreadBlocking(mScreenshotMCP::destroy);
        super.tearDownTest();
    }
}
