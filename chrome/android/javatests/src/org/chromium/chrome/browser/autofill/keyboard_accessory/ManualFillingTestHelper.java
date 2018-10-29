// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.keyboard_accessory;

import static android.support.test.espresso.Espresso.onView;
import static android.support.test.espresso.matcher.ViewMatchers.isAssignableFrom;
import static android.support.test.espresso.matcher.ViewMatchers.isDisplayed;
import static android.support.test.espresso.matcher.ViewMatchers.isRoot;

import static org.hamcrest.core.AllOf.allOf;

import static org.chromium.chrome.test.util.ViewUtils.VIEW_GONE;
import static org.chromium.chrome.test.util.ViewUtils.VIEW_INVISIBLE;
import static org.chromium.chrome.test.util.ViewUtils.VIEW_NULL;
import static org.chromium.chrome.test.util.ViewUtils.waitForView;
import static org.chromium.ui.base.LocalizationUtils.setRtlForTesting;

import android.app.Activity;
import android.support.design.widget.TabLayout;
import android.support.test.espresso.PerformException;
import android.support.test.espresso.UiController;
import android.support.test.espresso.ViewAction;
import android.support.test.espresso.ViewInteraction;
import android.view.View;
import android.view.ViewGroup;

import org.hamcrest.Matcher;
import org.junit.Assert;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.UrlUtils;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.ChromeWindow;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.content_public.browser.ImeAdapter;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.test.util.Criteria;
import org.chromium.content_public.browser.test.util.CriteriaHelper;
import org.chromium.content_public.browser.test.util.DOMUtils;
import org.chromium.content_public.browser.test.util.TestInputMethodManagerWrapper;
import org.chromium.ui.DropdownPopupWindowInterface;

import java.util.concurrent.ExecutionException;
import java.util.concurrent.TimeoutException;
import java.util.concurrent.atomic.AtomicReference;

/**
 * Helpers in this class simplify interactions with the Keyboard Accessory and the sheet below it.
 */
public class ManualFillingTestHelper {
    private final ChromeTabbedActivityTestRule mActivityTestRule;
    private final AtomicReference<WebContents> mWebContentsRef = new AtomicReference<>();
    private TestInputMethodManagerWrapper mInputMethodManagerWrapper;

    public FakeKeyboard getKeyboard() {
        return (FakeKeyboard) mActivityTestRule.getKeyboardDelegate();
    }

    ManualFillingTestHelper(ChromeTabbedActivityTestRule activityTestRule) {
        mActivityTestRule = activityTestRule;
    }

    public void loadTestPage(boolean isRtl) throws InterruptedException {
        ChromeWindow.setKeyboardVisibilityDelegateFactory(FakeKeyboard::new);
        mActivityTestRule.startMainActivityWithURL(UrlUtils.encodeHtmlDataUri("<html"
                + (isRtl ? " dir=\"rtl\"" : "") + "><head>"
                + "<meta name=\"viewport\""
                + "content=\"width=device-width, initial-scale=1.0, maximum-scale=1.0\" /></head>"
                + "<body><form method=\"POST\">"
                + "<input type=\"password\" id=\"password\"/><br>"
                + "<input type=\"text\" id=\"email\" autocomplete=\"email\" /><br>"
                + "<input type=\"submit\" id=\"submit\" />"
                + "</form></body></html>"));
        setRtlForTesting(isRtl);
        ThreadUtils.runOnUiThreadBlocking(() -> {
            ChromeTabbedActivity activity = (ChromeTabbedActivity) mActivityTestRule.getActivity();
            mWebContentsRef.set(activity.getActivityTab().getWebContents());
            activity.getManualFillingController()
                    .getMediatorForTesting()
                    .setInsetObserverViewSupplier(
                            ()
                                    -> getKeyboard().createInsetObserver(
                                            activity.getInsetObserverView().getContext()));
            // The TestInputMethodManagerWrapper intercepts showSoftInput so that a keyboard is
            // never brought up.
            final ImeAdapter imeAdapter = ImeAdapter.fromWebContents(mWebContentsRef.get());
            mInputMethodManagerWrapper = TestInputMethodManagerWrapper.create(imeAdapter);
            imeAdapter.setInputMethodManagerWrapper(mInputMethodManagerWrapper);
        });
        DOMUtils.waitForNonZeroNodeBounds(mWebContentsRef.get(), "password");
    }

    public void clear() {
        ChromeWindow.resetKeyboardVisibilityDelegateFactory();
    }

    public void waitForKeyboard() {
        CriteriaHelper.pollUiThread(() -> {
            Activity activity = mActivityTestRule.getActivity();
            return getKeyboard().isAndroidSoftKeyboardShowing(activity, activity.getCurrentFocus());
        });
    }

    public void waitForKeyboardToDisappear() {
        CriteriaHelper.pollUiThread(() -> {
            Activity activity = mActivityTestRule.getActivity();
            return !getKeyboard().isAndroidSoftKeyboardShowing(
                    activity, activity.getCurrentFocus());
        });
    }

    public void clickPasswordField() throws TimeoutException, InterruptedException {
        DOMUtils.clickNode(mWebContentsRef.get(), "password");
        requestShowKeyboardAccessory();
        getKeyboard().showKeyboard(mActivityTestRule.getActivity().getCurrentFocus());
    }

