// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.keyboard_accessory;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.matcher.ViewMatchers.isAssignableFrom;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;

import static org.hamcrest.core.AllOf.allOf;

import static org.chromium.autofill.mojom.FocusedFieldType.FILLABLE_NON_SEARCH_FIELD;
import static org.chromium.base.test.util.CriteriaHelper.pollInstrumentationThread;
import static org.chromium.base.test.util.CriteriaHelper.pollUiThread;
import static org.chromium.chrome.browser.keyboard_accessory.bar_component.KeyboardAccessoryTestHelper.accessoryStartedHiding;
import static org.chromium.chrome.browser.keyboard_accessory.bar_component.KeyboardAccessoryTestHelper.accessoryStartedShowing;
import static org.chromium.chrome.browser.keyboard_accessory.bar_component.KeyboardAccessoryTestHelper.accessoryViewFullyHidden;
import static org.chromium.chrome.browser.keyboard_accessory.bar_component.KeyboardAccessoryTestHelper.accessoryViewFullyShown;
import static org.chromium.ui.base.LocalizationUtils.setRtlForTesting;
import static org.chromium.ui.test.util.ViewUtils.VIEW_GONE;
import static org.chromium.ui.test.util.ViewUtils.VIEW_INVISIBLE;
import static org.chromium.ui.test.util.ViewUtils.VIEW_NULL;
import static org.chromium.ui.test.util.ViewUtils.onViewWaiting;
import static org.chromium.ui.test.util.ViewUtils.waitForView;

import android.app.Activity;
import android.text.method.PasswordTransformationMethod;
import android.view.View;
import android.view.ViewGroup;
import android.widget.TextView;

import androidx.annotation.StringRes;
import androidx.recyclerview.widget.RecyclerView;
import androidx.test.espresso.PerformException;
import androidx.test.espresso.UiController;
import androidx.test.espresso.ViewAction;
import androidx.test.espresso.ViewInteraction;
import androidx.test.espresso.matcher.BoundedMatcher;
import androidx.test.platform.app.InstrumentationRegistry;

import org.hamcrest.Description;
import org.hamcrest.Matcher;
import org.hamcrest.Matchers;
import org.junit.Assert;

import org.chromium.base.ThreadUtils;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.chrome.browser.ChromeKeyboardVisibilityDelegate;
import org.chromium.chrome.browser.ChromeWindow;
import org.chromium.chrome.browser.app.ChromeActivity;
import org.chromium.chrome.browser.autofill.AutofillTestHelper;
import org.chromium.chrome.browser.keyboard_accessory.bar_component.KeyboardAccessoryCoordinator;
import org.chromium.chrome.browser.keyboard_accessory.button_group_component.KeyboardAccessoryButtonGroupView;
import org.chromium.chrome.browser.keyboard_accessory.data.KeyboardAccessoryData;
import org.chromium.chrome.browser.keyboard_accessory.data.KeyboardAccessoryData.AccessorySheetData;
import org.chromium.chrome.browser.keyboard_accessory.data.PropertyProvider;
import org.chromium.chrome.browser.keyboard_accessory.sheet_tabs.AddressAccessorySheetCoordinator;
import org.chromium.chrome.browser.keyboard_accessory.sheet_tabs.CreditCardAccessorySheetCoordinator;
import org.chromium.chrome.browser.keyboard_accessory.sheet_tabs.PasswordAccessorySheetCoordinator;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.components.autofill.AutofillProfile;
import org.chromium.content_public.browser.ImeAdapter;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.test.util.DOMUtils;
import org.chromium.content_public.browser.test.util.TestInputMethodManagerWrapper;
import org.chromium.net.test.EmbeddedTestServer;
import org.chromium.net.test.ServerCertificate;
import org.chromium.ui.DropdownPopupWindowInterface;
import org.chromium.ui.test.util.ViewUtils;
import org.chromium.ui.widget.ChromeImageButton;

import java.util.concurrent.TimeoutException;
import java.util.concurrent.atomic.AtomicReference;

/**
 * Helpers in this class simplify interactions with the Keyboard Accessory and the sheet below it.
 */
public class ManualFillingTestHelper {
    private static final String PASSWORD_NODE_ID = "password_field";
    private static final String USERNAME_NODE_ID = "username_field";
    private static final String SUBMIT_NODE_ID = "input_submit_button";
    private static final String NO_COMPLETION_FIELD_ID = "field_without_completion";

