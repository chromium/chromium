// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doAnswer;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.JniMocker;
import org.chromium.components.browser_ui.site_settings.WebsitePreferenceBridge;
import org.chromium.components.browser_ui.site_settings.WebsitePreferenceBridgeJni;
import org.chromium.components.content_settings.ContentSettingValues;
import org.chromium.components.content_settings.ContentSettingsType;
import org.chromium.content_public.browser.BrowserContextHandle;
import org.chromium.url.GURL;

/**
 * Unit tests for {@link RequestDesktopUtils}.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class RequestDesktopUtilsUnitTest {
    @Rule
    public JniMocker mJniMocker = new JniMocker();

    @Mock
    WebsitePreferenceBridge.Natives mWebsitePreferenceBridgeJniMock;
    @Mock
    BrowserContextHandle mBrowserContextHandleMock;
    @Mock
    GURL mGurlMock;

    private @ContentSettingValues int mRdsDefaultValue;
    private @ContentSettingValues int mLastRdsContentSettingExceptionValue;

    @Before
    public void setup() {
        MockitoAnnotations.initMocks(this);

        mJniMocker.mock(WebsitePreferenceBridgeJni.TEST_HOOKS, mWebsitePreferenceBridgeJniMock);

        doAnswer(invocation -> mRdsDefaultValue)
                .when(mWebsitePreferenceBridgeJniMock)
                .getDefaultContentSetting(any(), eq(ContentSettingsType.REQUEST_DESKTOP_SITE));

        doAnswer(invocation -> {
            mLastRdsContentSettingExceptionValue = invocation.getArgument(4);
            return null;
        })
                .when(mWebsitePreferenceBridgeJniMock)
                .setContentSettingDefaultScope(any(), eq(ContentSettingsType.REQUEST_DESKTOP_SITE),
                        any(), any(), anyInt());
        doAnswer(invocation -> mLastRdsContentSettingExceptionValue)
                .when(mWebsitePreferenceBridgeJniMock)
                .getContentSetting(
                        any(), eq(ContentSettingsType.REQUEST_DESKTOP_SITE), any(), any());
    }

    @Test
    public void testSetRequestDesktopSiteContentSettingsForUrl_DefaultBlock() {
        mRdsDefaultValue = ContentSettingValues.BLOCK;
        RequestDesktopUtils.setRequestDesktopSiteContentSettingsForUrl(
                mBrowserContextHandleMock, mGurlMock, true);
        Assert.assertEquals("Request Desktop Site content settings Exception is not set correctly.",
                ContentSettingValues.ALLOW, mLastRdsContentSettingExceptionValue);

        RequestDesktopUtils.setRequestDesktopSiteContentSettingsForUrl(
                mBrowserContextHandleMock, mGurlMock, false);
        Assert.assertEquals("Request Desktop Site content settings should be removed.",
                ContentSettingValues.DEFAULT, mLastRdsContentSettingExceptionValue);
    }

    @Test
    public void testSetRequestDesktopSiteContentSettingsForUrl_DefaultAllow() {
        mRdsDefaultValue = ContentSettingValues.ALLOW;
        RequestDesktopUtils.setRequestDesktopSiteContentSettingsForUrl(
                mBrowserContextHandleMock, mGurlMock, false);
        Assert.assertEquals("Request Desktop Site content settings Exception is not set correctly.",
                ContentSettingValues.BLOCK, mLastRdsContentSettingExceptionValue);

        RequestDesktopUtils.setRequestDesktopSiteContentSettingsForUrl(
                mBrowserContextHandleMock, mGurlMock, true);
        Assert.assertEquals("Request Desktop Site content settings should be removed.",
                ContentSettingValues.DEFAULT, mLastRdsContentSettingExceptionValue);
    }
}
