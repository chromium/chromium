// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download.tray;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.verify;
import static org.robolectric.Shadows.shadowOf;

import android.app.Activity;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.ImageView;
import android.widget.TextView;

import androidx.core.view.accessibility.AccessibilityNodeInfoCompat;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Robolectric;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.download.R;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

/** Unit tests for {@link DownloadTrayItemViewBinder}. */
@RunWith(BaseRobolectricTestRunner.class)
public class DownloadTrayItemViewBinderTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private View.OnClickListener mOnOpen;

    private View mView;

    @Before
    public void setUp() {
        Activity activity = Robolectric.buildActivity(Activity.class).setup().get();
        activity.setTheme(R.style.Theme_BrowserUI_DayNight);
        mView = LayoutInflater.from(activity).inflate(R.layout.download_tray_item_row, null);
    }

    @Test
    public void testBind_showsRowAndRoutesClicks() {
        PropertyModel model =
                new PropertyModel.Builder(DownloadTrayItemProperties.ALL_KEYS)
                        .with(DownloadTrayItemProperties.ICON, R.drawable.ic_drive_image_24dp)
                        .with(DownloadTrayItemProperties.TITLE, "report-final.pdf")
                        .with(DownloadTrayItemProperties.DESCRIPTION, "4.30 MB • Done")
                        .with(DownloadTrayItemProperties.ON_OPEN, mOnOpen)
                        .build();
        PropertyModelChangeProcessor.create(model, mView, DownloadTrayItemViewBinder::bind);

        ImageView icon = mView.findViewById(R.id.download_tray_item_icon);
        assertEquals(
                R.drawable.ic_drive_image_24dp, shadowOf(icon.getDrawable()).getCreatedFromResId());
        assertEquals(
                "report-final.pdf",
                mView.<TextView>findViewById(R.id.download_tray_item_title).getText().toString());
        assertEquals(
                "4.30 MB • Done",
                mView.<TextView>findViewById(R.id.download_tray_item_description)
                        .getText()
                        .toString());

        // Clicking the row opens the download, and TalkBack announces it as "Open".
        mView.performClick();
        verify(mOnOpen).onClick(mView);
        AccessibilityNodeInfoCompat rowInfo =
                AccessibilityNodeInfoCompat.wrap(mView.createAccessibilityNodeInfo());
        String openLabel = mView.getContext().getString(R.string.open_downloaded_label);
        assertTrue(
                rowInfo.getActionList().stream()
                        .anyMatch(
                                action ->
                                        action.getId() == AccessibilityNodeInfoCompat.ACTION_CLICK
                                                && openLabel.equals(
                                                        String.valueOf(action.getLabel()))));

        // Rows update in place, e.g. after a rename.
        model.set(DownloadTrayItemProperties.TITLE, "renamed.pdf");
        assertEquals(
                "renamed.pdf",
                mView.<TextView>findViewById(R.id.download_tray_item_title).getText().toString());
    }
}
