// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.chrome.browser.supervised_user;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.matcher.ViewMatchers.isCompletelyDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.withId;

import static org.chromium.base.test.transit.ViewFinder.waitForView;

import android.text.TextUtils;
import android.view.View;
import android.widget.Button;
import android.widget.TextView;

import androidx.annotation.IdRes;

import org.hamcrest.Description;
import org.hamcrest.Matcher;
import org.hamcrest.Matchers;
import org.hamcrest.TypeSafeMatcher;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.transit.ViewElement;
import org.chromium.base.test.util.Criteria;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetTestSupport;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.test.util.DOMUtils;

import java.util.concurrent.TimeoutException;

@NullMarked
class WebsiteParentApprovalTestUtils {
    private static final String LOCAL_APPROVALS_BUTTON_NODE_ID = "local-approvals-button";
    private static final String REMOTE_APPROVALS_BUTTON_NODE_ID = "remote-approvals-button";

    static void checkLocalApprovalsButtonIsVisible(WebContents webContents) {
        try {
            Criteria.checkThat(
                    DOMUtils.getNodeContents(webContents, LOCAL_APPROVALS_BUTTON_NODE_ID).trim(),
                    Matchers.not(Matchers.isEmptyString()));
        } catch (TimeoutException e) {
            throw new RuntimeException("Local approval button not found");
        }
    }

    static void checkRemoteApprovalsButtonIsVisible(WebContents webContents) {
        try {
            Criteria.checkThat(
                    DOMUtils.getNodeContents(webContents, REMOTE_APPROVALS_BUTTON_NODE_ID).trim(),
                    Matchers.not(Matchers.isEmptyString()));
        } catch (TimeoutException e) {
            throw new RuntimeException("Remote approval button not found");
        }
    }

    static void clickAskInPerson(WebContents webContents) {
        checkLocalApprovalsButtonIsVisible(webContents);
        DOMUtils.clickNodeWithJavaScript(webContents, LOCAL_APPROVALS_BUTTON_NODE_ID);
    }

    private static void checkParentApprovalBottomSheetVisible(
            BottomSheetTestSupport bottomSheetTestSupport) {
        waitForView(
                withId(R.id.local_parent_approval_layout), ViewElement.displayingAtLeastOption(1));
        // Ensure all bottom-sheet animations have ended before interacting with the view.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    bottomSheetTestSupport.endAllAnimations();
                });
    }

    static void clickApprove(BottomSheetTestSupport bottomSheetTestSupport) {
        clickApprovalActionButton(bottomSheetTestSupport, R.id.approve_button);
    }

    static void clickDoNotApprove(BottomSheetTestSupport bottomSheetTestSupport) {
        clickApprovalActionButton(bottomSheetTestSupport, R.id.deny_button);
    }

    private static void clickApprovalActionButton(
            BottomSheetTestSupport bottomSheetTestSupport, @IdRes int inLayoutButtonId) {
        checkParentApprovalBottomSheetVisible(bottomSheetTestSupport);
        Matcher<View> buttonMatcher = isApprovalActionButton(inLayoutButtonId);
        waitForView(buttonMatcher, ViewElement.displayingAtLeastOption(100));
        onView(buttonMatcher).check(matches(isCompletelyDisplayed())).perform(click());
    }

    /**
     * Matches the active Approve/Deny button across both legacy flag-off presentation (the visible
     * in-layout {@code R.id.approve_button}/{@code R.id.deny_button}) and flag-on Adaptive UI
     * bottom-sheet or modal-dialog presentation (where the in-layout buttons are {@link View#GONE}
     * and the Adaptive UI primary/secondary {@link Button} carries the same localized label inside
     * the same window root as {@code R.id.local_parent_approval_layout}).
     */
    private static Matcher<View> isApprovalActionButton(@IdRes int inLayoutButtonId) {
        return new TypeSafeMatcher<View>() {
            @Override
            public void describeTo(Description description) {
                description.appendText(
                        "displayed approval action button for resource id " + inLayoutButtonId);
            }

            @Override
            protected boolean matchesSafely(View view) {
                if (!(view instanceof Button) || !view.isShown()) {
                    return false;
                }
                if (view.getId() == inLayoutButtonId) {
                    return true;
                }
                View root = view.getRootView();
                if (root == null) {
                    return false;
                }
                @Nullable View approvalLayout =
                        root.findViewById(R.id.local_parent_approval_layout);
                if (approvalLayout == null) {
                    return false;
                }
                @Nullable TextView inLayoutButton = approvalLayout.findViewById(inLayoutButtonId);
                if (inLayoutButton == null || inLayoutButton.getVisibility() == View.VISIBLE) {
                    return false;
                }
                return TextUtils.equals(((Button) view).getText(), inLayoutButton.getText());
            }
        };
    }
}
