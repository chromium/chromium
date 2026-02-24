// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.permissions;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.assertion.ViewAssertions.doesNotExist;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.withContentDescription;
import static androidx.test.espresso.matcher.ViewMatchers.withId;
import static androidx.test.espresso.matcher.ViewMatchers.withTagValue;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.hamcrest.CoreMatchers.allOf;
import static org.hamcrest.CoreMatchers.is;

import static org.chromium.ui.test.util.ViewUtils.onViewWaiting;

import androidx.annotation.IdRes;
import androidx.annotation.IntDef;

import org.hamcrest.Matchers;
import org.junit.Assert;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.CriteriaNotSatisfiedException;
import org.chromium.chrome.browser.app.ChromeActivity;
import org.chromium.chrome.browser.omnibox.LocationBarCoordinator;
import org.chromium.chrome.browser.profiles.ProfileManager;
import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.R;
import org.chromium.chrome.test.util.InfoBarTestAnimationListener;
import org.chromium.chrome.test.util.InfoBarUtil;
import org.chromium.components.browser_ui.modaldialog.ModalDialogView;
import org.chromium.components.browser_ui.site_settings.GeolocationSetting;
import org.chromium.components.browser_ui.site_settings.WebsitePreferenceBridge;
import org.chromium.components.content_settings.ContentSetting;
import org.chromium.components.content_settings.ContentSettingsType;
import org.chromium.components.infobars.InfoBar;
import org.chromium.components.messages.MessagesTestHelper;
import org.chromium.components.permissions.PermissionDialogController;
import org.chromium.components.user_prefs.UserPrefs;
import org.chromium.content_public.browser.test.util.TouchCommon;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogProperties;
import org.chromium.ui.test.util.ViewUtils;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * TestRule for permissions UI testing on Android.
 *
 * <p>This class allows for easy testing of permissions infobar and dialog prompts. Writing a test
 * simply requires a HTML file containing JavaScript methods which trigger a permission prompt. The
 * methods should update the page's title with <prefix>: <count>, where <count> is the number of
 * updates expected (usually 1, although some APIs like Geolocation's watchPosition may trigger
 * callbacks repeatedly).
 *
 * <p>Subclasses may then call runAllowTest to start a test server, navigate to the provided HTML
 * page, and run the JavaScript method. The permission will be granted, and the test will verify
 * that the page title is updated as expected.
 *
 * <p>runAllowTest has several parameters to specify the conditions of the test, including whether a
 * persistence toggle is expected, whether it should be explicitly toggled, whether to trigger the
 * JS call with a gesture, and whether an infobar or a dialog is expected.
 */
public class PermissionTestRule extends ChromeTabbedActivityTestRule {
    /** Content description for the "allowed" notification icon/status. */
    public static final int NOTIFICATIONS_ALLOWED_ID =
            R.string.permissions_notification_allowed_confirmation_screenreader_announcement;

    /** Content description for the "blocked" notification icon/status. */
    public static final int NOTIFICATIONS_NOT_ALLOWED_ID =
            R.string.permissions_notification_not_allowed_confirmation_screenreader_announcement;

    /** Text ID for when a permission is blocked at the Android OS level. */
    public static final int ANDROID_SETTINGS_BLOCKED_ID =
            R.string.page_info_android_permission_blocked;

    /** Title ID for the notifications permission setting. */
    public static final int NOTIFICATIONS_TITLE_ID = R.string.push_notifications_permission_title;

    /** Summary text ID for allowed permissions in Page Info. */
    public static final int PERMISSIONS_SUMMARY_ALLOWED_ID =
            R.string.page_info_permissions_summary_1_allowed;

    /** Summary text ID for blocked permissions in Page Info. */
    public static final int PERMISSIONS_SUMMARY_BLOCKED_ID =
            R.string.page_info_permissions_summary_1_blocked;

    /** Warning text ID shown in Page Info when OS level settings conflict. */
    public static final int PERMISSIONS_OS_WARNING_ID = R.string.page_info_permissions_os_warning;

