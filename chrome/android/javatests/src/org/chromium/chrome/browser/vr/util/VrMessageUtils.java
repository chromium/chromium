// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.vr.util;

import static org.chromium.chrome.browser.vr.XrTestFramework.POLL_CHECK_INTERVAL_SHORT_MS;
import static org.chromium.chrome.browser.vr.XrTestFramework.POLL_TIMEOUT_SHORT_MS;

import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.chrome.test.ChromeActivityTestRule;
import org.chromium.components.messages.MessageDispatcher;
import org.chromium.components.messages.MessageDispatcherProvider;
import org.chromium.components.messages.MessageIdentifier;
import org.chromium.components.messages.MessageStateHandler;
import org.chromium.components.messages.MessagesTestHelper;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.List;
import java.util.concurrent.ExecutionException;

/**
 * Class containing utility functions for interacting with messages at
 * a high level.
 */
public class VrMessageUtils {
    /**
     * Determines whether messages are present in the current activity.
     *
     * @param rule The ChromeActivityTestRule to get the messages from.
     * @return True if there are any messages present, false otherwise.
     */
    @SuppressWarnings("unchecked")
    public static boolean isMessagePresent(ChromeActivityTestRule rule) throws ExecutionException {
        return getVrInstallUpdateMessage(rule) != null;
    }

    /**
     * Determines is there is any message present in the given View hierarchy.
     *
     * @param rule The ChromeActivityTestRule to get the messages from.
     * @param present Whether a message should be present.
     */
    public static void expectMessagePresent(
            final ChromeActivityTestRule rule, final boolean present) {
        CriteriaHelper.pollUiThread(()
                                            -> isMessagePresent(rule) == present,
                "Message did not " + (present ? "appear" : "disappear"), POLL_TIMEOUT_SHORT_MS,
                POLL_CHECK_INTERVAL_SHORT_MS);
    }

    /**
     * Returns the {@link PropertyModel} of an enqueued VR install/update message.
     *
     * @param rule The ChromeActivityTestRule to get the messages from.
     * @return The {@link PropertyModel} of an enqueued VR install/update message, null if the
     *         message is not present.
     */
    public static PropertyModel getVrInstallUpdateMessage(ChromeActivityTestRule rule)
            throws ExecutionException {
        MessageDispatcher messageDispatcher = TestThreadUtils.runOnUiThreadBlocking(
                () -> MessageDispatcherProvider.from(rule.getActivity().getWindowAndroid()));
        List<MessageStateHandler> messages = MessagesTestHelper.getEnqueuedMessages(
                messageDispatcher, MessageIdentifier.VR_SERVICES_UPGRADE);
        return messages == null || messages.isEmpty()
                ? null
                : MessagesTestHelper.getCurrentMessage(messages.get(0));
    }
}
