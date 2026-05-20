// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.messages;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.withId;

import android.graphics.Rect;

import androidx.core.view.accessibility.AccessibilityNodeInfoCompat;
import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.FakeTimeTestRule;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.Features;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.transit.ChromeTransitTestRules;
import org.chromium.chrome.test.transit.FreshCtaTransitTestRule;
import org.chromium.chrome.test.transit.page.WebPageStation;
import org.chromium.components.messages.MessageBannerProperties;
import org.chromium.components.messages.MessageDispatcher;
import org.chromium.components.messages.MessageDispatcherProvider;
import org.chromium.components.messages.MessagesTestHelper;
import org.chromium.components.messages.PrimaryActionClickBehavior;
import org.chromium.content.browser.accessibility.WebContentsAccessibilityImpl;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.WebContentsAccessibility;
import org.chromium.ui.accessibility.AccessibilityFeatures;
import org.chromium.ui.accessibility.AccessibilityState;
import org.chromium.ui.base.DeviceFormFactor;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.concurrent.TimeoutException;

/** Integration test of Message. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@Batch(Batch.PER_CLASS)
public class MessageTest {
    @Rule
    public FreshCtaTransitTestRule mActivityTestRule =
            ChromeTransitTestRules.freshChromeTabbedActivityRule();

    @Rule public FakeTimeTestRule mFakeTimeTestRule = new FakeTimeTestRule();

    private WebPageStation mPage;
    private ChromeTabbedActivity mActivity;
    private MessageDispatcher mMessageDispatcher;

    @Before
    public void setUp() {
        mPage =
                mActivityTestRule.startOnUrl(
                        "data:text/html,<!DOCTYPE html><html><body><button id='test_button'"
                                + " style='height: 100px; width: 100px;'>Button</button>"
                                + "</body></html>");
        mActivity = mPage.getActivity();
        mMessageDispatcher =
                ThreadUtils.runOnUiThreadBlocking(
                        () -> MessageDispatcherProvider.from(mActivity.getWindowAndroid()));
    }

    // TODO (gmarcoesau): move this to a shared location e.g. mActivityTestRule.
    private int findNodeIdWithText(WebContentsAccessibilityImpl wcax, int nodeId, String text) {
        AccessibilityNodeInfoCompat node = wcax.createAccessibilityNodeInfo(nodeId);
        if (node == null) return -1;
        CharSequence nodeText = node.getText();
        if (nodeText != null && nodeText.toString().contains(text)) {
            return nodeId;
        }
        CharSequence contentDesc = node.getContentDescription();
        if (contentDesc != null && contentDesc.toString().contains(text)) {
            return nodeId;
        }
        int[] children = wcax.getChildIdsForTesting(nodeId);
        if (children != null) {
            for (int childId : children) {
                int found = findNodeIdWithText(wcax, childId, text);
                if (found != -1) return found;
            }
        }
        return -1;
    }

    /** Test that the message container occludes the web contents. */
    @Test
    @SmallTest
    // TODO(crbug.com/514848255): Failing on other larger form factors.
    @Restriction(DeviceFormFactor.PHONE)
    @Features.EnableFeatures({AccessibilityFeatures.ACCESSIBILITY_HANDLE_OCCLUDING_VIEWS})
    public void testMessageContainerOccludesWebContents() throws TimeoutException {
        CallbackHelper helper = new CallbackHelper();
        PropertyModel model =
                ThreadUtils.runOnUiThreadBlocking(
                        () ->
                                new PropertyModel.Builder(MessageBannerProperties.ALL_KEYS)
                                        .with(MessageBannerProperties.TITLE, "Test title")
                                        .with(MessageBannerProperties.PRIMARY_BUTTON_TEXT, "Action")
                                        .with(MessageBannerProperties.ON_DISMISSED, (v) -> {})
                                        .with(
                                                MessageBannerProperties.ON_PRIMARY_ACTION,
                                                () -> {
                                                    return PrimaryActionClickBehavior
                                                            .DISMISS_IMMEDIATELY;
                                                })
                                        .build());

        Rect initialBounds = new Rect();
        int[] buttonNodeId = new int[1];

        WebContentsAccessibilityImpl wcax =
                ThreadUtils.runOnUiThreadBlocking(
                        () -> {
                            AccessibilityState.setIsAnyAccessibilityServiceEnabledForTesting(true);
                            WebContents webContents = mActivity.getActivityTab().getWebContents();
                            return (WebContentsAccessibilityImpl)
                                    WebContentsAccessibility.fromWebContents(webContents);
                        });

        CriteriaHelper.pollUiThread(
                () -> wcax.getAccessibilityNodeProvider() != null,
                "AccessibilityNodeProvider should be initialized",
                8000L,
                CriteriaHelper.DEFAULT_POLLING_INTERVAL);

        CriteriaHelper.pollUiThread(
                () -> {
                    int rootId = wcax.getRootIdForTesting();
                    buttonNodeId[0] = findNodeIdWithText(wcax, rootId, "Button");
                    return buttonNodeId[0] != -1;
                },
                "Button should be found",
                8000L,
                CriteriaHelper.DEFAULT_POLLING_INTERVAL);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    AccessibilityNodeInfoCompat buttonNode =
                            wcax.createAccessibilityNodeInfo(buttonNodeId[0]);
                    buttonNode.getBoundsInScreen(initialBounds);
                });

        ThreadUtils.runOnUiThreadBlocking(
                () -> mMessageDispatcher.enqueueWindowScopedMessage(model, true));

        onView(withId(R.id.message_primary_button)).check(matches(isDisplayed()));

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    AccessibilityNodeInfoCompat buttonNode =
                            wcax.createAccessibilityNodeInfo(buttonNodeId[0]);
                    Assert.assertFalse(
                            "Button node should be mostly occluded, thus not visible",
                            buttonNode.isVisibleToUser());
                });
    }

    /**
     * Test that message is not clickable within tap protection period and becomes clickable once it
     * ends.
     */
    @Test
    @SmallTest
    public void testTapProtection() throws TimeoutException {
        MessagesTestHelper.enableTapProtectionDuration(500);
        CallbackHelper helper = new CallbackHelper();
        PropertyModel model =
                ThreadUtils.runOnUiThreadBlocking(
                        () ->
                                new PropertyModel.Builder(MessageBannerProperties.ALL_KEYS)
                                        .with(MessageBannerProperties.TITLE, "Test title")
                                        .with(MessageBannerProperties.PRIMARY_BUTTON_TEXT, "Action")
                                        .with(MessageBannerProperties.ON_DISMISSED, (v) -> {})
                                        .with(
                                                MessageBannerProperties.ON_PRIMARY_ACTION,
                                                () -> {
                                                    helper.notifyCalled();
                                                    return PrimaryActionClickBehavior
                                                            .DISMISS_IMMEDIATELY;
                                                })
                                        .build());
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mMessageDispatcher.enqueueWindowScopedMessage(model, true);
                });
        onView(withId(R.id.message_primary_button)).check(matches(isDisplayed()));
        onView(withId(R.id.message_primary_button)).perform(click());
        Assert.assertEquals("Not clickable with tap protection period", 0, helper.getCallCount());
        mFakeTimeTestRule.advanceMillis(1500);
        onView(withId(R.id.message_primary_button)).perform(click());
        helper.waitForNext("Should able to click when tap protection period ends");
    }
}