    /** View ID for the "Allow" button in the Clapper Loud UI message. */
    public static final int CLAPPER_LOUD_ALLOW_BUTTON_ID = R.id.message_primary_button;

    /** View ID for the gear (settings) button in the Clapper Loud UI message. */
    public static final int CLAPPER_LOUD_GEAR_BUTTON_ID = R.id.message_secondary_button;

    /** Text for the "Manage" button in Clapper Loud UI. */
    public static final String CLAPPER_LOUD_MANAGE_BUTTON_TEXT = "Manage";

    /** Text for the "Reset" button in the Page Info reset dialog. */
    public static final String CLAPPER_PAGE_INFO_RESET_DIALOG_BUTTON_TEXT = "Reset";

    /** Text for the "Cancel" button in the Page Info reset dialog. */
    public static final String CLAPPER_PAGE_INFO_CANCEL_DIALOG_BUTTON_TEXT = "Cancel";

    /** Text ID for the subscribe button in Page Info. */
    public static final int CLAPPER_PAGE_INFO_SUBSCRIBE_BUTTON_TEXT_ID =
            R.string.notifications_permission_subscribe;

    /** Text ID for the reset button in Page Info. */
    public static final int CLAPPER_PAGE_INFO_RESET_BUTTON_TEXT_ID =
            R.string.page_info_permissions_reset;

    /** Text ID for the "Don't Allow" button. */
    public static final int CLAPPER_LOUD_DONT_ALLOW_BUTTON_TEXT_ID = R.string.permission_dont_allow;

    private InfoBarTestAnimationListener mListener;

    /**
     * Enumeration of the possible decisions that can be made by the user on a permission prompt.
     * These values are used to drive the test harness to simulate user interaction (e.g. clicking a
     * specific button).
     *
     * <p>These values map to methods on the {@code PermissionPrompt::Delegate} interface in C++
     * (defined in //components/permissions/permission_prompt.h), such as {@code Accept()}, {@code
     * AcceptThisTime()}, and {@code Deny()}.
     */
    @IntDef({
        PromptDecision.ALLOW,
        PromptDecision.ALLOW_ONCE,
        PromptDecision.DENY,
        PromptDecision.NONE
    })
    @Retention(RetentionPolicy.SOURCE)
    public @interface PromptDecision {
        int ALLOW = 0;
        int ALLOW_ONCE = 1;
        int DENY = 2;
        int NONE = 3;
    }

    /**
     * Enumeration of the possible actions that can be taken on a permission prompt. These values
     * correspond to the values used in UMA histograms.
     *
     * <p>These values correspond to the {@code PermissionAction} enum in C++ (defined in
     * //components/permissions/permission_util.h).
     */
    @IntDef({
        PromptAction.GRANTED,
        PromptAction.DENIED,
        PromptAction.DISMISSED,
        PromptAction.IGNORED
    })
    @Retention(RetentionPolicy.SOURCE)
    public @interface PromptAction {
        int GRANTED = 0;
        int DENIED = 1;
        int DISMISSED = 2;
        int IGNORED = 3;
    }

    /**
     * Waits till a JavaScript callback which updates the page title is called the specified number
     * of times. The page title is expected to be of the form <prefix>: <count>.
     */
    public static class PermissionUpdateWaiter extends EmptyTabObserver {
        private final CallbackHelper mCallbackHelper;
        private final String mPrefix;
        private String mExpectedTitle;
        private final ChromeActivity mActivity;

        public PermissionUpdateWaiter(String prefix, ChromeActivity activity) {
            mCallbackHelper = new CallbackHelper();
            mPrefix = prefix;
            mActivity = activity;
        }

        @Override
        public void onTitleUpdated(Tab tab) {
            if (getTitle().equals(mExpectedTitle)) {
                mCallbackHelper.notifyCalled();
            }
        }