    private final ChromeTabbedActivityTestRule mActivityTestRule;
    private final AtomicReference<WebContents> mWebContentsRef = new AtomicReference<>();
    private TestInputMethodManagerWrapper mInputMethodManagerWrapper;

    private EmbeddedTestServer mEmbeddedTestServer;

    private RecyclerView mKeyboardAccessoryBarItems;

    public FakeKeyboard getKeyboard() {
        return (FakeKeyboard) mActivityTestRule.getKeyboardDelegate();
    }

    public ManualFillingTestHelper(ChromeTabbedActivityTestRule activityTestRule) {
        mActivityTestRule = activityTestRule;
    }

    public EmbeddedTestServer getOrCreateTestServer() {
        if (mEmbeddedTestServer == null) {
            mEmbeddedTestServer =
                    EmbeddedTestServer.createAndStartHTTPSServer(
                            InstrumentationRegistry.getInstrumentation().getContext(),
                            ServerCertificate.CERT_OK);
        }
        return mEmbeddedTestServer;
    }

    public void loadTestPage(boolean isRtl) {
        loadTestPage("/chrome/test/data/password/password_form.html", isRtl);
    }

    public void loadTestPage(String url, boolean isRtl) {
        loadTestPage(url, isRtl, false, FakeKeyboard::new);
    }

    public void loadTestPage(
            String url,
            boolean isRtl,
            boolean waitForNode,
            ChromeWindow.KeyboardVisibilityDelegateFactory keyboardDelegate) {
        getOrCreateTestServer();
        ChromeWindow.setKeyboardVisibilityDelegateFactory(keyboardDelegate);
        if (mActivityTestRule.getActivity() == null) {
            mActivityTestRule.startMainActivityWithURL(mEmbeddedTestServer.getURL(url));
        } else {
            mActivityTestRule.loadUrl(mEmbeddedTestServer.getURL(url));
        }
        setRtlForTesting(isRtl);
        updateWebContentsDependentState();
        cacheCredentials("mpark@gmail.com", "S3cr3t"); // Providing suggestions ensures visibility.
        if (waitForNode) DOMUtils.waitForNonZeroNodeBounds(mWebContentsRef.get(), PASSWORD_NODE_ID);
    }

    public void loadUrl(String url) {
        mActivityTestRule.loadUrl(mActivityTestRule.getTestServer().getURL(url));
        mWebContentsRef.set(mActivityTestRule.getWebContents());
    }

