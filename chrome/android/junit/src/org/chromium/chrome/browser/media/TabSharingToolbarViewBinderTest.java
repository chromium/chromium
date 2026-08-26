// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.media;

import static org.junit.Assert.assertEquals;
import static org.mockito.Mockito.verify;

import android.app.Activity;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.TextView;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Robolectric;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.R;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

/** Unit tests for {@link TabSharingToolbarViewBinder}. */
@RunWith(BaseRobolectricTestRunner.class)
public class TabSharingToolbarViewBinderTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private Runnable mStopListener;
    @Mock private Runnable mShareInsteadListener;

    private View mView;
    private PropertyModel mModel;
    private PropertyModelChangeProcessor mChangeProcessor;

    @Before
    public void setUp() {
        Activity activity = Robolectric.buildActivity(Activity.class).create().get();
        activity.setTheme(R.style.Theme_BrowserUI_DayNight);
        mView = LayoutInflater.from(activity).inflate(R.layout.tab_sharing_toolbar, null);
        mModel = new PropertyModel.Builder(TabSharingToolbarProperties.ALL_KEYS).build();
        mChangeProcessor =
                PropertyModelChangeProcessor.create(
                        mModel, mView, TabSharingToolbarViewBinder::bind);
    }

    @Test
    public void testStatusTextBinding() {
        String testStatus = "Sharing test tab";
        mModel.set(TabSharingToolbarProperties.STATUS_TEXT, testStatus);
        TextView messageView = mView.findViewById(R.id.tab_sharing_message);
        assertEquals(testStatus, messageView.getText().toString());
    }

    @Test
    public void testStopSharingClickListenerBinding() {
        mModel.set(TabSharingToolbarProperties.STOP_SHARING_CLICK_LISTENER, mStopListener);
        View stopButton = mView.findViewById(R.id.tab_sharing_stop_button);
        stopButton.performClick();
        verify(mStopListener).run();
    }

    @Test
    public void testShareInsteadBinding() {
        mModel.set(TabSharingToolbarProperties.SHARE_INSTEAD_BUTTON_VISIBLE, true);
        View shareButton = mView.findViewById(R.id.tab_sharing_share_instead_button);
        assertEquals(View.VISIBLE, shareButton.getVisibility());

        mModel.set(TabSharingToolbarProperties.SHARE_INSTEAD_CLICK_LISTENER, mShareInsteadListener);
        shareButton.performClick();
        verify(mShareInsteadListener).run();
    }

    @Test
    public void testPropertiesAndNullListeners() {
        mModel.set(TabSharingToolbarProperties.SHARE_INSTEAD_BUTTON_VISIBLE, false);
        View shareButton = mView.findViewById(R.id.tab_sharing_share_instead_button);
        assertEquals(View.GONE, shareButton.getVisibility());

        mModel.set(TabSharingToolbarProperties.STOP_SHARING_CLICK_LISTENER, null);
        View stopButton = mView.findViewById(R.id.tab_sharing_stop_button);
        stopButton.performClick();

        mModel.set(TabSharingToolbarProperties.SHARE_INSTEAD_CLICK_LISTENER, null);
        shareButton.performClick();
    }
}