        /**
         * Wait for the page title to reach the expected number of updates. The page is expected to
         * update the title like so: `prefix` + numUpdates. In essence this waits for the page to
         * update the title to match, and does not actually count page title updates.
         *
         * @param numUpdates The number that should be after the prefix for the wait to be over. `0`
         *     to only wait for the prefix.
         */
        public void waitForNumUpdates(int numUpdates) throws Exception {
            int callbackCountBefore = mCallbackHelper.getCallCount();

            // Update might have already happened, check before waiting for title udpdates.
            mExpectedTitle = mPrefix;
            if (numUpdates != 0) mExpectedTitle += numUpdates;
            if (getTitle().equals(mExpectedTitle)) {
                return;
            }

            mCallbackHelper.waitForCallback(callbackCountBefore);
        }

        private String getTitle() {
            return ThreadUtils.runOnUiThreadBlocking(() -> mActivity.getActivityTab().getTitle());
        }
    }

    public PermissionTestRule() {
        this(false);
    }

    public PermissionTestRule(boolean useHttpsServer) {
        getEmbeddedTestServerRule().setServerUsesHttps(useHttpsServer);
    }

    @Override
    protected void before() throws Throwable {
        super.before();
        ModalDialogView.disableButtonTapProtectionForTesting();
    }

    /** Starts an activity and listens for info-bars appearing/disappearing. */
    public void setUpActivity() throws InterruptedException {
        startMainActivityOnBlankPage();
        mListener = new InfoBarTestAnimationListener();
        ThreadUtils.runOnUiThreadBlocking(
                () -> getInfoBarContainer().addAnimationListener(mListener));
    }

    /**
     * Navigates to a relative URL in relation to the embedded server host directly without going
     * through the UrlBar. This bypasses the page preloading mechanism of the UrlBar.
     *
     * @param relativeUrl The relative URL for which an absolute URL will be computed and loaded in
     *     the current tab.
     */
    public void setUpUrl(final String relativeUrl) {
        loadUrl(getURL(relativeUrl));
    }

    /**
     * Navigates to a relative URL in relation to the specified host directly without going through
     * the UrlBar. This bypasses the page preloading mechanism of the UrlBar.
     *
     * @param relativeUrl The relative URL for which an absolute URL will be computed and loaded in
     *     the current tab.
     * @param hostName The host name which should be used.
     */
    public void setupUrlWithHostName(String hostName, String relativeUrl) {
        loadUrl(getURLWithHostName(hostName, relativeUrl));
    }

    public String getURL(String url) {
        return getTestServer().getURL(url);
    }

    public String getOrigin() {
        return getTestServer().getURL("/");
    }

    public String getURLWithHostName(String hostName, String url) {
        return getTestServer().getURLWithHostName(hostName, url);
    }

    /**
     * Sets up the page and triggers a notification permission request.
     *
     * <p>This method loads the test page (either via HTTP or file URL) and injects a click handler
     * into the page's window object. This click handler is necessary because
     * `Notification.requestPermission()` requires a user gesture to show the prompt.
     *
     * @param url The relative URL to load.
     * @throws Exception If an error occurs during javascript execution.
     */
    public void setupPageAndTriggerNotificationPermissionRequest(String url) throws Exception {
        if (url.startsWith("http")) {
            loadUrl(url);
        } else {
            setUpUrl(url);
        }

        // Inject a click handler to satisfy the User Gesture requirement.
        runJavaScriptCodeInCurrentTab(
                "window.onclick = function() { if (window.functionToRun) {"
                        + " eval(window.functionToRun); } };");

        // Trigger notification permission with a gesture.
        runJavaScriptCodeInCurrentTabWithGesture("Notification.requestPermission()");
    }

    /** Waits for the Page Info UI to be opened. */
    public void waitForPageInfoOpen() {
        onViewWaiting(withId(R.id.page_info_url_wrapper));
    }

    /** Waits for the Page Info UI to be closed. */
    public void waitForPageInfoClose() {
        CriteriaHelper.pollInstrumentationThread(
                () -> {
                    try {
                        onView(withId(R.id.page_info_url_wrapper)).check(doesNotExist());
                    } catch (AssertionError e) {
                        throw new CriteriaNotSatisfiedException(e);
                    }
                });
    }

    /** Opens the Page Info UI by clicking on the location bar status icon. */
    public void openPageInfoFromStatusIcon() {
        onViewWaiting(withId(R.id.location_bar_status_icon)).perform(click());
        waitForPageInfoOpen();
    }

