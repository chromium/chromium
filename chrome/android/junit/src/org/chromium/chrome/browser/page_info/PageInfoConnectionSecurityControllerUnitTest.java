// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.page_info;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.content.Context;
import android.view.View;
import android.widget.TextView;

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
import org.chromium.components.page_info.ConnectionSecurityView;
import org.chromium.components.page_info.PageInfoConnectionSecurityController;
import org.chromium.components.page_info.PageInfoConnectionSecurityControllerJni;
import org.chromium.components.page_info.PageInfoControllerDelegate;
import org.chromium.components.page_info.PageInfoMainController;
import org.chromium.components.page_info.PageInfoRowView;
import org.chromium.content_public.browser.WebContents;

/** Unit tests for {@link PageInfoConnectionSecurityController}. */
@RunWith(BaseRobolectricTestRunner.class)
public class PageInfoConnectionSecurityControllerUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private PageInfoMainController mMainController;
    @Mock private WebContents mWebContents;
    @Mock private PageInfoControllerDelegate mDelegate;
    @Mock private PageInfoConnectionSecurityController.Natives mNativeMock;

    private Context mContext;
    private ConnectionSecurityView mView;
    private PageInfoRowView mRowView;
    private PageInfoConnectionSecurityController mController;

    @Before
    public void setUp() {
        mContext = ApplicationProvider.getApplicationContext();
        mContext.setTheme(R.style.Theme_BrowserUI_DayNight);
        mView = new ConnectionSecurityView(mContext, null);
        mRowView = new PageInfoRowView(mContext, null);
        PageInfoConnectionSecurityControllerJni.setInstanceForTesting(mNativeMock);
        when(mNativeMock.init(any(), any())).thenReturn(1L);

        mController =
                new PageInfoConnectionSecurityController(
                        mMainController, mView, mRowView, mWebContents, mDelegate, null);
    }

    @Test
    public void testShowSecurityPageButton_regularPage() {
        when(mDelegate.getPdfPageType()).thenReturn(0);

        mController.showSecurityPageButton("Connection is secure");

        assertEquals(View.VISIBLE, mRowView.getVisibility());
        TextView title = mRowView.findViewById(R.id.page_info_row_title);
        assertNotNull(title);
        assertEquals("Connection is secure", title.getText().toString());
        verify(mMainController).updateConnectionWrapperVisibility();
    }

    @Test
    public void testShowSecurityPageButton_pdfPage() {
        when(mDelegate.getPdfPageType()).thenReturn(1);
        when(mDelegate.getPdfPageConnectionMessage()).thenReturn("You're viewing a PDF file");

        mController.showSecurityPageButton("Connection is secure");

        assertEquals(View.VISIBLE, mRowView.getVisibility());
        TextView title = mRowView.findViewById(R.id.page_info_row_title);
        assertNotNull(title);
        assertEquals("You're viewing a PDF file", title.getText().toString());
        verify(mMainController).updateConnectionWrapperVisibility();
    }

    @Test
    public void testShowSecurityPageButton_pdfPage_nullMessageFallback() {
        when(mDelegate.getPdfPageType()).thenReturn(1);
        when(mDelegate.getPdfPageConnectionMessage()).thenReturn(null);

        mController.showSecurityPageButton("Connection is secure");

        assertEquals(View.VISIBLE, mRowView.getVisibility());
        TextView title = mRowView.findViewById(R.id.page_info_row_title);
        assertNotNull(title);
        assertEquals("Connection is secure", title.getText().toString());
        verify(mMainController).updateConnectionWrapperVisibility();
    }

    @Test
    public void testShowSecurityPageButton_contentPublisher() {
        PageInfoConnectionSecurityController controllerWithPublisher =
                new PageInfoConnectionSecurityController(
                        mMainController, mView, mRowView, mWebContents, mDelegate, "example.com");

        controllerWithPublisher.showSecurityPageButton("Connection is secure");

        assertEquals(View.VISIBLE, mRowView.getVisibility());
        TextView title = mRowView.findViewById(R.id.page_info_row_title);
        assertNotNull(title);
        assertEquals(
                mContext.getString(R.string.page_info_domain_hidden, "example.com"),
                title.getText().toString());
        verify(mMainController).updateConnectionWrapperVisibility();
    }

    @Test
    public void testSetSecurityDescription_regularPage() {
        when(mDelegate.getPdfPageType()).thenReturn(0);

        mController.showSecurityInfo();
        mController.setSecurityDescription(
                0, 0, "Summary", "Details", false, new byte[0][], false, new byte[0][], null);

        TextView summary = mView.findViewById(R.id.security_description_summary);
        TextView details = mView.findViewById(R.id.security_description_details);
        assertNotNull(summary);
        assertNotNull(details);
        assertEquals(View.VISIBLE, summary.getVisibility());
        assertEquals("Summary", summary.getText().toString());
        assertEquals(View.VISIBLE, details.getVisibility());
        assertTrue(details.getText().toString().contains("Details"));
    }

    @Test
    public void testSetSecurityDescription_pdfPage() {
        when(mDelegate.getPdfPageType()).thenReturn(1);
        when(mDelegate.getPdfPageConnectionMessage()).thenReturn("You're viewing a PDF file");

        mController.showSecurityInfo();
        mController.setSecurityDescription(
                0, 0, "Summary", "Details", false, new byte[0][], false, new byte[0][], null);

        TextView summary = mView.findViewById(R.id.security_description_summary);
        TextView details = mView.findViewById(R.id.security_description_details);
        assertNotNull(summary);
        assertNotNull(details);
        assertEquals(View.GONE, summary.getVisibility());
        assertEquals(View.VISIBLE, details.getVisibility());
        assertEquals("You're viewing a PDF file", details.getText().toString());
    }

    @Test
    public void testCreateViewForSubpage_pdfPage() {
        when(mDelegate.getPdfPageType()).thenReturn(1);
        when(mDelegate.getPdfPageConnectionMessage()).thenReturn("You're viewing a PDF file");

        mController.setSecurityDescription(
                0, 0, "Summary", "Details", false, new byte[0][], false, new byte[0][], null);
        View subpageView = mController.createViewForSubpage(null);

        TextView summary = subpageView.findViewById(R.id.security_description_summary);
        TextView details = subpageView.findViewById(R.id.security_description_details);
        assertNotNull(summary);
        assertNotNull(details);
        assertEquals(View.GONE, summary.getVisibility());
        assertEquals(View.VISIBLE, details.getVisibility());
        assertEquals("You're viewing a PDF file", details.getText().toString());
    }

    @Test
    public void testSetSecurityDescription_offlinePage() {
        when(mDelegate.getPdfPageType()).thenReturn(0);
        when(mDelegate.getOfflinePageConnectionMessage()).thenReturn("Showing offline copy");

        mController.showSecurityInfo();
        mController.setSecurityDescription(
                0, 0, "Summary", "Details", false, new byte[0][], false, new byte[0][], null);

        TextView summary = mView.findViewById(R.id.security_description_summary);
        TextView details = mView.findViewById(R.id.security_description_details);
        assertNotNull(summary);
        assertNotNull(details);
        assertEquals(View.GONE, summary.getVisibility());
        assertEquals(View.VISIBLE, details.getVisibility());
        assertEquals("Showing offline copy", details.getText().toString());
    }

    @Test
    public void testCreateViewForSubpage_offlinePage() {
        when(mDelegate.getPdfPageType()).thenReturn(0);
        when(mDelegate.getOfflinePageConnectionMessage()).thenReturn("Showing offline copy");

        mController.setSecurityDescription(
                0, 0, "Summary", "Details", false, new byte[0][], false, new byte[0][], null);
        View subpageView = mController.createViewForSubpage(null);

        TextView summary = subpageView.findViewById(R.id.security_description_summary);
        TextView details = subpageView.findViewById(R.id.security_description_details);
        assertNotNull(summary);
        assertNotNull(details);
        assertEquals(View.GONE, summary.getVisibility());
        assertEquals(View.VISIBLE, details.getVisibility());
        assertEquals("Showing offline copy", details.getText().toString());
    }

    @Test
    public void testSetSecurityDescription_contentPublisher() {
        PageInfoConnectionSecurityController controllerWithPublisher =
                new PageInfoConnectionSecurityController(
                        mMainController, mView, mRowView, mWebContents, mDelegate, "example.com");

        controllerWithPublisher.showSecurityInfo();
        controllerWithPublisher.setSecurityDescription(
                0, 0, "Summary", "Details", false, new byte[0][], false, new byte[0][], null);

        TextView summary = mView.findViewById(R.id.security_description_summary);
        TextView details = mView.findViewById(R.id.security_description_details);
        assertNotNull(summary);
        assertNotNull(details);
        assertEquals(View.GONE, summary.getVisibility());
        assertEquals(View.VISIBLE, details.getVisibility());
        assertEquals(
                mContext.getString(R.string.page_info_domain_hidden, "example.com"),
                details.getText().toString());
    }

    @Test
    public void testCreateViewForSubpage_contentPublisher() {
        PageInfoConnectionSecurityController controllerWithPublisher =
                new PageInfoConnectionSecurityController(
                        mMainController, mView, mRowView, mWebContents, mDelegate, "example.com");

        controllerWithPublisher.setSecurityDescription(
                0, 0, "Summary", "Details", false, new byte[0][], false, new byte[0][], null);
        View subpageView = controllerWithPublisher.createViewForSubpage(null);

        TextView summary = subpageView.findViewById(R.id.security_description_summary);
        TextView details = subpageView.findViewById(R.id.security_description_details);
        assertNotNull(summary);
        assertNotNull(details);
        assertEquals(View.GONE, summary.getVisibility());
        assertEquals(View.VISIBLE, details.getVisibility());
        assertEquals(
                mContext.getString(R.string.page_info_domain_hidden, "example.com"),
                details.getText().toString());
    }
}