    public void clickEmailField(boolean forceAccessory)
            throws TimeoutException, InterruptedException {
        DOMUtils.clickNode(mWebContentsRef.get(), "email");
        if (forceAccessory) {
            requestShowKeyboardAccessory();
        } else {
            requestHideKeyboardAccessory();
        }
        getKeyboard().showKeyboard(mActivityTestRule.getActivity().getCurrentFocus());
    }

    public DropdownPopupWindowInterface waitForAutofillPopup(String filterInput)
            throws InterruptedException, ExecutionException, TimeoutException {
        final WebContents webContents = mActivityTestRule.getActivity().getCurrentWebContents();
        final ViewGroup view = webContents.getViewAndroidDelegate().getContainerView();

        // Wait for InputConnection to be ready and fill the filterInput. Then wait for the anchor.
        CriteriaHelper.pollUiThread(
                Criteria.equals(1, () -> mInputMethodManagerWrapper.getShowSoftInputCounter()));
        ThreadUtils.runOnUiThreadBlocking(() -> {
            ImeAdapter.fromWebContents(webContents).setComposingTextForTest(filterInput, 4);
        });
        CriteriaHelper.pollUiThread(new Criteria("Autofill Popup anchor view was never added.") {
            @Override
            public boolean isSatisfied() {
                return view.findViewById(R.id.dropdown_popup_window) != null;
            }
        });
        View anchorView = view.findViewById(R.id.dropdown_popup_window);

        Assert.assertTrue(anchorView.getTag() instanceof DropdownPopupWindowInterface);
        final DropdownPopupWindowInterface popup =
                (DropdownPopupWindowInterface) anchorView.getTag();
        CriteriaHelper.pollUiThread(new Criteria("Autofill Popup anchor view was never added.") {
            @Override
            public boolean isSatisfied() {
                // Wait until the popup is showing and onLayout() has happened.
                return popup.isShowing() && popup.getListView() != null
                        && popup.getListView().getHeight() != 0;
            }
        });

        return popup;
    }

    /**
     * Although the submit button has no effect, it takes the focus from the input field and should
     * hide the keyboard.
     */
    public void clickSubmit() throws TimeoutException, InterruptedException {
        DOMUtils.clickNode(mWebContentsRef.get(), "submit");
        getKeyboard().hideAndroidSoftKeyboard(null);
    }

    /**
     * Creates and adds a password tab to keyboard accessory and sheet.
     */
    public void createTestTab() {
        KeyboardAccessoryData.Provider<KeyboardAccessoryData.Item> provider =
                new KeyboardAccessoryData.PropertyProvider<>();
        mActivityTestRule.getActivity().getManualFillingController().registerPasswordProvider(
                provider);
        provider.notifyObservers(new KeyboardAccessoryData.Item[] {
                KeyboardAccessoryData.Item.createSuggestion("TestName", "", false, null, null),
                KeyboardAccessoryData.Item.createSuggestion(
                        "TestPassword", "", false, (item) -> {}, null)});
    }

    /**
     * Use in a |onView().perform| action to select the tab at |tabIndex| for the found tab layout.
     * @param tabIndex The index to be selected.
     * @return The action executed by |perform|.
     */
    static public ViewAction selectTabAtPosition(int tabIndex) {
        return new ViewAction() {
            @Override
            public Matcher<View> getConstraints() {
                return allOf(isDisplayed(), isAssignableFrom(TabLayout.class));
            }

            @Override
            public String getDescription() {
                return "with tab at index " + tabIndex;
            }

            @Override
            public void perform(UiController uiController, View view) {
                TabLayout tabLayout = (TabLayout) view;
                if (tabLayout.getTabAt(tabIndex) == null) {
                    throw new PerformException.Builder()
                            .withCause(new Throwable("No tab at index " + tabIndex))
                            .build();
                }
                ThreadUtils.runOnUiThread(() -> tabLayout.getTabAt(tabIndex).select());
            }
        };
    }

    /**
     * Use like {@link android.support.test.espresso.Espresso#onView}. It waits for a view matching
     * the given |matcher| to be displayed and allows to chain checks/performs on the result.
     * @param matcher The matcher matching exactly the view that is expected to be displayed.
     * @return An interaction on the view matching |matcher.
     */
    public static ViewInteraction whenDisplayed(Matcher<View> matcher) {
        onView(isRoot()).check((r, e) -> waitForView((ViewGroup) r, allOf(matcher, isDisplayed())));
        return onView(matcher);
    }

    public void waitToBeHidden(Matcher<View> matcher) {
        onView(isRoot()).check((r, e) -> {
            waitForView((ViewGroup) r, matcher, VIEW_INVISIBLE | VIEW_NULL | VIEW_GONE);
        });
    }

    /**
     * In order to make sure the keyboard accessory is only shown on appropriate fields, a request
     * to show is usually sent from the native side. This method simulates that request.
     */
    private void requestShowKeyboardAccessory() {
        ThreadUtils.runOnUiThreadBlocking(() -> {
            mActivityTestRule.getActivity()
                    .getManualFillingController()
                    .getMediatorForTesting()
                    .showWhenKeyboardIsVisible();
        });
    }

    /**
     * In order to make sure the keyboard accessory is only shown on appropriate fields, a request
     * from the native side can request to hide it. This method simulates that request.
     */
    private void requestHideKeyboardAccessory() {
        ThreadUtils.runOnUiThreadBlocking(() -> {
            mActivityTestRule.getActivity()
                    .getManualFillingController()
                    .getMediatorForTesting()
                    .hide();
        });
    }
}