    /**
     * Verifies that the permissions row in Page Info displays the expected text.
     *
     * @param permissionNameId Resource ID for the permission name (e.g. "Notifications").
     * @param statusFormatId Resource ID for the status format string (e.g. "%s allowed").
     */
    public void verifyPageInfoPermissionsRow(int permissionNameId, int statusFormatId) {
        String permissionName = getActivity().getString(permissionNameId);
        String expectedText = getActivity().getString(statusFormatId, permissionName);
        onViewWaiting(withText(expectedText));
    }

    /**
     * Verifies that no permissions row exists for the given permission name in Page Info.
     *
     * @param permissionNameId Resource ID for the permission name.
     */
    public void verifyNoPageInfoPermissionsRow(int permissionNameId) {
        String permissionName = getActivity().getString(permissionNameId);
        onView(withText(permissionName)).check(doesNotExist());
    }

    /**
     * Waits for a view with the given content description to appear.
     *
     * @param contentDescriptionRes Resource ID for the content description.
     */
    public void waitForStatusIcon(int contentDescriptionRes) {
        onViewWaiting(withContentDescription(contentDescriptionRes));
    }

    /**
     * Waits for a view with the given content description to disappear.
     *
     * @param contentDescriptionRes Resource ID for the content description.
     */
    public void waitForStatusIconGone(int contentDescriptionRes) {
        CriteriaHelper.pollInstrumentationThread(
                () -> {
                    try {
                        onView(withContentDescription(contentDescriptionRes)).check(doesNotExist());
                    } catch (AssertionError e) {
                        throw new CriteriaNotSatisfiedException(e);
                    }
                });
    }

