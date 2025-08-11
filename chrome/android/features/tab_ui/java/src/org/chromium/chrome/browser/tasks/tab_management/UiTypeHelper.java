// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.tasks.tab_management.TabProperties.UiType;
import org.chromium.chrome.browser.tasks.tab_management.TabSwitcherMessageManager.MessageType;

/** List of helper methods for operations involving {@link UiType}. */
@NullMarked
public class UiTypeHelper {
    /**
     * @return True if the given integer is a valid UiType.
     */
    public static boolean isValidUiType(int type) {
        return switch (type) {
            case UiType.TAB,
                    UiType.STRIP,
                    UiType.TAB_GROUP,
                    UiType.PRICE_MESSAGE,
                    UiType.INCOGNITO_REAUTH_PROMO_MESSAGE,
                    UiType.ARCHIVED_TABS_IPH_MESSAGE,
                    UiType.ARCHIVED_TABS_MESSAGE,
                    UiType.TAB_GROUP_SUGGESTION_MESSAGE,
                    UiType.IPH_MESSAGE,
                    UiType.COLLABORATION_ACTIVITY_MESSAGE -> true;
            default -> false;
        };
    }

    /**
     * @param type The {@link UiType} to check.
     * @return whether the given `type` is a message card.
     */
    public static boolean isMessageCard(@UiType int type) {
        // Since message cards and non-message cards are separated in @UiType,
        // we can use a GT check to avoid multiple or checks.
        return type >= UiType.PRICE_MESSAGE;
    }

    /**
     * @param type The {@link UiType} to check.
     * @return whether the given `type` is a large message card.
     */
    public static boolean isLargeMessageCard(@UiType int type) {
        return type == UiType.PRICE_MESSAGE || type == UiType.INCOGNITO_REAUTH_PROMO_MESSAGE;
    }

    /**
     * Maps a {@link MessageType} to a {@link UiType}.
     *
     * @param type The {@link MessageType} to convert.
     * @throws IllegalArgumentException if the {@link MessageType} is not a message card that can be
     *     converted to a {@link UiType}.
     */
    public static @UiType int messageTypeToUiType(@MessageType int type) {
        return switch (type) {
            case MessageType.IPH -> UiType.IPH_MESSAGE;
            case MessageType.PRICE_MESSAGE -> UiType.PRICE_MESSAGE;
            case MessageType.INCOGNITO_REAUTH_PROMO_MESSAGE -> UiType
                    .INCOGNITO_REAUTH_PROMO_MESSAGE;
            case MessageType.ARCHIVED_TABS_MESSAGE -> UiType.ARCHIVED_TABS_MESSAGE;
            case MessageType.ARCHIVED_TABS_IPH_MESSAGE -> UiType.ARCHIVED_TABS_IPH_MESSAGE;
            case MessageType.COLLABORATION_ACTIVITY -> UiType.COLLABORATION_ACTIVITY_MESSAGE;
            case MessageType.TAB_GROUP_SUGGESTION_MESSAGE -> UiType.TAB_GROUP_SUGGESTION_MESSAGE;
            default -> throw new IllegalArgumentException();
        };
    }
}
