// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.popups.testhtmls;

import org.chromium.base.test.transit.TripBuilder;
import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.transit.page.CtaPageStation;
import org.chromium.chrome.test.transit.page.WebPageStation;
import org.chromium.content_public.browser.test.transit.HtmlElement;
import org.chromium.content_public.browser.test.transit.HtmlElementSpec;
import org.chromium.content_public.browser.test.util.JavaScriptUtils;

/**
 * PageStation for popup_launcher.html, which contains links to open pop-ups with different
 * parameters.
 */
@NullMarked
public class PopupLauncherPageStation extends WebPageStation {
    private static final String PATH = "/chrome/test/data/android/popup_launcher.html";

    private final HtmlElement mLinkToOpenBasicPopup;
    private final HtmlElement mLinkToOpenPopupWithBounds;
    private final HtmlElement mLinkToOpenPopupWithSmallBounds;
    private final HtmlElement mLinkToOpenPopupWithMediumBounds;
    private final HtmlElement mLinkToClosePopup;
    private final HtmlElement mLinkToFocusPopup;
    private final HtmlElement mLinkToMovePopupTo;
    private final HtmlElement mLinkToMovePopupBy;
    private final HtmlElement mLinkToResizePopupTo;
    private final HtmlElement mLinkToResizePopupToSmallBounds;
    private final HtmlElement mLinkToResizePopupBy;

    protected PopupLauncherPageStation(Config config) {
        super(config);

        mLinkToOpenBasicPopup = getElementById("link_open_popup");
        mLinkToOpenPopupWithBounds = getElementById("link_open_popup_bounds");
        mLinkToOpenPopupWithSmallBounds = getElementById("link_open_popup_small_bounds");
        mLinkToOpenPopupWithMediumBounds = getElementById("link_open_popup_medium_bounds");
        mLinkToClosePopup = getElementById("link_close_popup");
        mLinkToFocusPopup = getElementById("link_focus_popup");
        mLinkToMovePopupTo = getElementById("link_move_popup_to");
        mLinkToMovePopupBy = getElementById("link_move_popup_by");
        mLinkToResizePopupTo = getElementById("link_resize_popup_to");
        mLinkToResizePopupToSmallBounds = getElementById("link_resize_popup_to_small_bounds");
        mLinkToResizePopupBy = getElementById("link_resize_popup_by");
    }

    /**
     * Load popup_launcher.html in current tab.
     *
     * @return A {@link PopupLauncherPageStation} representing the popup_launcher.html website.
     */
    public static PopupLauncherPageStation loadInCurrentTab(
            ChromeTabbedActivityTestRule activityTestRule, CtaPageStation currentPageStation) {
        final Builder<PopupLauncherPageStation> builder =
                new Builder<>(PopupLauncherPageStation::new);

        final String url = activityTestRule.getTestServer().getURL(PATH);
        return currentPageStation.loadPageProgrammatically(url, builder);
    }

    /**
     * Opens a sample page as a pop-up.
     *
     * @return A {@link TripBuilder} representing the click trigger.
     */
    public TripBuilder clickOpenPopup() {
        return mLinkToOpenBasicPopup.clickTo();
    }

    /**
     * Opens a sample page as a pop-up with launch bounds specified.
     *
     * @return A {@link TripBuilder} representing the click trigger.
     */
    public TripBuilder clickOpenPopupWithBounds() {
        return mLinkToOpenPopupWithBounds.clickTo();
    }

    /**
     * Opens a sample page as a pop-up with small launch bounds specified.
     *
     * @return A {@link TripBuilder} representing the click trigger.
     */
    public TripBuilder clickOpenPopupWithSmallBounds() {
        return mLinkToOpenPopupWithSmallBounds.clickTo();
    }

    /**
     * Opens a sample page as a pop-up with medium launch bounds specified.
     *
     * @return A {@link TripBuilder} representing the click trigger.
     */
    public TripBuilder clickOpenPopupWithMediumBounds() {
        return mLinkToOpenPopupWithMediumBounds.clickTo();
    }

    /**
     * Closes the previously opened pop-up using the {@code window.close()} web API.
     *
     * @return A {@link TripBuilder} representing the click trigger.
     */
    public TripBuilder clickClosePopup() throws Exception {
        assertPopupWindowExists();
        return mLinkToClosePopup.clickTo();
    }

    /**
     * Focuses the previously opened pop-up using the {@code window.focus()} web API.
     *
     * @return A {@link TripBuilder} representing the click trigger.
     */
    public TripBuilder clickFocusPopup() throws Exception {
        assertPopupWindowExists();
        return mLinkToFocusPopup.clickTo();
    }

    /**
     * Moves the previously opened pop-up to (75, 75) using the {@code window.moveTo()} web API.
     *
     * @return A {@link TripBuilder} representing the click trigger.
     */
    public TripBuilder clickMovePopupTo() throws Exception {
        assertPopupWindowExists();
        return mLinkToMovePopupTo.clickTo();
    }

    /**
     * Moves the previously opened pop-up by vector (30, -20) using the {@code window.moveBy()} web
     * API.
     *
     * @return A {@link TripBuilder} representing the click trigger.
     */
    public TripBuilder clickMovePopupBy() throws Exception {
        assertPopupWindowExists();
        return mLinkToMovePopupBy.clickTo();
    }

    /**
     * Resizes the previously opened pop-up to (420, 380) using the {@code window.resizeTo()} web
     * API.
     *
     * @return A {@link TripBuilder} representing the click trigger.
     */
    public TripBuilder clickResizePopupTo() throws Exception {
        assertPopupWindowExists();
        return mLinkToResizePopupTo.clickTo();
    }

    /**
     * Resizes the previously opened pop-up to (100, 100) using the {@code window.resizeTo()} web
     * API.
     *
     * @return A {@link TripBuilder} representing the click trigger.
     */
    public TripBuilder clickResizePopupToSmallBounds() throws Exception {
        assertPopupWindowExists();
        return mLinkToResizePopupToSmallBounds.clickTo();
    }

    /**
     * Resizes the previously opened pop-up by (-15, 35) using the {@code window.resizeBy()} web
     * API.
     *
     * @return A {@link TripBuilder} representing the click trigger.
     */
    public TripBuilder clickResizePopupBy() throws Exception {
        assertPopupWindowExists();
        return mLinkToResizePopupBy.clickTo();
    }

    private HtmlElement getElementById(String id) {
        return declareElement(new HtmlElement(new HtmlElementSpec(id), webContentsElement));
    }

    private void assertPopupWindowExists() throws Exception {
        final String result =
                JavaScriptUtils.executeJavaScriptAndWaitForResult(
                        webContentsElement.get(), "basicPopupWindow === null");
        if (result.equals("true")) {
            throw new IllegalStateException("There is no pop-up window to act upon.");
        }
    }
}