    /**
     * Resets notification settings for the test profile.
     *
     * @param enable_quiet_ui Whether to enable Quiet UI.
     */
    public void resetNotificationsSettingsForTest(boolean enableQuietUi) {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    WebsitePreferenceBridge.resetNotificationsSettingsForTest(
                            ProfileManager.getLastUsedRegularProfile());
                    UserPrefs.get(ProfileManager.getLastUsedRegularProfile())
                            .setBoolean(
                                    "profile.content_settings.enable_quiet_permission_ui.notifications",
                                    enableQuietUi);
                });
    }

    /**
     * Sets the permission setting for a specific origin.
     *
     * @param url The origin URL.
     * @param setting The ContentSetting to apply.
     * @param contentSettingsType The ContentSettingsType to apply.
     */
    public void setPermissionSettingForOrigin(
            String url,
            @ContentSetting int setting,
            @ContentSettingsType.EnumType int contentSettingsType) {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    WebsitePreferenceBridge.setPermissionSettingForOrigin(
                            /* browserContextHandle= */ ProfileManager.getLastUsedRegularProfile(),
                            contentSettingsType,
                            /* origin= */ url,
                            /* embedder= */ url,
                            setting);
                });
    }

    /**
     * Returns the permission setting for the given origin and content settings type.
     *
     * @param contentSettingsType The ContentSettingsType to check.
     * @param pageUrl The origin URL of the page.
     */
    public @ContentSetting int getPermissionSettingForOrigin(
            @ContentSettingsType.EnumType int contentSettingsType, String pageUrl) {
        String url = getURL(pageUrl);
        return ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    return WebsitePreferenceBridge.getPermissionSettingForOrigin(
                            /* browserContextHandle= */ ProfileManager.getLastUsedRegularProfile(),
                            /* contentSettingsType= */ contentSettingsType,
                            /* origin= */ url,
                            /* embedder= */ url);
                });
    }

    /**
     * Returns the geolocation permission setting for the given origin.
     *
     * @param pageUrl The origin URL of the page.
     */
    public GeolocationSetting getGeolocationSettingForOrigin(String pageUrl) {
        String url = getURL(pageUrl);
        return ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    return WebsitePreferenceBridge.getGeolocationSettingForOrigin(
                            /* browserContextHandle= */ ProfileManager.getLastUsedRegularProfile(),
                            /* contentSettingsType= */ ContentSettingsType.GEOLOCATION_WITH_OPTIONS,
                            /* origin= */ url,
                            /* embedder= */ url);
                });
    }

    /**
     * Checks the permission setting for the given origin and content settings type.
     *
     * @param contentSettingsType The ContentSettingsType to check.
     * @param expectedSetting The expected content setting.
     * @param pageUrl The origin URL of the page.
     */
    public void checkPermissionSettingForOrigin(
            @ContentSettingsType.EnumType int contentSettingsType,
            @ContentSetting int expectedSetting,
            String pageUrl) {
        String url = getURL(pageUrl);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    @ContentSetting
                    int currentSetting =
                            getPermissionSettingForOrigin(contentSettingsType, pageUrl);
                    Assert.assertEquals(expectedSetting, currentSetting);
                });
    }

    /**
     * Checks the geolocation permission setting for the given origin.
     *
     * @param usePrecise If true, use precise location, otherwise approximate.
     * @param expectedSetting The expected content setting.
     * @param pageUrl The origin URL of the page.
     */
    public void checkGeolocationSettingForOrigin(
            boolean usePrecise, @ContentSetting int expectedSetting, String pageUrl) {
        GeolocationSetting currentSetting = getGeolocationSettingForOrigin(pageUrl);
        Assert.assertNotNull("Geolocation setting is null", currentSetting);
        if (usePrecise) {
            Assert.assertEquals(
                    "Precise setting mismatch", expectedSetting, currentSetting.mPrecise);
        } else {
            Assert.assertEquals(
                    "Approximate setting mismatch", expectedSetting, currentSetting.mApproximate);
        }
    }

    /**
     * Wait for the permission setting for the given origin to change to the expected setting.
     *
     * @param contentSettingsType The ContentSettingsType to wait for.
     * @param expectedSetting The expected content setting.
     * @param pageUrl The origin URL of the page.
     */
    public void waitForPermissionSettingForOrigin(
            @ContentSettingsType.EnumType int contentSettingsType,
            @ContentSetting int expectedSetting,
            String pageUrl) {
        CriteriaHelper.pollInstrumentationThread(
                () -> {
                    try {
                        checkPermissionSettingForOrigin(
                                contentSettingsType, expectedSetting, pageUrl);
                    } catch (AssertionError e) {
                        throw new CriteriaNotSatisfiedException(e);
                    }
                });
    }

    /**
     * Wait for the geolocation permission setting for the given origin to change to the expected
     * setting.
     *
     * @param usePrecise If true, use precise location, otherwise approximate.
     * @param expectedSetting The expected content setting.
     * @param pageUrl The origin URL of the page.
     */
    public void waitForGeolocationSettingForOrigin(
            boolean usePrecise, @ContentSetting int expectedSetting, String pageUrl) {
        CriteriaHelper.pollInstrumentationThread(
                () -> {
                    try {
                        checkGeolocationSettingForOrigin(usePrecise, expectedSetting, pageUrl);
                    } catch (AssertionError e) {
                        throw new CriteriaNotSatisfiedException(e);
                    }
                });
    }

    /**
     * Runs a permission prompt test that grants the permission and expects the page title to be
     * updated in response.
     *
     * @param updateWaiter The update waiter to wait for callbacks. Should be added as an observer
     *     to the current tab prior to calling this method.
     * @param javascript The JS function to run in the current tab to execute the test and update
     *     the page title.
     * @param nUpdates How many updates of the page title to wait for.
     * @param withGesture True if we require a user gesture to trigger the prompt.
     * @param isDialog True if we are expecting a permission dialog, false for an infobar.
     */
    public void runAllowTest(
            PermissionUpdateWaiter updateWaiter,
            final String url,
            String javascript,
            int nUpdates,
            boolean withGesture,
            boolean isDialog)
            throws Exception {
        setUpUrl(url);
        if (withGesture) {
            runJavaScriptCodeInCurrentTabWithGesture(javascript);
        } else {
            runJavaScriptCodeInCurrentTab(javascript);
        }
        replyToPromptAndWaitForUpdates(updateWaiter, PromptDecision.ALLOW, nUpdates, isDialog);
    }

    /**
     * Runs a permission prompt test that does not grant the permission and expects the page title
     * to be updated in response.
     *
     * @param updateWaiter The update waiter to wait for callbacks. Should be added as an observer
     *     to the current tab prior to calling this method.
     * @param javascript The JS function to run in the current tab to execute the test and update
     *     the page title.
     * @param nUpdates How many updates of the page title to wait for.
     * @param withGesture True if we require a user gesture to trigger the prompt.
     * @param isDialog True if we are expecting a permission dialog, false for an infobar.
     */
    public void runDenyTest(
            PermissionUpdateWaiter updateWaiter,
            final String url,
            String javascript,
            int nUpdates,
            boolean withGesture,
            boolean isDialog)
            throws Exception {
        setUpUrl(url);
        if (withGesture) {
            runJavaScriptCodeInCurrentTabWithGesture(javascript);
        } else {
            runJavaScriptCodeInCurrentTab(javascript);
        }
        replyToPromptAndWaitForUpdates(updateWaiter, PromptDecision.DENY, nUpdates, isDialog);
    }

    /**
     * Runs a permission prompt test that expects no prompt to be displayed because the permission
     * is already granted/blocked and expects the page title to be updated.
     *
     * @param updateWaiter The update waiter to wait for callbacks. Should be added as an observer
     *     to the current tab prior to calling this method.
     * @param javascript The JS function to run in the current tab to execute the test and update
     *     the page title.
     * @param nUpdates How many updates of the page title to wait for.
     * @param withGesture True if we require a user gesture.
     * @param isDialog True if we are testing a permission dialog, false for an infobar.
     */
    public void runNoPromptTest(
            PermissionUpdateWaiter updateWaiter,
            final String url,
            String javascript,
            int nUpdates,
            boolean withGesture,
            boolean isDialog)
            throws Exception {
        setUpUrl(url);
        if (withGesture) {
            runJavaScriptCodeInCurrentTabWithGesture(javascript);
        } else {
            runJavaScriptCodeInCurrentTab(javascript);
        }
        waitForUpdatesAndAssertNoPrompt(updateWaiter, nUpdates, isDialog);
    }

    private void replyToPromptAndWaitForUpdates(
            PermissionUpdateWaiter updateWaiter,
            @PromptDecision int decision,
            int nUpdates,
            boolean isDialog)
            throws Exception {
        if (isDialog) {
            waitForDialogShownState(true);
            replyToDialogAndWaitForUpdates(updateWaiter, nUpdates, decision);
        } else {
            replyToInfoBarAndWaitForUpdates(updateWaiter, nUpdates, decision);
        }
    }

    private void waitForUpdatesAndAssertNoPrompt(
            PermissionUpdateWaiter updateWaiter, int nUpdates, boolean isDialog) throws Exception {
        updateWaiter.waitForNumUpdates(nUpdates);

        if (isDialog) {
            Assert.assertFalse(
                    "Modal permission prompt shown when none expected",
                    PermissionDialogController.getInstance().isDialogShownForTest());
        } else {
            Assert.assertEquals(
                    "Permission infobar shown when none expected", 0, getInfoBars().size());
        }
    }

    public void runJavaScriptCodeInCurrentTabWithGesture(String javascript)
            throws java.util.concurrent.TimeoutException {
        runJavaScriptCodeInCurrentTab("functionToRun = '" + javascript + "'");
        TouchCommon.singleClickView(getActivityTab().getView());
    }

    /**
     * Replies to an infobar permission prompt and waits for a provided number of updates to the
     * page title in response.
     */
    private void replyToInfoBarAndWaitForUpdates(
            PermissionUpdateWaiter updateWaiter, int nUpdates, @PromptDecision int decison)
            throws Exception {
        mListener.addInfoBarAnimationFinished("InfoBar not added.");
        InfoBar infobar = getInfoBars().get(0);
        Assert.assertNotNull(infobar);

        switch (decison) {
            case PromptDecision.ALLOW ->
                    Assert.assertTrue(
                            "Allow button wasn't found", InfoBarUtil.clickPrimaryButton(infobar));
            case PromptDecision.ALLOW_ONCE ->
                    throw new AssertionError("Allowing once is not supported on infobars.");
            case PromptDecision.DENY ->
                    Assert.assertTrue(
                            "Block button wasn't found", InfoBarUtil.clickSecondaryButton(infobar));
        }
        updateWaiter.waitForNumUpdates(nUpdates);
    }

    /**
     * Replies to a dialog permission prompt and waits for a provided number of updates to the page
     * title in response.
     */
    private void replyToDialogAndWaitForUpdates(
            PermissionUpdateWaiter updateWaiter,
            int nUpdates,
            final @PermissionTestRule.PromptDecision int decison)
            throws Exception {
        replyToDialog(decison, getActivity());
        updateWaiter.waitForNumUpdates(nUpdates);
    }

    /** Verify the shown state of the dialog. */
    protected void waitForDialogShownState(boolean expectedShowState) {
        waitForDialogShownState(getActivity(), expectedShowState);
    }

    /** Utility functions to support permissions testing in other contexts. */
    public static void replyToDialog(
            final @PermissionTestRule.PromptDecision int decision, ChromeActivity activity) {
        // Wait for button view to appear in view hierarchy. If the browser controls are not visible
        // then ModalDialogPresenter will first trigger animation for showing browser controls and
        // only then add modal dialog view into the container.
        @IdRes
        int buttonId =
                switch (decision) {
                    case PromptDecision.ALLOW -> ModalDialogProperties.ButtonType.POSITIVE;
                    case PromptDecision.ALLOW_ONCE ->
                            ModalDialogProperties.ButtonType.POSITIVE_EPHEMERAL;
                    case PromptDecision.DENY -> ModalDialogProperties.ButtonType.NEGATIVE;
                    default -> throw new IllegalStateException("Unexpected value: " + decision);
                };

        ViewUtils.onViewWaiting(
                        allOf(
                                withTagValue(is(ModalDialogView.getTagForButtonType(buttonId))),
                                isDisplayed()))
                .perform(click());
    }

    /** Wait for the permission dialog to be in the expected shown state. */
    public static void waitForDialogShownState(ChromeActivity activity, boolean expectedShowState) {
        ModalDialogManager dialogManager =
                ThreadUtils.runOnUiThreadBlocking(activity::getModalDialogManager);
        CriteriaHelper.pollUiThread(
                () -> {
                    boolean isDialogShownForTest =
                            PermissionDialogController.getInstance().isDialogShownForTest();
                    Criteria.checkThat(isDialogShownForTest, Matchers.is(expectedShowState));
                });
    }

    /** Wait for the permission dialog to be shown. */
    public static void waitForDialog(ChromeActivity activity) {
        waitForDialogShownState(activity, true);
    }

    /** Verify the shown state of the message. */
    protected void waitForMessageShownState(boolean expectedShowState) {
        waitForMessageShownState(getActivity(), expectedShowState);
    }

    /** Wait for the message to be in the expected shown state. */
    public static void waitForMessageShownState(
            ChromeActivity activity, boolean expectedShowState) {
        WindowAndroid windowAndroid = activity.getWindowAndroid();
        CriteriaHelper.pollUiThread(
                () -> {
                    int messageCount = MessagesTestHelper.getMessageCount(windowAndroid);
                    Criteria.checkThat(
                            "Message shown state does not match expectation",
                            messageCount > 0,
                            Matchers.is(expectedShowState));
                });
    }

    /** Wait for a specific permission state in the omnibox. */
    public void waitForOmniboxPermissionState(
            @ContentSettingsType.EnumType int contentSettingsType) {
        CriteriaHelper.pollUiThread(
                () -> {
                    LocationBarCoordinator locationBar =
                            (LocationBarCoordinator)
                                    getActivity().getToolbarManager().getLocationBar();
                    int lastPermission =
                            locationBar
                                    .getStatusCoordinator()
                                    .getMediatorForTesting()
                                    .getPermissionStatusHandler()
                                    .getLastPermissionForTest();
                    Criteria.checkThat(lastPermission, Matchers.is(contentSettingsType));
                });
    }
}