    public void updateWebContentsDependentState() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    ChromeActivity activity = mActivityTestRule.getActivity();
                    mWebContentsRef.set(activity.getActivityTab().getWebContents());
                    // The TestInputMethodManagerWrapper intercepts showSoftInput so that a keyboard
                    // is never brought up.
                    final ImeAdapter imeAdapter = ImeAdapter.fromWebContents(mWebContentsRef.get());
                    mInputMethodManagerWrapper = TestInputMethodManagerWrapper.create(imeAdapter);
                    imeAdapter.setInputMethodManagerWrapper(mInputMethodManagerWrapper);
                });
    }

    public void clear() {
        ChromeWindow.resetKeyboardVisibilityDelegateFactory();
    }

    // --------------------------------------
    // Helpers interacting with the web page.
    // --------------------------------------

    public WebContents getWebContents() {
        return mWebContentsRef.get();
    }

    ManualFillingCoordinator getManualFillingCoordinator() {
        return (ManualFillingCoordinator)
                mActivityTestRule.getActivity().getManualFillingComponent();
    }

    public RecyclerView getAccessoryBarView() {
        final ViewGroup keyboardAccessory =
                ThreadUtils.runOnUiThreadBlocking(
                        () ->
                                mActivityTestRule
                                        .getActivity()
                                        .findViewById(R.id.keyboard_accessory));
        assert keyboardAccessory != null;
        return keyboardAccessory.findViewById(R.id.bar_items_view);
    }

    public void focusPasswordField() throws TimeoutException {
        focusPasswordField(true);
    }

    public void focusPasswordField(boolean useFakeKeyboard) throws TimeoutException {
        DOMUtils.focusNode(mActivityTestRule.getWebContents(), PASSWORD_NODE_ID);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mActivityTestRule.getWebContents().scrollFocusedEditableNodeIntoView();
                });

        ChromeKeyboardVisibilityDelegate keyboard;
        if (useFakeKeyboard) {
            keyboard = getKeyboard();
        } else {
            keyboard = (ChromeKeyboardVisibilityDelegate) mActivityTestRule.getKeyboardDelegate();
        }
        keyboard.showKeyboard(mActivityTestRule.getActivity().getCurrentFocus());
    }

    public String getPasswordText() throws TimeoutException {
        return DOMUtils.getNodeValue(mWebContentsRef.get(), PASSWORD_NODE_ID);
    }

    public String getFieldText(String nodeId) throws TimeoutException {
        return DOMUtils.getNodeValue(mWebContentsRef.get(), nodeId);
    }

    public void clickEmailField(boolean forceAccessory) throws TimeoutException {
        // TODO(fhorschig): This should be |focusNode|. Change with autofill popup deprecation.
        DOMUtils.clickNode(mWebContentsRef.get(), USERNAME_NODE_ID);
        if (forceAccessory) {
            ThreadUtils.runOnUiThreadBlocking(
                    () -> {
                        getManualFillingCoordinator().getMediatorForTesting().show(true);
                    });
        }
        getKeyboard().showKeyboard(mActivityTestRule.getActivity().getCurrentFocus());
    }

    public void clickFieldWithoutCompletion() throws TimeoutException {
        DOMUtils.waitForNonZeroNodeBounds(mWebContentsRef.get(), PASSWORD_NODE_ID);
        DOMUtils.focusNode(mWebContentsRef.get(), NO_COMPLETION_FIELD_ID);
        DOMUtils.clickNode(mWebContentsRef.get(), NO_COMPLETION_FIELD_ID);
    }

    public void clickNodeAndShowKeyboard(String node, long focusedFieldId) throws TimeoutException {
        clickNodeAndShowKeyboard(node, focusedFieldId, FILLABLE_NON_SEARCH_FIELD);
    }

    public void clickNodeAndShowKeyboard(String node, long focusedFieldId, int focusedFieldType)
            throws TimeoutException {
        clickNode(node, focusedFieldId, focusedFieldType);
        getKeyboard().showKeyboard(mActivityTestRule.getActivity().getCurrentFocus());
    }

    public void clickNode(String node, long focusedFieldId, int focusedFieldType)
            throws TimeoutException {
        DOMUtils.clickNode(mWebContentsRef.get(), node);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    ManualFillingComponentBridge.notifyFocusedFieldType(
                            mActivityTestRule.getWebContents(), focusedFieldId, focusedFieldType);
                });
    }

    /**
     * Although the submit button has no effect, it takes the focus from the input field and should
     * hide the keyboard.
     */
    public void clickSubmit() throws TimeoutException {
        DOMUtils.clickNode(mWebContentsRef.get(), SUBMIT_NODE_ID);
        getKeyboard().hideSoftKeyboardOnly(null);
    }

    // ---------------------------------
    // Helpers to wait for accessory UI.
    // ---------------------------------

    public void waitForKeyboardToDisappear() {
        pollUiThread(
                () -> {
                    Activity activity = mActivityTestRule.getActivity();
                    return !getKeyboard()
                            .isSoftKeyboardShowing(activity, activity.getCurrentFocus());
                });
    }

    public void waitForKeyboardAccessoryToDisappear() {
        pollInstrumentationThread(() -> accessoryStartedHiding(getKeyboardAccessoryBar()));
        pollUiThread(() -> accessoryViewFullyHidden(mActivityTestRule.getActivity()));
    }

    public void waitForKeyboardAccessoryToBeShown() {
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();
        waitForKeyboardAccessoryToBeShown(false);
    }

    public void waitForKeyboardToShow() {
        CriteriaHelper.pollUiThread(
                () -> {
                    boolean isKeyboardShowing =
                            mActivityTestRule
                                    .getKeyboardDelegate()
                                    .isKeyboardShowing(
                                            mActivityTestRule.getActivity(),
                                            mActivityTestRule.getActivity().getTabsView());
                    Criteria.checkThat(isKeyboardShowing, Matchers.is(true));
                });
    }

    public void waitForKeyboardAccessoryToBeShown(boolean waitForSuggestionsToLoad) {
        pollInstrumentationThread(() -> accessoryStartedShowing(getKeyboardAccessoryBar()));
        pollUiThread(() -> accessoryViewFullyShown(mActivityTestRule.getActivity()));
        if (waitForSuggestionsToLoad) {
            pollUiThread(
                    () -> {
                        return getFirstAccessorySuggestion() != null;
                    },
                    "Waited for suggestions that never appeared.");
        }
    }

    public DropdownPopupWindowInterface waitForAutofillPopup(String filterInput) {
        final WebContents webContents = mActivityTestRule.getActivity().getCurrentWebContents();
        final View view = webContents.getViewAndroidDelegate().getContainerView();

        // Wait for InputConnection to be ready and fill the filterInput. Then wait for the anchor.
        pollUiThread(
                () -> {
                    Criteria.checkThat(
                            mInputMethodManagerWrapper.getShowSoftInputCounter(), Matchers.is(1));
                });
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    ImeAdapter.fromWebContents(webContents).setComposingTextForTest(filterInput, 4);
                });
        pollUiThread(
                () -> {
                    Criteria.checkThat(
                            "Autofill Popup anchor view was never added.",
                            view.findViewById(R.id.dropdown_popup_window),
                            Matchers.notNullValue());
                });
        View anchorView = view.findViewById(R.id.dropdown_popup_window);

        Assert.assertTrue(anchorView.getTag() instanceof DropdownPopupWindowInterface);
        final DropdownPopupWindowInterface popup =
                (DropdownPopupWindowInterface) anchorView.getTag();
        pollUiThread(
                () -> {
                    Criteria.checkThat(popup.isShowing(), Matchers.is(true));
                    Criteria.checkThat(popup.getListView(), Matchers.notNullValue());
                    Criteria.checkThat(popup.getListView().getHeight(), Matchers.not(0));
                });
        return popup;
    }

    public PasswordAccessorySheetCoordinator getOrCreatePasswordAccessorySheet() {
        return (PasswordAccessorySheetCoordinator)
                getManualFillingCoordinator()
                        .getMediatorForTesting()
                        .getOrCreateSheet(mWebContentsRef.get(), AccessoryTabType.PASSWORDS);
    }

    public AddressAccessorySheetCoordinator getOrCreateAddressAccessorySheet() {
        return (AddressAccessorySheetCoordinator)
                getManualFillingCoordinator()
                        .getMediatorForTesting()
                        .getOrCreateSheet(mWebContentsRef.get(), AccessoryTabType.ADDRESSES);
    }

    public CreditCardAccessorySheetCoordinator getOrCreateCreditCardAccessorySheet() {
        return (CreditCardAccessorySheetCoordinator)
                getManualFillingCoordinator()
                        .getMediatorForTesting()
                        .getOrCreateSheet(mWebContentsRef.get(), AccessoryTabType.CREDIT_CARDS);
    }

    private KeyboardAccessoryCoordinator getKeyboardAccessoryBar() {
        return getManualFillingCoordinator().getMediatorForTesting().getKeyboardAccessory();
    }

    private View getFirstAccessorySuggestion() {
        ViewGroup recyclerView = getAccessoryBarView();
        assert recyclerView != null;
        View view = recyclerView.getChildAt(0);
        return isAssignableFrom(KeyboardAccessoryButtonGroupView.class).matches(view) ? null : view;
    }

    // ----------------------------------
    // Helpers to set up the native side.
    // ----------------------------------

    /**
     * Calls cacheCredentials with two simple credentials.
     * @see ManualFillingTestHelper#cacheCredentials(String, String)
     */
    public void cacheTestCredentials() {
        cacheCredentials(
                new String[] {"mpark@gmail.com", "mayapark@googlemail.com"},
                new String[] {"TestPassword", "SomeReallyLongPassword"},
                false);
    }

    /**
     * @see ManualFillingTestHelper#cacheCredentials(String, String)
     * @param username A {@link String} to be used as display text for a username chip.
     * @param password A {@link String} to be used as display text for a password chip.
     */
    public void cacheCredentials(String username, String password) {
        cacheCredentials(new String[] {username}, new String[] {password}, false);
    }

    /**
     * Creates credential pairs from these strings and writes them into the cache of the native
     * controller. The controller will only refresh this cache on page load.
     *
     * @param usernames {@link String}s to be used as display text for username chips.
     * @param passwords {@link String}s to be used as display text for password chips.
     * @param originDenylisted boolean indicating whether password saving is disabled for the
     *     origin.
     */
    public void cacheCredentials(String[] usernames, String[] passwords, boolean originDenylisted) {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    ManualFillingComponentBridge.cachePasswordSheetData(
                            mActivityTestRule.getWebContents(),
                            usernames,
                            passwords,
                            originDenylisted);
                });
    }

    public static void createAutofillTestProfiles() throws TimeoutException {
        new AutofillTestHelper()
                .setProfile(
                        AutofillProfile.builder()
                                .setFullName("Johnathan Smithonian-Jackson")
                                .setCompanyName("Acme Inc")
                                .setStreetAddress("1 Main\nApt A")
                                .setRegion("CA")
                                .setLocality("San Francisco")
                                .setPostalCode("94102")
                                .setCountryCode("US")
                                .setPhoneNumber("(415) 888-9999")
                                .setEmailAddress("john.sj@acme-mail.inc")
                                .setLanguageCode("en")
                                .build());
        new AutofillTestHelper()
                .setProfile(
                        AutofillProfile.builder()
                                .setFullName("Jane Erika Donovanova")
                                .setCompanyName("Acme Inc")
                                .setStreetAddress("1 Main\nApt A")
                                .setRegion("CA")
                                .setLocality("San Francisco")
                                .setPostalCode("94102")
                                .setCountryCode("US")
                                .setPhoneNumber("(415) 999-0000")
                                .setEmailAddress("donovanova.j@acme-mail.inc")
                                .setLanguageCode("en")
                                .build());
        new AutofillTestHelper()
                .setProfile(
                        AutofillProfile.builder()
                                .setFullName("Marcus McSpartangregor")
                                .setCompanyName("Acme Inc")
                                .setStreetAddress("1 Main\nApt A")
                                .setRegion("CA")
                                .setLocality("San Francisco")
                                .setPostalCode("94102")
                                .setCountryCode("US")
                                .setPhoneNumber("(415) 999-0000")
                                .setEmailAddress("marc@acme-mail.inc")
                                .setLanguageCode("en")
                                .build());
    }

    public static void disableServerPredictions() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    ManualFillingComponentBridge.disableServerPredictionsForTesting();
                });
    }

    // --------------------------------------------------
    // Generic helpers to match, check or wait for views.
    // TODO(fhorschig): Consider Moving to ViewUtils.
    // --------------------------------------------------

    /**
     * Use in a |onView().perform| action to select the tab at |tabIndex| for the found tab layout.
     * @param tabIndex The index to be selected.
     * @return The action executed by |perform|.
     */
    public static ViewAction selectTabAtPosition(int tabIndex) {
        return new ViewAction() {
            @Override
            public Matcher<View> getConstraints() {
                return allOf(
                        isDisplayed(), isAssignableFrom(KeyboardAccessoryButtonGroupView.class));
            }

            @Override
            public String getDescription() {
                return "with tab at index " + tabIndex;
            }

            @Override
            public void perform(UiController uiController, View view) {
                KeyboardAccessoryButtonGroupView buttonGroupView =
                        (KeyboardAccessoryButtonGroupView) view;
                if (tabIndex >= buttonGroupView.getButtons().size()) {
                    throw new PerformException.Builder()
                            .withCause(new Throwable("No button at index " + tabIndex))
                            .build();
                }
                PostTask.runOrPostTask(
                        TaskTraits.UI_DEFAULT,
                        () -> buttonGroupView.getButtons().get(tabIndex).performClick());
            }
        };
    }

    /**
     * Use in a |onView().perform| action to select the tab at |tabIndex| for the found tab layout.
     * @param tabIndex The index to be selected.
     * @return The action executed by |perform|.
     */
    public static ViewAction selectTabWithDescription(@StringRes int descriptionResId) {
        return new ViewAction() {
            @Override
            public Matcher<View> getConstraints() {
                return allOf(
                        isDisplayed(), isAssignableFrom(KeyboardAccessoryButtonGroupView.class));
            }

            @Override
            public String getDescription() {
                return "with tab with matching description.";
            }

            @Override
            public void perform(UiController uiController, View view) {
                String descriptionToMatch = view.getContext().getString(descriptionResId);
                KeyboardAccessoryButtonGroupView buttonGroupView =
                        (KeyboardAccessoryButtonGroupView) view;
                for (int buttonIndex = 0;
                        buttonIndex < buttonGroupView.getButtons().size();
                        buttonIndex++) {
                    final ChromeImageButton button = buttonGroupView.getButtons().get(buttonIndex);
                    if (descriptionToMatch.equals(button.getContentDescription())) {
                        PostTask.runOrPostTask(TaskTraits.UI_DEFAULT, button::performClick);
                        return;
                    }
                }
                throw new PerformException.Builder()
                        .withCause(
                                new Throwable("No button with description: " + descriptionToMatch))
                        .build();
            }
        };
    }

    /**
     * Use in a |onView().perform| action to scroll to the end of a {@link RecyclerView}.
     * @return The action executed by |perform|.
     */
    public static ViewAction scrollToLastElement() {
        return new ViewAction() {
            @Override
            public Matcher<View> getConstraints() {
                return allOf(isDisplayed(), isAssignableFrom(RecyclerView.class));
            }

            @Override
            public String getDescription() {
                return "scrolling to end of view";
            }

            @Override
            public void perform(UiController uiController, View view) {
                RecyclerView recyclerView = (RecyclerView) view;
                int itemCount = recyclerView.getAdapter().getItemCount();
                if (itemCount <= 0) {
                    throw new PerformException.Builder()
                            .withCause(new Throwable("RecyclerView has no items."))
                            .build();
                }
                recyclerView.scrollToPosition(itemCount - 1);
            }
        };
    }

    /**
     * Matches any {@link TextView} which applies a {@link PasswordTransformationMethod}.
     * @return The matcher checking the transformation method.
     */
    public static Matcher<View> isTransformed() {
        return new BoundedMatcher<View, TextView>(TextView.class) {
            @Override
            public boolean matchesSafely(TextView textView) {
                return textView.getTransformationMethod() instanceof PasswordTransformationMethod;
            }

            @Override
            public void describeTo(Description description) {
                description.appendText("is a transformed password.");
            }
        };
    }

    /**
     * Use like {@link androidx.test.espresso.Espresso#onView}. It waits for a view matching
     * the given |matcher| to be displayed and allows to chain checks/performs on the result.
     * @param matcher The matcher matching exactly the view that is expected to be displayed.
     * @return An interaction on the view matching |matcher|.
     */
    public static ViewInteraction whenDisplayed(Matcher<View> matcher) {
        return onViewWaiting(allOf(matcher, isDisplayed()));
    }

    public ViewInteraction waitForViewOnRoot(View root, Matcher<View> matcher) {
        waitForView((ViewGroup) root, allOf(matcher, isDisplayed()));
        return onView(matcher);
    }

    public ViewInteraction waitForViewOnActivityRoot(Matcher<View> matcher) {
        return waitForViewOnRoot(
                mActivityTestRule.getActivity().findViewById(android.R.id.content).getRootView(),
                matcher);
    }

    public static void waitToBeHidden(Matcher<View> matcher) {
        ViewUtils.waitForViewCheckingState(matcher, VIEW_INVISIBLE | VIEW_NULL | VIEW_GONE);
    }

    public String getAttribute(String node, String attribute)
            throws InterruptedException, TimeoutException {
        return DOMUtils.getNodeAttribute(attribute, mWebContentsRef.get(), node, String.class);
    }

    // --------------------------------------------
    // Helpers that force override the native side.
    // TODO(fhorschig): Search alternatives.
    // --------------------------------------------

    public void addGenerationButton() {
        PropertyProvider<KeyboardAccessoryData.Action[]> generationActionProvider =
                new PropertyProvider<>(AccessoryAction.GENERATE_PASSWORD_AUTOMATIC);
        getManualFillingCoordinator()
                .registerActionProvider(mWebContentsRef.get(), generationActionProvider);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    generationActionProvider.notifyObservers(
                            new KeyboardAccessoryData.Action[] {
                                new KeyboardAccessoryData.Action(
                                        AccessoryAction.GENERATE_PASSWORD_AUTOMATIC, result -> {})
                            });
                });
    }

    public void signalAutoGenerationStatus(boolean available) {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    ManualFillingComponentBridge.signalAutoGenerationStatus(
                            mActivityTestRule.getWebContents(), available);
                });
    }

    public void registerSheetDataProvider(@AccessoryTabType int tabType) {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    PropertyProvider<AccessorySheetData> sheetDataProvider =
                            new PropertyProvider<>();
                    getManualFillingCoordinator()
                            .registerSheetDataProvider(
                                    mWebContentsRef.get(), tabType, sheetDataProvider);
                });
    }
}
