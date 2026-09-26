// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.share.screenshot;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import android.app.Activity;
import android.view.View;
import android.view.ViewGroup;
import android.widget.FrameLayout;
import android.widget.ImageView;
import android.widget.LinearLayout;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.Robolectric;

import org.chromium.base.Callback;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.R;
import org.chromium.ui.base.TestActivity;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

import java.util.concurrent.atomic.AtomicBoolean;

/** Tests for the {@link ScreenshotShareSheetView}. */
@RunWith(BaseRobolectricTestRunner.class)
public class ScreenshotShareSheetViewTest {
    private Activity mActivity;
    private ScreenshotShareSheetView mScreenshotView;
    private PropertyModel mScreenshotModel;
    private PropertyModelChangeProcessor mScreenshotMCP;

    private final AtomicBoolean mCloseClicked = new AtomicBoolean();
    private final AtomicBoolean mShareClicked = new AtomicBoolean();
    private final AtomicBoolean mSaveClicked = new AtomicBoolean();

    private final Callback<Integer> mMockNoArgListener =
            new Callback<>() {
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

    @Before
    public void setUp() {
        mActivity = Robolectric.buildActivity(TestActivity.class).setup().get();
        ViewGroup view = new LinearLayout(mActivity);
        FrameLayout.LayoutParams params =
                new FrameLayout.LayoutParams(
                        ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.MATCH_PARENT);
        mActivity.setContentView(view, params);

        mScreenshotView =
                (ScreenshotShareSheetView)
                        mActivity
                                .getLayoutInflater()
                                .inflate(R.layout.screenshot_share_sheet, null);

        view.addView(mScreenshotView);

        mScreenshotModel =
                new PropertyModel.Builder(ScreenshotShareSheetViewProperties.ALL_KEYS)
                        .with(
                                ScreenshotShareSheetViewProperties.NO_ARG_OPERATION_LISTENER,
                                mMockNoArgListener)
                        .build();
        mScreenshotMCP =
                PropertyModelChangeProcessor.create(
                        mScreenshotModel, mScreenshotView, ScreenshotShareSheetViewBinder::bind);
    }

    @Test
    public void testClickToClose() {
        ImageView closeButton = mScreenshotView.findViewById(R.id.close_button);

        assertFalse(mCloseClicked.get());
        closeButton.performClick();
        assertTrue(mCloseClicked.get());
        mCloseClicked.set(false);
    }

    @Test
    public void testClickDeleteToClose() {
        View deleteButton = mScreenshotView.findViewById(R.id.delete);

        assertFalse(mCloseClicked.get());
        deleteButton.performClick();
        assertTrue(mCloseClicked.get());
        mCloseClicked.set(false);
    }

    @Test
    public void testShare() {
        View shareButton = mScreenshotView.findViewById(R.id.share);

        assertFalse(mShareClicked.get());
        shareButton.performClick();
        assertTrue(mShareClicked.get());
        mShareClicked.set(false);
    }

    @Test
    public void testSave() {
        View saveButton = mScreenshotView.findViewById(R.id.save);

        assertFalse(mSaveClicked.get());
        saveButton.performClick();
        assertTrue(mSaveClicked.get());
        mSaveClicked.set(false);
    }

    @After
    public void tearDown() {
        mScreenshotMCP.destroy();
    }
}
