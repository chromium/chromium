// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.page_info;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.when;

import android.content.Context;
import android.content.res.ColorStateList;
import android.widget.ImageView;
import android.widget.TextView;

import androidx.core.widget.ImageViewCompat;
import androidx.test.core.app.ApplicationProvider;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.R;
import org.chromium.components.page_info.PageInfoConnectionController;
import org.chromium.components.page_info.PageInfoControllerDelegate;
import org.chromium.components.page_info.PageInfoMainController;
import org.chromium.components.page_info.PageInfoRowView;
import org.chromium.components.security_state.ConnectionMaliciousContentStatus;
import org.chromium.components.security_state.ConnectionSecurityLevel;
import org.chromium.components.security_state.SecurityStateModel;
import org.chromium.components.security_state.SecurityStateModelJni;
import org.chromium.content_public.browser.WebContents;

/** Unit tests for {@link PageInfoConnectionController}. */
@RunWith(BaseRobolectricTestRunner.class)
public class PageInfoConnectionControllerUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private PageInfoMainController mMainController;
    @Mock private WebContents mWebContents;
    @Mock private PageInfoControllerDelegate mDelegate;
    @Mock private SecurityStateModel.Natives mSecurityStateModelJni;

    private Context mContext;
    private PageInfoRowView mRowView;
    private PageInfoConnectionController mController;

    @Before
    public void setUp() {
        mContext = ApplicationProvider.getApplicationContext();
        mContext.setTheme(R.style.Theme_BrowserUI_DayNight);
        mRowView = new PageInfoRowView(mContext, null);
        SecurityStateModelJni.setInstanceForTesting(mSecurityStateModelJni);

        mController =
                new PageInfoConnectionController(
                        mMainController,
                        mRowView,
                        mWebContents,
                        mDelegate,
                        /* publisher= */ null,
                        /* isInternalPage= */ false);
    }

    @Test
    public void testSetSecurityDescription_warnableSuspiciousSite() {
        when(mSecurityStateModelJni.getSecurityLevelForWebContents(mWebContents))
                .thenReturn(ConnectionSecurityLevel.DANGEROUS);
        when(mSecurityStateModelJni.getMaliciousContentStatusForWebContents(mWebContents))
                .thenReturn(ConnectionMaliciousContentStatus.WARNABLE_SUSPICIOUS_SITE);

        mController.setSecurityDescription(
                "Suspicious site",
                "Chrome detected unusual activity. <link>Learn more</link>",
                /* isSuspiciousSite= */ true);

        TextView title = mRowView.findViewById(R.id.page_info_row_title);
        TextView subtitle = mRowView.findViewById(R.id.page_info_row_subtitle);
        assertNotNull(title);
        assertNotNull(subtitle);
        assertEquals("Suspicious site", title.getText().toString());
        assertTrue(subtitle.getText().toString().contains("Chrome detected unusual activity."));

        // Verify title error color tint.
        assertEquals(
                mContext.getColor(R.color.default_text_color_error),
                title.getTextColors().getDefaultColor());

        // Verify icon tint is set to error color.
        ImageView icon = mRowView.findViewById(R.id.page_info_row_icon);
        assertNotNull(icon);
        assertEquals(
                ColorStateList.valueOf(mContext.getColor(R.color.default_text_color_error)),
                ImageViewCompat.getImageTintList(icon));
    }

    @Test
    public void testSetSecurityDescription_secureSite() {
        when(mSecurityStateModelJni.getSecurityLevelForWebContents(mWebContents))
                .thenReturn(ConnectionSecurityLevel.SECURE);
        when(mSecurityStateModelJni.getMaliciousContentStatusForWebContents(mWebContents))
                .thenReturn(ConnectionMaliciousContentStatus.NONE);

        mController.setSecurityDescription(
                "Connection is secure",
                "Your information is private.",
                /* isSuspiciousSite= */ false);

        TextView title = mRowView.findViewById(R.id.page_info_row_title);
        assertNotNull(title);
        assertEquals("Connection is secure", title.getText().toString());
        assertEquals(
                mContext.getColorStateList(R.color.default_text_color_list).getDefaultColor(),
                title.getTextColors().getDefaultColor());

        // Verify icon tint is default tint list for non-suspicious secure site.
        ImageView icon = mRowView.findViewById(R.id.page_info_row_icon);
        assertNotNull(icon);
        assertEquals(
                mContext.getColorStateList(R.color.default_icon_color_tint_list),
                ImageViewCompat.getImageTintList(icon));
    }
}
