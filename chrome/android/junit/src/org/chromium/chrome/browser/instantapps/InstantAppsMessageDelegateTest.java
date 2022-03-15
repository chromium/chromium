// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.instantapps;

import android.content.Context;
import android.content.res.Resources;
import android.graphics.Bitmap;
import android.graphics.drawable.BitmapDrawable;

import androidx.appcompat.content.res.AppCompatResources;
import androidx.test.core.app.ApplicationProvider;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.R;
import org.chromium.components.messages.DismissReason;
import org.chromium.components.messages.MessageBannerProperties;
import org.chromium.components.messages.MessageDispatcher;
import org.chromium.components.messages.MessageScopeType;
import org.chromium.components.webapps.WebappsIconUtils;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.modelutil.PropertyModel;

/** Tests for {@link InstantAppsMessageDelegate}. */
@RunWith(BaseRobolectricTestRunner.class)
public class InstantAppsMessageDelegateTest {
    private static final String TEST_APP_NAME = "Test App";
    private static final String PRIMARY_ACTION_LABEL = "Open app";

    @Rule
    public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock
    private WebContents mWebContents;

    @Mock
    private MessageDispatcher mMessageDispatcher;

    @Mock
    private Bitmap mAppIcon;

    @Rule
    public JniMocker jniMocker = new JniMocker();

    @Mock
    private InstantAppsMessageDelegate.Natives mNativeMock;

    private InstantAppsMessageDelegate mDelegate;
    private Context mContext;
    private InstantAppsBannerData mData;

    @Before
    public void setup() {
        jniMocker.mock(InstantAppsMessageDelegateJni.TEST_HOOKS, mNativeMock);
        mContext = ApplicationProvider.getApplicationContext();
    }

    /**
     * Tests the Instant Apps message properties.
     */
    @Test
    public void testShowMessage() {
        initializeDelegate();
        mDelegate.showMessage();
        PropertyModel message = mDelegate.getMessageForTesting();

        Resources resources = ApplicationProvider.getApplicationContext().getResources();

        Assert.assertEquals("Message title should match.",
                String.format(resources.getString(R.string.instant_apps_message_title),
                        mData.getAppName()),
                message.get(MessageBannerProperties.TITLE));
        Assert.assertEquals("Message title content description should match.",
                String.format(
                        resources.getString(
                                R.string.accessibility_instant_apps_message_title_content_description),
                        mData.getAppName()),
                message.get(MessageBannerProperties.TITLE_CONTENT_DESCRIPTION));
        Assert.assertTrue("Message description icon should match.",
                ((BitmapDrawable) AppCompatResources.getDrawable(
                         mContext, R.drawable.google_play_dark))
                        .getBitmap()
                        .sameAs(((BitmapDrawable) message.get(
                                         MessageBannerProperties.DESCRIPTION_ICON))
                                        .getBitmap()));
        Assert.assertTrue("Message description icon should be resized.",
                message.get(MessageBannerProperties.RESIZE_DESCRIPTION_ICON));
        Assert.assertEquals("Message icon should match.", mAppIcon,
                ((BitmapDrawable) message.get(MessageBannerProperties.ICON)).getBitmap());
        Assert.assertTrue(
                "Message icon should be large.", message.get(MessageBannerProperties.LARGE_ICON));
        Assert.assertEquals("Message icon should have a rounded corner radius.",
                WebappsIconUtils.getIdealIconCornerRadiusPxForPromptUI(),
                message.get(MessageBannerProperties.ICON_ROUNDED_CORNER_RADIUS_PX));
        Assert.assertEquals("Message primary button text should match.",
                mData.getPrimaryActionLabel(),
                message.get(MessageBannerProperties.PRIMARY_BUTTON_TEXT));

        Mockito.verify(mMessageDispatcher)
                .enqueueMessage(message, mWebContents, MessageScopeType.WEB_CONTENTS, false);
        Mockito.verify(mNativeMock)
                .onMessageShown(mWebContents, mData.getUrl(), mData.isInstantAppDefault());
    }

    /**
     * Tests that the Instant Apps message primary action callback invokes the native method to
     * account for the primary action.
     */
    @Test
    public void testMessagePrimaryActionCallback() {
        initializeDelegate();
        mDelegate.showMessage();
        PropertyModel message = mDelegate.getMessageForTesting();

        message.get(MessageBannerProperties.ON_PRIMARY_ACTION).get();
        Mockito.verify(mNativeMock).onPrimaryAction(mData.isInstantAppDefault());
    }

    /**
     * Tests that the Instant Apps message dismissal callback invokes the native method to account
     * for message dismissal.
     */
    @Test
    public void testMessageDismissalCallback() {
        initializeDelegate();
        mDelegate.showMessage();
        PropertyModel message = mDelegate.getMessageForTesting();

        message.get(MessageBannerProperties.ON_DISMISSED)
                .onResult(DismissReason.DISMISSED_BY_FEATURE);
        Mockito.verify(mNativeMock)
                .onMessageDismissed(mWebContents, mData.getUrl(), mData.isInstantAppDefault());
    }

    /**
     * Helper function that creates the InstantAppsMessageDelegate.
     */
    private void initializeDelegate() {
        mData = new InstantAppsBannerData(TEST_APP_NAME, mAppIcon, null, null, null,
                PRIMARY_ACTION_LABEL, mWebContents, true);
        mDelegate = InstantAppsMessageDelegate.create(
                mContext, mWebContents, mMessageDispatcher, mData);
        Mockito.verify(mNativeMock)
                .initializeNativeDelegate(mDelegate, mWebContents, mData.getUrl());
    }
}
